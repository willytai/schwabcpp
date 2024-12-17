#include "candle.h"

namespace schwabcpp {

void Candle::to_json(json &j, const Candle &self)
{
    j = {
        {"close", self.close},    
        {"datetime", self.datetime}, 
        {"high", self.high},     
        {"low", self.low},      
        {"open", self.open},     
        {"volume", self.volume},   
    };
}

void Candle::from_json(const json &j, Candle &data)
{
    j.at("close").get_to(data.close);    
    j.at("datetime").get_to(data.datetime); 
    j.at("high").get_to(data.high);     
    j.at("low").get_to(data.low);      
    j.at("open").get_to(data.open);     
    j.at("volume").get_to(data.volume);   
}

}
