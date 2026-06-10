add_library(rpgkit_core INTERFACE)
add_library(rpg::core ALIAS rpgkit_core)
target_include_directories(rpgkit_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(rpgkit_core INTERFACE cxx_std_20)

if(RPGKIT_BUILD_TESTS)
  add_executable(rpgkit_core_tests tests/version_test.cpp)
  target_link_libraries(rpgkit_core_tests PRIVATE rpg::core GTest::gtest_main)
  # add GTest::gmock here when a test first needs it — core v1 tests use hand-written fakes
  gtest_discover_tests(rpgkit_core_tests)
endif()
