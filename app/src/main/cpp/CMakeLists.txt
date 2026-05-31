cmake_minimum_required(VERSION 3.22.1)
project("voicechanger")

add_library(voicechanger SHARED native-lib.cpp AudioEngine.cpp)
find_library(log-lib log)
target_link_libraries(voicechanger aaudio ${log-lib})