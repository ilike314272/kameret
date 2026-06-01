#pragma once
#include "types.hpp"

namespace eyetracker {

// ── Camera HAL ─────────────────────────────────────────────────────────────
class ICamera {
public:
    virtual ~ICamera() = default;
    virtual bool open(int device_id) = 0;
    virtual void close()             = 0;
    virtual bool grab(Frame& out)    = 0; // fills data/width/height/stride/ts
    virtual int  width()  const      = 0;
    virtual int  height() const      = 0;
};

// ── Gaze filter HAL ────────────────────────────────────────────────────────
class IGazeFilter {
public:
    virtual ~IGazeFilter() = default;
    virtual GazePoint filter(GazePoint raw, TimePoint ts) = 0;
    virtual void      reset()                              = 0;
};

// ── Gaze estimator HAL ─────────────────────────────────────────────────────
// Implemented by adapters/mediapipe (or a stub)
class IGazeEstimator {
public:
    virtual ~IGazeEstimator() = default;
    // raw BGR frame in → tracking result out
    virtual TrackingResult estimate(const Frame& frame) = 0;
};

} // namespace eyetracker
