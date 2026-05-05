cmake_minimum_required(VERSION 3.22.1)
project(xdl)

file(GLOB XDL_SRC *.c)
add_library(xdl STATIC ${XDL_SRC})

target_compile_features(xdl PUBLIC c_std_17)
target_include_directories(xdl PUBLIC include .)