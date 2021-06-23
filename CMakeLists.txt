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

if(CMAKE_SYSTEM_PROCESSOR MATCHES ^arm.*)
    execute_process(COMMAND "cat" "/proc/cpuinfo" OUTPUT_VARIABLE CPUINFO)
    string(FIND ${CPUINFO} "neon" NEON_POSITION)
    if(NEON_POSITION GREATER_EQUAL 0)
        set(HAS_NEON true)
    endif()
    if (HAS_NEON)
        SET(CMAKE_CXX_FLAGS "-ffast-math -march=armv7-a -mtune=cortex-a8 -funsafe-math-optimizations -Wformat=0 -mfloat-abi=hard -mfpu=neon -mvectorize-with-neon-quad")
        SET(CMAKE_C_FLAGS "-ffast-math -march=armv7-a -mtune=cortex-a8 -funsafe-math-optimizations -Wformat=0 -mfloat-abi=hard -mfpu=neon -mvectorize-with-neon-quad")
    endif()
endif()

SET(CMAKE_CXX_FLAGS_DEBUG "-g -O3 -fopt-info-vec")
SET(CMAKE_C_FLAGS_DEBUG "-g -O3 -fopt-info-vec")
SET(CMAKE_CXX_FLAGS_RELEASE "-O2")
SET(CMAKE_C_FLAGS_RELEASE "-O2")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2 -DNDEBUG")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-g -O2 -DNDEBUG")

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
