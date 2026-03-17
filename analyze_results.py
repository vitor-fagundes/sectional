#!/usr/bin/env python3
"""
SECTIONAL - Análise de Resultados de Simulação NS-3
Extrai métricas dos logs e gera CSVs para o artigo SBRC 2026.

Abordagens:
  - optimal: sem falhas (baseline)
  - with-fail: com falhas, sem RL (baseline com falha)
  - rl1: RL v1 — realoca para cluster existente ou não aloca
  - rl2: RL v2 — forma novos clusters a partir de órfãos
  - rl3: RL v3 — 3 ações: não alocar, realocar existente, formar novo cluster
  - rl3.1: RL v3.1 — RL3 com processamento context-aware de órfãos e bônus por grupo
  - intuitive: Aprendizado Intuitivo — framework dual-system (S1/S2) com QI adaptativo

Cada abordagem RL tem 7 sub-cenários × 35 rodadas.
"""

import os
import re
import math
import logging
from collections import defaultdict
from pathlib import Path

import pandas as pd
import numpy as np
from scipy import stats

logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
log = logging.getLogger(__name__)

BASE_DIR = Path(__file__).parent / "all_results"
OUTPUT_DIR = Path(__file__).parent / "csvs_analise"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)


# ── Scenario discovery ──────────────────────────────────────────────────────

def discover_scenarios():
    """Discover all approach/scenario combinations available."""
    scenarios = []

    # RL approaches + intuitive — structure: approach/cenarioX_*/200_nodes/config/
    for approach in ["rl1", "rl2", "rl3", "intuitive"]:
        approach_dir = BASE_DIR / approach
        if not approach_dir.exists():
            continue
        for cenario_dir in sorted(approach_dir.iterdir()):
            if not cenario_dir.is_dir():
                continue
            nodes_dir = cenario_dir / "200_nodes"
            if not nodes_dir.exists():
                continue
            for sub_dir in sorted(nodes_dir.iterdir()):
                if not sub_dir.is_dir():
                    continue
                outputs_dir = sub_dir / "outputs"
                if outputs_dir.exists():
                    scenario_name = f"{cenario_dir.name}/{sub_dir.name}"
                    scenarios.append({
                        "approach": approach,
                        "scenario": scenario_name,
                        "outputs_dir": outputs_dir,
                        "apstats_dir": sub_dir / "APStats",
                        "qtables_dir": sub_dir / "qtables",
                        "tasks_dir": sub_dir / "tasks",
                    })

    # RL3.1 — different dir structure: rl3.1/cenario1-310s/10percent/ (no 200_nodes level)
    # Config dir names like "10percent" need to be mapped to match other approaches ("310s_10percent")
    rl31_dir = BASE_DIR / "rl3.1"
    if rl31_dir.exists():
        rl31_scenario_mapping = {
            "cenario1-310s": "cenario1_tempo_fixo",
            "cenario2-180-600s": "cenario2_tempo_aleatorio",
            "cenario3-full-random": "cenario3_tudo_aleatorio",
        }
        rl31_config_mapping = {
            "cenario1-310s": {
                "10percent": "310s_10percent",
                "20percent": "310s_20percent",
                "30percent": "310s_30percent",
            },
            "cenario2-180-600s": {
                "10percent": "180-600s_10percent",
                "20percent": "180-600s_20percent",
                "30percent": "180-600s_30percent",
            },
            "cenario3-full-random": {},  # already matches: "180-600s_10-30percent"
        }
        for cenario_dir in sorted(rl31_dir.iterdir()):
            if not cenario_dir.is_dir():
                continue
            cenario_mapped = rl31_scenario_mapping.get(cenario_dir.name, cenario_dir.name)
            config_map = rl31_config_mapping.get(cenario_dir.name, {})
            for config_dir in sorted(cenario_dir.iterdir()):
                if not config_dir.is_dir():
                    continue
                outputs_dir = config_dir / "outputs"
                if outputs_dir.exists():
                    config_mapped = config_map.get(config_dir.name, config_dir.name)
                    scenario_name = f"{cenario_mapped}/{config_mapped}"
                    scenarios.append({
                        "approach": "rl3.1",
                        "scenario": scenario_name,
                        "outputs_dir": outputs_dir,
                        "apstats_dir": config_dir / "APStats",
                        "qtables_dir": config_dir / "qtables",
                        "tasks_dir": config_dir / "tasks",
                    })

    # with-fail (different directory structure)
    wf_dir = BASE_DIR / "with-fail"
    if wf_dir.exists():
        wf_mapping = {
            "with-fail/200_nodes/310s_10percent": "cenario1_tempo_fixo/310s_10percent",
            "with-fail/200_nodes/310s_20percent": "cenario1_tempo_fixo/310s_20percent",
            "with-fail/200_nodes/310s_30percent": "cenario1_tempo_fixo/310s_30percent",
            "with-fail-random-time/200_nodes/180-600s_10percent": "cenario2_tempo_aleatorio/180-600s_10percent",
            "with-fail-random-time/200_nodes/180-600s_20percent": "cenario2_tempo_aleatorio/180-600s_20percent",
            "with-fail-random-time/200_nodes/180-600s_30percent": "cenario2_tempo_aleatorio/180-600s_30percent",
            "with-fail-full-random/200_nodes/120-600s_10-30percent": "cenario3_tudo_aleatorio/180-600s_10-30percent",
        }
        for wf_path, scenario_name in wf_mapping.items():
            sub_dir = wf_dir / wf_path
            outputs_dir = sub_dir / "outputs"
            if outputs_dir.exists():
                scenarios.append({
                    "approach": "with-fail",
                    "scenario": scenario_name,
                    "outputs_dir": outputs_dir,
                    "apstats_dir": sub_dir / "APStats",
                    "qtables_dir": None,
                    "tasks_dir": sub_dir / "tasks",
                })

    # optimal (single scenario, no failures)
    opt_dir = BASE_DIR / "optimal" / "200_nodes"
    if opt_dir.exists():
        scenarios.append({
            "approach": "optimal",
            "scenario": "sem_falha",
            "outputs_dir": opt_dir / "outputs",
            "apstats_dir": opt_dir / "APStats",
            "qtables_dir": None,
            "tasks_dir": opt_dir / "tasks",
        })

    return scenarios


