#!/usr/bin/env python3
"""
SECTIONAL - Gerador de Gráficos para o artigo SBRC 2026
Lê os CSVs gerados por analyze_results.py e gera gráficos matplotlib.
"""

import math
from pathlib import Path

import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy import stats

CSV_DIR = Path(__file__).parent / "csvs_analise"
PLOT_DIR = CSV_DIR / "plots"
PLOT_DIR.mkdir(parents=True, exist_ok=True)

APPROACH_LABELS = {
    'optimal': 'Optimal',
    'with-fail': 'With-Fail',
    'rl1': 'RL1',
    'rl2': 'RL2',
    'rl3': 'RL3',
    'rl3.1': 'RL3.1',
}
APPROACH_COLORS = {
    'optimal': '#2196F3',
    'with-fail': '#F44336',
    'rl1': '#FF9800',
    'rl2': '#4CAF50',
    'rl3': '#9C27B0',
    'rl3.1': '#00BCD4',
}
APPROACH_ORDER = ['optimal', 'with-fail', 'rl1', 'rl2', 'rl3', 'rl3.1']

SCENARIO_SHORT = {
    'cenario1_tempo_fixo/310s_10percent': 'C1-10%',
    'cenario1_tempo_fixo/310s_20percent': 'C1-20%',
    'cenario1_tempo_fixo/310s_30percent': 'C1-30%',
    'cenario2_tempo_aleatorio/180-600s_10percent': 'C2-10%',
    'cenario2_tempo_aleatorio/180-600s_20percent': 'C2-20%',
    'cenario2_tempo_aleatorio/180-600s_30percent': 'C2-30%',
    'cenario3_tudo_aleatorio/180-600s_10-30percent': 'C3-rand',
    'sem_falha': 'Optimal',
}


def ci95(series):
    n = series.count()
    if n < 2:
        return 0
    se = series.std() / math.sqrt(n)
    return stats.t.ppf(0.975, df=n - 1) * se


def load_data():
    df_tasks = pd.read_csv(CSV_DIR / "metricas_tarefas.csv")
    df_clust = pd.read_csv(CSV_DIR / "metricas_clustering.csv")
    df_realloc = pd.read_csv(CSV_DIR / "metricas_realocacao_rl.csv")
    df_qtable = pd.read_csv(CSV_DIR / "evolucao_qtable.csv")
    df_summary = pd.read_csv(CSV_DIR / "resumo_estatistico.csv")
    return df_tasks, df_clust, df_realloc, df_qtable, df_summary


def plot_task_success_rate(df_tasks):
    """Bar chart: task success rate per scenario, grouped by approach."""
    # Only scenarios with failures
    fail_scenarios = [s for s in SCENARIO_SHORT if s != 'sem_falha']
    df = df_tasks[df_tasks['scenario'].isin(fail_scenarios)]
    approaches = [a for a in APPROACH_ORDER if a in df['approach'].unique()]

    fig, ax = plt.subplots(figsize=(14, 6))
    x = np.arange(len(fail_scenarios))
    width = 0.8 / len(approaches)

    for i, approach in enumerate(approaches):
        means = []
        cis = []
        for sc in fail_scenarios:
            mask = (df['approach'] == approach) & (df['scenario'] == sc)
            vals = df.loc[mask, 'task_success_rate']
            if len(vals) > 0:
                means.append(vals.mean() * 100)
                cis.append(ci95(vals) * 100)
            else:
                means.append(0)
                cis.append(0)
        offset = (i - len(approaches) / 2 + 0.5) * width
        bars = ax.bar(x + offset, means, width, yerr=cis, capsize=3,
                       label=APPROACH_LABELS[approach], color=APPROACH_COLORS[approach],
                       alpha=0.85)

    ax.set_xlabel('Cenário', fontsize=12)
    ax.set_ylabel('Taxa de Sucesso (%)', fontsize=12)
    ax.set_title('Taxa de Sucesso de Tarefas por Cenário e Abordagem', fontsize=14)
    ax.set_xticks(x)
    ax.set_xticklabels([SCENARIO_SHORT[s] for s in fail_scenarios], fontsize=10)
    ax.legend(fontsize=10)
    ax.set_ylim(0, 110)
    ax.grid(axis='y', alpha=0.3)
    fig.tight_layout()
    fig.savefig(PLOT_DIR / "task_success_rate.png", dpi=150)
    plt.close(fig)
    print("  task_success_rate.png")


