cmake_minimum_required(VERSION 3.30)
project(ada)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++17")

include_directories(code/source)

add_executable(ada
        code/source/streaming_graph.h
        code/main.cpp
        code/source/rpq_forest.h
        code/source/fsa.h
        code/source/query_handler.h
        code/source/sink.h
)