def get_run_files(outputs_dir):
    """Get sorted list of (run_number, filepath) for output files."""
    files = []
    for f in sorted(outputs_dir.glob("output_run_*.txt")):
        m = re.search(r'output_run_(\d+)\.txt', f.name)
        if m:
            files.append((int(m.group(1)), f))
    return sorted(files, key=lambda x: x[0])


# ── Parsing ──────────────────────────────────────────────────────────────────

def parse_output_file(filepath, approach):
    """Parse a single output file and extract all metrics."""
    try:
        content = filepath.read_text(errors='replace')
    except Exception as e:
        log.warning(f"Cannot read {filepath}: {e}")
        return None

    metrics = {}

    # ── Determine failure time first (needed for cluster filtering) ──
    # Time can be integer (320) or decimal (331.173) depending on scenario
    failure_events = re.findall(r'FAILURE: Líder (\S+) falhou no tempo (\d+\.?\d*)', content)
    failure_events_wf = re.findall(r'FAILURE: Líder (\S+) falhou no tempo (\d+\.?\d*) - (\d+) nós órfãos', content)
    failure_times = [float(t) for _, t in failure_events]
    if not failure_times and failure_events_wf:
        failure_times = [float(t) for _, t, _ in failure_events_wf]
    failure_time = failure_times[0] if failure_times else float('nan')
    metrics['failure_time'] = failure_time

    # ── Clustering ──
    # For the intuitive approach, count only clusters registered before the
    # first failure (LR: timestamp < failureTime).  CONTASKI forms clusters
    # between t≈90 and t≈150; any LR: after t≈200 would be an artefact of
    # the intuitive motor's RECLUSTER_ORPHANS action.
    if approach == 'intuitive' and not math.isnan(failure_time):
        lr_events = re.findall(r'LR: (\S+) at ([\d.]+)', content)
        pre_failure_leaders = set()
        for leader, ts in lr_events:
            if float(ts) < failure_time:
                pre_failure_leaders.add(leader)
        metrics['total_clusters'] = len(pre_failure_leaders)
    else:
        ls_events = re.findall(r'N: LS (\S+)', content)
        metrics['total_clusters'] = len(ls_events)

    # Clusters that accepted at least one task (LA = Leader Accepts)
    la_events = re.findall(r'N: LA (\S+), (\d+), (\S+)', content)
    clusters_apt = set()
    for leader, task_id, time in la_events:
        clusters_apt.add(leader)
    metrics['clusters_apt'] = len(clusters_apt)
    metrics['clusters_idle'] = max(0, metrics['total_clusters'] - metrics['clusters_apt'])

    # ── Tasks ──
    td_events = re.findall(r'AP: TD (\d+) (\S+)', content)
    ta_events = re.findall(r'AP: TA (\d+), (\S+) (\S+)', content)
    quorum_satisfied = re.findall(r'AP: Task (\d+) quorum satisfied: (\d+)/(\d+)', content)
    quorum_failed = re.findall(r'AP: Task (\d+) failed quorum check: (\d+)/(\d+)', content)

    metrics['tasks_dispatched'] = len(td_events)
    metrics['tasks_quorum_ok'] = len(quorum_satisfied)
    metrics['tasks_quorum_fail'] = len(quorum_failed)

    # Unique tasks that got >= 1 TA
    task_acceptances = defaultdict(list)
    for task_id, leader_ip, time in ta_events:
        task_acceptances[task_id].append(float(time))
    metrics['tasks_accepted_count'] = len(task_acceptances)
    metrics['total_acceptances'] = len(ta_events)

    # Success rate
    if metrics['tasks_dispatched'] > 0:
        metrics['task_success_rate'] = metrics['tasks_quorum_ok'] / metrics['tasks_dispatched']
    else:
        metrics['task_success_rate'] = float('nan')

    # Acceptance latency
    td_times = {}
    for task_id, time in td_events:
        td_times[task_id] = float(time)

    latencies = []
    for task_id, times in task_acceptances.items():
        if task_id in td_times:
            latency = min(times) - td_times[task_id]
            if latency >= 0:
                latencies.append(latency)
    metrics['avg_accept_latency'] = np.mean(latencies) if latencies else float('nan')
    metrics['median_accept_latency'] = np.median(latencies) if latencies else float('nan')

    # ── Failures / Orphans ──
    # For the intuitive approach, prefer the FAILURE summary line
    # ("FAILURE: X de Y líderes") which gives the authoritative count,
    # and extract orphans only from the first failure block (before the
    # first INTUITIVE_DECISION: cycle).
    if approach == 'intuitive':
        # Authoritative failure count from summary line
        failure_summary = re.search(r'FAILURE: (\d+) de (\d+) líderes', content)
        if failure_summary:
            metrics['num_leader_failures'] = int(failure_summary.group(1))
        else:
            metrics['num_leader_failures'] = len(failure_events)

        # Orphan count: use ORPHAN_TOTAL if available, else sum from
        # "FAILURE: Líder X ... - Y nós órfãos" lines from the FIRST
        # failure block only (between first FAILURE: and the next
        # INTUITIVE: t= decision line after it).
        orphan_total_m = re.search(r'ORPHAN_TOTAL: (\d+)', content)
        if orphan_total_m:
            metrics['orphan_total'] = int(orphan_total_m.group(1))
        elif failure_events_wf:
            # Delimit the first failure block: from first FAILURE: to the
            # next "INTUITIVE: t=" decision line that follows it.
            first_failure_pos = content.find('FAILURE:')
            if first_failure_pos >= 0:
                # Find the next INTUITIVE decision cycle after the failure block
                next_decision = content.find('INTUITIVE: t=', first_failure_pos)
                if next_decision > 0:
                    first_block = content[first_failure_pos:next_decision]
                else:
                    first_block = content[first_failure_pos:]
            else:
                first_block = content
            first_block_wf = re.findall(
                r'FAILURE: Líder \S+ falhou no tempo [\d.]+ - (\d+) nós órfãos',
                first_block
            )
            metrics['orphan_total'] = sum(int(n) for n in first_block_wf)
        else:
            orphan_events = re.findall(r'ORPHAN: (\S+) \(was member of (\S+)\)', content)
            metrics['orphan_total'] = len(orphan_events)
    else:
        # Non-intuitive approaches: original logic
        orphan_total_m = re.search(r'ORPHAN_TOTAL: (\d+)', content)
        orphan_events = re.findall(r'ORPHAN: (\S+) \(was member of (\S+)\)', content)
        metrics['num_leader_failures'] = max(len(failure_events), len(failure_events_wf))
        metrics['orphan_total'] = int(orphan_total_m.group(1)) if orphan_total_m else len(orphan_events)
        # with-fail reports orphans inline in FAILURE message
        if metrics['orphan_total'] == 0 and failure_events_wf:
            metrics['orphan_total'] = sum(int(n) for _, _, n in failure_events_wf)

    metrics['orphan_percentage'] = metrics['orphan_total'] / 200.0

    # ── RL metrics ──
    metrics['rl_successful_realloc'] = 0
    metrics['rl_failed_realloc'] = 0
    metrics['rl_new_clusters'] = 0
    metrics['rl_realloc_existing'] = 0
    metrics['realloc_rate'] = float('nan')

    if approach == 'rl1':
        m = re.search(r'RL_METRICS: Successful reallocations: (\d+)', content)
        if m: metrics['rl_successful_realloc'] = int(m.group(1))
        m = re.search(r'RL_METRICS: Failed reallocations: (\d+)', content)
        if m: metrics['rl_failed_realloc'] = int(m.group(1))
        metrics['rl_realloc_existing'] = metrics['rl_successful_realloc']

    elif approach == 'rl2':
        m = re.search(r'RL2_METRICS: Successful reallocations: (\d+)', content)
        if m: metrics['rl_successful_realloc'] = int(m.group(1))
        m = re.search(r'RL2_METRICS: Failed reallocations: (\d+)', content)
        if m: metrics['rl_failed_realloc'] = int(m.group(1))
        m = re.search(r'RL2_METRICS: New clusters formed: (\d+)', content)
        if m: metrics['rl_new_clusters'] = int(m.group(1))

    elif approach == 'rl3':
        m = re.search(r'RL3_METRICS: Successful reallocations: (\d+)', content)
        if m: metrics['rl_successful_realloc'] = int(m.group(1))
        m = re.search(r'RL3_METRICS: Failed reallocations: (\d+)', content)
        if m: metrics['rl_failed_realloc'] = int(m.group(1))
        m = re.search(r'RL3_METRICS: Reallocated to existing clusters: (\d+)', content)
        if m: metrics['rl_realloc_existing'] = int(m.group(1))
        m_s = re.search(
            r'RL3_SUMMARY: (\d+) reallocated to existing, (\d+) new clusters formed, (\d+) nodes in new clusters, (\d+) still orphaned',
            content
        )
        if m_s:
            metrics['rl_realloc_existing'] = int(m_s.group(1))
            metrics['rl_new_clusters'] = int(m_s.group(2))

    elif approach == 'rl3.1':
        m = re.search(r'RL3\.1_METRICS: Successful reallocations: (\d+)', content)
        if m: metrics['rl_successful_realloc'] = int(m.group(1))
        m = re.search(r'RL3\.1_METRICS: Failed reallocations: (\d+)', content)
        if m: metrics['rl_failed_realloc'] = int(m.group(1))
        m = re.search(r'RL3\.1_METRICS: Reallocated to existing clusters: (\d+)', content)
        if m: metrics['rl_realloc_existing'] = int(m.group(1))
        m_s = re.search(
            r'RL3\.1_SUMMARY: (\d+) reallocated to existing, (\d+) new clusters formed, (\d+) nodes in new clusters, (\d+) still orphaned',
            content
        )
        if m_s:
            metrics['rl_realloc_existing'] = int(m_s.group(1))
            metrics['rl_new_clusters'] = int(m_s.group(2))

    elif approach == 'intuitive':
        # Stateful line-by-line parsing to avoid double-counting when
        # RECLUSTER and REALLOCATE both act on the same orphan pool.
        lines = content.split('\n')
        remaining_orphans = metrics['orphan_total']
        realloc_existing = 0
        not_reallocated = 0
        recluster_nodes = 0
        recluster_count = 0
        first_realloc_found = False
        first_recluster_found = False
        pending_cluster_followers = []

        for line in lines:
            # Recluster summary (preferred) — first effective only
            if not first_recluster_found:
                ncs = re.search(
                    r'INTUITIVE_ACTION: (\d+) novos clusters formados de (\d+) órfãos', line
                )
                if ncs:
                    n_clusters = int(ncs.group(1))
                    n_orphans = int(ncs.group(2))
                    counted = min(n_orphans, max(0, remaining_orphans))
                    recluster_nodes += counted
                    recluster_count += n_clusters
                    remaining_orphans -= counted
                    pending_cluster_followers = []
                    first_recluster_found = True
                    continue

            # Individual new cluster line (fallback) — only before first summary
            if not first_recluster_found:
                ncm = re.search(
                    r'INTUITIVE_ACTION: Novo cluster formado - Líder \S+ com (\d+) seguidores', line
                )
                if ncm:
                    pending_cluster_followers.append(int(ncm.group(1)))
                    continue

            # Flush pending cluster lines on new cycle
            if pending_cluster_followers and re.match(r'INTUITIVE:', line):
                n_from = sum(f + 1 for f in pending_cluster_followers)
                counted = min(n_from, max(0, remaining_orphans))
                recluster_nodes += counted
                recluster_count += len(pending_cluster_followers)
                remaining_orphans -= counted
                pending_cluster_followers = []
                first_recluster_found = True

            # Reallocate summary — first effective only
            if not first_realloc_found:
                rm = re.search(
                    r'INTUITIVE_ACTION: Realocados (\d+), não realocados (\d+)', line
                )
                if rm:
                    r, nr = int(rm.group(1)), int(rm.group(2))
                    if r > 0 or nr > 0:
                        counted = min(r, max(0, remaining_orphans))
                        realloc_existing = counted
                        not_reallocated = nr
                        remaining_orphans -= counted
                        first_realloc_found = True

        # Flush remaining pending cluster lines at EOF
        if pending_cluster_followers and not first_recluster_found:
            n_from = sum(f + 1 for f in pending_cluster_followers)
            counted = min(n_from, max(0, remaining_orphans))
            recluster_nodes += counted
            recluster_count += len(pending_cluster_followers)

        metrics['rl_realloc_existing'] = realloc_existing
        metrics['rl_new_clusters'] = recluster_count
        metrics['rl_successful_realloc'] = realloc_existing + recluster_nodes
        metrics['rl_failed_realloc'] = not_reallocated

    # Reallocation rate (clamped to [0, 1])
    if metrics['orphan_total'] > 0 and approach in ('rl1', 'rl2', 'rl3', 'rl3.1', 'intuitive'):
        metrics['realloc_rate'] = min(1.0, metrics['rl_successful_realloc'] / metrics['orphan_total'])
    elif approach == 'with-fail':
        metrics['realloc_rate'] = 0.0

    return metrics


