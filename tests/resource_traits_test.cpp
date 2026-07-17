#include "jumpcastle/renderer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace jumpcastle;

TEST_CASE("graphics resources cannot be copied") {
    STATIC_CHECK_FALSE(std::is_copy_constructible_v<TextureResource>);
    STATIC_CHECK_FALSE(std::is_copy_assignable_v<TextureResource>);
    STATIC_CHECK(std::is_move_constructible_v<TextureResource>);
    STATIC_CHECK(std::is_move_assignable_v<TextureResource>);
    STATIC_CHECK_FALSE(std::is_copy_constructible_v<RenderTargetResource>);
    STATIC_CHECK(std::is_move_constructible_v<RenderTargetResource>);
}
