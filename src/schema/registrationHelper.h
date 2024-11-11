#include "nlohmann/json.hpp"

#define REGISTER_TO_JSON(Type)                          \
namespace nlohmann {                                    \
    template <>                                         \
    struct adl_serializer<Type> {                       \
        static void to_json(json& j, const Type& t) {   \
            Type::to_json(j, t);                        \
        }                                               \
                                                        \
        static void from_json(const json& j, Type& t) { \
            Type::from_json(j, t);                      \
        }                                               \
    };                                                  \
}