def plot_realloc_rate(df_realloc):
    """Bar chart: RL reallocation rate per scenario (only RL approaches)."""
    fail_scenarios = [s for s in SCENARIO_SHORT if s != 'sem_falha']
    rl_approaches = ['rl1', 'rl2', 'rl3', 'rl3.1']
    df = df_realloc[df_realloc['approach'].isin(rl_approaches)]

    fig, ax = plt.subplots(figsize=(16, 6))
    x = np.arange(len(fail_scenarios))
    width = 0.8 / len(rl_approaches)

    for i, approach in enumerate(rl_approaches):
        means = []
        cis = []
        for sc in fail_scenarios:
            mask = (df['approach'] == approach) & (df['scenario'] == sc)
            vals = df.loc[mask, 'realloc_rate']
            if len(vals) > 0:
                means.append(vals.mean() * 100)
                cis.append(ci95(vals) * 100)
            else:
                means.append(0)
                cis.append(0)
        offset = (i - len(rl_approaches) / 2 + 0.5) * width
        ax.bar(x + offset, means, width, yerr=cis, capsize=3,
               label=APPROACH_LABELS[approach], color=APPROACH_COLORS[approach], alpha=0.85)

    ax.set_xlabel('Cenário', fontsize=12)
    ax.set_ylabel('Taxa de Realocação (%)', fontsize=12)
    ax.set_title('Taxa de Realocação RL por Cenário', fontsize=14)
    ax.set_xticks(x)
    ax.set_xticklabels([SCENARIO_SHORT[s] for s in fail_scenarios], fontsize=10)
    ax.legend(fontsize=10)
    ax.set_ylim(80, 102)
    ax.grid(axis='y', alpha=0.3)
    fig.tight_layout()
    fig.savefig(PLOT_DIR / "realloc_rate.png", dpi=150)
    plt.close(fig)
    print("  realloc_rate.png")


def plot_orphans_boxplot(df_clust):
    """Boxplot: orphan count distribution per approach (all scenarios combined)."""
    fail_approaches = ['with-fail', 'rl1', 'rl2', 'rl3', 'rl3.1']
    df = df_clust[df_clust['approach'].isin(fail_approaches) & (df_clust['orphan_total'] > 0)]

    fig, ax = plt.subplots(figsize=(10, 6))
    data = [df[df['approach'] == a]['orphan_total'].values for a in fail_approaches]
    bp = ax.boxplot(data, labels=[APPROACH_LABELS[a] for a in fail_approaches],
                    patch_artist=True, showmeans=True)
    for i, approach in enumerate(fail_approaches):
        bp['boxes'][i].set_facecolor(APPROACH_COLORS[approach])
        bp['boxes'][i].set_alpha(0.7)

    ax.set_ylabel('Nós Órfãos', fontsize=12)
    ax.set_title('Distribuição de Nós Órfãos por Abordagem', fontsize=14)
    ax.grid(axis='y', alpha=0.3)
    fig.tight_layout()
    fig.savefig(PLOT_DIR / "orphans_boxplot.png", dpi=150)
    plt.close(fig)
    print("  orphans_boxplot.png")


def plot_qtable_heatmap(df_qtable):
    """Heatmap of average Q-values per approach (cenario1, 10%)."""
    scenario = 'cenario1_tempo_fixo/310s_10percent'

    for approach in ['rl1', 'rl2', 'rl3', 'rl3.1']:
        mask = (df_qtable['approach'] == approach) & (df_qtable['scenario'] == scenario)
        df = df_qtable[mask]
        if len(df) == 0:
            continue

        pivot = df.groupby(['state_name', 'action_name'])['qvalue'].mean().reset_index()
        pivot_table = pivot.pivot(index='state_name', columns='action_name', values='qvalue').fillna(0)

        fig, ax = plt.subplots(figsize=(8, 4))
        im = ax.imshow(pivot_table.values, cmap='RdYlGn', aspect='auto')
        ax.set_xticks(range(len(pivot_table.columns)))
        ax.set_xticklabels(pivot_table.columns, fontsize=9, rotation=15)
        ax.set_yticks(range(len(pivot_table.index)))
        ax.set_yticklabels(pivot_table.index, fontsize=9)
        ax.set_title(f'Q-Values Médios - {APPROACH_LABELS[approach]} (C1-10%)', fontsize=13)

        for i in range(len(pivot_table.index)):
            for j in range(len(pivot_table.columns)):
                ax.text(j, i, f'{pivot_table.values[i, j]:.2f}',
                        ha='center', va='center', fontsize=11, fontweight='bold')

        fig.colorbar(im, ax=ax, shrink=0.8)
        fig.tight_layout()
        fig.savefig(PLOT_DIR / f"qtable_heatmap_{approach}.png", dpi=150)
        plt.close(fig)
        print(f"  qtable_heatmap_{approach}.png")


