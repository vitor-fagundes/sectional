#include "NodeAPApplication.h"
#include "NodeApplication.h"
#include "ns3/address.h"
#include "ns3/ipv6.h"
#include "ns3/udp-socket-factory.h"
#include "MyTag.h"
#include "constants.h"

#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Contaski_V1_AP");

namespace nr2{
    Ipv6Address NodeAPApplication::GetNodeIpAddress(){
        Ptr <Node> PtrNode = this->GetNode();
        Ptr<Ipv6> ipv6 = PtrNode->GetObject<Ipv6> ();
        Ipv6InterfaceAddress iaddr = ipv6->GetAddress (1,0);
        Ipv6Address ipAddr = iaddr.GetAddress();

        return ipAddr;
    }

    void NodeAPApplication::setup(){
        this->m_node = GetNodeIpAddress();
        this->m_tid = ns3::UdpSocketFactory::GetTypeId();
        this->m_socket = this->GetNode()->GetObject<Socket>();
        this->clusterLeaders = new std::vector<Ipv6Address>;
        this->aptLeaders = new std::vector<Ipv6Address>;
        this->dispatchedTasks = new taskVector();
        this->clusterMembers = new std::map<Ipv6Address, std::vector<Ipv6Address>>;
        this->clusterCapabilities = new std::map<Ipv6Address, capabilitiesVector>;
        this->orphanedNodes = new std::vector<Ipv6Address>;
        
        this->orphanGroups = new std::map<Ipv6Address, std::vector<Ipv6Address>>;  // RL2

        // Inicializar agente Q-Learning
        this->rlAgent = new QLearningAgent();
        
        // Contadores de métricas
        this->successfulReallocations = 0;
        this->failedReallocations = 0;

        this->newClustersFormed = 0;      // RL2
        this->nodesInNewClusters = 0;     // RL2

        this->confirmationsSinceLastDispatch = 0;
        
        // Inicializar parâmetros de falha
        this->failurePercentage = 0.0;
        this->failurePercentageMin = 0.0;
        this->failurePercentageMax = 0.0;
        this->failureTime = 310.0;  // Valor padrão: 310s
        this->failureTimeMin = 0.0;
        this->failureTimeMax = 0.0;
        this->actualFailureTime = 0.0;
        this->actualFailurePercentage = 0.0;

        for(auto task:*this->tasks){
            task->print();
        }
    }

    void NodeAPApplication::StartApplication(){
        // Tentar carregar Q-Table de rodadas anteriores
        this->rlAgent->loadQTable("qtable.csv");
        
        // If socket is not created yet
        if(!this->m_socket){
            // Create socket
            auto netdev = this->GetNode()->GetDevice(2);
            
            this->m_socket = Socket::CreateSocket(GetNode(), m_tid);
            this->m_socket->BindToNetDevice(netdev);
            
            this->m_socket->SetAllowBroadcast(true);
            this->m_socket->Bind(Inet6SocketAddress(Ipv6Address::GetAny (), 2020));
            this->m_socket->Listen();
            this->m_socket->SetRecvCallback(MakeCallback (&NodeAPApplication::recvCallback, this));
        }

        m_socket->SetRecvCallback(MakeCallback (&NodeAPApplication::recvCallback, this));
        Simulator::Schedule(Seconds(150), &NodeAPApplication::sendTaskToLeaders, this);

        // Determinar tempo de falha (fixo ou aleatório)
        double scheduledFailureTime = this->failureTime;  // Padrão: tempo fixo
        
        if(this->failureTimeMin > 0 && this->failureTimeMax > 0){
            // Modo aleatório: sortear tempo entre min e max
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<double> timeDist(this->failureTimeMin, this->failureTimeMax);
            scheduledFailureTime = timeDist(gen);
            NS_LOG_INFO("FAILURE_CONFIG: Random time selected: " << scheduledFailureTime << "s (range: " << this->failureTimeMin << "-" << this->failureTimeMax << "s)");
        }
        
        this->actualFailureTime = scheduledFailureTime;
        
        // Determinar porcentagem de falha (fixa ou aleatória)
        double scheduledFailurePercentage = this->failurePercentage;  // Padrão: porcentagem fixa
        
        if(this->failurePercentageMin > 0 && this->failurePercentageMax > 0){
            // Modo aleatório: sortear porcentagem entre min e max
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<double> percDist(this->failurePercentageMin, this->failurePercentageMax);
            scheduledFailurePercentage = percDist(gen);
            NS_LOG_INFO("FAILURE_CONFIG: Random percentage selected: " << scheduledFailurePercentage << "% (range: " << this->failurePercentageMin << "-" << this->failurePercentageMax << "%)");
        }
        
        this->actualFailurePercentage = scheduledFailurePercentage;

        // Agendar falhas se houver porcentagem configurada
        if(scheduledFailurePercentage > 0.0){
            NS_LOG_INFO("FAILURE_SCHEDULED: " << scheduledFailurePercentage << "% at " << scheduledFailureTime << "s");
            Simulator::Schedule(Seconds(scheduledFailureTime), &NodeAPApplication::triggerLeaderFailures, this);
        }
    }

