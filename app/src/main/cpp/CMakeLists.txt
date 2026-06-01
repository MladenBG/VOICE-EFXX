cmake_minimum_required(VERSION 3.22.1)
project("voicechanger")

# Find Oboe downloaded via Gradle Prefab
find_package(oboe REQUIRED CONFIG)

# Include directories for headers
include_directories(stk)
include_directories(soundtouch)

# Automatically grab all relevant .cpp files
file(GLOB STK_SOURCES "stk/*.cpp")

# Exclude networking, threads, and redundant STK audio device drivers to prevent NDK build errors
list(FILTER STK_SOURCES EXCLUDE REGEX ".*(InetWvOut|InetWvIn|UdpSocket|TcpClient|TcpServer|Socket|Thread|Mutex|RtAudio|RtMidi|RtWvIn|RtWvOut)\\.cpp$")

file(GLOB SOUNDTOUCH_SOURCES "soundtouch/*.cpp")

# Build the main shared library
add_library(
        voicechanger
        SHARED
        native-lib.cpp
        AudioEngine.cpp
        ${STK_SOURCES}
        ${SOUNDTOUCH_SOURCES}
)

find_library(log-lib log)

# Link the libraries
target_link_libraries(
        voicechanger
        oboe::oboe
        ${log-lib}
)