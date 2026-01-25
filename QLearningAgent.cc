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
        for(int s = 0; s < 2; s++){
            for(int a = 0; a < 2; a++){
                qTable[s][a] = 0.0;
            }
        }
    }

    Action QLearningAgent::chooseAction(State state){
        // Estratégia epsilon-greedy
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double randomValue = dist(this->rng);
        
        if(randomValue < this->epsilon){
            // Exploração: escolher ação aleatória
            std::uniform_int_distribution<int> actionDist(0, 1);
            return static_cast<Action>(actionDist(this->rng));
        } else {
            // Exploitation: escolher melhor ação baseado na Q-Table
            if(qTable[state][ALLOCATE] > qTable[state][DO_NOT_ALLOCATE]){
                return ALLOCATE;
            } else if(qTable[state][DO_NOT_ALLOCATE] > qTable[state][ALLOCATE]){
                return DO_NOT_ALLOCATE;
            } else {
                // Empate: escolher aleatoriamente
                std::uniform_int_distribution<int> actionDist(0, 1);
                return static_cast<Action>(actionDist(this->rng));
            }
        }
    }

    void QLearningAgent::updateQTable(State state, Action action, double reward, State nextState){
        // Fórmula Q-Learning:
        // Q(s,a) = Q(s,a) + α * [R + γ * max(Q(s',a')) - Q(s,a)]
        
        // Encontrar max Q(s', a') para o próximo estado
        double maxNextQ = std::max(qTable[nextState][ALLOCATE], qTable[nextState][DO_NOT_ALLOCATE]);
        
        // Atualizar Q-value
        double currentQ = qTable[state][action];
        double newQ = currentQ + this->alpha * (reward + this->gamma * maxNextQ - currentQ);
        
        qTable[state][action] = newQ;
    }

    State QLearningAgent::similarityToState(double similarity){
        // Converter similaridade contínua em estado discreto
        if(similarity >= SIMILARITY_HIGH_THRESHOLD){
            return SIMILARITY_HIGH;
        } else {
            return SIMILARITY_MEDIUM;
        }
    }

    void QLearningAgent::saveQTable(const std::string& filePath){
        std::ofstream file(filePath);
        
        if(file.is_open()){
            // Formato CSV: state,action,qvalue
            file << "state,action,qvalue\n";
            for(int s = 0; s < 2; s++){
                for(int a = 0; a < 2; a++){
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
                
                if(state >= 0 && state < 2 && action >= 0 && action < 2){
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
        std::cout << "=== Q-Table ===" << std::endl;
        std::cout << "State\\Action\t| DO_NOT_ALLOCATE\t| ALLOCATE" << std::endl;
        std::cout << "SIMILARITY_MEDIUM\t| " << qTable[SIMILARITY_MEDIUM][DO_NOT_ALLOCATE] 
                  << "\t\t\t| " << qTable[SIMILARITY_MEDIUM][ALLOCATE] << std::endl;
        std::cout << "SIMILARITY_HIGH\t\t| " << qTable[SIMILARITY_HIGH][DO_NOT_ALLOCATE] 
                  << "\t\t\t| " << qTable[SIMILARITY_HIGH][ALLOCATE] << std::endl;
        std::cout << "===============" << std::endl;
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