    void NodeAPApplication::StopApplication(){
        this->m_socket->Close();

        // Salvar Q-Table para próxima rodada
        this->rlAgent->saveQTable("qtable.csv");
        this->rlAgent->printQTable();
        
        // Log de métricas de realocação
        NS_LOG_INFO("RL2_METRICS: New clusters formed: " << this->newClustersFormed);
        NS_LOG_INFO("RL2_METRICS: Nodes in new clusters: " << this->nodesInNewClusters);
        NS_LOG_INFO("RL2_METRICS: Successful reallocations: " << this->successfulReallocations);
        NS_LOG_INFO("RL2_METRICS: Failed reallocations: " << this->failedReallocations);
        
        // Log de parâmetros de falha usados (para extração posterior)
        NS_LOG_INFO("FAILURE_ACTUAL: Time=" << this->actualFailureTime << "s Percentage=" << this->actualFailurePercentage << "%");

        stringstream out;
        for(auto task: *this->dispatchedTasks){
            out << task->serialize() << "\n";
        }

        out << "\n\n";

        for(auto task: *this->tasks){
            out << task->serialize() << "\n";
        }

        ofstream outFile("APStats.txt");
        outFile << out.str();
        outFile.close();
    }

    TypeId NodeAPApplication::GetTypeId(){
        static TypeId tid = TypeId ("ns3::NodeAPApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<NodeAPApplication>()
            .AddAttribute ("Protocol", "The type of protocol to use. This should be "
                   "a subclass of ns3::SocketFactory",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&NodeAPApplication::m_tid),
                   // This should check for SocketFactory as a parent
                   MakeTypeIdChecker ())
            ;

        return tid;
    }

    void NodeAPApplication::generateTasks(int totalDuration){
        if(this->tasks == nullptr){
            this->tasks = new taskVector();
        }else{
            this->tasks->clear();
        }

        for(int i = 0; i < 10; i++){
            this->tasks->push_back(new Task());
        }
    }

    void NodeAPApplication::sendTaskToLeaders(){
        // Select one task
        this->currentDispatchedTask = this->tasks->front();

        if(!this->currentDispatchedTask)
            return;

        this->tasks->pop_front();
        std::string serializedTask = this->currentDispatchedTask->serialize();

        // Guardar o quorum necessário (número de agrupamentos)
        this->requiredQuorum = this->currentDispatchedTask->getQuorum();

        // Dispatch
        uint16_t port = 2020;

        Ptr<Packet> pack = Create<Packet>(reinterpret_cast<const uint8_t*> (serializedTask.c_str()), serializedTask.size());
        MyTag tag;
        tag.SetSimpleValue(MessageTypes::TaskDispatch);
        pack->AddPacketTag(tag);

        for(size_t i = 0; i < clusterLeaders->size(); i++) {
            Inet6SocketAddress remote = Inet6SocketAddress(clusterLeaders->at(i), port);
            int status = this->m_socket->SendTo(pack, 0, remote);
            
            if(status == -1){
                NS_LOG_FUNCTION("Could not dispatch task to " << Address(remote.GetIpv6()) << " at port "+remote.GetPort());
            }
            NS_LOG_INFO("AP: MS (" << this->GetNodeIpAddress() << ", " << remote.GetIpv6() << ", " << status << ")");
        }

        this->confirmationsSinceLastDispatch = 0;

        NS_LOG_INFO("AP: TD " << this->currentDispatchedTask->getTid() << " " << Simulator::Now().GetSeconds());
        Simulator::Schedule(Seconds(10), &NodeAPApplication::taskConfirmation, this);
    }

