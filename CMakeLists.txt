cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0048 NEW)
project( opendps VERSION "1.0.0" DESCRIPTION "libopendps" )

include(GNUInstallDirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(opendps SHARED src/opendps.c)

set(DPSCTL_SRCS
  examples/dpsctl.c
)

add_executable( dpsctl ${DPSCTL_SRCS} )
set_target_properties(dpsctl PROPERTIES COMPILE_FLAGS "-Wall -Wformat-nonliteral")
set_target_properties(opendps PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories (opendps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(dpsctl LINK_PUBLIC opendps)

configure_file(opendps.pc.in opendps.pc @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/opendps.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
install(TARGETS opendps LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS dpsctl DESTINATION bin)
