# SECTIONAL — `sectional-rl3`

> **Versão unificada do Q-Learning: o agente escolhe entre realocar em cluster existente, formar novo cluster ou manter o nó órfão.**
> Combina as estratégias do `sectional-rl1` e `sectional-rl2` em um único agente com estado composto baseado em duas dimensões de similaridade.

---

## Visão Geral

O `sectional-rl3` é o modelo de Q-Learning mais completo da série. Para cada nó órfão, o agente calcula simultaneamente a similaridade com clusters existentes **e** com outros nós órfãos, classifica o contexto em um de 4 estados e escolhe entre 3 ações. A Q-table resultante tem dimensão **4 × 3**.

---

## O que há de novo em relação ao `sectional-rl2`

| Componente | Mudança |
|---|---|
| Estado composto | 4 estados baseados em duas similaridades (existente + órfão) |
| 3 ações | Inclui `REALLOCATE_EXISTING` (estratégia do rl1) além de `FORM_NEW_CLUSTER` |
| `processOrphansUnified()` | Função única que unifica o fluxo dos dois cenários anteriores |
| `determineState(simExisting, simOrphan)` | Classifica o estado com base em ambas as similaridades |

---

## Agente Q-Learning

### Estados (4)

| Estado | Condição |
|---|---|
| `SIM_BOTH_HIGH` | `simExisting ≥ 0.92` **e** `simOrphan ≥ 0.92` |
| `SIM_EXISTING_HIGH` | `simExisting ≥ 0.92` e `simOrphan < 0.92` |
| `SIM_ORPHAN_HIGH` | `simExisting < 0.92` e `simOrphan ≥ 0.92` |
| `SIM_BOTH_MEDIUM` | Ambas as similaridades abaixo de 0.92 |

`simExisting` = similaridade com o melhor cluster sobrevivente disponível.
`simOrphan` = similaridade média com os demais nós órfãos do grupo.

### Ações (3)

| Ação | Efeito |
|---|---|
| `DO_NOT_ALLOCATE` | Nó permanece órfão |
| `REALLOCATE_EXISTING` | Nó é adicionado ao melhor cluster existente (`sim ≥ 0.85`) |
| `FORM_NEW_CLUSTER` | Nó é marcado como candidato a novo cluster |

### Q-Table (4 × 3)

```
                    DO_NOT_ALLOC  |  REALLOC_EXIST  |  FORM_NEW_CLUST
SIM_BOTH_HIGH          Q[0][0]   |     Q[0][1]      |     Q[0][2]
SIM_EXISTING_HIGH      Q[1][0]   |     Q[1][1]      |     Q[1][2]
SIM_ORPHAN_HIGH        Q[2][0]   |     Q[2][1]      |     Q[2][2]
SIM_BOTH_MEDIUM        Q[3][0]   |     Q[3][1]      |     Q[3][2]
```

### Recompensas

| Situação | Recompensa |
|---|---|
| Realocação ou inclusão bem-sucedida | `+10.0` |
| Ação escolhida mas similaridade insuficiente | `−10.0` |
| Nó permanece órfão | `−5.0` |

---

## Fluxo de Processamento de Órfãos

Para cada grupo de órfãos (indexado pelo líder que falhou):

1. Para cada nó órfão do grupo:
   - Calcula `simExisting` = maior similaridade com clusters sobreviventes
   - Calcula `simOrphan` = similaridade média com os outros órfãos do grupo
   - Determina o estado via `determineState(simExisting, simOrphan)`
   - Agente escolhe ação (epsilon-greedy)
2. Nós marcados para `FORM_NEW_CLUSTER` são agrupados; se atingirem `MIN_NODES_FOR_NEW_CLUSTER`, elegem um líder
3. Nós com `REALLOCATE_EXISTING` são adicionados ao melhor cluster existente

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

| Parâmetro | Valor | Uso |
|---|---|---|
| `REALLOCATION_THRESHOLD` | 0.85 | Limiar mínimo para `REALLOCATE_EXISTING` ser válido |
| `SIMILARITY_HIGH_THRESHOLD` | 0.92 | Fronteira MEDIUM/HIGH nos estados |
| `CLUSTERING_THRESHOLD` | 0.95 | Limiar original de clusterização |
| `MIN_NODES_FOR_NEW_CLUSTER` | 2 | Mínimo de nós para formar novo cluster |

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
- Logs com prefixos `RL3_PROCESS:`, `RL3_GROUP:`, `RL3_DECISION:`, `RL3_REWARD:`, `RL3_SUMMARY:`, `RL3_METRICS:`

---

## Relação com as Outras Branches

| Branch | Diferença em relação a esta |
|---|---|
| `sectional-optimal` | Sem falhas e sem RL (baseline) |
| `sectional-with-fail` | Falhas sem recuperação RL |
| `sectional-rl1` | Apenas realocação em clusters existentes (2 estados × 2 ações) |
| `sectional-rl2` | Apenas formação de novos clusters (2 estados × 2 ações) |
| `sectional-rl3.1` | Estende esta branch com recompensa contextualizada pelo tamanho do grupo |
