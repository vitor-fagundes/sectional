#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>

using namespace std;

namespace nr2{
    enum capabilities{Temperature, Umidity, Presence, Luminosity, Sinchronization, GasPressure, ReservoirLevel};
    typedef std::vector<capabilities> capabilitiesVector;

    static capabilitiesVector basicCapabilities = {Temperature, Umidity, Presence};

    double capabilitiesSimilarity(capabilitiesVector* cap1, capabilitiesVector* cap2);
    double capabilitiesSimilarityUFD(capabilitiesVector* cap1, capabilitiesVector* cap2);
    capabilitiesVector* parseCapabilities(std::string capabilitiesString);
    std::string serializeCapabilities(capabilitiesVector* cap);
}