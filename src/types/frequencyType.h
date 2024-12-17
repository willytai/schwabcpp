#ifndef __FREQUENCY_TYPE_H__
#define __FREQUENCY_TYPE_H__

#include <string>

namespace schwabcpp {

class FrequencyType
{
public:
    enum FrequencyEnum {
        Minute,
        Daily,
        Weekly,
        Monthly,
    };

    FrequencyType(FrequencyEnum frequency) : m_frequency(frequency) {}

    std::string     toString() const;

private:
    FrequencyEnum      m_frequency;
};

}

#endif
