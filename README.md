# SECTIONAL — `sectional-rl2`

> **Segunda versão com Q-Learning: recuperação de órfãos por formação de novos clusters.**
> Estende o `sectional-rl1` com a capacidade de agrupar nós órfãos entre si para formar novos clusters, em vez de apenas realocá-los em clusters existentes.

---

## Visão Geral

O `sectional-rl2` foca exclusivamente na estratégia oposta ao rl1: quando líderes falham, os nós órfãos são agrupados por similaridade mútua para eleger um novo líder entre eles e reconstituir clusters funcionais. O agente Q-Learning decide quais órfãos participam do novo cluster com base na similaridade média do grupo.

---

## O que há de novo em relação ao `sectional-rl1`

| Componente | Mudança |
|---|---|
| `formOrphanClusters()` | Nova função: agrupa órfãos por líder falho e forma novos clusters |
| `electLeaderForOrphanCluster()` | Elege o nó com maior número de vizinhos como novo líder |
| `registerNewCluster()` | Registra o novo cluster no AP com líder e membros |
| `orphanGroups` | Mapa `líder_falho → [órfãos]` para organizar grupos |
| Métricas RL2 | `newClustersFormed`, `nodesInNewClusters` |

A estratégia de realocação em clusters existentes (rl1) **não está presente** nesta branch — rl2 é a alternativa pura de reconstrução.

---

## Agente Q-Learning

### Estados (2) — mesmos do rl1

| Estado | Condição |
|---|---|
| `SIMILARITY_MEDIUM` | `0.85 ≤ sim_média_do_grupo < 0.92` |
| `SIMILARITY_HIGH` | `sim_média_do_grupo ≥ 0.92` |

O estado é calculado com base na **similaridade média** do nó órfão em relação aos demais membros do grupo (não em relação a clusters existentes).

### Ações (2)

| Ação | Efeito |
|---|---|
| `DO_NOT_ALLOCATE` | Nó não entra no novo cluster |
| `ALLOCATE` | Nó é incluído no novo cluster em formação |

### Recompensas

| Situação | Recompensa |
|---|---|
| Nó incluído no novo cluster | `+10.0` |
| Tentativa de inclusão inválida | `−10.0` |
| Nó não incluído | `−5.0` |

---

## Fluxo de Formação de Novos Clusters

1. Ao detectar falhas, o AP agrupa os órfãos por líder que falhou (`orphanGroups`)
2. Para cada grupo com ao menos `MIN_NODES_FOR_NEW_CLUSTER = 2` nós:
   - Calcula a similaridade média de cada nó com os demais do grupo
   - Consulta o agente RL (estado + ação)
   - Nós com ação `ALLOCATE` e `sim_média ≥ 0.85` entram no novo cluster
3. Se o novo cluster atingir o mínimo de nós, elege-se o líder pelo maior número de vizinhos
4. O novo líder é configurado via `becomeLeader()` e registrado no AP via `registerNewCluster()`

Grupos com menos de 2 membros são descartados.

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
| `REALLOCATION_THRESHOLD` | 0.85 | Similaridade mínima para inclusão no grupo |
| `SIMILARITY_HIGH_THRESHOLD` | 0.92 | Fronteira entre estados MEDIUM e HIGH |
| `CLUSTERING_THRESHOLD` | 0.95 | Limiar original de clusterização |
| `MIN_NODES_FOR_NEW_CLUSTER` | 2 | Mínimo de nós para formar um cluster |

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
- Logs com prefixos `RL2_FORM_CLUSTERS:`, `RL2_GROUP:`, `RL2_DECISION:`, `RL2_REWARD:`, `RL2_CLUSTER_FORMED:`, `RL2_METRICS:`

---

## Relação com as Outras Branches

| Branch | Diferença em relação a esta |
|---|---|
| `sectional-optimal` | Sem falhas e sem RL (baseline) |
| `sectional-with-fail` | Falhas sem recuperação RL |
| `sectional-rl1` | Realoca órfãos em clusters existentes (estratégia oposta) |
| `sectional-rl3` | Unifica rl1 + rl2: decide entre realocar OU formar novo cluster |
| `sectional-rl3.1` | Estende rl3 com recompensa contextualizada pelo tamanho do grupo |
