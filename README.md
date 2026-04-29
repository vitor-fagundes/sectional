# SECTIONAL — `sectional-with-fail`

> **Baseline com falhas e sem recuperação RL.**
> Estende o `sectional-optimal` com injeção de falhas de líderes, mas sem nenhum mecanismo de recuperação. Serve como referência para quantificar o impacto das falhas antes da introdução do Q-Learning.

---

## Visão Geral

O `sectional-with-fail` introduz o mecanismo de falha de líderes na simulação sem implementar nenhuma estratégia de recuperação. Quando um líder falha, seus nós ficam órfãos permanentemente até o fim da simulação. Este cenário responde à pergunta: *quanto o sistema degrada sem nenhuma recuperação?*

---

## O que há de novo em relação ao `sectional-optimal`

| Componente | Adicionado |
|---|---|
| `triggerLeaderFailures()` | Remove líderes elegíveis em tempo configurável |
| Parâmetros de falha | Percentagem e tempo fixos ou aleatórios via CLI |
| `FollowerRegister` | Novo tipo de mensagem para rastrear membros de clusters |
| `results/` | Três sub-cenários de resultados experimentais |

Não há `QLearningAgent`, `reallocateOrphans()` nem `formOrphanClusters()` nesta branch.

---

## Mecanismo de Falha

### Líderes Elegíveis

Somente líderes que possuem ao menos 2 membros no cluster são elegíveis para falha (clusters de 1 nó são excluídos). O número de líderes a falhar é calculado como:

```
leadersToFail = round(totalEligibleLeaders × failurePercentage / 100)
```

### Comportamento Pós-Falha

O nó líder é desativado completamente (para de processar pacotes e transmitir). Seus membros tornam-se órfãos e permanecem inativos sem reintegração ao sistema.

---

## Cenários Experimentais

| Sub-cenário | Pasta de resultados | Configuração |
|---|---|---|
| Falha fixa | `results/with-fail/` | Tempo e porcentagem fixos (`failureTimeMin = failureTimeMax`) |
| Tempo aleatório | `results/with-fail-random-time/` | Tempo sorteado no intervalo `[failureTimeMin, failureTimeMax]` |
| Totalmente aleatório | `results/with-fail-full-random/` | Tempo e porcentagem sorteados em intervalos configurados |

---

## Parâmetros CLI

| Parâmetro | Padrão | Descrição |
|---|---|---|
| `nNodes` | 3 | Número de nós na rede |
| `run` | 0 | Número da rodada (seed) |
| `failurePercentage` | 0% | Porcentagem fixa de líderes elegíveis a falhar |
| `failurePercentageMin` | 0% | Porcentagem mínima (modo aleatório) |
| `failurePercentageMax` | 0% | Porcentagem máxima (modo aleatório) |
| `failureTimeMin` | 310 s | Tempo mínimo da falha (tempo fixo quando igual ao Max) |
| `failureTimeMax` | 310 s | Tempo máximo da falha |

Quando `failureTimeMin == failureTimeMax`, a falha ocorre naquele instante exato. Quando diferem, o tempo é sorteado uniformemente no intervalo.

---

## Como Compilar e Executar

```bash
./ns3 build

# Falha fixa: 30% dos líderes em t=310s
./ns3 run "contaski --nNodes=200 --run=1 --failurePercentage=30 --failureTimeMin=310 --failureTimeMax=310"

# Tempo aleatório: 20% dos líderes entre t=180s e t=600s
./ns3 run "contaski --nNodes=200 --run=1 --failurePercentage=20 --failureTimeMin=180 --failureTimeMax=600"

# Totalmente aleatório: 10-30% dos líderes entre t=180s e t=600s
./ns3 run "contaski --nNodes=200 --run=1 \
  --failurePercentageMin=10 --failurePercentageMax=30 \
  --failureTimeMin=180 --failureTimeMax=600"
```

---

## Saída

- `APStats_run_<R>.txt` — tarefas despachadas e pendentes ao fim da simulação
- Logs com prefixos `FAILURE:`, `FAILURE_SCHEDULED:`, `FAILURE_CONFIG:`, `FAILURE_ELIGIBLE:`

---

## Relação com as Outras Branches

| Branch | Diferença em relação a esta |
|---|---|
| `sectional-optimal` | Sem falhas (baseline ideal) |
| `sectional-rl1` | Adiciona QL para realocar órfãos em clusters existentes |
| `sectional-rl2` | Adiciona QL para formar novos clusters entre órfãos |
| `sectional-rl3` | QL unificado: decide entre 3 estratégias com estado composto |
| `sectional-rl3.1` | RL3 com recompensa contextualizada pelo tamanho do grupo |