def parse_qtable_file(filepath):
    """Parse Q-Table CSV."""
    try:
        return pd.read_csv(filepath)
    except Exception:
        return None


# ── Confidence interval ──────────────────────────────────────────────────────

def ci95(series):
    """95% CI half-width using t-distribution."""
    n = series.count()
    if n < 2:
        return float('nan')
    se = series.std() / math.sqrt(n)
    return stats.t.ppf(0.975, df=n - 1) * se


# ── Main ─────────────────────────────────────────────────────────────────────

def analyze_all():
    scenarios = discover_scenarios()
    log.info(f"Discovered {len(scenarios)} scenario configurations")

    all_clustering = []
    all_tasks = []
    all_realloc = []
    all_qtables = []
    all_resilience = []

    for sc in scenarios:
        approach = sc['approach']
        scenario = sc['scenario']
        run_files = get_run_files(sc['outputs_dir'])
        log.info(f"  {approach}/{scenario}: {len(run_files)} runs")

        for run_num, filepath in run_files:
            m = parse_output_file(filepath, approach)
            if m is None:
                continue

            # CSV 1: Clustering
            all_clustering.append({
                'approach': approach,
                'scenario': scenario,
                'run': run_num,
                'total_clusters': m['total_clusters'],
                'clusters_apt': m['clusters_apt'],
                'clusters_idle': m['clusters_idle'],
                'orphan_total': m['orphan_total'],
                'orphan_percentage': round(m['orphan_percentage'] * 100, 2),
            })

            # CSV 2: Tasks
            all_tasks.append({
                'approach': approach,
                'scenario': scenario,
                'run': run_num,
                'tasks_dispatched': m['tasks_dispatched'],
                'tasks_accepted': m['tasks_accepted_count'],
                'total_acceptances': m['total_acceptances'],
                'tasks_quorum_ok': m['tasks_quorum_ok'],
                'tasks_quorum_fail': m['tasks_quorum_fail'],
                'task_success_rate': round(m['task_success_rate'], 4) if not math.isnan(m['task_success_rate']) else float('nan'),
                'avg_accept_latency_s': round(m['avg_accept_latency'], 4) if not math.isnan(m['avg_accept_latency']) else float('nan'),
                'median_accept_latency_s': round(m['median_accept_latency'], 4) if not math.isnan(m['median_accept_latency']) else float('nan'),
            })

            # CSV 3: Reallocation (not for optimal)
            if approach != 'optimal':
                all_realloc.append({
                    'approach': approach,
                    'scenario': scenario,
                    'run': run_num,
                    'num_leader_failures': m['num_leader_failures'],
                    'orphan_total': m['orphan_total'],
                    'rl_successful_realloc': m['rl_successful_realloc'],
                    'rl_failed_realloc': m['rl_failed_realloc'],
                    'realloc_rate': round(m['realloc_rate'], 4) if not math.isnan(m['realloc_rate']) else float('nan'),
                    'rl_new_clusters': m['rl_new_clusters'],
                    'rl_realloc_existing': m['rl_realloc_existing'],
                })

            # CSV 5: Resilience (not for optimal)
            if approach != 'optimal':
                conn_idx = m['rl_successful_realloc'] / m['orphan_total'] if m['orphan_total'] > 0 else float('nan')
                all_resilience.append({
                    'approach': approach,
                    'scenario': scenario,
                    'run': run_num,
                    'failure_time': m['failure_time'],
                    'orphan_total': m['orphan_total'],
                    'successful_realloc': m['rl_successful_realloc'],
                    'failed_realloc': m['rl_failed_realloc'],
                    'tasks_quorum_ok': m['tasks_quorum_ok'],
                    'tasks_quorum_fail': m['tasks_quorum_fail'],
                    'connectivity_index': round(conn_idx, 4) if not math.isnan(conn_idx) else float('nan'),
                })

            # CSV 4: Q-Tables
            if sc['qtables_dir'] and sc['qtables_dir'].exists():
                qt_file = sc['qtables_dir'] / f"qtable_run_{run_num}.csv"
                if qt_file.exists():
                    qt_df = parse_qtable_file(qt_file)
                    if qt_df is not None:
                        if approach == 'rl3.1':
                            state_names = {0: 'SIM_BOTH_HIGH', 1: 'SIM_EXISTING_HIGH', 2: 'SIM_ORPHAN_HIGH', 3: 'SIM_BOTH_MEDIUM'}
                            action_names = {0: 'DO_NOT_ALLOC', 1: 'REALLOC_EXIST', 2: 'FORM_NEW_CLUST'}
                        elif approach == 'rl3':
                            state_names = {0: 'SIM_ORPHAN_HIGH', 1: 'SIM_EXISTING_HIGH', 2: 'SIM_ORPHAN_LOW', 3: 'SIM_EXISTING_LOW'}
                            action_names = {0: 'DO_NOT_ALLOC', 1: 'REALLOC_EXIST', 2: 'FORM_NEW_CLUST'}
                        elif approach == 'rl2':
                            state_names = {0: 'SIMILARITY_MEDIUM', 1: 'SIMILARITY_HIGH'}
                            action_names = {0: 'DO_NOT_INCLUDE', 1: 'INCLUDE_IN_CLUSTER'}
                        else:
                            state_names = {0: 'SIMILARITY_MEDIUM', 1: 'SIMILARITY_HIGH'}
                            action_names = {0: 'DO_NOT_ALLOCATE', 1: 'ALLOCATE'}

                        for _, row in qt_df.iterrows():
                            s = int(row['state'])
                            a = int(row['action'])
                            all_qtables.append({
                                'approach': approach,
                                'scenario': scenario,
                                'run': run_num,
                                'state': s,
                                'state_name': state_names.get(s, f'STATE_{s}'),
                                'action': a,
                                'action_name': action_names.get(a, f'ACTION_{a}'),
                                'qvalue': round(float(row['qvalue']), 6),
                            })

    # ── Save CSVs ──
    log.info("\nSaving CSVs...")

    df_clustering = pd.DataFrame(all_clustering)
    df_clustering.to_csv(OUTPUT_DIR / "metricas_clustering.csv", index=False)
    log.info(f"  metricas_clustering.csv: {len(df_clustering)} rows")

    df_tasks = pd.DataFrame(all_tasks)
    df_tasks.to_csv(OUTPUT_DIR / "metricas_tarefas.csv", index=False)
    log.info(f"  metricas_tarefas.csv: {len(df_tasks)} rows")

    df_realloc = pd.DataFrame(all_realloc)
    df_realloc.to_csv(OUTPUT_DIR / "metricas_realocacao_rl.csv", index=False)
    log.info(f"  metricas_realocacao_rl.csv: {len(df_realloc)} rows")

    df_qtables = pd.DataFrame(all_qtables)
    df_qtables.to_csv(OUTPUT_DIR / "evolucao_qtable.csv", index=False)
    log.info(f"  evolucao_qtable.csv: {len(df_qtables)} rows")

    df_resilience = pd.DataFrame(all_resilience)
    df_resilience.to_csv(OUTPUT_DIR / "metricas_resiliencia.csv", index=False)
    log.info(f"  metricas_resiliencia.csv: {len(df_resilience)} rows")

    # CSV 6: Summary
    generate_summary(df_clustering, df_tasks, df_realloc, df_resilience)

    # Terminal summary
    print_summary(df_clustering, df_tasks, df_realloc)

    return df_clustering, df_tasks, df_realloc, df_qtables, df_resilience


