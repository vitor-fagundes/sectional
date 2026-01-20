#include "NodeApplication.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv6.h"
#include "ns3/lr-wpan-net-device.h"
#include "ns3/lr-wpan-spectrum-value-helper.h"
#include "ns3/spectrum-value.h"

#include "MyTag.h"
#include "constants.h"

#include <memory>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Contaski_V1_Nodes");

namespace nr2{

    // ADICIONADO: Limiar de similaridade conforme Figura 3 do artigo
    // S1 = Fraco:  0.65 - 0.82
    // S2 = Médio:  0.82 - 1.00
    // S3 = Forte:  1.00
    // NOTA: Usando 0.87 para formar clusters mais homogêneos
    // Com 0.65, nós com apenas capacidades básicas em comum entram no mesmo cluster
    const double SIMILARITY_THRESHOLD = 0.95;

    Ipv6Address NodeApplication::GetNodeIpAddress(){
        Ptr <Node> PtrNode = this->GetNode();
        Ptr<Ipv6> ipv6 = PtrNode->GetObject<Ipv6> ();
        Ipv6InterfaceAddress iaddr = ipv6->GetAddress(1, 0);
        Ipv6Address ipAddr = iaddr.GetAddress();

        return ipAddr;
    }

    int NodeApplication::sendMessageHelper(MessageTypes type, Ipv6Address addr, uint8_t* buffer, int size){
        uint16_t port = 2020;
        int status;
        Inet6SocketAddress remote = Inet6SocketAddress(addr, port);
        /*status = this->m_socket->Connect(remote);
        if(status == -1){
            NS_LOG_INFO("Could not bind socket");

            return status;
        }*/

        Ptr<Packet> pack = Create<Packet>(buffer, size);
        MyTag tag;
        tag.SetSimpleValue(type);
        pack->AddPacketTag(tag);
        
        status = this->m_socket->SendTo(pack, 0, remote);

        if(status == -1){
            NS_LOG_INFO(this->GetNodeIpAddress() << "Could not send package");
        }

        NS_LOG_INFO("N: MS (" << this->GetNodeIpAddress() << ", " << addr << ", " << status << ")");

        return status;
    }

    void NodeApplication::sendBroadcastMessageHelper(MessageTypes type, uint8_t* buffer, int size){
        for (auto addr:this->allNodesAddrs){
            auto buf= new uint8_t[size];
            std::memcpy(buf, buffer, size);
            Simulator::Schedule(MilliSeconds(2.0),&NodeApplication::sendMessageHelper, this, type, addr, buf, size);
            //this->sendMessageHelper(type, addr, buffer, size);
        }
    }

    void NodeApplication::setup(capabilitiesVector cap){
        this->m_node = GetNodeIpAddress();
        this->m_tid = ns3::UdpSocketFactory::GetTypeId();
        this->m_socket = this->GetNode()->GetObject<Socket>();

        this->neighList = new std::map<Ipv6Address, int>;
        this->clusterList = new std::map<Ipv6Address, int>;
        this->neighCapabilities = new std::map<Ipv6Address, capabilitiesVector>;
        this->neighSimilatiries = new std::vector< std::pair<double, Ipv6Address>* >;
        this->clusterCapabilities = new capabilitiesVector();

        this->capabilities = new capabilitiesVector(cap);

        this->isLeader = false;
    }

    TypeId NodeApplication::GetTypeId(){
        static TypeId tid = TypeId ("ns3::NodeApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<NodeApplication>()
            .AddAttribute ("Protocol", "The type of protocol to use. This should be "
                   "a subclass of ns3::SocketFactory",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&NodeApplication::m_tid),
                   // This should check for SocketFactory as a parent
                   MakeTypeIdChecker ())
            ;

        return tid;
    }

    void NodeApplication::StartApplication(){
        // If socket is not created yet
        if(!this->m_socket){
            // Create socket
            auto netdev = this->GetNode()->GetDevice(2);
            
            this->m_socket = Socket::CreateSocket(GetNode(), m_tid);
            this->m_socket->BindToNetDevice(netdev);
            
            this->m_socket->SetAllowBroadcast(true);
            this->m_socket->Bind(Inet6SocketAddress(Ipv6Address::GetAny (), 2020));
            this->m_socket->Listen();
            this->m_socket->SetRecvCallback(MakeCallback (&NodeApplication::recvCallback, this));
        }

        // Run beaconing
        //this->beacon();
        Simulator::ScheduleNow(&NodeApplication::beacon, this);

        // Disseminate capabilities
        //this->disseminateCapabilities();
        Simulator::Schedule( (Seconds(60.0) - Simulator::Now()) , &NodeApplication::disseminateCapabilities, this);

        // Calculate similarities
        //this->similarityCalculation();
        Simulator::Schedule( (Seconds(90.5) - Simulator::Now()),&NodeApplication::similarityCalculation, this);
    }

