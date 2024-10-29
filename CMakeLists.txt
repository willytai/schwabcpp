cmake_minimum_required(VERSION 3.10)

project(l2viz)

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

# enable objc++
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ObjC++ -fobjc-weak -fobjc-arc")

# CURL
find_package(CURL REQUIRED)

# Boost
find_package(Boost REQUIRED)

# OpenSSL
find_package(OpenSSL REQUIRED)

# glfw
find_package(glfw3 3.3 REQUIRED)

include_directories(
    external/base64/include
    external/spdlog/include
    external/imgui/backends
    external/imgui
    external/metal-cpp
    ${Boost_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

file(GLOB_RECURSE SOURCES_CPP   src/*.cpp)
file(GLOB_RECURSE SOURCES_CMM   src/*.mm)

set_source_files_properties(${SOURCES_CMM} PROPERTIES COMPILE_FLAGS "-ObjC++ -fobjc-weak")

add_executable(l2viz ${SOURCES_CPP} ${SOURCES_CMM})

target_link_libraries(l2viz PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pthread
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    glfw
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework QuartzCore"
)
