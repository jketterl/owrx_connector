add_library(owrx-connector SHARED connector.cpp iq_connection.cpp rtl_tcp_connection.cpp control_connection.cpp gainspec.cpp)
target_link_libraries(owrx-connector Csdr::csdr ${LIBS})
set_target_properties(owrx-connector PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
file(GLOB OWRX_CONNECTOR_HEADERS "${PROJECT_SOURCE_DIR}/include/owrx/*.hpp")
set_target_properties(owrx-connector PROPERTIES PUBLIC_HEADER "${OWRX_CONNECTOR_HEADERS}")
install(TARGETS owrx-connector
    EXPORT OwrxConnectorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/owrx
)

if (IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PKGCONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(PKGCONFIG_LIBDIR "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if (IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(PKGCONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(PKGCONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

configure_file(owrx-connector.pc.in owrx-connector.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/owrx-connector.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(EXPORT OwrxConnectorTargets
    FILE OwrxConnectorTargets.cmake
    NAMESPACE OwrxConnector::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OwrxConnector
)

include(CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/OwrxConnectorConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OwrxConnector
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OwrxConnectorConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/OwrxConnectorConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OwrxConnectorConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OwrxConnector
)

