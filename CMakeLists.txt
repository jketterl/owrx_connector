cmake_minimum_required (VERSION 2.6)
project (digiham)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

find_package(RTLSDR REQUIRED)
find_package(Threads REQUIRED)
find_package(SoapySDR "0.5" REQUIRED)

INCLUDE_DIRECTORIES(
    "${RTLSDR_INCLUDE_DIRS}"
    "${SoapySDR_INCLUDE_DIRS}"
)

SET(LIBS ${LIBS} ${RTLSDR_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${SoapySDR_LIBRARIES})

add_library(version version.c)
target_include_directories (version PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(rtl_connector rtl_connector.c)
target_link_libraries(rtl_connector version ${LIBS})

add_executable(soapy_connector soapy_connector.c)
target_link_libraries(soapy_connector version ${LIBS})

install(TARGETS rtl_connector DESTINATION bin)
install(TARGETS soapy_connector DESTINATION bin)

SET(CMAKE_CXX_FLAGS "-O3")
SET(CMAKE_C_FLAGS "-O3")
