#include "QLearningAgent.h"
#include <fstream>
#include <iostream>
#include <cmath>

namespace nr2{

    QLearningAgent::QLearningAgent(){
        // Valores padrão dos parâmetros
        this->alpha = 0.1;      // Learning rate
        this->gamma = 0.9;      // Discount factor
        this->epsilon = 0.1;    // Exploration rate (10% exploração, 90% exploitation)
        
        // Inicializar gerador aleatório
        std::random_device rd;
        this->rng = std::mt19937(rd());
        
        this->qTableFilePath = "qtable.csv";
        
        initializeQTable();
    }

    QLearningAgent::QLearningAgent(double alpha, double gamma, double epsilon){
        this->alpha = alpha;
        this->gamma = gamma;
        this->epsilon = epsilon;
        
        // Inicializar gerador aleatório
        std::random_device rd;
        this->rng = std::mt19937(rd());
        
        this->qTableFilePath = "qtable.csv";
        
        initializeQTable();
    }

    QLearningAgent::~QLearningAgent(){
        // Salvar Q-Table ao destruir o agente
        saveQTable(this->qTableFilePath);
    }

    void QLearningAgent::initializeQTable(){
        // Inicializar todos os valores com 0
        for(int s = 0; s < NUM_STATES; s++){
            for(int a = 0; a < NUM_ACTIONS; a++){
                qTable[s][a] = 0.0;
            }
        }
    }

    Action QLearningAgent::chooseAction(State state){
        // Estratégia epsilon-greedy
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double randomValue = dist(this->rng);
        
        if(randomValue < this->epsilon){
            // Exploração: escolher ação aleatória entre as 3
            std::uniform_int_distribution<int> actionDist(0, NUM_ACTIONS - 1);
            return static_cast<Action>(actionDist(this->rng));
        } else {
            // Exploitation: escolher melhor ação baseado na Q-Table
            int bestAction = 0;
            double bestValue = qTable[state][0];
            
            for(int a = 1; a < NUM_ACTIONS; a++){
                if(qTable[state][a] > bestValue){
                    bestValue = qTable[state][a];
                    bestAction = a;
                }
            }
            
            // Verificar empates e resolver aleatoriamente
            std::vector<int> tiedActions;
            for(int a = 0; a < NUM_ACTIONS; a++){
                if(qTable[state][a] == bestValue){
                    tiedActions.push_back(a);
                }
            }
            
            if(tiedActions.size() > 1){
                std::uniform_int_distribution<int> tieDist(0, tiedActions.size() - 1);
                bestAction = tiedActions[tieDist(this->rng)];
            }
            
            return static_cast<Action>(bestAction);
        }
    }

    void QLearningAgent::updateQTable(State state, Action action, double reward, State nextState){
        // Fórmula Q-Learning:
        // Q(s,a) = Q(s,a) + α * [R + γ * max(Q(s',a')) - Q(s,a)]
        
        // Encontrar max Q(s', a') para o próximo estado
        double maxNextQ = qTable[nextState][0];
        for(int a = 1; a < NUM_ACTIONS; a++){
            if(qTable[nextState][a] > maxNextQ){
                maxNextQ = qTable[nextState][a];
            }
        }
        
        // Atualizar Q-value
        double currentQ = qTable[state][action];
        double newQ = currentQ + this->alpha * (reward + this->gamma * maxNextQ - currentQ);
        
        qTable[state][action] = newQ;
    }

    State QLearningAgent::determineState(double simExisting, double simOrphan){
        // RL3: Determinar estado composto a partir de DUAS similaridades
        bool existingHigh = (simExisting >= SIMILARITY_HIGH_THRESHOLD);
        bool orphanHigh = (simOrphan >= SIMILARITY_HIGH_THRESHOLD);
        
        if(existingHigh && orphanHigh){
            return SIM_BOTH_HIGH;
        } else if(existingHigh && !orphanHigh){
            return SIM_EXISTING_HIGH;
        } else if(!existingHigh && orphanHigh){
            return SIM_ORPHAN_HIGH;
        } else {
            return SIM_BOTH_MEDIUM;
        }
    }

