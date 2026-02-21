#pragma once

#include <string>
#include <random>

namespace nr2{

    // Número de estados e ações
    const int NUM_STATES = 4;
    const int NUM_ACTIONS = 3;

    // ========== RL3 MERGED: Estados baseados em DUAS similaridades ==========
    // Similaridade com clusters existentes (RL1) E com outros órfãos (RL2)
    enum State {
        SIM_BOTH_HIGH = 0,          // Alta sim. com existentes E com órfãos
        SIM_EXISTING_HIGH = 1,      // Alta sim. com existentes, baixa com órfãos
        SIM_ORPHAN_HIGH = 2,        // Baixa sim. com existentes, alta com órfãos
        SIM_BOTH_MEDIUM = 3         // Média sim. com ambos
    };

    // ========== RL3 MERGED: 3 ações possíveis ==========
    enum Action {
        DO_NOT_ALLOCATE = 0,        // Deixar órfão
        REALLOCATE_EXISTING = 1,    // Realocar para cluster existente (RL1)
        FORM_NEW_CLUSTER = 2        // Formar novo cluster com órfãos (RL2)
    };

    // Recompensas
    const double REWARD_SUCCESS = 10.0;      // Nó realocado com sucesso
    const double REWARD_ORPHAN = -5.0;       // Nó continua órfão
    const double REWARD_INVALID = -10.0;     // Tentou alocar sem similaridade válida

    // Threshold de similaridade para realocação
    const double REALLOCATION_THRESHOLD = 0.85;
    const double SIMILARITY_HIGH_THRESHOLD = 0.92;

    // Threshold de similaridade para clustering (mesmo do cenário ótimo)
    const double CLUSTERING_THRESHOLD = 0.95;
    
    // Mínimo de nós para formar um novo cluster
    const int MIN_NODES_FOR_NEW_CLUSTER = 2;

    class QLearningAgent {
        private:
            // Q-Table: 4 estados x 3 ações
            double qTable[NUM_STATES][NUM_ACTIONS];
            
            // Parâmetros do Q-Learning
            double alpha;       // Learning rate (taxa de aprendizado)
            double gamma;       // Discount factor (fator de desconto)
            double epsilon;     // Exploration rate (taxa de exploração)
            
            // Gerador de números aleatórios
            std::mt19937 rng;
            
            // Caminho do arquivo para persistência
            std::string qTableFilePath;

        public:
            QLearningAgent();
            QLearningAgent(double alpha, double gamma, double epsilon);
            ~QLearningAgent();
            
            // Inicializar Q-Table com zeros
            void initializeQTable();
            
            // Escolher ação baseado no estado (epsilon-greedy)
            Action chooseAction(State state);
            
            // Atualizar Q-Table após receber recompensa
            void updateQTable(State state, Action action, double reward, State nextState);
            
            // RL3: Determinar estado composto a partir de DUAS similaridades
            State determineState(double simExisting, double simOrphan);
            
            // Persistência da Q-Table
            void saveQTable(const std::string& filePath);
            void loadQTable(const std::string& filePath);
            void setQTableFilePath(const std::string& filePath);
            
            // Getters para debug/logging
            double getQValue(State state, Action action);
            void printQTable();
            
            // Setters para parâmetros
            void setAlpha(double alpha);
            void setGamma(double gamma);
            void setEpsilon(double epsilon);
    };
}