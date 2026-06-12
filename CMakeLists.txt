cmake_minimum_required(VERSION 3.25)
project(rpgkit VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RPGKIT_BUILD_TESTS "Build rpgkit tests" ${PROJECT_IS_TOP_LEVEL})
option(RPGKIT_BUILD_EXAMPLES "Build rpgkit examples" ${PROJECT_IS_TOP_LEVEL})

# Strict warnings for rpgkit's own targets (tests and, later, examples).
# Linked PRIVATE per target rather than set globally, so dependencies built in
# this tree (googletest via FetchContent) and consumers vendoring rpgkit are
# never subjected to our -Werror.
add_library(rpgkit_warnings INTERFACE)
if(MSVC)
  target_compile_options(rpgkit_warnings INTERFACE /W4 /WX)
else()
  target_compile_options(rpgkit_warnings INTERFACE -Wall -Wextra -Werror)
endif()

if(RPGKIT_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # MSVC
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  include(GoogleTest)
endif()

add_subdirectory(core)
if(RPGKIT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
