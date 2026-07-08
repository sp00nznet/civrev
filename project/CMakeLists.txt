# civrev - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(civrev LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(CIVREV_SOURCES
    src/main.cpp
    src/stubs.cpp
)

# Bring-up harvest scaffold (temporary): logs every unregistered indirect-call
# target so we can register the init-thunk class in one pass. Toggle OFF and
# remove src/dispatch_tolerance.cpp once the class is registered.
option(CIVREV_HARVEST "Build with the tolerant dispatch harvest scaffold" OFF)
if(CIVREV_HARVEST)
    list(APPEND CIVREV_SOURCES src/dispatch_tolerance.cpp)
endif()

if(WIN32)
    add_executable(civrev WIN32 ${CIVREV_SOURCES})
else()
    add_executable(civrev ${CIVREV_SOURCES})
endif()

if(CIVREV_HARVEST AND WIN32)
    target_link_options(civrev PRIVATE "LINKER:/force:multiple")
endif()

rexglue_setup_target(civrev)
