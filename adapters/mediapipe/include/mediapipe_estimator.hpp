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
        // TODO:
        // 1. Wrap f.data in a cv::Mat → ImageFrame packet
        // 2. graph_.AddPacketToInputStream("input_video", ...)
        // 3. Poll output stream "multi_face_landmarks"
        // 4. Extract iris landmarks (indices 468-471 left, 473-477 right)
        // 5. Compute iris centre, map to normalised coords
        // 6. Return populated TrackingResult
        (void)f;
        TrackingResult r; r.valid = false; return r;
    }

private:
    // mediapipe::CalculatorGraph graph_;
};

} // namespace eyetracker::adapters
