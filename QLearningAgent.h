#pragma once

#include <string>
#include <random>

namespace nr2{

    // Estados baseados na similaridade do nó órfão com cluster destino
    // (threshold de realocação >= 0.85)
    enum State {
        SIMILARITY_MEDIUM = 0,  // 0.85 - 0.92
        SIMILARITY_HIGH = 1     // >= 0.92
    };

    // Ações possíveis
    enum Action {
        DO_NOT_ALLOCATE = 0,    // Deixar órfão
        ALLOCATE = 1            // Alocar no cluster
    };

    // Recompensas
    const double REWARD_SUCCESS = 10.0;      // Nó realocado com sucesso
    const double REWARD_ORPHAN = -5.0;       // Nó continua órfão
    const double REWARD_INVALID = -10.0;     // Tentou alocar sem similaridade válida

    // Threshold de similaridade para realocação
    const double REALLOCATION_THRESHOLD = 0.85;
    const double SIMILARITY_HIGH_THRESHOLD = 0.92;

    // RL2: Threshold de similaridade para clustering (mesmo do cenário ótimo)
    const double CLUSTERING_THRESHOLD = 0.95;
    
    // RL2: Mínimo de nós para formar um novo cluster
    const int MIN_NODES_FOR_NEW_CLUSTER = 2;

    class QLearningAgent {
        private:
            // Q-Table: 2 estados x 2 ações
            double qTable[2][2];
            
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
            
            // Converter similaridade em estado discreto
            State similarityToState(double similarity);
            
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