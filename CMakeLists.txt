cmake_minimum_required (VERSION 3.0)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

SET(CMAKE_CXX_FLAGS_DEBUG "-O3 -fopt-info-vec")
SET(CMAKE_C_FLAGS_DEBUG "-O3 -fopt-info-vec")
SET(CMAKE_CXX_FLAGS_RELEASE "-g -O2")
SET(CMAKE_C_FLAGS_RELEASE "-g -O2")

project (owrx-connector VERSION 0.4.0)

include(CheckFunctionExists)
include(cmake/DetectIfunc.cmake)
if (HAS_IFUNC)
    add_definitions(-DOWRX_CONNECTOR_FMV)
    SET(CMAKE_CXX_FLAGS_RELEASE "-g -O3")
    SET(CMAKE_C_FLAGS_RELEASE "-g -O3")
endif()

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

find_package(Threads REQUIRED)
find_package(RTLSDR)
find_package(SoapySDR "0.5")

SET(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

add_library(version STATIC version.c)
target_include_directories (version PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (RTLSDR_FOUND)
    SET(RTL_LIBS ${LIBS} ${RTLSDR_LIBRARY})
    INCLUDE_DIRECTORIES("${RTLSDR_INCLUDE_DIRS}")

    SET(CMAKE_REQUIRED_LIBRARIES ${RTLSDR_LIBRARY})
    check_function_exists(rtlsdr_set_bias_tee HAS_RTLSDR_SET_BIAS_TEE)
    UNSET(CMAKE_REQUIRED_LIBRARIES)

    add_executable(rtl_connector rtl_connector.cpp connector.cpp rtl_handler.cpp conversions.c ringbuffer.cpp iq_connection.cpp)
    if (HAS_RTLSDR_SET_BIAS_TEE)
        target_compile_definitions(rtl_connector PUBLIC HAS_RTLSDR_SET_BIAS_TEE)
    endif()
    target_link_libraries(rtl_connector ${RTL_LIBS})
    install(TARGETS rtl_connector DESTINATION bin)
endif()

if (SoapySDR_FOUND)
    SET(SOAPY_LIBS ${LIBS} ${SoapySDR_LIBRARIES})
    INCLUDE_DIRECTORIES("${SoapySDR_INCLUDE_DIRS}")
    add_executable(soapy_connector soapy_connector.c strtolower.c)
    target_link_libraries(soapy_connector version ${SOAPY_LIBS})
    install(TARGETS soapy_connector DESTINATION bin)
endif()

if (NOT RTLSDR_FOUND AND NOT SoapySDR_FOUND)
    message(WARNING "WARNING: Neither SoapySDR nor rtlsdr libraries have been found, only rtl_tcp_conector will be built.")
endif()

add_executable(rtl_tcp_connector rtl_tcp_connector.c strtolower.c)
target_link_libraries(rtl_tcp_connector version ${LIBS})
install(TARGETS rtl_tcp_connector DESTINATION bin)

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
