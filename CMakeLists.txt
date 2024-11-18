cmake_minimum_required(VERSION 3.16)

project(schwabcpp)

set(DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# JSON
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(JSON_MultipleHeaders ON)
add_subdirectory(external/nlohmann_json)

# spdlog
add_subdirectory(external/spdlog)

# CURL
find_package(CURL REQUIRED)

# Boost
find_package(Boost REQUIRED)

# OpenSSL
find_package(OpenSSL REQUIRED)

include_directories(
    src
    include
    external/base64/include
    external/spdlog/include
    external/nlohmann_json/include
    ${Boost_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

file(GLOB_RECURSE SOURCES src/*.cpp)
list(FILTER SOURCES EXCLUDE REGEX "src/main/main.cpp$")

add_library(schwabcpp SHARED ${SOURCES})

target_link_libraries(schwabcpp PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pthread
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
)
# build the test executable if we are top level
if (schwabcpp_IS_TOP_LEVEL)
    message("Test for schwabcpp turned on.")
    add_executable(test src/main/main.cpp)
    target_link_libraries(test PRIVATE schwabcpp)
endif()
