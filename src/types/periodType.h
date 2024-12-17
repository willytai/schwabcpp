#ifndef __PERIOD_TYPE_H__
#define __PERIOD_TYPE_H__

#include <string>

namespace schwabcpp {

class PeriodType
{
public:
    enum PeriodEnum {
        Day,
        Month,
        Year,
        YTD,
    };

    PeriodType(PeriodEnum period) : m_period(period) {}

    std::string     toString() const;

private:
    PeriodEnum      m_period;
};

}

#endif
