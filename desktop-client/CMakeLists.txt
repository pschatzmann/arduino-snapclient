cmake_minimum_required(VERSION 2.34)

# set the project name
project(desktop-client)
set (CMAKE_CXX_STANDARD 17)
set (DCMAKE_CXX_FLAGS "-Werror")
# set (FETCHCONTENT_FULLY_DISCONNECTED ON)

include(FetchContent)

# Download miniaudio.h
file(DOWNLOAD https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio.h)

# Build with snapclient
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/snapclient )
endif()


# build sketch as executable
add_executable (desktop-client SnapClient.cpp)

# set preprocessor defines
target_compile_definitions(desktop-client PUBLIC -DARDUINO -DEXIT_ON_STOP -DIS_DESKTOP -DCONFIG_USE_PSRAM=0 -DCONFIG_SNAPCLIENT_SNTP_ENABLE=0 -DCONFIG_SNAPCLIENT_USE_MDNS=0)

# specify libraries
target_link_libraries(desktop-client snapclient arduino-audio-tools arduino_emulator arduino_libvorbis)

# to find include for miniaudio
target_include_directories(desktop-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})