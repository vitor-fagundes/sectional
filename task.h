#pragma once

#include <list>
#include <random>
#include <stdint.h>
#include <sstream>
#include "capabilities.h"

using namespace std;

namespace nr2 {
    class Task {
        private:
            uint32_t taskId;
            uint32_t duration;
            uint32_t quorum;
            capabilitiesVector* requiredCapabilities;
            
        public:
            Task();
            Task(uint32_t tid, uint32_t duration, uint32_t quorum, capabilitiesVector cap);
            Task(std::string taskString);
            ~Task();
            
            std::string serialize();
            uint32_t getDuration(){
                return this->duration;
            }

            uint32_t getTid(){
                return this->taskId;
            }

            uint32_t getQuorum(){
                return this->quorum;
            }

            capabilitiesVector* getCapabilities(){
                return new capabilitiesVector(*(this->requiredCapabilities));
            }
            
            void print();
            
    };

    typedef std::list<Task*> taskVector;
}
