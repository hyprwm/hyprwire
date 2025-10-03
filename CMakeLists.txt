cmake_minimum_required(VERSION 3.19)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} HYPRWIRE_VERSION)

add_compile_definitions(HYPRWIRE_VERSION="${HYPRWIRE_VERSION}")

project(
  hyprwire
  VERSION ${HYPRWIRE_VERSION}
  DESCRIPTION "A fast and consistent wire protocol for IPC")

include(CTest)
include(CheckIncludeFile)
include(GNUInstallDirs)

set(PREFIX ${CMAKE_INSTALL_PREFIX})
set(INCLUDE ${CMAKE_INSTALL_FULL_INCLUDEDIR})
set(LIBDIR ${CMAKE_INSTALL_FULL_LIBDIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(
  deps
  REQUIRED
  IMPORTED_TARGET
  hyprutils>=0.9.0
)

configure_file(hyprwire.pc.in hyprwire.pc @ONLY)

set(CMAKE_CXX_STANDARD 23)
add_compile_options(
  -Wall
  -Wextra
  -Wno-unused-parameter
  -Wno-unused-value
  -Wno-missing-field-initializers
  -Wpedantic)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Configuring hyprwire in Debug")
  add_compile_definitions(HYPRWIRE_DEBUG)
else()
  add_compile_options(-O3)
  message(STATUS "Configuring hyprwire in Release")
endif()

add_compile_definitions(HT_HIDDEN=public)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp" "include/*.hpp")
file(GLOB_RECURSE PUBLIC_HEADERS CONFIGURE_DEPENDS "include/*.hpp")

execute_process(COMMAND ./scripts/generateShaderIncludes.sh
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_library(hyprwire SHARED ${SRCFILES})
target_include_directories(
  hyprwire
  PUBLIC "./include"
  PRIVATE "./src" "${CMAKE_BINARY_DIR}")
set_target_properties(hyprwire PROPERTIES VERSION ${hyprwire_VERSION}
                                            SOVERSION 1)
target_link_libraries(hyprwire PkgConfig::deps)

check_include_file("sys/timerfd.h" HAS_TIMERFD)
pkg_check_modules(epoll IMPORTED_TARGET epoll-shim)
if(NOT HAS_TIMERFD AND epoll_FOUND)
  target_link_libraries(hyprwire PkgConfig::epoll)
endif()

# tests
add_custom_target(tests)

add_executable(client "tests/Client.cpp")
target_link_libraries(client PRIVATE PkgConfig::deps hyprwire)
add_dependencies(tests client)

add_executable(server "tests/Server.cpp")
target_link_libraries(server PRIVATE PkgConfig::deps hyprwire)
add_dependencies(tests server)

# Installation
install(TARGETS hyprwire)
install(DIRECTORY "include/hyprwire" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/hyprwire.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
