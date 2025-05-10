cmake_minimum_required(VERSION 3.16)

project(retrozip)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable parallel compilation
if(MSVC)
  add_compile_options(/MP) # Enable parallel compilation for MSVC
endif()

# Set output directories for executables and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/intermediate)

# Set intermediate object files directory
set(CMAKE_OBJECT_PATH_MAX 260) # Ensure compatibility with long paths
set(CMAKE_OBJECT_OUTPUT_DIR ${CMAKE_BINARY_DIR}/intermediate)

# Specify the directory for CMake files
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_FILES_DIRECTORY ${CMAKE_BINARY_DIR}/cmake)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

set(LIBS_DIR ${CMAKE_SOURCE_DIR}/libs)
include_directories(${LIBS_DIR}/nana/include)
include_directories(${LIBS_DIR}/lzma/include)
include_directories(${LIBS_DIR}/blterm/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

set(SOURCES_MINIZIP
  ${CMAKE_SOURCE_DIR}/src/tbx/formats/minizip/ioapi.c
  ${CMAKE_SOURCE_DIR}/src/tbx/formats/minizip/unzip.c
  ${CMAKE_SOURCE_DIR}/src/tbx/formats/minizip/zip.c
)

# Add CLI target
add_executable(cli
  src/gui/cli.cpp
  src/flow/flow.cpp
  src/tbx/base/path.cpp
  src/tbx/base/file_system.cpp
  src/tbx/base/strings.cpp
  ${SOURCES_MINIZIP}
)

if(APPLE)
  target_link_libraries(cli PRIVATE ${LIBS_DIR}/blterm/lib/macos/libBearLibTerminal.dylib)
  target_link_libraries(cli PRIVATE ${ZLIB_LIBRARIES})

  add_custom_command(TARGET cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${LIBS_DIR}/blterm/lib/macos/libBearLibTerminal.dylib
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  )

  add_custom_command(TARGET cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/data/consola.ttf
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  )
endif()