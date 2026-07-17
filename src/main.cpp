#include "jumpcastle/game.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        jumpcastle::Game game;
        game.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "JumpCastle: fatal error: " << error.what() << '\n';
        return 1;
    }
}
