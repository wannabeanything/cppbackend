cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


add_library(model STATIC
    src/model.cpp
    src/model.h
    src/loot_generator.cpp
    src/loot_generator.h
    src/extra_data.cpp
    src/extra_data.h
    src/objects.h
    src/tagged.h
    src/geom.h
    src/collision_detector.h
    src/collision_detector.cpp
)

target_include_directories(model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(model
    PUBLIC
        CONAN_PKG::boost
)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/request_handler.cpp
    src/request_handler.h
    src/json_loader.cpp
    src/json_loader.h
    src/boost_json.cpp
    src/sdk.h
)

target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server
    PRIVATE
        model
        CONAN_PKG::boost
        Threads::Threads
)


