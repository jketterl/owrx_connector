SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

find_package(RTLSDR)
find_package(SoapySDR "0.5")

if (RTLSDR_FOUND)
    SET(RTL_LIBS ${LIBS} ${RTLSDR_LIBRARY})
    INCLUDE_DIRECTORIES("${RTLSDR_INCLUDE_DIRS}")

    SET(CMAKE_REQUIRED_LIBRARIES ${RTLSDR_LIBRARY})
    check_function_exists(rtlsdr_set_bias_tee HAS_RTLSDR_SET_BIAS_TEE)
    UNSET(CMAKE_REQUIRED_LIBRARIES)

    add_executable(rtl_connector rtl_connector.cpp)
    if (HAS_RTLSDR_SET_BIAS_TEE)
        target_compile_definitions(rtl_connector PUBLIC HAS_RTLSDR_SET_BIAS_TEE)
    endif()
    target_link_libraries(rtl_connector owrx-connector ${RTL_LIBS})
    install(TARGETS rtl_connector DESTINATION bin)
endif()

if (SoapySDR_FOUND)
    SET(SOAPY_LIBS ${LIBS} ${SoapySDR_LIBRARIES})
    INCLUDE_DIRECTORIES("${SoapySDR_INCLUDE_DIRS}")
    add_executable(soapy_connector soapy_connector.cpp)
    target_link_libraries(soapy_connector owrx-connector ${SOAPY_LIBS})
    install(TARGETS soapy_connector DESTINATION bin)
endif()

if (NOT RTLSDR_FOUND AND NOT SoapySDR_FOUND)
    message(WARNING "WARNING: Neither SoapySDR nor rtlsdr libraries have been found, only rtl_tcp_conector will be built.")
endif()

add_executable(rtl_tcp_connector rtl_tcp_connector.cpp)
target_link_libraries(rtl_tcp_connector owrx-connector ${LIBS})
install(TARGETS rtl_tcp_connector DESTINATION bin)