    void NodeAPApplication::recvCallback(Ptr<Socket> socket){
        Ptr<Packet> packet;
        Address from;
        Ipv6Address fromIP;
        MyTag tag;
        
        while((packet = socket->RecvFrom(from))){
            fromIP = Inet6SocketAddress::ConvertFrom(from).GetIpv6();
            packet->PeekPacketTag(tag);
            uint8_t *buffer = new uint8_t[packet->GetSize()];
            packet->CopyData(buffer, packet->GetSize());

            switch (tag.GetSimpleValue()){
                case MessageTypes::LeaderRegister:
                {
                    NS_LOG_INFO("LR: " << fromIP << " at " << Simulator::Now().GetSeconds());
                    this->clusterLeaders->push_back(fromIP);
                    // REMOVIDO: fetchClusterMembers aqui - será feito antes de triggerLeaderFailures
                    break;
                }
                
                case MessageTypes::TaskAccept:
                    NS_LOG_INFO("AP: TA " << this->currentDispatchedTask->getTid() << ", " << fromIP << " " << Simulator::Now().GetSeconds());
                    this->confirmationsSinceLastDispatch++;
                    // Registrar líder como apto
                    if(std::find(this->aptLeaders->begin(), this->aptLeaders->end(), fromIP) == this->aptLeaders->end()){
                        this->aptLeaders->push_back(fromIP);
                    }
                    break;

                default:
                    break;
            }
            
            delete[] buffer;
        }
    }

    void NodeAPApplication::fetchClusterMembers(Ipv6Address leaderAddr){
        // Buscar o nó líder na rede e obter sua lista de seguidores diretamente
        for(uint32_t i = 0; i < this->networkNodes.GetN(); i++){
            Ptr<Node> node = this->networkNodes.Get(i);
            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            Ipv6Address addr = ipv6->GetAddress(1, 0).GetAddress();
            
            if(addr == leaderAddr){
                Ptr<Application> app = node->GetApplication(0);
                if(app){
                    Ptr<NodeApplication> nodeApp = DynamicCast<NodeApplication>(app);
                    if(nodeApp){
                        // Obter myFollowers do líder (nós que o elegeram)
                        std::vector<Ipv6Address>* followers = nodeApp->getMyFollowers();
                        
                        if(followers != nullptr && !followers->empty()){
                            // Copiar membros para nossa estrutura
                            (*this->clusterMembers)[leaderAddr] = *followers;
                            
                            // Obter capacidades do líder
                            capabilitiesVector* leaderCaps = nodeApp->getCapabilities();
                            if(leaderCaps != nullptr){
                                (*this->clusterCapabilities)[leaderAddr] = *leaderCaps;
                            }
                            
                            NS_LOG_INFO("AP: CM fetched from " << leaderAddr << " with " << followers->size() << " members");
                        } else {
                            NS_LOG_INFO("AP: CM fetched from " << leaderAddr << " with 0 members (empty followers)");
                        }
                    }
                }
                break;
            }
        }
    }

    void NodeAPApplication::fetchAllClusterMembers(){
        // Buscar membros de TODOS os líderes registrados
        NS_LOG_INFO("AP: Fetching cluster members from all " << this->clusterLeaders->size() << " leaders");
        
        this->clusterMembers->clear();
        this->clusterCapabilities->clear();
        
        for(auto& leaderAddr : *this->clusterLeaders){
            this->fetchClusterMembers(leaderAddr);
        }
        
        NS_LOG_INFO("AP: Total clusters with members: " << this->clusterMembers->size());
    }

