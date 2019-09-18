cmake_minimum_required(VERSION 2.8)
project( opendps DESCRIPTION "libopendps" )

include(GNUInstallDirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(opendps SHARED src/opendps.c)
set_target_properties(opendps PROPERTIES
    SOVERSION 1
    PUBLIC_HEADER opendps/opendps.h)

set(DPSCTL_SRCS
  examples/dpsctl.c
)

add_executable( dpsctl ${DPSCTL_SRCS} )
set_target_properties(dpsctl PROPERTIES COMPILE_FLAGS "-Wall -Wformat-nonliteral")
target_link_libraries( dpsctl opendps )
install (TARGETS dpsctl DESTINATION bin)
