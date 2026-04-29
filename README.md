# SECTIONAL — `sectional-rl3.1`

> **Q-Learning com recompensa contextualizada: o agente aprende levando em conta o tamanho do grupo de órfãos e a qualidade da similaridade.**
> Estende o `sectional-rl3` com uma função de recompensa mais sofisticada que escala o sinal de aprendizado com base no contexto da decisão, guiando o agente a estratégias mais adequadas para grupos grandes e pequenos.

---

## Visão Geral

O `sectional-rl3.1` mantém a mesma Q-table 4×3 e os mesmos estados e ações do `sectional-rl3`, mas reformula a função de recompensa. Em vez de recompensas binárias (`+10`/`−5`/`−10`), o agente recebe um sinal proporcional à qualidade da similaridade e modulado pelo tamanho do grupo de órfãos. Isso permite que o agente aprenda diferenças que a versão binária não captura.

---

## O que há de novo em relação ao `sectional-rl3`

| Componente | Mudança |
|---|---|
| Função de recompensa | Recompensa proporcional à similaridade × fator de capacidade / tamanho |
| Bônus/penalidade por tamanho do grupo | Incentivos que direcionam a estratégia conforme o contexto |
| `SIZE_FACTOR_*` / `CAP_FACTOR_*` | Constantes de escalonamento da recompensa |
| `BONUS_*` / `PENALTY_*` | Ajustes por tamanho de grupo aplicados sobre a recompensa base |

A Q-table, os estados, as ações e a estrutura de fluxo são idênticos ao rl3.

---

## Função de Recompensa Contextualizada

### Recompensa proporcional à similaridade

Quando a ação é executável com sucesso:

**`REALLOCATE_EXISTING`:**
```
r = REWARD_BASE × capFactor × simExisting + bonusReallocExisting
```

**`FORM_NEW_CLUSTER`:**
```
r = REWARD_BASE × sizeFactor × simOrphan + bonusFormNewCluster
```

**Fatores de capacidade (`capFactor`):**

| Faixa de `simExisting` | Fator |
|---|---|
| `≥ 0.95` | 1.0 |
| `≥ 0.90` | 0.8 |
| `< 0.90` | 0.4 |

**Fatores de tamanho (`sizeFactor`):**

| Tamanho do grupo | Fator |
|---|---|
| `> 10 nós` | 1.0 |
| `6–10 nós` | 0.7 |
| `≤ 5 nós` | 0.3 |

### Bônus/penalidade por tamanho de grupo

| Tamanho do grupo | `bonusFormNewCluster` | `bonusReallocExisting` | Efeito |
|---|---|---|---|
| `> 10 nós` | `+3.0` | `−2.0` | Incentiva formar cluster; desincentiva fragmentar |
| `≤ 5 nós` | `−3.0` | `+3.0` | Incentiva realocar; desincentiva cluster pequeno |
| `6–10 nós` | `0.0` | `0.0` | Agente decide livremente |

### Recompensas para ações inviáveis (mantidas do rl3)

| Situação | Recompensa |
|---|---|
| Ação escolhida mas similaridade insuficiente | `−10.0` |
| Nó permanece órfão (`DO_NOT_ALLOCATE`) | `−5.0` |

---

## Agente Q-Learning (herdado do rl3)

### Estados (4)

| Estado | Condição |
|---|---|
| `SIM_BOTH_HIGH` | `simExisting ≥ 0.92` e `simOrphan ≥ 0.92` |
| `SIM_EXISTING_HIGH` | `simExisting ≥ 0.92` e `simOrphan < 0.92` |
| `SIM_ORPHAN_HIGH` | `simExisting < 0.92` e `simOrphan ≥ 0.92` |
| `SIM_BOTH_MEDIUM` | Ambas abaixo de 0.92 |

### Ações (3)

| Ação | Efeito |
|---|---|
| `DO_NOT_ALLOCATE` | Nó permanece órfão |
| `REALLOCATE_EXISTING` | Nó adicionado ao melhor cluster existente |
| `FORM_NEW_CLUSTER` | Nó marcado como candidato a novo cluster |

---

## Parâmetros

### Q-Learning

| Parâmetro | Padrão | Flag CLI |
|---|---|---|
| `rlAlpha` (α) | 0.1 | `--rlAlpha` |
| `rlGamma` (γ) | 0.9 | `--rlGamma` |
| `rlEpsilon` (ε) | 0.1 | `--rlEpsilon` |

### Falhas

| Parâmetro | Padrão | Descrição |
|---|---|---|
| `failureTime` | 310 s | Tempo fixo da falha |
| `failureTimeMin/Max` | 0 | Intervalo para tempo aleatório |
| `failurePercentage` | 0% | Porcentagem fixa de líderes a falhar |
| `failurePercentageMin/Max` | 0 | Intervalo para porcentagem aleatória |

### Similaridade

| Parâmetro | Valor |
|---|---|
| `REALLOCATION_THRESHOLD` | 0.85 |
| `SIMILARITY_HIGH_THRESHOLD` | 0.92 |
| `CLUSTERING_THRESHOLD` | 0.95 |
| `MIN_NODES_FOR_NEW_CLUSTER` | 2 |

---

## Como Compilar e Executar

```bash
./ns3 build

# 200 nós, 30% de falha em t=310s, rodada 1
./ns3 run "contaski --nNodes=200 --run=1 --failurePercentage=30 --failureTime=310"
```

---

## Saída

- `qtable.csv` — Q-table persistida ao fim de cada rodada
- `APStats_run_<R>.txt` — tarefas despachadas e pendentes
- Logs com prefixos `RL3.1_PROCESS:`, `RL3.1_GROUP:`, `RL3.1_BONUS:`, `RL3.1_DECISION:`, `RL3.1_REWARD:`, `RL3.1_METRICS:`
- `results-rl31/` — resultados das simulações experimentais

---

## Relação com as Outras Branches

| Branch | Diferença em relação a esta |
|---|---|
| `sectional-optimal` | Sem falhas e sem RL (baseline) |
| `sectional-with-fail` | Falhas sem recuperação RL |
| `sectional-rl1` | Q-Learning com 2 estados × 2 ações, apenas realocação |
| `sectional-rl2` | Q-Learning com 2 estados × 2 ações, apenas novos clusters |
| `sectional-rl3` | Mesma estrutura mas com recompensas binárias |
| `intuitive-sectional` / `ddos-intuitive` | Sistema dual S1/S2 com aprendizado intuitivo |
