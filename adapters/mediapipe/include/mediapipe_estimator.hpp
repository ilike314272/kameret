#pragma once
// Real MediaPipe Face Mesh estimator.
// Only compiled when EYETRACKER_ENABLE_MEDIAPIPE=ON and mediapipe is in external/.
#include <eyetracker/hal.hpp>
#include <cmath>

// Forward-declare MediaPipe types to avoid pulling the whole header in here.
// Uncomment and adjust paths once external/mediapipe is populated.
//
// #include "mediapipe/framework/calculator_graph.h"
// #include "mediapipe/graphs/face_mesh/face_mesh_desktop_live.pb.h"

namespace eyetracker::adapters {

class MediaPipeEstimator final : public IGazeEstimator {
public:
    MediaPipeEstimator() {
        // TODO: build and start the MediaPipe CalculatorGraph here.
        // Example:
        //   graph_.Initialize(BuildFaceMeshConfig());
        //   graph_.StartRun({});
    }
    ~MediaPipeEstimator() override {
        // graph_.CloseAllPacketSources();
        // graph_.WaitUntilDone();
    }

    TrackingResult estimate(const Frame& f) override {
        // 1. After getting frame, normalize lighting
        auto [norm_frame, brightness] = gaze::normalize_lighting(cv_mat);

        // 2. Run MediaPipe → get 478 landmarks → convert to vector<gaze::Landmark>

        // 3. PnP solve
        auto cam = gaze::make_camera_matrix(cfg_.FW, cfg_.FH);
        auto pnp = gaze::solve_pnp(lm, cam, cfg_.FW, cfg_.FH);
        auto [plane_depth, tvec_ippe] = gaze::solve_pnp_ippe(lm, cam).value_or(...);

        // 4. Ellipse gaze per eye
        auto [ax_l, ay_l] = gaze::eye_gaze_ellipse(lm, gaze::LEFT_EYE, brightness,
            distance_, plane_depth, cfg_);

        // 5. PnP blend at edges
        auto pnp_gaze = gaze::calculate_pnp_gaze(lm, gaze::LEFT_EYE,
            pnp->rotation_matrix, pnp->tvec, brightness, ellipse_nd, true, FW, FH);

        // 6. Blink detection
        auto blink = gaze::detect_blink(lm, blink_state_, cfg_);
        (void)f;
        TrackingResult r; r.valid = false; return r;
    }

private:
    // mediapipe::CalculatorGraph graph_;
};

} // namespace eyetracker::adapters
