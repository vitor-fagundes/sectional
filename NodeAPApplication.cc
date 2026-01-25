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
        
        // Inicializar agente Q-Learning
        this->rlAgent = new QLearningAgent();
        
        // Contadores de métricas
        this->successfulReallocations = 0;
        this->failedReallocations = 0;

        this->confirmationsSinceLastDispatch = 0;
        this->failurePercentage = 0.0;

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

        // Agendar falhas no tempo 310s
        if(this->failurePercentage > 0.0){
            Simulator::Schedule(Seconds(310), &NodeAPApplication::triggerLeaderFailures, this);
        }
    }

    void NodeAPApplication::StopApplication(){
        this->m_socket->Close();

        // Salvar Q-Table para próxima rodada
        this->rlAgent->saveQTable("qtable.csv");
        this->rlAgent->printQTable();
        
        // Log de métricas de realocação
        NS_LOG_INFO("RL_METRICS: Successful reallocations: " << this->successfulReallocations);
        NS_LOG_INFO("RL_METRICS: Failed reallocations: " << this->failedReallocations);

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

    double NodeAPApplication::calculateSimilarity(Ipv6Address orphanAddr, Ipv6Address leaderAddr){
        // Obter capacidades do nó órfão
        capabilitiesVector* orphanCaps = this->getNodeCapabilities(orphanAddr);
        if(orphanCaps == nullptr){
            return 0.0;
        }
        
        // Obter capacidades do cluster (líder)
        auto it = this->clusterCapabilities->find(leaderAddr);
        if(it == this->clusterCapabilities->end()){
            return 0.0;
        }
        capabilitiesVector leaderCaps = it->second;
        
        // Calcular similaridade usando a função existente
        double sim = capabilitiesSimilarity(orphanCaps, &leaderCaps);
        
        return sim;
    }

    bool NodeAPApplication::addNodeToCluster(Ipv6Address orphanAddr, Ipv6Address leaderAddr){
        // Adicionar órfão à lista de membros do cluster
        auto it = this->clusterMembers->find(leaderAddr);
        if(it != this->clusterMembers->end()){
            it->second.push_back(orphanAddr);
            NS_LOG_INFO("RL_REALLOC: Node " << orphanAddr << " added to cluster " << leaderAddr);
            return true;
        }
        return false;
    }

    void NodeAPApplication::reallocateOrphans(){
        NS_LOG_INFO("RL_REALLOC: Starting reallocation of " << this->orphanedNodes->size() << " orphans");
        
        // Para cada nó órfão
        std::vector<Ipv6Address> stillOrphaned;
        
        for(auto& orphanAddr : *this->orphanedNodes){
            bool reallocated = false;
            double bestSimilarity = 0.0;
            Ipv6Address bestCluster;
            
            // Encontrar o melhor cluster para este órfão
            for(auto& leader : *this->clusterLeaders){
                double similarity = this->calculateSimilarity(orphanAddr, leader);
                
                // Verificar se passa no threshold de realocação (0.85)
                if(similarity >= REALLOCATION_THRESHOLD){
                    if(similarity > bestSimilarity){
                        bestSimilarity = similarity;
                        bestCluster = leader;
                    }
                }
            }
            
            // Se encontrou cluster válido, usar agente para decidir
            if(bestSimilarity >= REALLOCATION_THRESHOLD){
                // Converter similaridade em estado
                State state = this->rlAgent->similarityToState(bestSimilarity);
                
                // Agente escolhe ação
                Action action = this->rlAgent->chooseAction(state);
                
                NS_LOG_INFO("RL_DECISION: Orphan " << orphanAddr 
                            << " | Similarity: " << bestSimilarity 
                            << " | State: " << state 
                            << " | Action: " << action);
                
                double reward = 0.0;
                
                if(action == ALLOCATE){
                    // Tentar alocar
                    if(this->addNodeToCluster(orphanAddr, bestCluster)){
                        reallocated = true;
                        reward = REWARD_SUCCESS;
                        this->successfulReallocations++;
                        NS_LOG_INFO("RL_REWARD: SUCCESS (+10) - Node " << orphanAddr << " reallocated to " << bestCluster);
                    } else {
                        reward = REWARD_INVALID;
                        NS_LOG_INFO("RL_REWARD: INVALID (-10) - Failed to add node to cluster");
                    }
                } else {
                    // Agente escolheu não alocar
                    reward = REWARD_ORPHAN;
                    NS_LOG_INFO("RL_REWARD: ORPHAN (-5) - Agent chose not to allocate");
                }
                
                // Atualizar Q-Table
                // Próximo estado é o mesmo (simplificação para este cenário)
                this->rlAgent->updateQTable(state, action, reward, state);
                
            } else {
                // Nenhum cluster válido encontrado (similaridade < 0.85)
                NS_LOG_INFO("RL_NO_MATCH: Orphan " << orphanAddr << " has no valid cluster (best similarity: " << bestSimilarity << ")");
            }
            
            if(!reallocated){
                stillOrphaned.push_back(orphanAddr);
                this->failedReallocations++;
            }
        }
        
        // Atualizar lista de órfãos
        this->orphanedNodes->clear();
        for(auto& addr : stillOrphaned){
            this->orphanedNodes->push_back(addr);
        }
        
        NS_LOG_INFO("RL_REALLOC_SUMMARY: " << this->successfulReallocations << " reallocated, " 
                    << stillOrphaned.size() << " still orphaned");
    }

    void NodeAPApplication::triggerLeaderFailures(){
        // PRIMEIRO: Buscar membros de todos os clusters AGORA (quando followers já se registraram)
        this->fetchAllClusterMembers();
        
        if(this->aptLeaders->empty()){
            NS_LOG_INFO("FAILURE: Nenhum líder apto para aplicar falha no tempo " << Simulator::Now().GetSeconds());
            return;
        }

        // Calcular quantos líderes irão falhar (arredondamento para cima)
        int totalAptLeaders = this->aptLeaders->size();
        int leadersToFail = (int)std::ceil(totalAptLeaders * this->failurePercentage / 100.0);
        
        // Se a porcentagem > 0 mas o cálculo deu 0, forçar pelo menos 1
        if(leadersToFail == 0 && this->failurePercentage > 0){
            leadersToFail = 1;
        }

        NS_LOG_INFO("FAILURE: " << leadersToFail << " de " << totalAptLeaders << " líderes aptos irão falhar (" << this->failurePercentage << "%)");

        // Embaralhar lista de líderes aptos para seleção aleatória
        std::vector<Ipv6Address> shuffledLeaders(*this->aptLeaders);
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(shuffledLeaders.begin(), shuffledLeaders.end(), g);

        // Limpar lista de órfãos antes de processar novas falhas
        this->orphanedNodes->clear();

        // Aplicar falha nos líderes selecionados
        for(int i = 0; i < leadersToFail; i++){
            Ipv6Address leaderToFail = shuffledLeaders[i];

            // Identificar os membros órfãos deste líder
            auto it = this->clusterMembers->find(leaderToFail);
            if(it != this->clusterMembers->end()){
                for(auto& memberAddr : it->second){
                    this->orphanedNodes->push_back(memberAddr);
                    NS_LOG_INFO("ORPHAN: " << memberAddr << " (was member of " << leaderToFail << ")");
                }
                // Remover o cluster do mapa
                this->clusterMembers->erase(it);
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
        Simulator::Schedule(MilliSeconds(500), &NodeAPApplication::reallocateOrphans, this);
    }

    void NodeAPApplication::setQLearningParams(double alpha, double gamma, double epsilon){
        this->rlAgent->setAlpha(alpha);
        this->rlAgent->setGamma(gamma);
        this->rlAgent->setEpsilon(epsilon);
    }

    void NodeAPApplication::setQTableFilePath(const std::string& filePath){
        this->rlAgent->setQTableFilePath(filePath);
    }
}