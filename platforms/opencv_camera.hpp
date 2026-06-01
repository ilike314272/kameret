#pragma once
// OpenCV VideoCapture implementation of ICamera.
// Used on all three platforms — platform folder just sets compile flags.
#include <eyetracker/hal.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>

namespace eyetracker::platform {

class OpenCVCamera final : public ICamera {
    cv::VideoCapture cap_;
    cv::Mat          buf_;
    int w_ = 0, h_ = 0;
public:
    bool open(int device_id) override {
        // MSMF on Windows gives lower latency with CAP_MSMF;
        // V4L2 on Linux is default. Both work via the same call.
        bool ok = cap_.open(device_id, cv::CAP_ANY);
        if (!ok) return false;
        // Minimal buffer: reduce internal queue depth
        cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);
        cap_.set(cv::CAP_PROP_FRAME_WIDTH,  640);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap_.set(cv::CAP_PROP_FPS, 30);
        w_ = (int)cap_.get(cv::CAP_PROP_FRAME_WIDTH);
        h_ = (int)cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
        return true;
    }

    void close() override { cap_.release(); }

    bool grab(Frame& out) override {
        if (!cap_.read(buf_)) return false;
        if (buf_.channels() == 4)
            cv::cvtColor(buf_, buf_, cv::COLOR_BGRA2BGR);
        out.data   = buf_.data;
        out.width  = buf_.cols;
        out.height = buf_.rows;
        out.stride = (int)buf_.step;
        out.ts     = Clock::now();
        return true;
    }

    int width()  const override { return w_; }
    int height() const override { return h_; }
};

} // namespace eyetracker::platform