    void QLearningAgent::saveQTable(const std::string& filePath){
        std::ofstream file(filePath);
        
        if(file.is_open()){
            // Formato CSV: state,action,qvalue
            file << "state,action,qvalue\n";
            for(int s = 0; s < NUM_STATES; s++){
                for(int a = 0; a < NUM_ACTIONS; a++){
                    file << s << "," << a << "," << qTable[s][a] << "\n";
                }
            }
            file.close();
            std::cout << "Q-Table saved to " << filePath << std::endl;
        } else {
            std::cerr << "Error: Could not open file " << filePath << " for writing" << std::endl;
        }
    }

    void QLearningAgent::loadQTable(const std::string& filePath){
        std::ifstream file(filePath);
        
        if(file.is_open()){
            std::string line;
            // Pular header
            std::getline(file, line);
            
            while(std::getline(file, line)){
                int state, action;
                double qvalue;
                
                // Parse CSV line
                size_t pos1 = line.find(',');
                size_t pos2 = line.find(',', pos1 + 1);
                
                state = std::stoi(line.substr(0, pos1));
                action = std::stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
                qvalue = std::stod(line.substr(pos2 + 1));
                
                if(state >= 0 && state < NUM_STATES && action >= 0 && action < NUM_ACTIONS){
                    qTable[state][action] = qvalue;
                }
            }
            file.close();
            std::cout << "Q-Table loaded from " << filePath << std::endl;
        } else {
            std::cout << "Q-Table file not found, starting with fresh Q-Table" << std::endl;
            initializeQTable();
        }
    }

    void QLearningAgent::setQTableFilePath(const std::string& filePath){
        this->qTableFilePath = filePath;
    }

    double QLearningAgent::getQValue(State state, Action action){
        return qTable[state][action];
    }

    void QLearningAgent::printQTable(){
        std::cout << "=== Q-Table (RL3 Merged: 4 states x 3 actions) ===" << std::endl;
        std::cout << "State\\Action\t\t| DO_NOT_ALLOC\t| REALLOC_EXIST\t| FORM_NEW_CLUST" << std::endl;
        std::cout << "SIM_BOTH_HIGH\t\t| " << qTable[SIM_BOTH_HIGH][DO_NOT_ALLOCATE] 
                  << "\t| " << qTable[SIM_BOTH_HIGH][REALLOCATE_EXISTING] 
                  << "\t| " << qTable[SIM_BOTH_HIGH][FORM_NEW_CLUSTER] << std::endl;
        std::cout << "SIM_EXISTING_HIGH\t| " << qTable[SIM_EXISTING_HIGH][DO_NOT_ALLOCATE] 
                  << "\t| " << qTable[SIM_EXISTING_HIGH][REALLOCATE_EXISTING] 
                  << "\t| " << qTable[SIM_EXISTING_HIGH][FORM_NEW_CLUSTER] << std::endl;
        std::cout << "SIM_ORPHAN_HIGH\t\t| " << qTable[SIM_ORPHAN_HIGH][DO_NOT_ALLOCATE] 
                  << "\t| " << qTable[SIM_ORPHAN_HIGH][REALLOCATE_EXISTING] 
                  << "\t| " << qTable[SIM_ORPHAN_HIGH][FORM_NEW_CLUSTER] << std::endl;
        std::cout << "SIM_BOTH_MEDIUM\t\t| " << qTable[SIM_BOTH_MEDIUM][DO_NOT_ALLOCATE] 
                  << "\t| " << qTable[SIM_BOTH_MEDIUM][REALLOCATE_EXISTING] 
                  << "\t| " << qTable[SIM_BOTH_MEDIUM][FORM_NEW_CLUSTER] << std::endl;
        std::cout << "=================================================" << std::endl;
    }

    void QLearningAgent::setAlpha(double alpha){
        this->alpha = alpha;
    }

    void QLearningAgent::setGamma(double gamma){
        this->gamma = gamma;
    }

    void QLearningAgent::setEpsilon(double epsilon){
        this->epsilon = epsilon;
    }
}