    void NodeAPApplication::taskConfirmation(){
        // If no confimation: Re-enqeue the task
        if(this->confirmationsSinceLastDispatch < this->requiredQuorum){
            // Não atingiu o quorum mínimo de agrupamentos - re-enfileirar
            NS_LOG_INFO("AP: Task " << this->currentDispatchedTask->getTid() 
                        << " failed quorum check: " << this->confirmationsSinceLastDispatch 
                        << "/" << this->requiredQuorum << " clusters accepted");
            this->tasks->push_back(this->currentDispatchedTask);
            Simulator::Schedule(Seconds(1), &NodeAPApplication::sendTaskToLeaders, this);
        }
        else{
            // Quorum atingido - tarefa aceita por agrupamentos suficientes
            NS_LOG_INFO("AP: Task " << this->currentDispatchedTask->getTid() 
                        << " quorum satisfied: " << this->confirmationsSinceLastDispatch 
                        << "/" << this->requiredQuorum << " clusters accepted");
            // Wait for task to be completed
            auto current = this->currentDispatchedTask;
            this->dispatchedTasks->push_back(current);

            // Dispatch another task
            Simulator::Schedule(Seconds(this->currentDispatchedTask->getDuration()), &NodeAPApplication::sendTaskToLeaders, this);
        }
    }

    void NodeAPApplication::setTasks(taskVector* tasks){
        this->tasks = tasks;
    }

    void NodeAPApplication::setFailurePercentage(double percentage){
        this->failurePercentage = percentage;
    }

    void NodeAPApplication::setFailurePercentageRange(double min, double max){
        this->failurePercentageMin = min;
        this->failurePercentageMax = max;
    }

    void NodeAPApplication::setFailureTime(double time){
        this->failureTime = time;
    }

    void NodeAPApplication::setFailureTimeRange(double min, double max){
        this->failureTimeMin = min;
        this->failureTimeMax = max;
    }

    void NodeAPApplication::setNodes(NodeContainer nodes){
        this->networkNodes = nodes;
    }

    capabilitiesVector* NodeAPApplication::getNodeCapabilities(Ipv6Address nodeAddr){
        // Buscar o nó na rede e obter suas capacidades
        for(uint32_t i = 0; i < this->networkNodes.GetN(); i++){
            Ptr<Node> node = this->networkNodes.Get(i);
            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            Ipv6Address addr = ipv6->GetAddress(1, 0).GetAddress();
            
            if(addr == nodeAddr){
                Ptr<Application> app = node->GetApplication(0);
                if(app){
                    Ptr<NodeApplication> nodeApp = DynamicCast<NodeApplication>(app);
                    if(nodeApp){
                        return nodeApp->getCapabilities();
                    }
                }
            }
        }
        return nullptr;
    }

    double NodeAPApplication::calculateSimilarityBetweenNodes(Ipv6Address node1, Ipv6Address node2){
        capabilitiesVector* caps1 = this->getNodeCapabilities(node1);
        if(caps1 == nullptr) return 0.0;
        
        capabilitiesVector* caps2 = this->getNodeCapabilities(node2);
        if(caps2 == nullptr) return 0.0;
        
        return capabilitiesSimilarity(caps1, caps2);
    }

