cmake_minimum_required (VERSION 3.0)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

project (owrx-connector VERSION 0.4.0)
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
endif()

SET(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src/lib)
add_subdirectory(src/app)

SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jakob Ketterl <jakob.ketterl@gmx.de>")
SET(CPACK_DEBIAN_PACKAGE_DESCRIPTION "OpenWebRX SDR hardware interfacing binaries")
SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS "ON")
SET(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
if(DEFINED ENV{BUILD_NUMBER})
  SET(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}-$ENV{BUILD_NUMBER}")
endif()
INCLUDE(CPack)
