#pragma once

#include "ns3/tag.h"
#include "ns3/type-id.h"

using namespace ns3;

namespace nr2{
    class MyTag : public Tag
    {
    public:

    static TypeId 		GetTypeId (void);
    virtual TypeId 		GetInstanceTypeId (void) const;
    virtual uint32_t 		GetSerializedSize (void) const;
    virtual void 			Serialize (TagBuffer i) const;
    virtual void 			Deserialize (TagBuffer i);
    virtual void 			Print (std::ostream &os) const;
    void 					SetSimpleValue (uint8_t value);
    uint8_t 				GetSimpleValue (void) const;

    private:
    uint8_t 				m_simpleValue;  //!< tag value
    };
}