    void NodeAPApplication::formNewClustersFromOrphans(){
        NS_LOG_INFO("RL2_FORM_CLUSTERS: Starting new cluster formation from " << this->orphanGroups->size() << " orphan groups");
        
        // Para cada grupo de órfãos (por líder falho)
        for(auto& group : *this->orphanGroups){
            Ipv6Address failedLeader = group.first;
            std::vector<Ipv6Address>& orphans = group.second;
            
            NS_LOG_INFO("RL2_GROUP: Processing " << orphans.size() << " orphans from failed leader " << failedLeader);
            
            if(orphans.size() < (size_t)MIN_NODES_FOR_NEW_CLUSTER){
                NS_LOG_INFO("RL2_SKIP: Group too small (" << orphans.size() << " < " << MIN_NODES_FOR_NEW_CLUSTER << ")");
                this->failedReallocations += orphans.size();
                continue;
            }
            
            // Usar Q-Learning para decidir quais nós incluir no novo cluster
            std::vector<Ipv6Address> newClusterMembers;
            
            for(auto& orphan : orphans){
                // Calcular similaridade média com outros órfãos do grupo
                double avgSimilarity = 0.0;
                int count = 0;
                
                for(auto& other : orphans){
                    if(other != orphan){
                        avgSimilarity += this->calculateSimilarityBetweenNodes(orphan, other);
                        count++;
                    }
                }
                
                if(count > 0){
                    avgSimilarity /= count;
                }
                
                // Só considerar se passar no threshold
                if(avgSimilarity >= REALLOCATION_THRESHOLD){
                    // Converter similaridade em estado
                    State state = this->rlAgent->similarityToState(avgSimilarity);
                    
                    // Agente escolhe ação
                    Action action = this->rlAgent->chooseAction(state);
                    
                    NS_LOG_INFO("RL2_DECISION: Orphan " << orphan 
                                << " | AvgSimilarity: " << avgSimilarity 
                                << " | State: " << state 
                                << " | Action: " << action);
                    
                    double reward = 0.0;
                    
                    if(action == ALLOCATE){
                        newClusterMembers.push_back(orphan);
                        reward = REWARD_SUCCESS;
                        this->successfulReallocations++;
                        NS_LOG_INFO("RL2_REWARD: SUCCESS (+10) - Node " << orphan << " included in new cluster");
                    } else {
                        reward = REWARD_ORPHAN;
                        this->failedReallocations++;
                        NS_LOG_INFO("RL2_REWARD: ORPHAN (-5) - Agent chose not to include node");
                    }
                    
                    // Atualizar Q-Table
                    this->rlAgent->updateQTable(state, action, reward, state);
                } else {
                    NS_LOG_INFO("RL2_LOW_SIM: Orphan " << orphan << " has low avg similarity: " << avgSimilarity);
                    this->failedReallocations++;
                }
            }
            
            // Eleger líder para o novo cluster
            if(newClusterMembers.size() >= (size_t)MIN_NODES_FOR_NEW_CLUSTER){
                Ipv6Address newLeader = this->electLeaderForOrphanCluster(newClusterMembers);
                
                // Registrar novo cluster
                this->registerNewCluster(newLeader, newClusterMembers);
                
                this->newClustersFormed++;
                this->nodesInNewClusters += newClusterMembers.size();
                
                NS_LOG_INFO("RL2_CLUSTER_FORMED: New cluster with leader " << newLeader 
                            << " and " << newClusterMembers.size() << " members");
            } else {
                NS_LOG_INFO("RL2_NO_CLUSTER: Not enough members to form cluster (" 
                            << newClusterMembers.size() << " < " << MIN_NODES_FOR_NEW_CLUSTER << ")");
            }
        }
        
        NS_LOG_INFO("RL2_SUMMARY: " << this->newClustersFormed << " new clusters formed, "
                    << this->nodesInNewClusters << " nodes in new clusters, "
                    << this->failedReallocations << " still orphaned");
    }

