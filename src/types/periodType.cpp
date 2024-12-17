#include "periodType.h"

namespace schwabcpp {

std::string
PeriodType::toString() const
{
    switch (m_period) {
        case Day: return "day";
        case Month: return "month";
        case Year: return "year";
        case YTD: return "ytd";
    }

    return "BAD TYPE";
}

}
