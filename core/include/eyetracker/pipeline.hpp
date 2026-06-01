#pragma once
#include "hal.hpp"
#include "one_euro_filter.hpp"
#include <memory>
#include <functional>

namespace eyetracker {

// Non-owning callback
using ResultCallback = std::function<void(const TrackingResult&)>;

class Pipeline {
public:
    Pipeline(ICamera& cam, IGazeEstimator& est)
        : cam_(cam), est_(est) {}

    void set_callback(ResultCallback cb) { cb_ = std::move(cb); }

    // Call in your main loop: grab → estimate → filter → callback
    // Returns false when camera fails (caller should break loop).
    bool tick() {
        Frame f;
        if (!cam_.grab(f)) return false;
        TrackingResult r = est_.estimate(f);
        if (r.valid)
            r.gaze = filter_.filter(r.gaze, r.ts);
        if (cb_) cb_(r);
        return true;
    }

    void reset_filter() { filter_.reset(); }

private:
    ICamera&          cam_;
    IGazeEstimator&   est_;
    OneEuroGazeFilter filter_;
    ResultCallback    cb_;
};

} // namespace eyetracker