    void NodeAPApplication::triggerLeaderFailures(){
        // PRIMEIRO: Buscar membros de todos os clusters AGORA (quando followers já se registraram)
        this->fetchAllClusterMembers();
        
        if(this->aptLeaders->empty()){
            NS_LOG_INFO("FAILURE: Nenhum líder apto para aplicar falha no tempo " << Simulator::Now().GetSeconds());
            return;
        }
    
        // NOVO: Filtrar líderes aptos que tenham mais de 1 membro no cluster
        std::vector<Ipv6Address> eligibleLeaders;
        for(auto& leader : *this->aptLeaders){
            auto it = this->clusterMembers->find(leader);
            if(it != this->clusterMembers->end()){
                // Contar membros reais (excluindo o próprio líder)
                int realMembers = 0;
                for(auto& member : it->second){
                    if(member != leader){
                        realMembers++;
                    }
                }
                
                if(realMembers > 0){
                    eligibleLeaders.push_back(leader);
                    NS_LOG_INFO("FAILURE_ELIGIBLE: Leader " << leader << " with " << realMembers << " real followers");
                } else {
                    NS_LOG_INFO("FAILURE_SKIPPED: Leader " << leader << " has no real followers (cluster size = 1)");
                }
            }
        }
        
        if(eligibleLeaders.empty()){
            NS_LOG_INFO("FAILURE: Nenhum líder elegível (todos os clusters têm apenas 1 nó)");
            return;
        }
    
        // Usar a porcentagem real (já calculada no StartApplication)
        double percentageToUse = this->actualFailurePercentage;
    
        // Calcular quantos líderes irão falhar baseado nos ELEGÍVEIS
        int totalEligibleLeaders = eligibleLeaders.size();
        int leadersToFail = (int)std::ceil(totalEligibleLeaders * percentageToUse / 100.0);
        
        // Se a porcentagem > 0 mas o cálculo deu 0, forçar pelo menos 1
        if(leadersToFail == 0 && percentageToUse > 0){
            leadersToFail = 1;
        }
    
        NS_LOG_INFO("FAILURE: " << leadersToFail << " de " << totalEligibleLeaders << " líderes elegíveis irão falhar (" << percentageToUse << "%)");
    
        // Embaralhar lista de líderes ELEGÍVEIS para seleção aleatória
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(eligibleLeaders.begin(), eligibleLeaders.end(), g);
    
        // Limpar lista de órfãos antes de processar novas falhas
        this->orphanedNodes->clear();
        this->orphanGroups->clear();  // RL2

        // Aplicar falha nos líderes selecionados
        for(int i = 0; i < leadersToFail; i++){
            Ipv6Address leaderToFail = eligibleLeaders[i];
        
            // Identificar os membros órfãos deste líder (EXCLUINDO o próprio líder)
            auto it = this->clusterMembers->find(leaderToFail);
            // RL2: Vetor para guardar órfãos deste líder específico
            std::vector<Ipv6Address> orphansFromThisLeader;
            if(it != this->clusterMembers->end()){
                for(auto& memberAddr : it->second){
                    if(memberAddr != leaderToFail){
                        this->orphanedNodes->push_back(memberAddr);
                        orphansFromThisLeader.push_back(memberAddr);  // <-- só adiciona esta linha
                        NS_LOG_INFO("ORPHAN: " << memberAddr << " (was member of " << leaderToFail << ")");
                    }
                }
                // Remover o cluster do mapa
                this->clusterMembers->erase(it);
                // RL2: Guardar grupo de órfãos indexado pelo líder que falhou
                (*this->orphanGroups)[leaderToFail] = orphansFromThisLeader;
            }
            
            // Remover capacidades do cluster
            this->clusterCapabilities->erase(leaderToFail);
        
            // Encontrar o nó correspondente e desligá-lo
            for(uint32_t j = 0; j < this->networkNodes.GetN(); j++){
                Ptr<Node> node = this->networkNodes.Get(j);
                Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
                Ipv6Address nodeAddr = ipv6->GetAddress(1, 0).GetAddress();
            
                if(nodeAddr == leaderToFail){
                    // Parar a aplicação do nó (falha completa)
                    Ptr<Application> app = node->GetApplication(0);
                    if(app){
                        Simulator::ScheduleNow(&Application::SetStopTime, app, Simulator::Now());
                        // Fechar socket para impedir comunicação
                        Ptr<NodeApplication> nodeApp = DynamicCast<NodeApplication>(app);
                        if(nodeApp){
                            NS_LOG_INFO("FAILURE: Líder " << leaderToFail << " falhou no tempo " << Simulator::Now().GetSeconds());
                            nodeApp->StopApplication();
                        }
                    }
                    break;
                }
            }
        
            // Remover líder das listas
            auto leaderIt = std::find(this->clusterLeaders->begin(), this->clusterLeaders->end(), leaderToFail);
            if(leaderIt != this->clusterLeaders->end()){
                this->clusterLeaders->erase(leaderIt);
            }
            auto aptIt = std::find(this->aptLeaders->begin(), this->aptLeaders->end(), leaderToFail);
            if(aptIt != this->aptLeaders->end()){
                this->aptLeaders->erase(aptIt);
            }
        }
    
        NS_LOG_INFO("ORPHAN_TOTAL: " << this->orphanedNodes->size() << " nós órfãos identificados");
        
        // Iniciar realocação com Q-Learning
        Simulator::Schedule(MilliSeconds(500), &NodeAPApplication::formNewClustersFromOrphans, this);    
    }

