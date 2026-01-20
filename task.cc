#include <iostream>
#include "task.h"

using namespace std;

namespace nr2{
    Task::Task(uint32_t tid, uint32_t duration, uint32_t quorum, capabilitiesVector cap = basicCapabilities){
        this->taskId = tid;
        this->duration = duration;
        this->quorum = quorum;
        this->requiredCapabilities = new capabilitiesVector(cap);
    }

    Task::Task(){
        std::random_device rd;
        std::default_random_engine generator{rd()};
        std::uniform_int_distribution<int> distribution(3, 6);
        //std::uniform_int_distribution<int> duration(1, 2);
        //std::uniform_int_distribution<int> quorum(3, 5);
        this->taskId = generator();
        //this->quorum = quorum(generator);
        this->quorum = 1;
        //this->duration = duration(generator)*60; // Generates a task with duration either 60 or 120
        this->duration = 60;

        this->requiredCapabilities = new capabilitiesVector(basicCapabilities);

        for(int i = 3; i < 7; i++){
            this->requiredCapabilities->push_back( static_cast<capabilities>( distribution(generator) ) );
        }

        std::sort(this->requiredCapabilities->begin(), this->requiredCapabilities->end());
        auto last = std::unique(this->requiredCapabilities->begin(), this->requiredCapabilities->end());
        this->requiredCapabilities->erase(last, this->requiredCapabilities->end());
    }

    Task::Task(std::string taskString){
        size_t last = 0;
        size_t next = 0;
        
        //Get task id
        next = taskString.find("+", last);
        this->taskId = strtoul((taskString.substr(last, next-last)).c_str(), nullptr, 10);
        last = next + 1;

        // Get duration
        next = taskString.find("+", last);
        this->duration = strtoul((taskString.substr(last, next-last)).c_str(), nullptr, 10);
        last = next + 1;

        // Get duration
        next = taskString.find("+", last);
        this->quorum = strtoul((taskString.substr(last, next-last)).c_str(), nullptr, 10);
        last = next + 1;

        // Get capabilities
        next = taskString.find("+", last);
        this->requiredCapabilities = parseCapabilities(taskString.substr(last, next-last));
        last = next + 1;
    }

    std::string Task::serialize(){
        std::stringstream buf;

        buf << this->taskId << "+"
            << this->duration << "+"
            << this->quorum << "+"
            << serializeCapabilities(this->requiredCapabilities);

        return buf.str();
    }

    void Task::print(){
        cout << this->serialize() << endl;
    }
}