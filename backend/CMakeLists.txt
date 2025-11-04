cmake_minimum_required(VERSION 3.15)
project(streaming_rpq)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master
)
FetchContent_MakeAvailable(crow)

add_executable(streaming_rpq
    code/main.cpp
    code/routing/SignalHandler.cpp
)

target_include_directories(streaming_rpq PRIVATE 
    code/source
    code/routing
)
target_link_libraries(streaming_rpq PRIVATE 
    Crow::Crow
    pthread
)