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
            NodeContainer           networkNodes;       // Referência aos nós da rede

            // Mapeamento líder -> membros do cluster
            std::map<Ipv6Address, std::vector<Ipv6Address>>* clusterMembers;
            
            // Mapeamento líder -> capacidades do cluster (para cálculo de similaridade)
            std::map<Ipv6Address, capabilitiesVector>* clusterCapabilities;
            
            // Lista de nós órfãos após falha
            std::vector<Ipv6Address>* orphanedNodes;
            
            // Agente Q-Learning para realocação
            QLearningAgent* rlAgent;
            
            // Contadores para métricas
            int successfulReallocations;
            int failedReallocations;

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
            void setNodes(NodeContainer nodes);
            void triggerLeaderFailures();
            
            // Realocação com Q-Learning
            void reallocateOrphans();
            double calculateSimilarity(Ipv6Address orphanAddr, Ipv6Address leaderAddr);
            capabilitiesVector* getNodeCapabilities(Ipv6Address nodeAddr);
            bool addNodeToCluster(Ipv6Address orphanAddr, Ipv6Address leaderAddr);
            
            // Obter membros do cluster diretamente do nó líder
            void fetchClusterMembers(Ipv6Address leaderAddr);
            
            // Buscar membros de TODOS os líderes (chamado antes de triggerLeaderFailures)
            void fetchAllClusterMembers();
            
            // Configuração do agente RL
            void setQLearningParams(double alpha, double gamma, double epsilon);
            void setQTableFilePath(const std::string& filePath);
    };
}