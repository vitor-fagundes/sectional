# SECTIONAL — `sectional-rl1`

> **Primeira versão com Q-Learning: recuperação de órfãos por realocação em clusters existentes.**
> Estende o baseline `sectional-optimal` com injeção de falhas de líderes e um agente RL que decide se e para qual cluster existente realocar cada nó órfão.

---

## Visão Geral

O `sectional-rl1` é a primeira iteração de aprendizado por reforço do projeto. Quando líderes falham, os nós do seu cluster ficam órfãos. Um agente Q-Learning centralizado no AP observa a similaridade de capacidades entre cada nó órfão e os clusters sobreviventes e decide se realoca ou mantém o nó órfão.

---

## O que há de novo em relação ao `sectional-optimal`

| Componente | Adicionado |
|---|---|
| `QLearningAgent.cc/h` | Agente Q-Learning com Q-table 2×2 |
| Injeção de falhas | Líderes são removidos em `t = failureTime` |
| `reallocateOrphans()` | Loop RL sobre nós órfãos pós-falha |
| Persistência da Q-table | `qtable.csv` salvo/carregado entre rodadas |
| Métricas de realocação | Contadores `successfulReallocations` / `failedReallocations` |

---

## Agente Q-Learning

### Estados (2)

| Estado | Condição |
|---|---|
| `SIMILARITY_MEDIUM` | `0.85 ≤ sim < 0.92` entre o órfão e o melhor cluster disponível |
| `SIMILARITY_HIGH` | `sim ≥ 0.92` |

O estado é calculado somente se houver ao menos um cluster com `sim ≥ REALLOCATION_THRESHOLD (0.85)`. Caso contrário o nó fica órfão sem consulta ao agente.

### Ações (2)

| Ação | Efeito |
|---|---|
| `DO_NOT_ALLOCATE` | Nó permanece órfão |
| `ALLOCATE` | Nó é adicionado ao melhor cluster disponível |

### Q-Table

```
          DO_NOT_ALLOC  |  ALLOCATE
SIM_MEDIUM    Q[0][0]   |   Q[0][1]
SIM_HIGH      Q[1][0]   |   Q[1][1]
```

### Recompensas

| Situação | Recompensa |
|---|---|
| Realocação bem-sucedida | `+10.0` |
| Ação `ALLOCATE` mas falha na inserção | `−10.0` |
| Nó continua órfão | `−5.0` |

### Política

Epsilon-greedy: com probabilidade `ε` escolhe ação aleatória; caso contrário escolhe `argmax Q(s, a)`.

---

## Fluxo de Simulação

| Instante | Evento |
|---|---|
| `t = 0–90,5 s` | Beaconing, disseminação, clusterização (igual ao optimal) |
| `t = 150 s` | AP inicia despacho de tarefas |
| `t = failureTime` | AP remove `failurePercentage`% dos líderes elegíveis |
| Imediatamente após | `reallocateOrphans()`: agente RL processa cada nó órfão |
| Fim da simulação | Q-table salva em `qtable.csv` |

Líderes elegíveis para falha são os que possuem ao menos 2 membros no cluster (evita clusters de 1 nó).

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

---

## Como Compilar e Executar

```bash
./ns3 build

# Cenário com 200 nós, falha de 30% dos líderes em t=310s, rodada 1
./ns3 run "contaski --nNodes=200 --run=1 --failurePercentage=30 --failureTime=310"

# Modo aleatório: falha entre 10-30% dos líderes, entre t=180-600s
./ns3 run "contaski --nNodes=200 --run=1 \
  --failurePercentageMin=10 --failurePercentageMax=30 \
  --failureTimeMin=180 --failureTimeMax=600"
```

A Q-table é carregada de `qtable.csv` automaticamente se o arquivo existir, permitindo continuidade de aprendizado entre rodadas.

---

## Saída

- `qtable.csv` — Q-table persistida ao fim de cada rodada
- `APStats_run_<R>.txt` — tarefas despachadas e pendentes
- Logs com prefixos `RL_REALLOC:`, `RL_DECISION:`, `RL_REWARD:`, `RL_METRICS:`

---

## Relação com as Outras Branches

| Branch | Diferença em relação a esta |
|---|---|
| `sectional-optimal` | Sem falhas e sem RL (baseline) |
| `sectional-with-fail` | Falhas sem recuperação RL |
| `sectional-rl2` | Adiciona formação de novos clusters entre órfãos |
| `sectional-rl3` | Unifica rl1 + rl2 com estado composto (4 estados × 3 ações) |
| `sectional-rl3.1` | Estende rl3 com recompensa contextualizada pelo tamanho do grupo |
