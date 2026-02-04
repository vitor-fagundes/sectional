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

        this->confirmationsSinceLastDispatch = 0;
        this->failurePercentage = 0.0;
        this->failurePercentageMin = 0.0;
        this->failurePercentageMax = 0.0;
        this->failureTimeMin = 310.0;  // Default: 310s
        this->failureTimeMax = 310.0;  // Default: 310s (tempo fixo)
        this->actualFailureTime = 0.0;
        this->actualFailurePercentage = 0.0;

        for(auto task:*this->tasks){
            task->print();
        }
    }

    void NodeAPApplication::StartApplication(){
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

        // Calcular tempo e porcentagem de falha (pode ser fixo ou aleatório)
        std::random_device rd;
        std::mt19937 gen(rd());

        // Determinar tempo de falha
        if(this->failureTimeMin < this->failureTimeMax){
            // Tempo aleatório entre min e max
            std::uniform_real_distribution<> timeDist(this->failureTimeMin, this->failureTimeMax);
            this->actualFailureTime = timeDist(gen);
        } else {
            // Tempo fixo
            this->actualFailureTime = this->failureTimeMin;
        }

        // Determinar porcentagem de falha
        if(this->failurePercentageMin > 0 && this->failurePercentageMin < this->failurePercentageMax){
            // Porcentagem aleatória entre min e max
            std::uniform_real_distribution<> percDist(this->failurePercentageMin, this->failurePercentageMax);
            this->actualFailurePercentage = percDist(gen);
        } else if(this->failurePercentage > 0){
            // Porcentagem fixa
            this->actualFailurePercentage = this->failurePercentage;
        } else {
            this->actualFailurePercentage = 0.0;
        }

        // Agendar falhas se houver porcentagem configurada
        if(this->actualFailurePercentage > 0.0){
            NS_LOG_INFO("FAILURE_CONFIG: Tempo=" << this->actualFailureTime << "s, Porcentagem=" << this->actualFailurePercentage << "%");
            Simulator::Schedule(Seconds(this->actualFailureTime), &NodeAPApplication::triggerLeaderFailures, this);
        }
    }

    void NodeAPApplication::StopApplication(){
        this->m_socket->Close();

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
        /*
        Inet6SocketAddress remote = Inet6SocketAddress(Ipv6Address("FF02::1"), port);
        status = this->m_socket->Connect(remote);
        if(status == -1){
            NS_LOG_INFO("Could not bind socket");
        }*/


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

            switch (tag.GetSimpleValue()){
                case MessageTypes::LeaderRegister:
                    NS_LOG_INFO("LR: " << fromIP << " at " << Simulator::Now().GetSeconds());
                    this->clusterLeaders->push_back(fromIP);
                    break;
                
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
        }
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

    void NodeAPApplication::setFailureTimeRange(double min, double max){
        this->failureTimeMin = min;
        this->failureTimeMax = max;
    }

    void NodeAPApplication::setNodes(NodeContainer nodes){
        this->networkNodes = nodes;
    }

void NodeAPApplication::triggerLeaderFailures(){
        if(this->aptLeaders->empty()){
            NS_LOG_INFO("FAILURE: Nenhum líder apto para aplicar falha no tempo " << Simulator::Now().GetSeconds());
            return;
        }

        // NOVO: Filtrar líderes aptos que tenham mais de 1 membro no cluster
        // (clusters com apenas 1 nó não geram órfãos, então não faz sentido falhar)
        std::vector<Ipv6Address> eligibleLeaders;
        for(auto& leaderAddr : *this->aptLeaders){
            // Encontrar o nó líder e verificar tamanho do cluster
            for(uint32_t j = 0; j < this->networkNodes.GetN(); j++){
                Ptr<Node> node = this->networkNodes.Get(j);
                Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
                Ipv6Address nodeAddr = ipv6->GetAddress(1, 0).GetAddress();

                if(nodeAddr == leaderAddr){
                    Ptr<Application> app = node->GetApplication(0);
                    if(app){
                        Ptr<NodeApplication> nodeApp = DynamicCast<NodeApplication>(app);
                        if(nodeApp){
                            int clusterSize = nodeApp->getClusterSize();
                            // Só considerar se cluster tem mais de 1 membro
                            // (clusterSize inclui vizinhos similares, queremos pelo menos 2)
                            if(clusterSize > 1){
                                eligibleLeaders.push_back(leaderAddr);
                                NS_LOG_INFO("FAILURE_ELIGIBLE: Leader " << leaderAddr << " with cluster size " << clusterSize);
                            } else {
                                NS_LOG_INFO("FAILURE_SKIPPED: Leader " << leaderAddr << " has cluster size " << clusterSize << " (too small)");
                            }
                        }
                    }
                    break;
                }
            }
        }

        if(eligibleLeaders.empty()){
            NS_LOG_INFO("FAILURE: Nenhum líder elegível (todos os clusters têm apenas 1 nó) no tempo " << Simulator::Now().GetSeconds());
            return;
        }

        // Calcular quantos líderes irão falhar baseado nos ELEGÍVEIS
        int totalEligibleLeaders = eligibleLeaders.size();
        int leadersToFail = (int)std::ceil(totalEligibleLeaders * this->actualFailurePercentage / 100.0);
        
        // Se a porcentagem > 0 mas o cálculo deu 0, forçar pelo menos 1
        if(leadersToFail == 0 && this->actualFailurePercentage > 0){
            leadersToFail = 1;
        }

        NS_LOG_INFO("FAILURE: " << leadersToFail << " de " << totalEligibleLeaders << " líderes elegíveis irão falhar (" << this->actualFailurePercentage << "%)");
        NS_LOG_INFO("FAILURE: (Total aptos: " << this->aptLeaders->size() << ", Elegíveis após filtro: " << totalEligibleLeaders << ")");

        // Embaralhar lista de líderes ELEGÍVEIS para seleção aleatória
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(eligibleLeaders.begin(), eligibleLeaders.end(), g);

        // Aplicar falha nos líderes selecionados
        for(int i = 0; i < leadersToFail; i++){
            Ipv6Address leaderToFail = eligibleLeaders[i];

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
                            // Contar nós órfãos (membros do cluster do líder que falhou)
                            int orphanedNodes = nodeApp->getClusterSize();
                            NS_LOG_INFO("FAILURE: Líder " << leaderToFail << " falhou no tempo " << Simulator::Now().GetSeconds() << " - " << orphanedNodes << " nós órfãos");
                            nodeApp->StopApplication();
                        }
                    }
                    break;
                }
            }
        }
    }
}