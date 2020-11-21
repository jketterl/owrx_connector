add_library(owrx-connector SHARED connector.cpp ringbuffer.cpp iq_connection.cpp control_connection.cpp gainspec.cpp)
target_link_libraries(owrx-connector ${LIBS})
set_target_properties(owrx-connector PROPERTIES VERSION ${PROJECT_VERSION})
install(TARGETS owrx-connector LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