    void NodeApplication::StopApplication(){
        this->m_socket->Close();
    }

    void NodeApplication::recvCallback(Ptr<Socket> socket){
        Ptr<Packet> packet;
        Address from;
        Ipv6Address fromIP;
        MyTag tag;
        
        while((packet = socket->RecvFrom(from))){
            fromIP = Inet6SocketAddress::ConvertFrom(from).GetIpv6();
            packet->PeekPacketTag(tag);
            uint8_t *buffer = new uint8_t[packet->GetSize()];
	  	    packet->CopyData (buffer, packet->GetSize());

            switch (tag.GetSimpleValue()){
                case MessageTypes::Beacon:{
                    string s = string(buffer, buffer+packet->GetSize());
                    int neighReceived = atoi(s.c_str());

                    if(neighReceived > (*this->neighList)[fromIP])
                        (*this->neighList)[fromIP] = neighReceived;

                    //NS_LOG_INFO("N: BR " << this->GetNodeIpAddress() << " <- " << fromIP << " (" << neighReceived << ")");
                    break;
                }
                
                case MessageTypes::CapabilityDissemination:
                {
                    capabilitiesVector *cap = parseCapabilities( std::string((char*)buffer) );
                    (*this->neighCapabilities)[fromIP] = *cap;

                    NS_LOG_INFO("N: C " << this->GetNodeIpAddress() << " <- " << fromIP << " (" << serializeCapabilities(cap) << ")");
                    break;
                }
                case MessageTypes::LeaderToCluster:{
                    // Perform task
                    string sTask = string(buffer, buffer+packet->GetSize());
                    this->performTask(sTask);
                    break;
                }
                case MessageTypes::TaskDispatch:{
                    // If i'm the leader
                    if (this->isLeader) {
                        //NS_LOG_INFO("Task received by leader");
                        string sTask = string(buffer, buffer+packet->GetSize());
                        this->dispatchTaskToCluster(sTask);
                    }
                    
                    break;
                }
                
                default:
                    break;
            }
        }
    }

    void NodeApplication::beacon(){
        int neighSize = this->neighList->size();
        string buffer = std::to_string(neighSize);
        Ipv6Address addr = Ipv6Address("FF02::1");

        //this->sendBroadcastMessageHelper(MessageTypes::Beacon, (uint8_t*)buffer.c_str(), buffer.length()+1);
        this->sendMessageHelper(MessageTypes::Beacon, addr, (uint8_t*)buffer.c_str(), buffer.size()+1);
        //NS_LOG_INFO("N: BS " << this->GetNodeIpAddress() << " (" << buffer << ")");

        if ((Seconds(60.0) - Simulator::Now()) > Seconds(0.0)) {
            Simulator::Schedule(Seconds(0.5), &NodeApplication::beacon, this);
        }
    }

    void NodeApplication::disseminateCapabilities(){
        auto serializedCap = serializeCapabilities(this->capabilities);
        Ipv6Address addr = Ipv6Address("FF02::1");

        //this->sendBroadcastMessageHelper(MessageTypes::CapabilityDissemination, (uint8_t*)serializedCap.c_str(), serializedCap.size()+1);
        this->sendMessageHelper(MessageTypes::CapabilityDissemination, addr, (uint8_t*)serializedCap.c_str(), serializedCap.size()+1);

        if ((Seconds(90.0) - Simulator::Now()) > Seconds(0.0)) {
            Simulator::Schedule(Seconds(0.5), &NodeApplication::disseminateCapabilities, this);
        }
    }

    void NodeApplication::similarityCalculation(){
        //NS_LOG_INFO("N: S " << this->GetNodeIpAddress() << " at " << Simulator::Now().GetSeconds());

        for (auto const& neigh: *this->neighCapabilities){
            auto neighCap = neigh.second;
            capabilitiesVector* inter = new capabilitiesVector();
            
            // CORRIGIDO: Usar capabilitiesSimilarity (Equação 1 do artigo)
            // ao invés de capabilitiesSimilarityUFD
            double sim = capabilitiesSimilarity(this->capabilities, &neighCap, inter);

            //auto pair = make_pair(sim, neigh.first);
            auto pair = new std::pair<double, Ipv6Address>(sim, neigh.first);
            this->neighSimilatiries->emplace_back(pair);

            delete inter;
        }

        Simulator::ScheduleNow(&NodeApplication::doClustering, this);
    }

