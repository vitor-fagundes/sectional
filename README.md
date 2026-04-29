# SECTIONAL — `sectional-optimal`

> **Baseline ótimo sem falhas e sem aprendizado por reforço.**
> Esta branch implementa o comportamento ideal de referência: clusterização por similaridade de capacidades, eleição de líder e despacho de tarefas em rede IIoT simulada com NS-3.

---

## Visão Geral

O `sectional-optimal` é o ponto de partida do projeto. Executa a clusterização SECTIONAL completa em uma rede de nós IIoT sobre IEEE 802.15.4 (LR-WPAN) com 6LoWPAN/IPv6, sem injeção de falhas e sem módulo RL. Serve como baseline para comparação com as branches `sectional-rl*`.

---

## Estrutura de Arquivos

```
contaski.cc            — Entrada principal: configura nós, rede e agenda eventos
NodeApplication.cc/h   — Aplicação de cada nó (beaconing, disseminação, clustering)
NodeAPApplication.cc/h — Aplicação do AP (recebe registros, despacha tarefas)
capabilities.cc/h      — Vetores de capacidades e função de similaridade (Jaccard)
task.cc/h              — Modelo de tarefas com capacidades requeridas e quorum
constants.h            — Enum de tipos de mensagem
MyTag.cc/h             — Tag NS-3 para identificação de mensagens UDP
```

---

## Fluxo de Simulação

| Instante | Evento |
|---|---|
| `t = 0 s` | Nós iniciam beaconing (broadcast do tamanho da vizinhança a cada 0,5 s) |
| `t = 0–60 s` | Fase de beaconing: cada nó constrói `neighList` |
| `t = 60–90 s` | Disseminação de capacidades (broadcast das capacidades de cada nó) |
| `t = 90,5 s` | Cálculo de similaridade + formação de clusters + eleição de líder |
| `t ≈ 91 s` | Líderes eleitos registram-se no AP (`LeaderRegister`) |
| `t = 150 s` | AP despacha primeira tarefa a todos os líderes |
| `t = 150–900 s` | Ciclo de despacho → confirmação → próxima tarefa |

---

## Algoritmo de Clusterização

### Similaridade de Capacidades

Utiliza a função `capabilitiesSimilarity` (coeficiente de Jaccard):

```
sim(A, B) = |A ∩ B| / |A ∪ B|
```

O limiar de inclusão no cluster é **`SIMILARITY_THRESHOLD = 0.95`** — apenas nós com alta sobreposição de capacidades são agrupados.

### Eleição de Líder (3 critérios em cascata)

1. **Maior número de vizinhos** na `neighList` (conectividade)
2. **Maior número de capacidades** (desempate por riqueza funcional)
3. **Menor endereço IPv6** (desempate determinístico final)

O líder eleito aumenta sua potência de transmissão (10 dBm, canal 11) para alcançar o AP e envia `LeaderRegister`.

---

## Despacho de Tarefas (AP)

- O AP mantém uma fila de tarefas carregadas de `tasksFile-<N>.txt`
- Cada tarefa possui: identificador, conjunto de capacidades requeridas e quorum mínimo
- O AP envia a tarefa a todos os líderes registrados
- Cada líder verifica se `capacidades_cluster ⊇ capacidades_tarefa`; se sim, envia `TaskAccept` e repassa a tarefa aos membros via `LeaderToCluster`
- Se o quorum não for atingido em 10 s, a tarefa é re-enfileirada

---

## Parâmetros Principais

| Parâmetro | Valor | Descrição |
|---|---|---|
| `SIMILARITY_THRESHOLD` | 0.95 | Limiar Jaccard para ingresso no cluster |
| `SIMTIME` | 900 s | Duração total da simulação |
| Início do beaconing | 0 s | Imediato ao iniciar a aplicação |
| Disseminação de capacidades | 60–90 s | Broadcast a cada 0,5 s |
| Cálculo de similaridade | 90,5 s | Único disparo pós-disseminação |
| Despacho inicial de tarefas | 150 s | Após estabilização da clusterização |
| Timeout de confirmação | 10 s | Janela para quorum após despacho |

---

## Entrada de Dados

| Arquivo | Conteúdo |
|---|---|
| `capacitiesFile-<N>-contaski.txt` | Vetor de capacidades de cada nó (gerado automaticamente se ausente) |
| `tasksFile-<N>.txt` | Lista de 12 tarefas com capacidades e quorum (gerado automaticamente se ausente) |

---

## Saída

- `APStats.txt` — lista de tarefas despachadas e pendentes ao final da simulação
- Logs NS-3 em stdout via `NS_LOG_INFO` com prefixos `N:`, `AP:`, `LR:`

---

## Como Compilar e Executar

```bash
# Na raiz do ns-3-dev
./ns3 build

# Executar com N nós, rodada R
./ns3 run "contaski --nNodes=200 --run=1"
```

---

## Relação com as Outras Branches

| Branch | Diferença em relação a esta |
|---|---|
| `sectional-with-fail` | Adiciona injeção de falhas de líder sem recuperação RL |
| `sectional-rl1` | Adiciona Q-Learning para realocar órfãos em clusters existentes |
| `sectional-rl2` | Adiciona formação de novos clusters entre órfãos |
| `sectional-rl3` | Unifica rl1 + rl2 com estado composto (4 estados × 3 ações) |
| `sectional-rl3.1` | Estende rl3 com recompensa contextualizada pelo tamanho do grupo |
| `intuitive-sectional` / `ddos-intuitive` | Sistema dual S1/S2 com aprendizado intuitivo |
