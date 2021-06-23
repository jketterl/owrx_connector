cmake_minimum_required (VERSION 3.0)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_BUILD_TYPE STREQUAL None)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

project (owrx-connector VERSION 0.5.0)
add_definitions(-DVERSION="${PROJECT_VERSION}-dev")

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)

# these are conflicting settings in older SoapySDR cmake files.
# enable for testing purposes.
#add_compile_options(-fvisibility=hidden)
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")

include(CheckFunctionExists)
include(cmake/DetectIfunc.cmake)
include(GNUInstallDirs)

find_package(Threads REQUIRED)

SET(CMAKE_CXX_FLAGS_DEBUG "-g -O3 -fopt-info-vec")
SET(CMAKE_C_FLAGS_DEBUG "-g -O3 -fopt-info-vec")
SET(CMAKE_CXX_FLAGS_RELEASE "-O2")
SET(CMAKE_C_FLAGS_RELEASE "-O2")

if (HAS_IFUNC)
    add_definitions(-DOWRX_CONNECTOR_FMV)
    SET(CMAKE_CXX_FLAGS_RELEASE "-O3")
    SET(CMAKE_C_FLAGS_RELEASE "-O3")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3 -DNDEBUG")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-g -O3 -DNDEBUG")
endif()

SET(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src/lib)
add_subdirectory(src/app)
