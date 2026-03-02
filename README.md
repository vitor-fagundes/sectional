# SECTIONAL

**Agrupamentos Resilientes em Redes IIoT Baseados em Q-Learning para Realocação Adaptativa de Nós Órfãos**

> Projeto de dissertação de mestrado — Vítor Fagundes  
> Orientadores: Prof. Aldri Santos (UFMG) e Carlos Pedroso (UFPR)  
> Universidade Federal de Minas Gerais (UFMG) / CCSC

---

## Sumário

- [Visão Geral](#visão-geral)
- [Fundamentação: O Protocolo CONTASKI](#fundamentação-o-protocolo-contaski)
- [O Sistema SECTIONAL](#o-sistema-sectional)
- [Estratégias de Aprendizado por Reforço](#estratégias-de-aprendizado-por-reforço)
- [Arquitetura do Código](#arquitetura-do-código)
- [Cenários Experimentais](#cenários-experimentais)
- [Estrutura de Branches](#estrutura-de-branches)
- [Execução](#execução)
- [Resultados](#resultados)
- [Tecnologias e Ferramentas](#tecnologias-e-ferramentas)

---

## Visão Geral

O SECTIONAL é um sistema de resiliência para redes IIoT (Industrial Internet of Things) que integra **Q-Learning** ao protocolo de agrupamento cooperativo **CONTASKI**. O objetivo é criar mecanismos adaptativos para lidar com falhas de nós líderes em redes de sensores sem fio, realocando automaticamente os nós órfãos resultantes dessas falhas.

Quando um líder de cluster falha durante a execução de uma tarefa, seus membros ficam "órfãos" — desconectados do ciclo de tarefas e sem coordenação. O SECTIONAL utiliza um agente Q-Learning centralizado no Access Point (AP) para decidir, nó a nó, a melhor estratégia de recuperação: realocar para um cluster existente, formar um novo cluster com outros órfãos, ou não agir.

---

## Fundamentação: O Protocolo CONTASKI

O SECTIONAL se baseia no algoritmo **CONTASKI** (Cooperative Task Assignment based on Relational Consensus for IIoT Networks), que implementa agrupamento por similaridade de capacidades sensoriais.

### Fases do protocolo

1. **Beaconing (0–60s):** Os nós descobrem vizinhos por broadcast, trocando informações sobre o tamanho de suas listas de vizinhança.

2. **Disseminação de Capacidades (60–90s):** Cada nó anuncia suas capacidades sensoriais (Temperatura, Umidade, Presença, Luminosidade, Sincronização, Pressão de Gás, Nível de Reservatório) via broadcast.

3. **Cálculo de Similaridade (~90s):** Cada nó calcula a similaridade com seus vizinhos usando a Equação 1 do artigo CONTASKI:

   ```
   sim(ob1, ob2) = |Cob1 ∩ Cob2| / √(|Cob1| × |Cob2|)
   ```

4. **Clusterização:** Nós com similaridade ≥ 0.95 (threshold configurável) são agrupados no mesmo cluster.

5. **Eleição de Líder:** Dentro de cada cluster, o nó com mais vizinhos é eleito líder. Em caso de empate, desempata-se por número de capacidades; persistindo o empate, pelo menor endereço IPv6.

6. **Registro e Despacho de Tarefas:** Líderes registram-se no AP, que então despacha tarefas. Clusters aptos (que possuem as capacidades requeridas) aceitam as tarefas; os demais permanecem ociosos.

### Conceitos-chave

- **Cluster Apto (AAT):** Agrupamento cujas capacidades atendem aos requisitos da tarefa e que possui quorum suficiente. Aceita tarefas do AP.
- **Cluster Ocioso (AO):** Agrupamento que não atende os requisitos da tarefa atual. Aguarda próximas tarefas.

---

## O Sistema SECTIONAL

O SECTIONAL estende o CONTASKI com um mecanismo de **falha e recuperação** baseado em Aprendizado por Reforço.

### Mecanismo de Falha

- Apenas **líderes de clusters aptos** (executando tarefas) estão sujeitos a falha.
- A falha desativa completamente o nó líder (para de funcionar e comunicar), mas seus seguidores permanecem na rede como **nós órfãos**.
- Taxas de falha configuráveis: 10%, 20%, 30% dos líderes aptos.
- Tempo de falha configurável: fixo (310s) ou aleatório (180–600s).

### Realocação com Q-Learning

O AP hospeda um agente Q-Learning que decide como realocar cada nó órfão individualmente, com base na similaridade do órfão com clusters existentes e com outros órfãos do mesmo grupo.

---

## Estratégias de Aprendizado por Reforço

O projeto implementa quatro estratégias incrementais, cada uma em sua branch:

### RL1 (`sectional-rl1`)

A abordagem mais simples. O agente decide se realoca cada órfão para um **cluster existente** com similaridade suficiente.

- **Q-Table:** 2 estados × 2 ações
- **Estados:** SIMILARITY_MEDIUM (0.85–0.92), SIMILARITY_HIGH (≥ 0.92)
- **Ações:** ALLOCATE, DO_NOT_ALLOCATE
- **Recompensas:** +10 (sucesso), −5 (órfão), −10 (inválido)
- **Threshold de realocação:** 0.85

### RL2 (`sectional-rl2`)

Os nós órfãos **formam novos clusters entre si**, passando pelo processo completo de similaridade, clusterização e eleição de líder — como no cenário ótimo.

- **Q-Table:** 2 estados × 2 ações
- **Lógica:** Órfãos do mesmo líder falho tendem a ter alta similaridade (estavam no mesmo cluster), favorecendo a formação de novos clusters viáveis.

### RL3 (`sectional-rl3`)

Abordagem **unificada** que combina RL1 e RL2. O agente decide, nó a nó, entre três ações possíveis, considerando dois eixos de similaridade.

- **Q-Table:** 4 estados × 3 ações
- **Estados:**
  - `SIM_BOTH_HIGH` — alta similaridade com clusters existentes E com outros órfãos
  - `SIM_EXISTING_HIGH` — alta similaridade apenas com clusters existentes
  - `SIM_ORPHAN_HIGH` — alta similaridade apenas com outros órfãos
  - `SIM_BOTH_MEDIUM` — similaridade média com ambos
- **Ações:** DO_NOT_ALLOCATE, REALLOCATE_EXISTING (RL1), FORM_NEW_CLUSTER (RL2)
- **Threshold alto:** ≥ 0.92

### RL3.1 (evolução do RL3)

Aprimoramento do RL3 com **função de recompensa sensível ao contexto**. Mesma Q-Table 4×3, mas com recompensas proporcionais à qualidade da similaridade e ao tamanho do grupo de órfãos.

- **Recompensa proporcional à similaridade:**
  - `REALLOCATE_EXISTING:` reward = base × simExisting × fator_capacidade
  - `FORM_NEW_CLUSTER:` reward = base × simOrphan × fator_tamanho
  - Fator de capacidade: 1.0 (sim ≥ 0.95), 0.8 (sim ≥ 0.90), 0.4 (sim < 0.90)
  - Fator de tamanho: 1.0 (grupo > 10), 0.7 (grupo 6–10), 0.3 (grupo ≤ 5)

- **Consciência do tamanho do grupo:**
  - Grupo grande (> 10 nós): bônus +3 para FORM, penalidade −2 para REALLOC
  - Grupo pequeno (≤ 5 nós): penalidade −3 para FORM, bônus +3 para REALLOC
  - Grupo médio (6–10): sem bônus, agente decide livremente

---

## Arquitetura do Código

```
scratch/sectional/
├── contaski.cc              # Setup da simulação NS-3 (main)
├── NodeApplication.cc/h     # Aplicação dos nós sensores
├── NodeAPApplication.cc/h   # Aplicação do Access Point
├── QLearningAgent.cc/h      # Agente Q-Learning
├── capabilities.cc/h        # Cálculo de similaridade de capacidades
├── task.cc/h                # Modelo de tarefas
├── MyTag.cc/h               # Tags para tipos de mensagem em pacotes
└── constants.h              # Enum de MessageTypes
```

### Componentes Principais

**`contaski.cc`** — Arquivo principal. Configura a simulação NS-3 com 200 nós em topologia grid (200m × 200m), usando IEEE 802.15.4 com 6LoWPAN sobre IPv6. Define potência de transmissão (−10 dBm para nós, +9 dBm para AP, +10 dBm para líderes). Tempo total de simulação: 900s.

**`NodeApplication`** — Aplicação instalada em cada nó sensor. Gerencia beacon, disseminação de capacidades, cálculo de similaridade, clusterização, eleição de líder e execução de tarefas. Comunicação via UDP na porta 2020.

**`NodeAPApplication`** — Aplicação do Access Point. Registra líderes, despacha tarefas com verificação de quorum, gerencia confirmações. Nas branches com RL, também hospeda o QLearningAgent, detecta falhas de líderes, identifica nós órfãos e executa a realocação.

**`QLearningAgent`** — Implementação do agente Q-Learning com política ε-greedy. Parâmetros: α = 0.1 (taxa de aprendizado), γ = 0.9 (fator de desconto), ε = 0.1 (exploração). Suporta persistência da Q-Table em arquivo CSV entre rodadas de simulação.

**`capabilities`** — Define 7 tipos de capacidades sensoriais e implementa duas funções de similaridade: a baseada em conjuntos (Equação 1 do CONTASKI) e a UFD (pesos por capacidade). O SECTIONAL utiliza a primeira.

**`Task`** — Modelo de tarefa com ID, duração (60s), quorum (1–3 clusters), e capacidades requeridas. Tarefas são geradas aleatoriamente e serializadas para transmissão via rede.

**`constants.h`** — Define os tipos de mensagem: TaskDispatch, TaskAccept, LeaderRegister, CapabilityDissemination, LeaderToCluster, Beacon e FollowerRegister (adicionado nas branches com falha).

---

## Cenários Experimentais

A avaliação é organizada em três cenários, cada um com sub-cenários:

### Cenário 1: Tempo Fixo, Porcentagem Fixa

Falha ocorre no segundo 310 da simulação com porcentagem fixa de líderes aptos.

| Sub-cenário | Tempo de Falha | Taxa de Falha |
|-------------|----------------|---------------|
| 1.1         | 310s           | 10%           |
| 1.2         | 310s           | 20%           |
| 1.3         | 310s           | 30%           |

### Cenário 2: Tempo Aleatório, Porcentagem Fixa

Falha ocorre em tempo aleatório entre 180s e 600s.

| Sub-cenário | Tempo de Falha | Taxa de Falha |
|-------------|----------------|---------------|
| 2.1         | 180–600s       | 10%           |
| 2.2         | 180–600s       | 20%           |
| 2.3         | 180–600s       | 30%           |

### Cenário 3: Totalmente Aleatório (SECTIONAL Full Random)

| Sub-cenário | Tempo de Falha | Taxa de Falha |
|-------------|----------------|---------------|
| 3.1         | 180–600s       | 10–30%        |

**Total por estratégia:** 7 sub-cenários × 35 rodadas = **245 simulações**

Cada sub-cenário mantém **Q-Tables independentes** para evitar contaminação cruzada entre configurações.

---

## Estrutura de Branches

| Branch                   | Descrição                                          |
|--------------------------|----------------------------------------------------|
| `sectional-optimal`      | Baseline sem falhas (cenário ótimo)                |
| `sectional-with-fail`    | Falhas sem aprendizado (baseline com falhas)       |
| `sectional-rl1`          | RL1: Realocação para clusters existentes           |
| `sectional-rl2`          | RL2: Formação de novos clusters entre órfãos       |
| `sectional-rl3`          | RL3: Abordagem unificada (RL1 + RL2)              |

---

## Execução

### Pré-requisitos

- NS-3.36
- GCC com suporte a C++17
- Python 3 (para scripts de análise)

### Compilação

```bash
cd ~/workspace/ns-3-dev/
./ns3 build
```

### Execução simples

```bash
./ns3 run "scratch/sectional/contaski --nNodes=200 --run=0"
```

### Execução com falha e Q-Learning

```bash
./ns3 run "scratch/sectional/contaski \
  --nNodes=200 \
  --run=0 \
  --failurePercentage=10 \
  --failureTime=310 \
  --qTablePath=qtable.csv"
```

### Parâmetros de linha de comando

| Parâmetro            | Descrição                                    | Padrão  |
|----------------------|----------------------------------------------|---------|
| `--nNodes`           | Número de nós sensores                       | 200     |
| `--run`              | Número da rodada de simulação                | 0       |
| `--failurePercentage`| Porcentagem de líderes aptos que falham      | 0       |
| `--failureTime`      | Tempo (s) em que ocorre a falha              | 310     |
| `--qTablePath`       | Caminho do arquivo da Q-Table (persistência) | —       |
| `--rlAlpha`          | Taxa de aprendizado (α)                      | 0.1     |
| `--rlGamma`          | Fator de desconto (γ)                        | 0.9     |
| `--rlEpsilon`        | Taxa de exploração (ε)                       | 0.1     |

---

## Resultados

### Métricas Coletadas

- **Clustering:** Total de clusters, clusters aptos, clusters ociosos, tamanho médio dos clusters
- **Tarefas:** Tarefas despachadas, aceitas, taxa de sucesso
- **Realocação:** Nós órfãos, nós realocados, taxa de realocação, novos clusters formados
- **Q-Learning:** Evolução dos Q-Values, convergência da política, taxa de exploração vs. exploitation

### Resultados Representativos (RL1)

| Cenário              | Taxa de Realocação | IC 95% |
|----------------------|-------------------|--------|
| 310s / 10%           | ~94.4%            | ±2%    |
| 310s / 20%           | ~95.4%            | ±2%    |
| 310s / 30%           | ~94.8%            | ±2%    |
| Full Random (FR)     | ~96.2%            | ±2%    |

### Convergência do Q-Learning

O agente demonstra convergência rápida (~15 rodadas) com política estável. Exemplo de Q-Table convergida (RL3):

| Estado           | DO_NOT_ALLOC | REALLOC_EXISTING | FORM_NEW_CLUSTER |
|------------------|-------------|-----------------|-----------------|
| SIM_BOTH_HIGH    | 7.35        | 0               | **44.17**       |
| SIM_EXISTING_HIGH| **42.81**   | **97.40**       | 0               |

A política aprendida é coerente: quando ambas as similaridades são altas, forma novos clusters (grupo autossuficiente); quando apenas a similaridade com existentes é alta, realoca para clusters existentes.

---

## Tecnologias e Ferramentas

| Componente       | Tecnologia                                              |
|------------------|---------------------------------------------------------|
| Simulador        | NS-3.36                                                 |
| Comunicação      | IEEE 802.15.4 / 6LoWPAN / IPv6 / UDP                   |
| Linguagem (sim)  | C++17                                                   |
| Linguagem (análise) | Python 3                                             |
| Controle de versão | Git / GitHub                                          |
| Análise de dados | Python (pandas, matplotlib)                             |
| Visualização     | NetAnim, scripts Python                                 |
| Validação        | 35 rodadas × 7 sub-cenários com intervalos de confiança |

---

## Referências

- **CONTASKI:** Atribuições Cooperativas de Tarefas de Sensoriamento Baseada em Consenso Relacional para Redes IIoT

---

## Licença

Projeto acadêmico — Universidade Federal de Minas Gerais (UFMG).