    void NodeApplication::doClustering(){
        //float mode = this->getSimilarityMode();
        std::sort(this->neighSimilatiries->begin(), this->neighSimilatiries->end());
        //double maxSim = (* --this->neighSimilatiries->end())->first;
        //double maxSim = 1.0;  // REMOVIDO: Não mais usado

        //Filter in nodes between mode and 1
        for (auto it = this->neighSimilatiries->begin(); it != this->neighSimilatiries->end(); it++){
            // CORRIGIDO: Usar SIMILARITY_THRESHOLD (0.65) ao invés de comparar com maxSim = 1.0
            // Antes: double sim = std::abs((*it)->first - maxSim); if(sim < 0.000001)
            if((*it)->first >= SIMILARITY_THRESHOLD){
                try{
                    //Join them into the cluster
                    (*this->clusterList)[(*it)->second] = this->neighList->at((*it)->second);
                }catch(...){
                    NS_LOG_INFO("N: " << this->GetNodeIpAddress() << " could not join " << (*it)->second << " to cluster");
                }
            }
        }

        //Select leader
        this->selectAndRegisterLeader();
    }

    float NodeApplication::getSimilarityMode(){
        std::sort(this->neighSimilatiries->begin(), this->neighSimilatiries->end());

        float current = (* this->neighSimilatiries->begin())->first;
        float mode = current;

        int count = 1;
        int countMode = 1;

        for (auto it = this->neighSimilatiries->begin()++; it != this->neighSimilatiries->end(); it++){
            if( (*it)->first == current){
                if(count > countMode){
                    countMode = count;
                    mode = current;
                }
            }else{
                if(count > countMode){
                    countMode = count;
                    mode = current;
                }

                count = 1;
                current = (*it)->first;
            }
        }
        
        return mode;
    }

    void NodeApplication::selectAndRegisterLeader(){
        Ipv6Address leader = tiebreakLeader();

        if( leader == this->GetNodeIpAddress()){
            NS_LOG_INFO("N: LS " << this->GetNodeIpAddress());

            //If i'm leader, increase tx power so AP can be reached
            Ptr<LrWpanNetDevice> nodenetdev = DynamicCast<LrWpanNetDevice>(this->GetNode()->GetDevice(1));
		    auto phy = nodenetdev->GetPhy();
		    LrWpanSpectrumValueHelper svh;
		    Ptr<SpectrumValue> psd = svh.CreateTxPowerSpectralDensity (10, 11);
		    phy->SetTxPowerSpectralDensity(psd);

            //this->registerLeader();
            Simulator::Schedule(Seconds(90.5+this->delay) - Now(), &NodeApplication::registerLeader, this);
            this->isLeader = true;


            /*float mode = this->getSimilarityMode();
            auto it = std::find_if( this->neighSimilatiries->begin(), this->neighSimilatiries->end(),
                [&mode](const std::pair<float, Ipv6Address>* element){
                    return element->first == mode;
                }
            );

            capabilitiesSimilarity(this->capabilities, &((*this->neighCapabilities)[(*it)->second]), this->clusterCapabilities);*/
            this->clusterCapabilities = this->capabilities;
        }
   }

