cmake_minimum_required(VERSION 3.16)

# set the project name
project(snapclient)

# lots of warnings and all warnings as errors
## add_compile_options(-Wall -Wextra )
set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC_LIST_C CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/src/lightsnapcast/*.c" )

# define libraries
add_library (snapclient ${SRC_LIST_C})

# define location for header files
target_include_directories(snapclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/lightsnapcast )

# prevent compile errors
#target_compile_options(snapclient PRIVATE -DUSE_DEFAULT_STDLIB)