def generate_summary(df_clust, df_tasks, df_realloc, df_resil):
    """CSV 6: summary statistics per approach/scenario with 95% CI."""
    rows = []
    for approach in df_clust['approach'].unique():
        for scenario in df_clust[df_clust['approach'] == approach]['scenario'].unique():
            mc = (df_clust['approach'] == approach) & (df_clust['scenario'] == scenario)
            mt = (df_tasks['approach'] == approach) & (df_tasks['scenario'] == scenario)
            c = df_clust[mc]
            t = df_tasks[mt]

            row = {
                'approach': approach,
                'scenario': scenario,
                'n_runs': len(c),
                'clusters_mean': round(c['total_clusters'].mean(), 2),
                'clusters_ci95': round(ci95(c['total_clusters']), 2),
                'clusters_apt_mean': round(c['clusters_apt'].mean(), 2),
                'clusters_apt_ci95': round(ci95(c['clusters_apt']), 2),
                'orphans_mean': round(c['orphan_total'].mean(), 2),
                'orphans_ci95': round(ci95(c['orphan_total']), 2),
                'tasks_dispatched_mean': round(t['tasks_dispatched'].mean(), 2),
                'tasks_dispatched_ci95': round(ci95(t['tasks_dispatched']), 2),
                'tasks_success_rate_mean': round(t['task_success_rate'].mean(), 4),
                'tasks_success_rate_ci95': round(ci95(t['task_success_rate']), 4),
                'accept_latency_mean': round(t['avg_accept_latency_s'].mean(), 4),
                'accept_latency_ci95': round(ci95(t['avg_accept_latency_s']), 4),
            }

            if approach != 'optimal' and len(df_realloc) > 0:
                mr = (df_realloc['approach'] == approach) & (df_realloc['scenario'] == scenario)
                r = df_realloc[mr]
                if len(r) > 0:
                    row['realloc_success_mean'] = round(r['rl_successful_realloc'].mean(), 2)
                    row['realloc_success_ci95'] = round(ci95(r['rl_successful_realloc']), 2)
                    row['realloc_rate_mean'] = round(r['realloc_rate'].mean(), 4)
                    row['realloc_rate_median'] = round(r['realloc_rate'].median(), 4)
                    row['realloc_rate_ci95'] = round(ci95(r['realloc_rate']), 4)
                    row['new_clusters_mean'] = round(r['rl_new_clusters'].mean(), 2)
                    row['new_clusters_ci95'] = round(ci95(r['rl_new_clusters']), 2)

            if approach != 'optimal' and len(df_resil) > 0:
                mres = (df_resil['approach'] == approach) & (df_resil['scenario'] == scenario)
                res = df_resil[mres]
                if len(res) > 0:
                    row['connectivity_index_mean'] = round(res['connectivity_index'].mean(), 4)
                    row['connectivity_index_ci95'] = round(ci95(res['connectivity_index']), 4)

            rows.append(row)

    df = pd.DataFrame(rows)
    df.to_csv(OUTPUT_DIR / "resumo_estatistico.csv", index=False)
    log.info(f"  resumo_estatistico.csv: {len(df)} rows")