    void NodeAPApplication::setQLearningParams(double alpha, double gamma, double epsilon){
        this->rlAgent->setAlpha(alpha);
        this->rlAgent->setGamma(gamma);
        this->rlAgent->setEpsilon(epsilon);
    }

    void NodeAPApplication::setQTableFilePath(const std::string& filePath){
        this->rlAgent->setQTableFilePath(filePath);
    }

    int NodeAPApplication::getNodeNeighborCount(Ipv6Address nodeAddr){
        for(uint32_t i = 0; i < this->networkNodes.GetN(); i++){
            Ptr<Node> node = this->networkNodes.Get(i);
            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            Ipv6Address addr = ipv6->GetAddress(1, 0).GetAddress();
            
            if(addr == nodeAddr){
                Ptr<Application> app = node->GetApplication(0);
                if(app){
                    Ptr<NodeApplication> nodeApp = DynamicCast<NodeApplication>(app);
                    if(nodeApp){
                        return nodeApp->getNeighborCount();
                    }
                }
            }
        }
        return 0;
    }

    Ipv6Address NodeAPApplication::electLeaderForOrphanCluster(std::vector<Ipv6Address>& members){
        Ipv6Address bestLeader = members[0];
        int maxNeighbors = this->getNodeNeighborCount(members[0]);
        int maxCaps = 0;
        
        capabilitiesVector* caps = this->getNodeCapabilities(members[0]);
        if(caps) maxCaps = caps->size();
        
        for(size_t i = 1; i < members.size(); i++){
            int neighbors = this->getNodeNeighborCount(members[i]);
            int capsSize = 0;
            caps = this->getNodeCapabilities(members[i]);
            if(caps) capsSize = caps->size();
            
            // Critério: mais vizinhos, desempate por mais capacidades
            if(neighbors > maxNeighbors || (neighbors == maxNeighbors && capsSize > maxCaps)){
                bestLeader = members[i];
                maxNeighbors = neighbors;
                maxCaps = capsSize;
            }
        }
        
        return bestLeader;
    }

    // ========== FUNÇÃO CORRIGIDA PARA RL2 ==========
    void NodeAPApplication::registerNewCluster(Ipv6Address newLeader, std::vector<Ipv6Address>& members){
        // 1. Registrar nas listas do AP
        this->clusterLeaders->push_back(newLeader);
        this->aptLeaders->push_back(newLeader);
        
        // 2. Registrar membros do cluster no AP
        (*this->clusterMembers)[newLeader] = members;
        
        // 3. Registrar capacidades (usar as do líder)
        capabilitiesVector* leaderCaps = this->getNodeCapabilities(newLeader);
        if(leaderCaps){
            (*this->clusterCapabilities)[newLeader] = *leaderCaps;
        }
        
        // ========== CORREÇÃO PRINCIPAL ==========
        // 4. Configurar o NÓ como líder (aumentar TX power, marcar isLeader, etc.)
        for(uint32_t i = 0; i < this->networkNodes.GetN(); i++){
            Ptr<Node> node = this->networkNodes.Get(i);
            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            Ipv6Address addr = ipv6->GetAddress(1, 0).GetAddress();
            
            if(addr == newLeader){
                Ptr<Application> app = node->GetApplication(0);
                if(app){
                    Ptr<NodeApplication> nodeApp = DynamicCast<NodeApplication>(app);
                    if(nodeApp){
                        // Chamar becomeLeader() para configurar o nó corretamente
                        nodeApp->becomeLeader();
                        
                        // Adicionar todos os membros como seguidores do novo líder
                        for(auto& memberAddr : members){
                            if(memberAddr != newLeader){
                                nodeApp->addFollower(memberAddr);
                            }
                        }
                        
                        NS_LOG_INFO("RL2_REGISTER: Node " << newLeader << " configured as leader with " 
                                    << (members.size() - 1) << " followers");
                    }
                }
                break;
            }
        }
        
        NS_LOG_INFO("RL2_REGISTER: New cluster registered with leader " << newLeader 
                    << " and " << members.size() << " total members");
    }
}