    Ipv6Address NodeApplication::tiebreakLeader(){
        Ipv6Address ipv6 = this->GetNodeIpAddress();
        Ipv6Address selectedLeader;
        
        // CORRIGIDO: Usar tamanho do cluster (clusterList->size()) ao invés de neighList->size()
        // Isso garante consistência: todos os nós no cluster usam a mesma métrica
        (*this->clusterList)[ipv6] = this->neighList->size();

        /*auto leader = std::max(this->clusterList->begin(), this->clusterList->end(),
        [](std::map<Ipv6Address, int>::iterator it1, std::map<Ipv6Address, int>::iterator it2 ){
            return it1->second > it2->second;
        });*/
        auto leader = std::max_element(this->clusterList->begin(), this->clusterList->end(),
            [](std::pair<Ipv6Address, int> it1, std::pair<Ipv6Address, int> it2 ){
                return it1.second < it2.second;
            }
        );

        int maxNeigh = leader->second;

        std::map<Ipv6Address, int> *leaderCandidates = new map<Ipv6Address, int>;

        std::copy_if(this->clusterList->begin(), this->clusterList->end(),
            std::inserter(*leaderCandidates, leaderCandidates->end() ),
            [maxNeigh](std::pair<Ipv6Address, int> a){
                return a.second == maxNeigh;
            }
        );


        if( leaderCandidates->size() == 0){
            selectedLeader = leader->first;
        }else if(leaderCandidates->size() == 1){
            selectedLeader = (*leaderCandidates->begin()).first;
        }else{
            //Tiebreak
            auto tiebraker = new std::map<Ipv6Address, int>;
            
            for(auto candidate: *leaderCandidates) {
                if(candidate.first == ipv6){
                    // Próprio nó: usar capacidades locais
                    (*tiebraker)[candidate.first] = this->capabilities->size();
                } else {
                    // Vizinho: buscar em neighCapabilities
                    auto it = this->neighCapabilities->find(candidate.first);
                    if(it != this->neighCapabilities->end()){
                        (*tiebraker)[candidate.first] = it->second.size();
                    } else {
                        (*tiebraker)[candidate.first] = 0;
                    }
                }
            }
            
            /*Ipv6Address maiorIp;
            int maiorCap = 0;
            for(auto lCand : tiebraker){
                if(lCand.second>maiorCap){
                    maiorCap = lCand.second;
                    maiorIp = lCand.first;
                }
            }
            return maiorIp;*/
            /*auto leader2 = std::max( tiebraker->begin(), tiebraker->end(),
                [](std::map<Ipv6Address, int>::iterator it1, std::map<Ipv6Address, int>::iterator it2 ){
                    return it1->second < it2->second;
                }
            );*/
            // CORRIGIDO: Usar tiebraker ao invés de clusterList para desempate por capacidades
            auto leader2 = std::max_element(tiebraker->begin(), tiebraker->end(),
                [](std::pair<Ipv6Address, int> a, std::pair<Ipv6Address, int> b){
                    return a.second < b.second;
                }
            );
            
            int maxCap = leader2->second;
            
            // Filtrar todos com máximo de capacidades
            std::vector<Ipv6Address> finalCandidates;
            for(auto& c : *tiebraker){
                if(c.second == maxCap){
                    finalCandidates.push_back(c.first);
                }
            }
            
            // Desempate final por menor IP
            if(finalCandidates.size() == 1){
                selectedLeader = finalCandidates[0];
            } else {
                std::sort(finalCandidates.begin(), finalCandidates.end(),
                    [](const Ipv6Address& a, const Ipv6Address& b){
                        uint8_t bufA[16], bufB[16];
                        a.GetBytes(bufA);
                        b.GetBytes(bufB);
                        return memcmp(bufA, bufB, 16) < 0;
                    }
                );
                selectedLeader = finalCandidates[0];
            }
            
            delete tiebraker;
        }

        NS_LOG_INFO("N: " << ipv6 << " CLSize " << this->clusterList->size() << " elects " << selectedLeader);
        
        delete leaderCandidates;
        
        return selectedLeader;
   }

    void NodeApplication::registerLeader(){
        NS_LOG_INFO("N: LR " << this->GetNodeIpAddress() << " at " << Now().GetSeconds());
        Simulator::ScheduleNow(&NodeApplication::sendMessageHelper, this, MessageTypes::LeaderRegister, this->apAddress, (uint8_t*)0, 0 );
        //this->sendMessageHelper(MessageTypes::LeaderRegister, this->apAddress, 0, 0);
    }

   void NodeApplication::setAPAddress(Ipv6Address ip){
       this->apAddress = ip;
   }

    void NodeApplication::dispatchTaskToCluster(string sTask){
        auto task = new Task(sTask);
        auto taskCap = task->getCapabilities();
        
        std::sort(this->capabilities->begin(), this->capabilities->end());
        std::sort(taskCap->begin(), taskCap->end());

        //Check if cluster can perform the task
        if( 
            std::includes(this->capabilities->begin(), this->capabilities->end(),
                taskCap->begin(), taskCap->end()
            )
        ){
            this->sendMessageHelper(MessageTypes::TaskAccept, this->apAddress, 0, 0);

            NS_LOG_INFO("N: LA " << this->GetNodeIpAddress() << ", " << task->getTid() << ", " << Simulator::Now().GetSeconds());

            for (auto node : *this->clusterList){
                this->sendMessageHelper(MessageTypes::LeaderToCluster, node.first,
                    (uint8_t*) sTask.c_str(), sTask.size()+1);
            }
        }
    }

    void NodeApplication::performTask(string sTask){
        auto task = new Task(sTask);
        Simulator::Schedule(Seconds(task->getDuration()), &NodeApplication::nullFunction, this);
    }

    void NodeApplication::nullFunction(){
    }

    void NodeApplication::setAllNodesAddrs(std::vector<Ipv6Address> addrs){
        this->allNodesAddrs = addrs;
    }

    void NodeApplication::setDelay(double delay){
        this->delay = delay;
    }
}