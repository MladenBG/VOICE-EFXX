cmake_minimum_required(VERSION 3.22.1)
project("voicechanger")

# Find Oboe downloaded via Gradle Prefab
find_package(oboe REQUIRED CONFIG)

# Tell the compiler where to look for the headers for STK and SoundTouch
include_directories(stk)
include_directories(soundtouch)

# Automatically grab all .cpp files from STK and SoundTouch folders
file(GLOB STK_SOURCES "stk/*.cpp")

# OVO JE PROŠIRENO: Izbacuje mrežne fajlove, Mutex/Thread, i STK mikrofonske/zvučne drajvere (RtWvIn, RtAudio...)
list(FILTER STK_SOURCES EXCLUDE REGEX ".*(InetWvOut|InetWvIn|UdpSocket|TcpClient|TcpServer|Socket|Thread|Mutex|RtAudio|RtMidi|RtWvIn|RtWvOut)\\.cpp$")

file(GLOB SOUNDTOUCH_SOURCES "soundtouch/*.cpp")

# Build the main shared library including all external source files
add_library(
        voicechanger
        SHARED
        native-lib.cpp
        AudioEngine.cpp
        ${STK_SOURCES}
        ${SOUNDTOUCH_SOURCES}
)

find_library(log-lib log)

# Link everything together (Oboe, Android Log, and our code)
target_link_libraries(
        voicechanger
        oboe::oboe
        ${log-lib}
)