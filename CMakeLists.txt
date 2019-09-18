cmake_minimum_required(VERSION 2.8)
project( opendps DESCRIPTION "libopendps" )

include(GNUInstallDirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(opendps SHARED src/opendps.c)

set(DPSCTL_SRCS
  examples/dpsctl.c
)

add_executable( dpsctl ${DPSCTL_SRCS} )
set_target_properties(dpsctl PROPERTIES COMPILE_FLAGS "-Wall -Wformat-nonliteral")
target_include_directories (opendps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(dpsctl LINK_PUBLIC opendps)

install(TARGETS opendps DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/opendps/opendps.h DESTINATION include/opendps)
install(TARGETS dpsctl DESTINATION bin)
