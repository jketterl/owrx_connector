cmake_minimum_required (VERSION 2.6)
project (digiham)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

find_package(Threads REQUIRED)
find_package(RTLSDR)
find_package(SoapySDR "0.5")


SET(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

add_library(version version.c)
target_include_directories (version PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (RTLSDR_FOUND)
    SET(RTL_LIBS ${LIBS} ${RTLSDR_LIBRARY})
    INCLUDE_DIRECTORIES("${RTLSDR_INCLUDE_DIRS}")
    add_executable(rtl_connector rtl_connector.c)
    target_link_libraries(rtl_connector version ${RTL_LIBS})
    install(TARGETS rtl_connector DESTINATION bin)
endif()

if (SoapySDR_FOUND)
    SET(SOAPY_LIBS ${LIBS} ${SoapySDR_LIBRARIES})
    INCLUDE_DIRECTORIES("${SoapySDR_INCLUDE_DIRS}")
    add_executable(soapy_connector soapy_connector.c)
    target_link_libraries(soapy_connector version ${SOAPY_LIBS})
    install(TARGETS soapy_connector DESTINATION bin)
endif()

SET(CMAKE_CXX_FLAGS "-O3")
SET(CMAKE_C_FLAGS "-O3")
