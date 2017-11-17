set(srcs ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

find_package(ZLIB REQUIRED)

include_directories(
  .
  base
  box
  core
  crypto
  filters
  hash
  libs
  libs/fmt
  patch/xdelta3
  ${ZLIB_INCLUDE_DIRS}
)


add_subdirectory(base)
add_subdirectory(box)
add_subdirectory(core)
add_subdirectory(crypto)
add_subdirectory(filters)
add_subdirectory(hash)
add_subdirectory(libs)
add_subdirectory(patch)

add_executable(tests tests.cpp)

set(LIBRARIES
  LIB_Base
  LIB_Box
  LIB_Crypto
  LIB_Filters
  LIB_Hash
  LIB_Libs_Fmt
  LIB_Patch_Xdelta3
  ${ZLIB_LIBRARIES}
)

target_link_libraries(tests ${LIBRARIES})