def plot_comparison_summary(df_summary):
    """Grouped comparison: success rate and realloc rate side by side."""
    # Only C1 scenarios for cleaner comparison
    c1_scenarios = [
        'cenario1_tempo_fixo/310s_10percent',
        'cenario1_tempo_fixo/310s_20percent',
        'cenario1_tempo_fixo/310s_30percent',
    ]
    df = df_summary[df_summary['scenario'].isin(c1_scenarios)]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # Left: success rate
    approaches = [a for a in APPROACH_ORDER if a in df['approach'].unique()]
    x = np.arange(len(c1_scenarios))
    width = 0.8 / len(approaches)
    for i, a in enumerate(approaches):
        vals = []
        errs = []
        for sc in c1_scenarios:
            row = df[(df['approach'] == a) & (df['scenario'] == sc)]
            if len(row) > 0:
                vals.append(row['tasks_success_rate_mean'].values[0] * 100)
                errs.append(row['tasks_success_rate_ci95'].values[0] * 100)
            else:
                vals.append(0)
                errs.append(0)
        offset = (i - len(approaches) / 2 + 0.5) * width
        ax1.bar(x + offset, vals, width, yerr=errs, capsize=3,
                label=APPROACH_LABELS[a], color=APPROACH_COLORS[a], alpha=0.85)
    ax1.set_xlabel('Cenário')
    ax1.set_ylabel('Taxa de Sucesso (%)')
    ax1.set_title('Taxa de Sucesso - Cenário 1 (Tempo Fixo)')
    ax1.set_xticks(x)
    ax1.set_xticklabels(['10%', '20%', '30%'])
    ax1.legend(fontsize=9)
    ax1.set_ylim(0, 110)
    ax1.grid(axis='y', alpha=0.3)

    # Right: realloc rate (RL only)
    rl_approaches = ['rl1', 'rl2', 'rl3', 'rl3.1']
    rl_df = df[df['approach'].isin(rl_approaches)]
    width_r = 0.8 / len(rl_approaches)
    for i, a in enumerate(rl_approaches):
        vals = []
        errs = []
        for sc in c1_scenarios:
            row = rl_df[(rl_df['approach'] == a) & (rl_df['scenario'] == sc)]
            if len(row) > 0 and 'realloc_rate_mean' in row.columns:
                v = row['realloc_rate_mean'].values[0]
                e = row['realloc_rate_ci95'].values[0]
                vals.append(v * 100 if not pd.isna(v) else 0)
                errs.append(e * 100 if not pd.isna(e) else 0)
            else:
                vals.append(0)
                errs.append(0)
        offset = (i - len(rl_approaches) / 2 + 0.5) * width_r
        ax2.bar(x + offset, vals, width_r, yerr=errs, capsize=3,
                label=APPROACH_LABELS[a], color=APPROACH_COLORS[a], alpha=0.85)
    ax2.set_xlabel('Percentual de Falha')
    ax2.set_ylabel('Taxa de Realocação (%)')
    ax2.set_title('Taxa de Realocação RL - Cenário 1')
    ax2.set_xticks(x)
    ax2.set_xticklabels(['10%', '20%', '30%'])
    ax2.legend(fontsize=9)
    ax2.set_ylim(85, 102)
    ax2.grid(axis='y', alpha=0.3)

    fig.tight_layout()
    fig.savefig(PLOT_DIR / "comparison_c1.png", dpi=150)
    plt.close(fig)
    print("  comparison_c1.png")


def plot_latency_comparison(df_tasks):
    """Box/violin plot of acceptance latency per approach."""
    fail_scenarios = [s for s in SCENARIO_SHORT if s != 'sem_falha']
    approaches = [a for a in APPROACH_ORDER if a in df_tasks['approach'].unique()]
    df = df_tasks[df_tasks['scenario'].isin(fail_scenarios)]

    fig, ax = plt.subplots(figsize=(10, 6))
    data = []
    labels = []
    colors = []
    for a in approaches:
        vals = df[df['approach'] == a]['avg_accept_latency_s'].dropna()
        if len(vals) > 0:
            data.append(vals.values)
            labels.append(APPROACH_LABELS[a])
            colors.append(APPROACH_COLORS[a])

    bp = ax.boxplot(data, labels=labels, patch_artist=True, showmeans=True)
    for i, c in enumerate(colors):
        bp['boxes'][i].set_facecolor(c)
        bp['boxes'][i].set_alpha(0.7)

    ax.set_ylabel('Latência de Aceite (s)', fontsize=12)
    ax.set_title('Latência de Aceite de Tarefa por Abordagem', fontsize=14)
    ax.grid(axis='y', alpha=0.3)
    fig.tight_layout()
    fig.savefig(PLOT_DIR / "latency_boxplot.png", dpi=150)
    plt.close(fig)
    print("  latency_boxplot.png")


def main():
    print("Carregando dados...")
    df_tasks, df_clust, df_realloc, df_qtable, df_summary = load_data()

    print(f"\nGerando gráficos em {PLOT_DIR}/")
    plot_task_success_rate(df_tasks)
    plot_realloc_rate(df_realloc)
    plot_orphans_boxplot(df_clust)
    plot_qtable_heatmap(df_qtable)
    plot_comparison_summary(df_summary)
    plot_latency_comparison(df_tasks)

    print(f"\nTodos os gráficos salvos em: {PLOT_DIR}")


if __name__ == '__main__':
    main()
