add_library(owrx-connector SHARED connector.cpp ringbuffer.cpp iq_connection.cpp control_connection.cpp gainspec.cpp)
target_link_libraries(owrx-connector ${LIBS})
set_target_properties(owrx-connector PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(owrx-connector PROPERTIES PUBLIC_HEADER include/owrx/*.h)
install(TARGETS owrx-connector
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_file(owrx-connector.pc.in owrx-connector.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/owrx-connector.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR})