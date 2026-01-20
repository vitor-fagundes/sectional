#pragma once

#include "ns3/core-module.h"
#include "ns3/application.h"
#include "ns3/socket.h"

#include "capabilities.h"
#include "task.h"

using namespace ns3;

namespace nr2{
    class NodeAPApplication : public Application{
        private:
            taskVector*             tasks;              // Vector of tasks waiting to be dispatched
            taskVector*             dispatchedTasks;    // Vector of dispatched tasks
            Task*                   currentDispatchedTask;
            std::vector<Ipv6Address>*   clusterLeaders;     // Addresses of clusterLeaders
            Ptr<Socket>     		m_socket;       	// Associated socket
            Address					m_node;				// Node's
            TypeId          		m_tid;          	// Type of the socket used
            Ipv6Address             GetNodeIpAddress();
            int                     confirmationsSinceLastDispatch;

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
    };
}