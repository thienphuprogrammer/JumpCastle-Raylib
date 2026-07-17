cmake_minimum_required(VERSION 3.24)

project(JumpCastle VERSION 1.0.0 LANGUAGES CXX)

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

add_library(
  jumpcastle_core STATIC
  src/collision.cpp
  src/player.cpp
  src/tilemap.cpp
)
target_include_directories(jumpcastle_core PUBLIC include)
target_link_libraries(jumpcastle_core PUBLIC raylib)

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
    tests/player_test.cpp
    tests/resource_traits_test.cpp
    tests/tilemap_test.cpp
  )
  target_link_libraries(jumpcastle_tests PRIVATE jumpcastle_core Catch2::Catch2WithMain)
  jumpcastle_enable_warnings(jumpcastle_tests)

  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)
  catch_discover_tests(jumpcastle_tests)
endif()

add_custom_command(
  TARGET jumpcastle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${PROJECT_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:jumpcastle>/assets
)

install(TARGETS jumpcastle RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION bin/assets)
