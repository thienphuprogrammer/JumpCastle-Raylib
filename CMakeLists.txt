cmake_minimum_required(VERSION 3.24)

project(
  JumpCastle
  VERSION 1.0.0
  DESCRIPTION "A pixel-art charge-jumping platformer built with C++20 and raylib"
  HOMEPAGE_URL "https://github.com/thienphuprogrammer/JumpCastle-Raylib"
  LANGUAGES CXX
)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(
  jumpcastle_core STATIC
  src/assets.cpp
  src/campaign.cpp
  src/collision.cpp
  src/level.cpp
  src/player.cpp
  src/simulation.cpp
  src/solver.cpp
  src/tilemap.cpp
)
add_library(JumpCastle::Core ALIAS jumpcastle_core)
target_include_directories(jumpcastle_core PUBLIC include)
target_link_libraries(jumpcastle_core PRIVATE nlohmann_json::nlohmann_json)

add_executable(
  jumpcastle
  src/game.cpp
  src/main.cpp
  src/renderer.cpp
)
target_link_libraries(jumpcastle PRIVATE jumpcastle_core raylib)

function(jumpcastle_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

jumpcastle_enable_warnings(jumpcastle)
jumpcastle_enable_warnings(jumpcastle_core)

add_executable(jumpcastle_level_solver src/solver_main.cpp)
target_link_libraries(jumpcastle_level_solver PRIVATE jumpcastle_core)
jumpcastle_enable_warnings(jumpcastle_level_solver)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(JUMPCASTLE_BUILD_TESTS_DEFAULT ON)
else()
  set(JUMPCASTLE_BUILD_TESTS_DEFAULT OFF)
endif()
option(JUMPCASTLE_BUILD_TESTS "Build JumpCastle tests" ${JUMPCASTLE_BUILD_TESTS_DEFAULT})

if(JUMPCASTLE_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(
    jumpcastle_tests
    tests/collision_test.cpp
    tests/campaign_test.cpp
    tests/campaign_reachability_test.cpp
    tests/assets_test.cpp
    tests/assets_manifest_test.cpp
    tests/level_test.cpp
    tests/math_test.cpp
    tests/player_test.cpp
    tests/player_view_test.cpp
    tests/resource_traits_test.cpp
    tests/simulation_test.cpp
    tests/solver_test.cpp
    tests/tilemap_test.cpp
  )
  target_link_libraries(
    jumpcastle_tests PRIVATE jumpcastle_core raylib Catch2::Catch2WithMain
  )
  target_compile_definitions(
    jumpcastle_tests PRIVATE
    JUMPCASTLE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  jumpcastle_enable_warnings(jumpcastle_tests)

  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)
  catch_discover_tests(jumpcastle_tests)

  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(
    NAME asset_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/verify_assets.py
            ${PROJECT_SOURCE_DIR}/assets/generated/manifest.json
  )
endif()

add_custom_command(
  TARGET jumpcastle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          $<TARGET_FILE_DIR:jumpcastle>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${PROJECT_SOURCE_DIR}/assets/generated
          $<TARGET_FILE_DIR:jumpcastle>/assets/generated
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${PROJECT_SOURCE_DIR}/assets/levels
          $<TARGET_FILE_DIR:jumpcastle>/assets/levels
)

install(TARGETS jumpcastle RUNTIME DESTINATION bin)
install(DIRECTORY assets/generated/ DESTINATION bin/assets/generated)
install(DIRECTORY assets/levels/ DESTINATION bin/assets/levels)
