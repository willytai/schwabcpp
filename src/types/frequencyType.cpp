#include "frequencyType.h"

namespace schwabcpp {

std::string
FrequencyType::toString() const
{
    switch (m_frequency) {
        case Minute: return "minute";
        case Daily: return "daily";
        case Weekly: return "weekly";
        case Monthly: return "monthly";
    }

    return "BAD TYPE";
}

}
