cmake_minimum_required(VERSION 3.0.2)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE "Debug")
endif()

if(APPLE)
  set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE STRING "" FORCE)
  set(CMAKE_C_COMPILER "/usr/bin/clang" CACHE STRING "" FORCE)
elseif(UNIX)
  set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE STRING "" FORCE)
  set(CMAKE_C_COMPILER "/usr/bin/clang" CACHE STRING "" FORCE)
elseif(WIN32)

endif()



if(APPLE)
  set(BASE_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping -Wno-reorder -std=c++14 -stdlib=libc++ -fdiagnostics-show-option") # -Wall
elseif(UNIX)
  set(BASE_FLAGS "${CMAKE_CXX_FLAGS} -Wno-reorder -Wno-inconsistent-missing-override -std=c++14 -stdlib=libstdc++ -fdiagnostics-show-option") # -Wall
elseif(WIN32)
  add_compile_options(/w4068)
endif()


add_definitions(-DCMAKE_BUILD)
if (DEBUG_LEVEL)
  add_definitions(-DDEBUG=${DEBUG_LEVEL})
endif()

#set(BASE_FLAGS "${BASE_FLAGS} -fno-rtti -fno-exceptions")

if (APPLE OR UNIX)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fstandalone-debug -DDEBUG=1 ${BASE_FLAGS}" CACHE FILEPATH "cache")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-O3 -flto -DNDEBUG ${BASE_FLAGS}" CACHE FILEPATH "cache")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -DNDEBUG ${BASE_FLAGS}" CACHE FILEPATH "cache")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -flto -g ${BASE_FLAGS}" CACHE FILEPATH "cache")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g -fstandalone-debug -DDEBUG=1" CACHE FILEPATH "cache")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-flto -O3 -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -flto -g" CACHE STRING "" FORCE)

endif()

message("Build Type is ${CMAKE_BUILD_TYPE}")
message("Compiler is ${CMAKE_CXX_COMPILER}")
message("C flags are ${CMAKE_C_FLAGS}")
message("CXX flags are ${CMAKE_CXX_FLAGS}")
message("CMAKE_SOURCE_DIR is ${CMAKE_SOURCE_DIR}")

#if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
#  message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
#endif()

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project("retrozip" CXX C)

add_subdirectory(src)

