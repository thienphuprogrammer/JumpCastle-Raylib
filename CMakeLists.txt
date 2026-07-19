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
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
  src/asset_root.cpp
  src/assets.cpp
  src/camera.cpp
  src/campaign.cpp
  src/collision.cpp
  src/convex.cpp
  src/sat.cpp
  src/map_format.cpp
  src/collision_world.cpp
  src/campaign_world.cpp
  src/editor.cpp
  src/player.cpp
  src/presentation.cpp
  src/replay.cpp
  src/simulation.cpp
  src/solver.cpp
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

add_executable(jumpcastle_solver src/solver_main.cpp)
target_link_libraries(jumpcastle_solver PRIVATE jumpcastle_core)
jumpcastle_enable_warnings(jumpcastle_solver)

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
    tests/camera_test.cpp
    tests/collision_test.cpp
    tests/convex_test.cpp
    tests/sat_test.cpp
    tests/map_format_test.cpp
    tests/collision_world_test.cpp
    tests/editor_test.cpp
    tests/campaign_test.cpp
    tests/asset_root_test.cpp
    tests/assets_test.cpp
    tests/assets_manifest_test.cpp
    tests/math_test.cpp
    tests/player_test.cpp
    tests/player_view_test.cpp
    tests/presentation_test.cpp
    tests/resource_traits_test.cpp
    tests/replay_test.cpp
    tests/simulation_test.cpp
    tests/solver_test.cpp
    tests/world_test.cpp
    tests/world_reachability_test.cpp
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
  add_test(
    NAME asset_selection
    COMMAND ${Python3_EXECUTABLE} -m pytest
            ${PROJECT_SOURCE_DIR}/tests/test_asset_selection.py -q
  )
  set_tests_properties(asset_selection PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
  add_test(
    NAME asset_pixels
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/verify_asset_pixels.py
            ${PROJECT_SOURCE_DIR}/assets/generated/manifest.json
  )
  add_test(
    NAME asset_recovery
    COMMAND ${CMAKE_COMMAND}
            -DBINARY_DIR=${PROJECT_BINARY_DIR}
            -DASSET_DIR=$<TARGET_FILE_DIR:jumpcastle>/assets
            -DSOURCE_ASSETS=${PROJECT_SOURCE_DIR}/assets
            -DCONFIG=$<CONFIG>
            -P ${PROJECT_SOURCE_DIR}/tests/cmake/asset_recovery.cmake
  )
  add_test(
    NAME asset_render_smoke
    COMMAND ${Python3_EXECUTABLE} -m pytest
            ${PROJECT_SOURCE_DIR}/tests/test_render_asset_smoke.py -q
  )
  set_tests_properties(asset_render_smoke PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
  add_test(
    NAME narrow_campaign_authoring
    COMMAND ${Python3_EXECUTABLE} -m pytest
            ${PROJECT_SOURCE_DIR}/tests/test_narrow_campaign.py -q
  )
  set_tests_properties(narrow_campaign_authoring PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endif()

set(JUMPCASTLE_RUNTIME_ASSETS
  generated/castle.png
  generated/knight.png
  generated/ui.png
  generated/manifest.json
  levels/campaign.level
  levels/campaign-route.json
)

add_custom_target(
  jumpcastle_assets ALL
  COMMAND ${CMAKE_COMMAND}
          -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/assets
          -DDESTINATION_ROOT=$<TARGET_FILE_DIR:jumpcastle>/assets
          "-DFILES=${JUMPCASTLE_RUNTIME_ASSETS}"
          -P ${PROJECT_SOURCE_DIR}/cmake/SyncAssets.cmake
  COMMENT "Synchronizing JumpCastle runtime assets"
  VERBATIM
)
add_dependencies(jumpcastle jumpcastle_assets)

install(TARGETS jumpcastle RUNTIME DESTINATION bin)
install(DIRECTORY assets/generated/ DESTINATION bin/assets/generated)
install(DIRECTORY assets/levels/ DESTINATION bin/assets/levels)
