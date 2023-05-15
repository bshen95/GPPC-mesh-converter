project(startkit)
cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-w -pedantic -Wall -Wno-strict-aliasing -Wno-long-long -Wno-deprecated -Wno-deprecated-declarations")
set(DEV_CXX_FLAGS "-g -ggdb -O0 -fno-omit-frame-pointer")
set(RELEASE_CXX_FLAGS "-O3 -DNDEBUG")
#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS_DEBUG "${DEV_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${RELEASE_CXX_FLAGS}")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L../meshConverter/lib/lib_APPLE -lfade2d")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/../)
file(GLOB SOURCES
        "include_cdt/*.h"
        "*.h"
        )



message("LIBRARIES = ${LIBRARIES}")
include_directories("include_cdt")


set(SRC
        main.cpp
        )

add_executable(run ${SRC} ${SOURCES} main.cpp)