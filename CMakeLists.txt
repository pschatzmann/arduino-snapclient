cmake_minimum_required(VERSION 3.16)

# set the project name
project(snapclient)

# we use FetchContent to install 
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)

# Disable Portaudio
option(ADD_PORTAUDIO  "Do not use Portaudio" OFF) 
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

set(CODEC "opus" CACHE STRING "codec selected by the user")
set_property(CACHE CODEC PROPERTY STRINGS flac ogg opus pcm)


# lots of warnings and all warnings as errors ?
## add_compile_options(-Wall -Wextra )

# audio tools
FetchContent_Declare(arduino-audio-tools GIT_REPOSITORY https://github.com/pschatzmann/arduino-audio-tools.git GIT_TAG main  )
FetchContent_GetProperties(arduino-audio-tools)
if(NOT arduino-audio-tools_POPULATED)
    FetchContent_Populate(arduino-audio-tools)
    add_subdirectory(${arduino-audio-tools_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools)
endif()

# opus
if(CODEC==opus)
    FetchContent_Declare(arduino_libopus GIT_REPOSITORY https://github.com/pschatzmann/arduino-libopus.git GIT_TAG main  )
    FetchContent_GetProperties(arduino_libopus)
    if(NOT arduino_libopus_POPULATED)
        FetchContent_Populate(arduino_libopus)
        add_subdirectory(${arduino_libopus_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino_libopus)
    endif()
endif()

# flac
if(CODEC==flac)
    FetchContent_Declare(arduino_libflac GIT_REPOSITORY https://github.com/pschatzmann/arduino-libflac.git GIT_TAG main  )
    FetchContent_GetProperties(arduino_libflac)
    if(NOT arduino_libflac_POPULATED)
        FetchContent_Populate(arduino_libflac)
        add_subdirectory(${arduino_libflac_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino_libflac)
    endif()
endif()


add_library(snapclient INTERFACE)

# define location for header files
target_include_directories(snapclient INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src )

# specify libraries
if(CODEC==opus)
    target_link_libraries(snapclient INTERFACE arduino-audio-tools arduino_emulator arduino_libopus)
endif()
if(CODEC==flac)
    target_link_libraries(snapclient INTERFACE arduino-audio-tools arduino_emulator arduino_libflac)
endif()

target_compile_definitions(snapclient INTERFACE -DARDUINO -DEXIT_ON_STOP -DIS_DESKTOP -DCONFIG_USE_RTOS=0 -DCONFIG_USE_PSRAM=0 -DCONFIG_SNAPCLIENT_SNTP_ENABLE=0 -DCONFIG_SNAPCLIENT_USE_MDNS=0)


