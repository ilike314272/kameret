#include <eyetracker/pipeline.hpp>
#include <eyetracker/one_euro_filter.hpp>

// Platform camera (shared OpenCV impl)
#include "../../platforms/opencv_camera.hpp"

// Estimator: real MediaPipe if built, otherwise stub
#ifdef KAMERET_MEDIAPIPE
#  include "../../adapters/mediapipe/include/mediapipe_estimator.hpp"
   using Estimator = eyetracker::adapters::MediaPipeEstimator;
#else
#  include "../../adapters/mediapipe/include/stub_estimator.hpp"
   using Estimator = eyetracker::adapters::StubGazeEstimator;
#endif

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <Eigen/Dense>
#include <fmt/core.h>
#include <fmt/chrono.h>

#include <atomic>
#include <chrono>

// ── Helpers ─────────────────────────────────────────────────────────────────

// Map normalised gaze → pixel, smooth with an Eigen lerp for the overlay dot.
static cv::Point gaze_to_px(const eyetracker::GazePoint& g, int w, int h) {
    Eigen::Vector2f p(g.x * w, g.y * h);
    return { (int)p.x(), (int)p.y() };
}

// Draw a minimal HUD onto the preview frame.
static void draw_hud(cv::Mat& img, const eyetracker::TrackingResult& r,
                     double fps)
{
    const int W = img.cols, H = img.rows;
    if (r.valid) {
        cv::Point dot = gaze_to_px(r.gaze, W, H);
        cv::circle(img, dot, 14, {0, 255, 80}, 2);
        cv::circle(img, dot, 4,  {0, 255, 80}, -1);

        // Openness bars (left / right)
        int bw = (int)(r.left.openness  * 60);
        cv::rectangle(img, {10, H-30}, {10+bw, H-18}, {100,200,255}, -1);
        bw = (int)(r.right.openness * 60);
        cv::rectangle(img, {W-70, H-30}, {W-70+bw, H-18}, {100,200,255}, -1);
    }

    auto label = fmt::format("fps:{:.1f}  gaze:({:.3f},{:.3f})  valid:{}",
                             fps, r.gaze.x, r.gaze.y, r.valid ? "Y" : "N");
    cv::putText(img, label, {8, 22}, cv::FONT_HERSHEY_SIMPLEX,
                0.55, {30,30,30}, 3);
    cv::putText(img, label, {8, 22}, cv::FONT_HERSHEY_SIMPLEX,
                0.55, {220,220,220}, 1);
}

// ── Main ────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    int device_id = (argc > 1) ? std::stoi(argv[1]) : 0;

    eyetracker::platform::OpenCVCamera cam;
    if (!cam.open(device_id)) {
        fmt::print(stderr, "kameret: cannot open camera {}\n", device_id);
        return 1;
    }
    fmt::print("kameret: camera {}x{}\n", cam.width(), cam.height());

    Estimator estimator;
    eyetracker::Pipeline pipeline(cam, estimator);

    // FPS smoothing via Eigen exponential moving average
    Eigen::Array<double,1,1> fps_ema; fps_ema << 30.0;
    constexpr double ALPHA = 0.1;

    eyetracker::TrackingResult last_result;
    auto t0 = eyetracker::Clock::now();

    pipeline.set_callback([&](const eyetracker::TrackingResult& r) {
        last_result = r;
        auto now = eyetracker::Clock::now();
        double dt = std::chrono::duration<double>(now - t0).count();
        t0 = now;
        fps_ema = (1-ALPHA)*fps_ema + ALPHA*(1.0/std::max(dt,1e-6));
    });

    cv::namedWindow("kameret", cv::WINDOW_NORMAL);

    fmt::print("kameret: running — press ESC/q to quit\n");

    while (true) {
        if (!pipeline.tick()) {
            fmt::print(stderr, "kameret: camera read failed\n");
            break;
        }

        // Re-grab last frame for display (zero-copy ref via OpenCV Mat header)
        eyetracker::Frame raw;
        cam.grab(raw); // second grab for display; acceptable at 30 fps
        cv::Mat display(raw.height, raw.width, CV_8UC3,
                        const_cast<uint8_t*>(raw.data), raw.stride);
        cv::Mat vis = display.clone();

        draw_hud(vis, last_result, fps_ema(0));
        cv::imshow("kameret", vis);

        int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q') break;
    }

    cam.close();
    fmt::print("kameret: done\n");
    return 0;
}
