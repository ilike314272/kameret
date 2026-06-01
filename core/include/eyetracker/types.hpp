#pragma once
#include <cstdint>
#include <chrono>

namespace eyetracker {

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct Frame {
    const uint8_t* data   = nullptr;
    int            width  = 0;
    int            height = 0;
    int            stride = 0;   // bytes per row (BGR, 3 ch)
    TimePoint      ts;
};

struct GazePoint { float x = 0, y = 0; };   // normalised [0,1]

struct EyeLandmarks {
    float iris_x   = 0, iris_y = 0;  // normalised
    float openness = 1.0f;
};

struct TrackingResult {
    EyeLandmarks left, right;
    GazePoint    gaze;           // averaged, filtered
    TimePoint    ts;
    bool         valid = false;
};

} // namespace eyetracker
