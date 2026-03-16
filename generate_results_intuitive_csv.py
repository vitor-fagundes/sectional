#!/usr/bin/env python3
"""
SECTIONAL - Gerador de CSVs para resultados da abordagem Intuitive Learning.
Extrai métricas dos logs de simulação e gera um CSV por sub-cenário.

Uso:
    python3 generate_results_intuitive_csv.py all_results/intuitive --all
    python3 generate_results_intuitive_csv.py all_results/intuitive/cenario1_tempo_fixo/200_nodes/310s_10percent
"""

import argparse
import csv
import math
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


def parse_intuitive_qvalues(filepath):
    """Parse IntuitiveQValues_run_X.txt file."""
    result = {
        'qv_do_nothing': float('nan'),
        'qv_reallocate_existing': float('nan'),
        'qv_recluster_orphans': float('nan'),
        'epsilon_final': float('nan'),
        's1_count': 0,
        's2_count': 0,
    }
    try:
        content = filepath.read_text(errors='replace')
    except Exception:
        return result

    m = re.search(r'DO_NOTHING:\s*([\d.e+-]+)', content)
    if m: result['qv_do_nothing'] = float(m.group(1))
    m = re.search(r'REALLOCATE_TO_EXISTING:\s*([\d.e+-]+)', content)
    if m: result['qv_reallocate_existing'] = float(m.group(1))
    m = re.search(r'RECLUSTER_ORPHANS:\s*([\d.e+-]+)', content)
    if m: result['qv_recluster_orphans'] = float(m.group(1))
    m = re.search(r'Epsilon final:\s*([\d.e+-]+)', content)
    if m: result['epsilon_final'] = float(m.group(1))
    m = re.search(r'S1 count:\s*(\d+)', content)
    if m: result['s1_count'] = int(m.group(1))
    m = re.search(r'S2 count:\s*(\d+)', content)
    if m: result['s2_count'] = int(m.group(1))

    return result


