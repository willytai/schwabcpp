#include "streamerField.h"

namespace schwabcpp {

StreamerField::LevelOneEquity
StreamerField::toLevelOneEquityField(const std::string& key)
{
    LevelOneEquity result(LevelOneEquity::Unknown);

    try {
        result = static_cast<LevelOneEquity>(std::stoi(key));
    } catch (...) {
        // do nothing
    }

    return result;
}

}
