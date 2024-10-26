cmake_minimum_required(VERSION 3.10)

project(l2viz)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# JSON
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(JSON_MultipleHeaders ON)
add_subdirectory(external/nlohmann_json)

# CURL
find_package(CURL REQUIRED)

# Boost
find_package(Boost REQUIRED)

find_package(OpenSSL REQUIRED)

include_directories(
    external/base64/include
    external/spdlog/include
    ${Boost_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(l2viz ${SOURCES})

target_link_libraries(l2viz PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pthread
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)
