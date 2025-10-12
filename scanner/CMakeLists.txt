cmake_minimum_required(VERSION 3.19)

project(
  hyprwire-scanner
  DESCRIPTION "A protocol code generator for hyprwire"
  VERSION ${HYPRWIRE_VERSION})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(PREFIX ${CMAKE_INSTALL_PREFIX})

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

# configure
set(CMAKE_CXX_STANDARD 23)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value
                    -Wno-missing-field-initializers -Wno-narrowing)

add_compile_definitions(SCANNER_VERSION="${HYPRWIRE_VERSION}")

configure_file(hyprwire-scanner.pc.in hyprwire-scanner.pc @ONLY)

# dependencies
message(STATUS "Checking deps...")

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET pugixml)

add_executable(hyprwire-scanner main.cpp)
target_link_libraries(hyprwire-scanner PRIVATE rt Threads::Threads
                                               PkgConfig::deps)

target_include_directories(hyprwire-scanner
                           PUBLIC "${CMAKE_SOURCE_DIR}/include")

configure_package_config_file(
  hyprwire-scanner-config.cmake.in hyprwire-scanner-config.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hyprwire-scanner"
  PATH_VARS CMAKE_INSTALL_BINDIR)
write_basic_package_version_file(
  "hyprwire-scanner-config-version.cmake"
  VERSION "${VERSION}" ARCH_INDEPENDENT
  COMPATIBILITY AnyNewerVersion)

# Installation
install(TARGETS hyprwire-scanner)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hyprwire-scanner.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hyprwire-scanner-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/hyprwire-scanner-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hyprwire-scanner)
