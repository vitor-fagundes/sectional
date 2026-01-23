#include "capabilities.h"
#include <map>

using namespace std;

namespace nr2{
    double capabilitiesSimilarity(capabilitiesVector* cap1, capabilitiesVector* cap2){
        double sim = 0;
        capabilitiesVector intersection(cap1->size()+cap2->size());

        std::sort(cap1->begin(), cap1->end());
        std::sort(cap2->begin(), cap2->end());

        auto it = std::set_intersection(cap1->begin(),cap1->end(),cap2->begin(),cap2->end(), intersection.begin());

        intersection.resize(it-intersection.begin());
        
        // CORRIGIDO: Equação 1 do artigo CONTASKI
        // sim(ob1, ob2) = |Cob1 ∩ Cob2| / √(|Cob1| * |Cob2|)
        // sim = (double)intersection->size() / (double)cap1->size();
        sim = (double)intersection.size() / sqrt((double)cap1->size() * (double)cap2->size());

        return sim;
    }

    double capabilitiesSimilarityUFD(capabilitiesVector* cap1, capabilitiesVector* cap2){
        std::map<capabilities, double> translateLayer;
        
        translateLayer[capabilities::Temperature] = 3.0;
        translateLayer[capabilities::Umidity] = 5.0;
        translateLayer[capabilities::Presence] = 7.0;
        translateLayer[capabilities::Luminosity] = 11.0;
        translateLayer[capabilities::Sinchronization] = 13.0;
        translateLayer[capabilities::GasPressure] = 17.0;
        translateLayer[capabilities::ReservoirLevel] = 19.0;

        double sim = 0;
        capabilitiesVector intersection(cap1->size()+cap2->size());

        std::sort(cap1->begin(), cap1->end());
        std::sort(cap2->begin(), cap2->end());

        auto it = std::set_intersection(cap1->begin(),cap1->end(),cap2->begin(),cap2->end(), intersection.begin());
        intersection.resize(it-intersection.begin());

        double upper = 0.0;
        for(auto cap: intersection){
            upper = upper + pow(translateLayer[cap], 2);
        }
        //upper = sqrt(upper);

        double cap1Norm = 0.0;
        for(auto cap: *cap1){
            cap1Norm = cap1Norm + pow(translateLayer[cap], 2);
        }
        cap1Norm = sqrt(cap1Norm);

        double cap2Norm = 0.0;
        // CORRIGIDO: Antes iterava sobre cap1, agora itera sobre cap2
        for(auto cap: *cap2){
            cap2Norm = cap2Norm + pow(translateLayer[cap], 2);
        }
        cap2Norm = sqrt(cap2Norm);


        sim = upper / (cap1Norm*cap2Norm);

        return sim;
    }

    capabilitiesVector* parseCapabilities(std::string capabilitiesString){
        capabilitiesVector* cap = new capabilitiesVector;
        size_t last = 0;
        size_t next = 0;
        
        while ((next = capabilitiesString.find(",", last)) != string::npos) {
            cap->push_back(static_cast<capabilities>(
                atoi((capabilitiesString.substr(last, next-last)).c_str())
            ));
            last = next + 1;
        }
        cap->push_back(static_cast<capabilities>(
                atoi((capabilitiesString.substr(last, next-last)).c_str())
        ));

        return cap;
    }

    std::string serializeCapabilities(capabilitiesVector* cap){
        std::stringstream buf;
        
        for(auto it = cap->begin(); it != cap->end(); it++){
            buf << (*it);

            if(next(it, 1) != cap->end()){
                buf << ",";
            }
        }
        
        return buf.str();
    }
}