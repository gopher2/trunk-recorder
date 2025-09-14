cmake_minimum_required(VERSION 3.12)

# Plugin name
set(PLUGIN_NAME p25_control_streaming)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system filesystem log)

# Plugin source files
set(PLUGIN_SOURCES
    p25_control_streaming.cc
    udp_streamer.cc
    library_streamer.cc
)

# Create shared library
add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

# Set library properties
set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX "lib"
    SUFFIX ".so"
)

# Include directories
target_include_directories(${PLUGIN_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PLUGIN_NAME}
    ${Boost_LIBRARIES}
    dl
    pthread
)

# Compiler flags
target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_14)
target_compile_options(${PLUGIN_NAME} PRIVATE
    -fPIC
    -Wall
    -Wextra
)

# Install target
install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION lib/plugins
)