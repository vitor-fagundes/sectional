#pragma once

#include "ns3/core-module.h"
#include "ns3/application.h"
#include "ns3/socket.h"

#include "capabilities.h"
#include "task.h"
#include "constants.h"

using namespace ns3;

namespace nr2{
    class NodeApplication : public Application{
        private:
            std::map<Ipv6Address, int>*                             neighList;
            std::map<Ipv6Address, int>*                             clusterList;

            std::map< Ipv6Address, capabilitiesVector>*             neighCapabilities;
            std::vector< std::pair<double, Ipv6Address>* >*          neighSimilatiries;
            Ptr<Socket>                                             m_socket;       	// Associated socket
            Address                                                 m_node;				// Node's
            TypeId                                                  m_tid;          	// Type of the socket used
            Address                                                 leaderNode;
            Ipv6Address                                             apAddress;

            capabilitiesVector*                                     capabilities;
            bool                                                    isLeader;
            capabilitiesVector*                                     clusterCapabilities;
            double                                                  delay;

            std::vector<Ipv6Address>                                allNodesAddrs;

        public:
            void setup(capabilitiesVector cap);
            void recvCallback(Ptr<Socket> socket);
            void StartApplication();
            void StopApplication();
            static TypeId GetTypeId();
            Ipv6Address GetNodeIpAddress();

            void beacon();
            void disseminateCapabilities();
            void similarityCalculation();
            void doClustering();
            float getSimilarityMode();
            void selectAndRegisterLeader();
            void registerLeader();

            Ipv6Address tiebreakLeader();
            void setAPAddress(Ipv6Address ip);
            void dispatchTaskToCluster(string cap);
            int sendMessageHelper(MessageTypes type, Ipv6Address addr, uint8_t* buffer, int size);
            void sendBroadcastMessageHelper(MessageTypes type, uint8_t* buffer, int size);
            void performTask(string);

            void nullFunction();
            void setAllNodesAddrs(std::vector<Ipv6Address>);
            void setDelay(double);
    };
}