def parse_output_file(filepath):
    """Parse a single output_run_X.txt file and extract all metrics."""
    try:
        content = filepath.read_text(errors='replace')
    except Exception as e:
        print(f"  WARN: Cannot read {filepath}: {e}")
        return None

    m = {}

    # ── Clustering ──
    ls_events = re.findall(r'N: LS (\S+)', content)
    m['total_agrupamentos'] = len(set(ls_events))

    # Leaders that accepted at least one task
    ta_events = re.findall(r'AP: TA (\d+), (\S+) (\S+)', content)
    leaders_apt = set()
    task_acceptances = defaultdict(list)
    for task_id, leader_ip, time_str in ta_events:
        leaders_apt.add(leader_ip)
        task_acceptances[task_id].append(float(time_str))
    m['agrupamentos_aptos'] = len(leaders_apt)
    m['agrupamentos_ociosos'] = max(0, m['total_agrupamentos'] - m['agrupamentos_aptos'])

    # ── Tasks ──
    td_events = re.findall(r'AP: TD (\d+) (\S+)', content)
    quorum_ok = re.findall(r'AP: Task (\d+) quorum satisfied: (\d+)/(\d+)', content)
    quorum_fail = re.findall(r'AP: Task (\d+) failed quorum check: (\d+)/(\d+)', content)

    td_times = {}
    for task_id, time_str in td_events:
        td_times[task_id] = float(time_str)

    m['tarefas_despachadas'] = len(set(td_times.keys()))
    m['tarefas_executadas'] = len(quorum_ok)

    # Acceptance latency (for quorum-satisfied tasks)
    quorum_ok_ids = set(tid for tid, _, _ in quorum_ok)
    latencies = []
    for task_id in quorum_ok_ids:
        if task_id in td_times and task_id in task_acceptances:
            lat = max(task_acceptances[task_id]) - td_times[task_id]
            if lat >= 0:
                latencies.append(lat)
    m['latencia_aceite_media'] = round(sum(latencies) / len(latencies), 6) if latencies else float('nan')

    # ── Failures ──
    failure_lines = re.findall(
        r'FAILURE: Líder (\S+) falhou no tempo (\d+\.?\d*) - (\d+) nós órfãos', content
    )
    m['lideres_falharam'] = len(failure_lines)
    m['nos_orfaos'] = sum(int(n) for _, _, n in failure_lines)
    failure_times = [float(t) for _, t, _ in failure_lines]
    m['tempo_falha'] = failure_times[0] if failure_times else float('nan')

    # Failure percentage
    pct_match = re.search(r'FAILURE: \d+ de \d+ líderes elegíveis irão falhar \(([\d.]+)%\)', content)
    m['porcentagem_falha'] = float(pct_match.group(1)) if pct_match else float('nan')

    # ── Intuitive actions ──
    # Reallocated to existing — use ONLY the FIRST effective summary
    # (subsequent cycles may repeat the action but find no orphans)
    m['realocados_existente'] = 0
    m['nao_realocados'] = 0
    realloc_lines = re.finditer(
        r'INTUITIVE_ACTION: Realocados (\d+), não realocados (\d+)', content
    )
    for rm in realloc_lines:
        r, nr = int(rm.group(1)), int(rm.group(2))
        if r > 0 or nr > 0:
            m['realocados_existente'] = r
            m['nao_realocados'] = nr
            break  # only first effective occurrence

    # New clusters formed — use summary line if available, else individual lines
    # Each new cluster's leader is also an orphan that was reallocated
    recluster_summaries = re.findall(
        r'INTUITIVE_ACTION: (\d+) novos clusters formados de (\d+) órfãos', content
    )
    if recluster_summaries:
        m['novos_clusters_formados'] = sum(int(n) for n, _ in recluster_summaries)
        m['realocados_novo_cluster'] = sum(int(o) for _, o in recluster_summaries)
    else:
        # Fallback: sum individual "Novo cluster formado" lines (followers + 1 leader each)
        new_cluster_followers = re.findall(
            r'INTUITIVE_ACTION: Novo cluster formado - Líder \S+ com (\d+) seguidores', content
        )
        m['novos_clusters_formados'] = len(new_cluster_followers)
        m['realocados_novo_cluster'] = sum(int(n) + 1 for n in new_cluster_followers)

    m['total_realocados'] = m['realocados_existente'] + m['realocados_novo_cluster']
    m['taxa_realocacao'] = round(
        min(1.0, m['total_realocados'] / m['nos_orfaos']) * 100, 2
    ) if m['nos_orfaos'] > 0 else 0.0

    # ── INTUITIVE decision cycles ──
    # Format: INTUITIVE: t=330 [S2] REALLOCATE_TO_EXISTING QI=0.949345 SR=1 ...
    intuitive_cycles = re.findall(
        r'INTUITIVE: t=(\d+\.?\d*) \[(S[12])\] (\S+) QI=([\d.e+-]+) SR=([\d.e+-]+)',
        content
    )

    m['qi_pre_falha'] = float('nan')
    m['qi_pos_acao'] = float('nan')
    m['qi_recuperacao'] = float('nan')
    m['acao_principal'] = ''
    m['sistema_principal'] = ''

    if intuitive_cycles:
        failure_time = m['tempo_falha']

        if not math.isnan(failure_time):
            # QI before failure
            pre_cycles = [(float(t), qi) for t, sys, act, qi, sr in intuitive_cycles if float(t) < failure_time]
            if pre_cycles:
                m['qi_pre_falha'] = float(pre_cycles[-1][1])

            # Cycles after failure
            post_cycles = [(float(t), sys, act, float(qi)) for t, sys, act, qi, sr in intuitive_cycles if float(t) >= failure_time]
            if post_cycles:
                m['qi_pos_acao'] = post_cycles[0][3]
                m['qi_recuperacao'] = max(qi for _, _, _, qi in post_cycles)

                # Most frequent action post-failure
                action_counter = Counter(act for _, _, act, _ in post_cycles)
                m['acao_principal'] = action_counter.most_common(1)[0][0]

                # Most frequent system post-failure
                sys_counter = Counter(s for _, s, _, _ in post_cycles)
                m['sistema_principal'] = sys_counter.most_common(1)[0][0]
        else:
            # No failure — use all cycles
            all_qis = [float(qi) for _, _, _, qi, _ in intuitive_cycles]
            if all_qis:
                m['qi_pre_falha'] = all_qis[-1]
            action_counter = Counter(act for _, _, act, _, _ in intuitive_cycles)
            if action_counter:
                m['acao_principal'] = action_counter.most_common(1)[0][0]
            sys_counter = Counter(s for _, s, _, _, _ in intuitive_cycles)
            if sys_counter:
                m['sistema_principal'] = sys_counter.most_common(1)[0][0]

    return m


CSV_COLUMNS = [
    'rodada',
    'total_agrupamentos',
    'agrupamentos_aptos',
    'agrupamentos_ociosos',
    'tarefas_despachadas',
    'tarefas_executadas',
    'latencia_aceite_media',
    'lideres_falharam',
    'nos_orfaos',
    'tempo_falha',
    'porcentagem_falha',
    'realocados_existente',
    'realocados_novo_cluster',
    'total_realocados',
    'nao_realocados',
    'taxa_realocacao',
    'qi_pre_falha',
    'qi_pos_acao',
    'qi_recuperacao',
    'acao_principal',
    'sistema_principal',
    's1_count',
    's2_count',
]


