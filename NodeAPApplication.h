#pragma once

#include "ns3/core-module.h"
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/node-container.h"

#include "capabilities.h"
#include "task.h"
#include "QLearningAgent.h"

using namespace ns3;

namespace nr2{
    class NodeApplication;  // Forward declaration

    class NodeAPApplication : public Application{
        private:
            taskVector*             tasks;              // Vector of tasks waiting to be dispatched
            taskVector*             dispatchedTasks;    // Vector of dispatched tasks
            Task*                   currentDispatchedTask;
            std::vector<Ipv6Address>*   clusterLeaders;     // Addresses of clusterLeaders
            std::vector<Ipv6Address>*   aptLeaders;         // Leaders que aceitaram tarefas (aptos)
            Ptr<Socket>     		m_socket;       	// Associated socket
            Address					m_node;				// Node's
            TypeId          		m_tid;          	// Type of the socket used
            Ipv6Address             GetNodeIpAddress();
            uint32_t                confirmationsSinceLastDispatch;
            uint32_t                requiredQuorum;  // Número mínimo de agrupamentos que devem aceitar

            // Falhas
            double                  failurePercentage;  // Porcentagem de líderes aptos que irão falhar
            double                  failurePercentageMin;  // Porcentagem mínima (para modo aleatório)
            double                  failurePercentageMax;  // Porcentagem máxima (para modo aleatório)
            double                  failureTime;        // Tempo fixo de falha (segundos)
            double                  failureTimeMin;     // Tempo mínimo de falha (para modo aleatório)
            double                  failureTimeMax;     // Tempo máximo de falha (para modo aleatório)
            double                  actualFailureTime;  // Tempo real de falha (após sorteio)
            double                  actualFailurePercentage;  // Porcentagem real de falha (após sorteio)
            NodeContainer           networkNodes;       // Referência aos nós da rede

            // Mapeamento líder -> membros do cluster
            std::map<Ipv6Address, std::vector<Ipv6Address>>* clusterMembers;
            
            // Mapeamento líder -> capacidades do cluster (para cálculo de similaridade)
            std::map<Ipv6Address, capabilitiesVector>* clusterCapabilities;
            
            // Lista de nós órfãos após falha
            std::vector<Ipv6Address>* orphanedNodes;
            
            // Grupos de órfãos por líder falho
            std::map<Ipv6Address, std::vector<Ipv6Address>>* orphanGroups;

            // Agente Q-Learning para realocação
            QLearningAgent* rlAgent;
            
            // Contadores para métricas
            int successfulReallocations;
            int failedReallocations;
            int reallocatedToExisting;      // RL1: nós realocados para clusters existentes
            int newClustersFormed;          // RL2: novos clusters formados
            int nodesInNewClusters;         // RL2: nós em novos clusters

        public:
            void setup();
            void generateTasks(int totalDuration);
            void sendTaskToLeaders();
            void recvCallback(Ptr<Socket> socket);
            void taskConfirmation();
            void StartApplication();
            void StopApplication();
            static TypeId GetTypeId();
            void setTasks(taskVector*);

            // Falhas
            void setFailurePercentage(double percentage);
            void setFailurePercentageRange(double min, double max);
            void setFailureTime(double time);
            void setFailureTimeRange(double min, double max);
            void setNodes(NodeContainer nodes);
            void triggerLeaderFailures();
            
            // Getters para métricas (tempo e porcentagem reais usados)
            double getActualFailureTime() { return this->actualFailureTime; }
            double getActualFailurePercentage() { return this->actualFailurePercentage; }
            
            // ========== RL3 MERGED: Processamento unificado de órfãos ==========
            void processOrphans();
            
            // RL1: Cálculo de similaridade com clusters existentes
            double calculateSimilarityWithCluster(Ipv6Address orphan, Ipv6Address clusterLeader);
            double getBestExistingSimilarity(Ipv6Address orphan, Ipv6Address& bestCluster);
            void addNodeToCluster(Ipv6Address orphan, Ipv6Address clusterLeader);
            
            // RL2: Formação de novos clusters com órfãos
            double calculateSimilarityBetweenNodes(Ipv6Address node1, Ipv6Address node2);
            double getAvgOrphanSimilarity(Ipv6Address orphan, std::vector<Ipv6Address>& orphanGroup);
            capabilitiesVector* getNodeCapabilities(Ipv6Address nodeAddr);
            
            // RL2: Processo de clustering entre órfãos
            Ipv6Address electLeaderForOrphanCluster(std::vector<Ipv6Address>& members);
            void registerNewCluster(Ipv6Address newLeader, std::vector<Ipv6Address>& members);
            int getNodeNeighborCount(Ipv6Address nodeAddr);

            // Obter membros do cluster diretamente do nó líder
            void fetchClusterMembers(Ipv6Address leaderAddr);
            
            // Buscar membros de TODOS os líderes (chamado antes de triggerLeaderFailures)
            void fetchAllClusterMembers();
            
            // Configuração do agente RL
            void setQLearningParams(double alpha, double gamma, double epsilon);
            void setQTableFilePath(const std::string& filePath);
    };
}