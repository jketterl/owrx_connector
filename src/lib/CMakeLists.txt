add_library(owrx-connector SHARED connector.cpp iq_connection.cpp rtl_tcp_connection.cpp control_connection.cpp gainspec.cpp)
target_link_libraries(owrx-connector Csdr::csdr++ ${LIBS})
set_target_properties(owrx-connector PROPERTIES VERSION ${PROJECT_VERSION})
file(GLOB OWRX_CONNECTOR_HEADERS "${PROJECT_SOURCE_DIR}/include/owrx/*.hpp")
set_target_properties(owrx-connector PROPERTIES PUBLIC_HEADER "${OWRX_CONNECTOR_HEADERS}")
install(TARGETS owrx-connector
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/owrx
)

configure_file(owrx-connector.pc.in owrx-connector.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/owrx-connector.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
