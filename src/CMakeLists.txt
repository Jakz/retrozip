set(srcs ${CMAKE_CURRENT_SOURCE_DIR}/cli.cpp)

find_package(ZLIB REQUIRED)

include_directories(
  .
  box
  crypto
  filters
  hash
  libs
  ../toolbox/src
  ${ZLIB_INCLUDE_DIRS}
)


add_subdirectory(box)
add_subdirectory(crypto)
add_subdirectory(filters)
add_subdirectory(libs)
add_subdirectory(test)

add_subdirectory(../toolbox/src/tbx/base ./tbx/base)
add_subdirectory(../toolbox/src/tbx/extra/fmt ./tbx/extra/fmt)
add_subdirectory(../toolbox/src/tbx/hash ./tbx/hash)
add_subdirectory(../toolbox/src/tbx/streams ./tbx/streams)
add_subdirectory(../toolbox/src/tbx/formats/patch/xdelta3 ./tbx/formats/patch/xdelta3)

# add_executable(tests tests.cpp)
add_executable(retrozip ${srcs})

set(LIBRARIES
  LIB_Base
  LIB_Box
  LIB_Crypto
  LIB_Filters
  LIB_Hash
  LIB_Libs_Fmt
  LIB_Patch_Xdelta3
  LIB_Testing
  ${ZLIB_LIBRARIES}
)

set_target_properties(retrozip
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if (APPLE)
  target_link_libraries(retrozip ${CMAKE_SOURCE_DIR}/toolbox/libs/lzma/lib/macOS/liblzma.a)
elseif (UNIX)
  add_library(lzma SHARED IMPORTED)
  set_target_properties(lzma PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/toolbox/libs/lzma/lib/linux/x64/liblzma.so)
  target_link_libraries(retrozip lzma)
endif()

target_link_libraries(retrozip ${LIBRARIES})