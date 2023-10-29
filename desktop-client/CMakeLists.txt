cmake_minimum_required(VERSION 2.34)

# set the project name
project(desktop-client)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()
include(FetchContent)

# Disable Portaudio
option(ADD_PORTAUDIO  "Do not use Portaudio" OFF) 
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# Download miniaudio.h
file(DOWNLOAD https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio.h)

# Build with snapclient
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/snapclient )
endif()

# audio tools
FetchContent_Declare(arduino-audio-tools GIT_REPOSITORY https://github.com/pschatzmann/arduino-audio-tools.git GIT_TAG main  )
FetchContent_GetProperties(arduino-audio-tools)
if(NOT arduino-audio-tools_POPULATED)
    FetchContent_Populate(arduino-audio-tools)
    add_subdirectory(${arduino-audio-tools_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools)
endif()

# opus
FetchContent_Declare(arduino_libopus GIT_REPOSITORY https://github.com/pschatzmann/arduino-libopus.git GIT_TAG main  )
FetchContent_GetProperties(arduino_libopus)
if(NOT arduino_libopus_POPULATED)
    FetchContent_Populate(arduino_libopus)
    add_subdirectory(${arduino_libopus_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino_libopus)
endif()


# build sketch as executable
add_executable (desktop-client SnapClient.cpp)

# set preprocessor defines
target_compile_definitions(desktop-client PUBLIC -DARDUINO -DEXIT_ON_STOP -DIS_DESKTOP -DCONFIG_USE_RTOS=0 -DCONFIG_USE_PSRAM=0 -DCONFIG_SNAPCLIENT_SNTP_ENABLE=0 -DCONFIG_SNAPCLIENT_USE_MDNS=0)

# specify libraries
target_link_libraries(desktop-client arduino_emulator arduino_libopus snapclient arduino-audio-tools)

# to find include for miniaudio
target_include_directories(desktop-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})