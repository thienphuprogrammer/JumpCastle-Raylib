#pragma once

#include "jumpcastle/game_config.hpp"

#include <algorithm>

namespace jumpcastle {

class FixedStepClock {
public:
    [[nodiscard]] int consume(const float frame_delta) noexcept {
        accumulator_ += std::clamp(static_cast<double>(frame_delta), 0.0, 0.1);
        int ticks{};
        while (accumulator_ + 1.0e-12 >=
               static_cast<double>(config::fixed_delta)) {
            accumulator_ -= static_cast<double>(config::fixed_delta);
            ++ticks;
        }
        return ticks;
    }

    [[nodiscard]] double remainder() const noexcept {
        return accumulator_;
    }

    void reset() noexcept {
        accumulator_ = 0.0;
    }

private:
    double accumulator_{};
};

}  // namespace jumpcastle
