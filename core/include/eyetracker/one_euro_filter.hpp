#pragma once
// One-Euro Filter — Casiez et al. 2012 — low-latency, low-jitter
// Header-only, Eigen-free for portability; used by core pipeline.
#include "hal.hpp"
#include <cmath>

namespace eyetracker {

namespace detail {

struct OneEuro1D {
    double min_cutoff = 1.0;
    double beta       = 0.007;
    double d_cutoff   = 1.0;

    double x_prev  = 0, dx_prev = 0;
    bool   init    = false;

    static double alpha(double cutoff, double dt) {
        constexpr double PI = 3.14159265358979323846;
        double tau = 1.0 / (2.0 * PI * cutoff);
        return 1.0 / (1.0 + tau / dt);
    }

    double operator()(double x, double dt) {
        if (!init) { x_prev = x; dx_prev = 0; init = true; return x; }
        double dx      = (x - x_prev) / dt;
        double dx_hat  = dx_prev + alpha(d_cutoff, dt) * (dx - dx_prev);
        double cutoff  = min_cutoff + beta * std::abs(dx_hat);
        double x_hat   = x_prev + alpha(cutoff, dt) * (x - x_prev);
        x_prev = x_hat; dx_prev = dx_hat;
        return x_hat;
    }
};

} // namespace detail

// IGazeFilter implementation using One-Euro
class OneEuroGazeFilter final : public IGazeFilter {
    detail::OneEuro1D fx_, fy_;
    TimePoint         last_ts_;
    bool              first_ = true;
public:
    GazePoint filter(GazePoint raw, TimePoint ts) override {
        if (first_) { last_ts_ = ts; first_ = false;
                      return raw; }
        double dt = std::chrono::duration<double>(ts - last_ts_).count();
        last_ts_  = ts;
        if (dt <= 0) return raw;
        return { (float)fx_((double)raw.x, dt),
                 (float)fy_((double)raw.y, dt) };
    }
    void reset() override { fx_.init = false; fy_.init = false; first_ = true; }
};

} // namespace eyetracker