def print_summary(df_clust, df_tasks, df_realloc):
    """Print summary to terminal."""
    print("\n" + "=" * 100)
    print("RESUMO DAS MÉTRICAS POR ABORDAGEM E CENÁRIO")
    print("=" * 100)

    for approach in sorted(df_tasks['approach'].unique()):
        print(f"\n{'─' * 80}")
        print(f"  ABORDAGEM: {approach.upper()}")
        print(f"{'─' * 80}")

        for scenario in sorted(df_tasks[df_tasks['approach'] == approach]['scenario'].unique()):
            mt = (df_tasks['approach'] == approach) & (df_tasks['scenario'] == scenario)
            mc = (df_clust['approach'] == approach) & (df_clust['scenario'] == scenario)
            t = df_tasks[mt]
            c = df_clust[mc]

            sr = t['task_success_rate'].mean()
            sr_ci = ci95(t['task_success_rate'])
            td = t['tasks_dispatched'].mean()
            lat = t['avg_accept_latency_s'].mean()
            cl = c['total_clusters'].mean()
            orph = c['orphan_total'].mean()

            print(f"\n  Cenário: {scenario}")
            print(f"    Clusters formados:     {cl:.1f}")
            print(f"    Órfãos:                {orph:.1f}")
            print(f"    Tarefas despachadas:    {td:.1f}")
            print(f"    Taxa de sucesso:        {sr:.2%} ± {sr_ci:.2%}")
            print(f"    Latência média aceite:  {lat:.4f}s")

            if approach in ('rl1', 'rl2', 'rl3', 'rl3.1', 'intuitive') and len(df_realloc) > 0:
                mr = (df_realloc['approach'] == approach) & (df_realloc['scenario'] == scenario)
                r = df_realloc[mr]
                if len(r) > 0:
                    rr = r['realloc_rate'].mean()
                    rr_ci = ci95(r['realloc_rate'])
                    print(f"    Taxa realocação RL:     {rr:.2%} ± {rr_ci:.2%}")
                    print(f"    Novos clusters RL:      {r['rl_new_clusters'].mean():.1f}")

    print(f"\n{'=' * 100}")
    print(f"CSVs salvos em: {OUTPUT_DIR}")
    print(f"{'=' * 100}\n")


if __name__ == '__main__':
    analyze_all()
