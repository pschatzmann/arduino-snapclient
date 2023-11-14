cmake_minimum_required(VERSION 3.16)

# set the project name
project(snapclient)

set(CMAKE_CXX_STANDARD 17)

# lots of warnings and all warnings as errors
## add_compile_options(-Wall -Wextra )

add_library(snapclient INTERFACE)

# define location for header files
target_include_directories(snapclient INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src )



