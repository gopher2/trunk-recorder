# Plugin source files
set(PLUGIN_SOURCES
    p25_control_streaming.cc
    udp_streamer.cc
    library_streamer.cc
)

# Create shared library
add_library(p25_control_streaming MODULE ${PLUGIN_SOURCES})

# Link libraries
target_link_libraries(p25_control_streaming
    trunk_recorder_library
    ${Boost_LIBRARIES}
    dl
    pthread
    ${GNURADIO_PMT_LIBRARIES}
    ${GNURADIO_RUNTIME_LIBRARIES}
)

# Install target
install(TARGETS p25_control_streaming LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/trunk-recorder)