def process_scenario(scenario_dir):
    """Process one scenario directory and generate its CSV."""
    outputs_dir = scenario_dir / "outputs"
    intuitive_dir = scenario_dir / "intuitive"

    if not outputs_dir.exists():
        print(f"  SKIP: {scenario_dir} (no outputs/ dir)")
        return None

    # Find all run files
    run_files = []
    for f in sorted(outputs_dir.glob("output_run_*.txt")):
        match = re.search(r'output_run_(\d+)\.txt', f.name)
        if match:
            run_files.append((int(match.group(1)), f))
    run_files.sort(key=lambda x: x[0])

    if not run_files:
        print(f"  SKIP: {scenario_dir} (no output files)")
        return None

    rows = []
    for run_num, filepath in run_files:
        m = parse_output_file(filepath)
        if m is None:
            continue

        # Parse IntuitiveQValues
        qv_file = intuitive_dir / f"IntuitiveQValues_run_{run_num}.txt" if intuitive_dir.exists() else None
        qv = parse_intuitive_qvalues(qv_file) if qv_file and qv_file.exists() else {
            's1_count': 0, 's2_count': 0,
        }

        row = {'rodada': run_num}
        row.update(m)
        row['s1_count'] = qv.get('s1_count', 0)
        row['s2_count'] = qv.get('s2_count', 0)
        rows.append(row)

    # Determine CSV output path
    config_name = scenario_dir.name  # e.g. "310s_10percent"
    csv_path = scenario_dir / f"resultados_{config_name}.csv"

    with open(csv_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=CSV_COLUMNS, extrasaction='ignore')
        writer.writeheader()
        for row in rows:
            writer.writerow(row)

    print(f"  {csv_path.name}: {len(rows)} rows")

    # ── Post-generation validation ──
    warnings = 0
    for row in rows:
        run = row['rodada']
        rate = row.get('taxa_realocacao', 0)
        realloc = row.get('total_realocados', 0)
        orphans = row.get('nos_orfaos', 0)
        failures = row.get('lideres_falharam', 0)

        if rate > 100.0:
            print(f"  WARNING run {run}: taxa_realocacao={rate}% > 100%")
            warnings += 1
        if orphans > 0 and realloc > orphans:
            print(f"  WARNING run {run}: total_realocados={realloc} > nos_orfaos={orphans}")
            warnings += 1
        if '10percent' in config_name and failures > 2:
            print(f"  WARNING run {run}: {failures} leader failures in 10% scenario (expected 1-2)")
            warnings += 1
        if failures > 0 and orphans == 0:
            print(f"  WARNING run {run}: {failures} failures but 0 orphans")
            warnings += 1
        if failures > 0 and orphans > 0 and orphans / failures > 40:
            print(f"  WARNING run {run}: {orphans} orphans / {failures} failures = "
                  f"{orphans/failures:.0f} orphans/leader (unusually high)")
            warnings += 1

    if warnings:
        print(f"  ⚠ {warnings} validation warning(s) found")

    return csv_path


def discover_scenarios(base_dir):
    """Find all scenario directories under the base path."""
    scenarios = []
    for cenario_dir in sorted(base_dir.iterdir()):
        if not cenario_dir.is_dir():
            continue
        nodes_dir = cenario_dir / "200_nodes"
        if not nodes_dir.exists():
            # rl3.1-style flat structure
            for config_dir in sorted(cenario_dir.iterdir()):
                if config_dir.is_dir() and (config_dir / "outputs").exists():
                    scenarios.append(config_dir)
        else:
            for config_dir in sorted(nodes_dir.iterdir()):
                if config_dir.is_dir() and (config_dir / "outputs").exists():
                    scenarios.append(config_dir)
    return scenarios


def main():
    parser = argparse.ArgumentParser(
        description='Generate CSV results for SECTIONAL Intuitive Learning approach'
    )
    parser.add_argument('path', type=Path, help='Base results directory or specific scenario directory')
    parser.add_argument('--all', action='store_true', help='Process all sub-scenarios')
    args = parser.parse_args()

    if args.all:
        scenarios = discover_scenarios(args.path)
        if not scenarios:
            print(f"No scenarios found under {args.path}")
            sys.exit(1)
        print(f"Found {len(scenarios)} scenarios")
        for sc in scenarios:
            rel = sc.relative_to(args.path)
            print(f"\nProcessing {rel}...")
            process_scenario(sc)
    else:
        if not (args.path / "outputs").exists():
            print(f"Error: {args.path}/outputs/ not found")
            sys.exit(1)
        process_scenario(args.path)

    print("\nDone!")


if __name__ == '__main__':
    main()
