#pragma once
#include <eyetracker/hal.hpp>

namespace eyetracker::adapters {

// ── Stub ──────────────────────────────────────────────────────────────────
// Compiles without MediaPipe. Returns a synthetic gaze that drifts so the
// demo loop has something to display while you wire up the real impl.
class StubGazeEstimator final : public IGazeEstimator {
    int tick_ = 0;
public:
    TrackingResult estimate(const Frame& f) override {
        TrackingResult r;
        r.valid     = true;
        r.ts        = f.ts;
        float t     = (tick_++ % 200) / 200.0f;
        constexpr float PI = 3.14159265358979323846f;
        r.gaze = { 0.5f + 0.4f * std::cos(2 * PI * t),
                        0.5f + 0.3f * std::sin(2 * PI * t) };
        r.left.openness = r.right.openness = 1.0f;
        r.left.iris_x   = r.right.iris_x   = r.gaze.x;
        r.left.iris_y   = r.right.iris_y   = r.gaze.y;
        return r;
    }
};

} // namespace eyetracker::adapters
