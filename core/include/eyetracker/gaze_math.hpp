#pragma once
// gaze_math.hpp — C++23 port of gaze_calculator.py algorithms
// Uses: Eigen3 (linear algebra), OpenCV (solvePnP/Rodrigues/CLAHE), std::numbers (M_PI)
//
// No Python dicts — landmarks are a flat span<const Landmark> indexed directly.

#include <eyetracker/types.hpp>

#include <Eigen/Dense>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <fmt/core.h>

#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <span>

namespace eyetracker::gaze {

    // ── Landmark ─────────────────────────────────────────────────────────────────
    struct Landmark { float x, y, z; };  // normalised [0,1] x/y, depth z

    // ── Constants (tuned from Python source) ────────────────────────────────────
    struct Config {
        int   FW = 480, FH = 360;
        float SENS = 8.0f;
        float blink_threshold = 0.25f;
        int   blink_frames_required = 2;
        float reference_depth = 50.0f;   // cm
    };

    // ── Eye index sets (MediaPipe Face Mesh) ────────────────────────────────────
    struct EyeIndices {
        int in_, out_, top_, bot_, iris_;
    };
    inline constexpr EyeIndices LEFT_EYE{ 133,  33, 159, 145, 468 };
    inline constexpr EyeIndices RIGHT_EYE{ 362, 263, 386, 374, 473 };

    // ── Head-pose 6-point model (matches Python model_points) ───────────────────
    inline const std::array<cv::Point3d, 6> MODEL_POINTS{ {
        { 0.0,  0.0,   0.0  },
        { 0.0, -6.3,  -3.75 },
        {-4.3,  3.2,  -2.5  },
        { 4.3,  3.2,  -2.5  },
        {-2.85,-2.5,  -2.5  },
        { 2.85,-2.5,  -2.5  }
    } };
    inline constexpr int HEAD_INDICES[6]{ 1, 152, 33, 263, 61, 291 };


    // ═══════════════════════════════════════════════════════════════════════════
    // 1. Eye Aspect Ratio (EAR) — blink detection
    // ═══════════════════════════════════════════════════════════════════════════
    // EAR = vertical_dist / horizontal_dist
    // Typical open eye: 0.25–0.35; closed: < 0.20
    inline float eye_aspect_ratio(std::span<const Landmark> lm,
        const EyeIndices& idx,
        int FW, int FH)
    {
        auto px = [&](int i) -> Eigen::Vector2f {
            return { lm[i].x * FW, lm[i].y * FH };
            };
        Eigen::Vector2f inner = px(idx.in_);
        Eigen::Vector2f outer = px(idx.out_);
        Eigen::Vector2f top = px(idx.top_);
        Eigen::Vector2f bottom = px(idx.bot_);

        float vertical = (top - bottom).norm();
        float horizontal = (outer - inner).norm();
        if (horizontal < 1e-6f) return 0.3f;
        return vertical / horizontal;
    }

    // ── Blink state (per-eye pair) ───────────────────────────────────────────────
    struct BlinkState {
        int   frame_count = 0;
        bool  is_blinking = false;
        float last_blink_t = 0.0f;
    };

    struct BlinkResult {
        bool  is_blinking = false;
        float ear_left = 0.3f;
        float ear_right = 0.3f;
        float ear_avg = 0.3f;
        bool  blink_detected = false;
    };

    inline BlinkResult detect_blink(std::span<const Landmark> lm,
        BlinkState& state,
        const Config& cfg,
        float timestamp = 0.0f)
    {
        if (lm.size() < 478) return {};

        float ear_l = eye_aspect_ratio(lm, LEFT_EYE, cfg.FW, cfg.FH);
        float ear_r = eye_aspect_ratio(lm, RIGHT_EYE, cfg.FW, cfg.FH);
        float ear_avg = (ear_l + ear_r) * 0.5f;

        bool  is_closed = ear_avg < cfg.blink_threshold;
        bool  blink_detected = false;

        if (is_closed) {
            ++state.frame_count;
            if (!state.is_blinking && state.frame_count >= cfg.blink_frames_required) {
                state.is_blinking = true;
                state.last_blink_t = timestamp;
            }
        }
        else {
            if (state.is_blinking && state.frame_count >= cfg.blink_frames_required)
                blink_detected = true;
            state.frame_count = 0;
            state.is_blinking = false;
        }
        return { state.is_blinking, ear_l, ear_r, ear_avg, blink_detected };
    }


    // ═══════════════════════════════════════════════════════════════════════════
    // 2. Viewing angle from depth (pitch from MediaPipe z-coords)
    // ═══════════════════════════════════════════════════════════════════════════
    // Positive return = looking up; negative = looking down.
    inline float viewing_angle_from_depth(std::span<const Landmark> lm)
    {
        if (lm.size() < 478) return 0.0f;

        float forehead_z = lm[10].z;
        float chin_z = lm[18].z;
        float nose_z = lm[1].z;

        float vertical_depth_diff = chin_z - forehead_z;
        float face_depth = std::abs(nose_z);

        float normalized_diff = (face_depth > 1e-6f)
            ? vertical_depth_diff / face_depth
            : vertical_depth_diff * 5.0f;

        return std::atan(normalized_diff * 2.0f);
    }


    // ═══════════════════════════════════════════════════════════════════════════
    // 3. solvePnP helpers
    // ═══════════════════════════════════════════════════════════════════════════
    struct PnPResult {
        cv::Mat rvec, tvec, rotation_matrix;
        float   plane_depth = 50.0f;
    };

    // Build camera matrix for given frame size
    inline cv::Mat make_camera_matrix(int FW, int FH) {
        double f = FW;
        return (cv::Mat_<double>(3, 3) <<
            f, 0, FW / 2.0,
            0, f, FH / 2.0,
            0, 0, 1.0);
    }

    // Collect the 6 head-pose 2D image points from landmarks
    inline std::vector<cv::Point2d> head_image_points(std::span<const Landmark> lm,
        int FW, int FH)
    {
        std::vector<cv::Point2d> pts;
        pts.reserve(6);
        for (int idx : HEAD_INDICES)
            pts.push_back({ lm[idx].x * FW, lm[idx].y * FH });
        return pts;
    }

    // IPPE solve — plane depth extraction (for ellipse projection)
    inline std::optional<std::pair<float, cv::Mat>>
        solve_pnp_ippe(std::span<const Landmark> lm, const cv::Mat& cam_mat)
    {
        auto img_pts = head_image_points(lm, cam_mat.at<double>(0, 2) * 2,
            cam_mat.at<double>(1, 2) * 2);
        cv::Mat dist = cv::Mat::zeros(4, 1, CV_64F);
        cv::Mat rvec, tvec;

        std::vector<cv::Point3d> model(MODEL_POINTS.begin(), MODEL_POINTS.end());
        int flags = cv::SOLVEPNP_IPPE;
        bool ok = cv::solvePnP(model, img_pts, cam_mat, dist, rvec, tvec, false, flags);
        if (!ok) {
            ok = cv::solvePnP(model, img_pts, cam_mat, dist, rvec, tvec, false,
                cv::SOLVEPNP_ITERATIVE);
            if (!ok) return std::nullopt;
        }

        cv::Mat tvec_f = tvec.reshape(1, 3);
        float depth = std::clamp(std::abs((float)tvec_f.at<double>(2)), 20.0f, 150.0f);
        return std::make_pair(depth, tvec);
    }

    // Full solvePnP with pitch compensation from depth viewing angle
    inline std::optional<PnPResult>
        solve_pnp(std::span<const Landmark> lm, const cv::Mat& cam_mat,
            int FW, int FH)
    {
        auto img_pts = head_image_points(lm, FW, FH);
        cv::Mat dist = cv::Mat::zeros(4, 1, CV_64F);
        PnPResult r;
        std::vector<cv::Point3d> model(MODEL_POINTS.begin(), MODEL_POINTS.end());

        bool ok = cv::solvePnP(model, img_pts, cam_mat, dist,
            r.rvec, r.tvec, false, cv::SOLVEPNP_ITERATIVE);
        if (!ok) return std::nullopt;

        cv::Rodrigues(r.rvec, r.rotation_matrix);

        // Pitch compensation from depth
        float va = viewing_angle_from_depth(lm);
        auto& R = r.rotation_matrix;
        double sy = std::sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) +
            R.at<double>(1, 0) * R.at<double>(1, 0));
        float rot_pitch = (sy > 1e-6)
            ? (float)std::atan2(-R.at<double>(2, 0), sy) : 0.0f;

        float combined_pitch = va * 0.9f + rot_pitch * 0.1f;
        float compensation_angle = -combined_pitch * 0.6f;

        if (std::abs(compensation_angle) > 0.01f) {
            float c = std::cos(compensation_angle), s = std::sin(compensation_angle);
            cv::Mat pitch_comp = (cv::Mat_<double>(3, 3) <<
                1, 0, 0,
                0, c, -s,
                0, s, c);
            r.rotation_matrix = pitch_comp * r.rotation_matrix;
        }

        cv::Mat tv = r.tvec.reshape(1, 3);
        r.plane_depth = std::clamp(std::abs((float)tv.at<double>(2)), 20.0f, 150.0f);
        return r;
    }


    // ═══════════════════════════════════════════════════════════════════════════
    // 4. PnP-based gaze angles (head-pose driven, edge-boosted)
    // ═══════════════════════════════════════════════════════════════════════════
    struct PnpGazeResult { float angle_x, angle_y, edge_factor; };

    inline PnpGazeResult calculate_pnp_gaze(
        std::span<const Landmark> lm,
        const EyeIndices& eye_idx,
        const cv::Mat& rot_mat,
        const cv::Mat& tvec,
        float brightness,
        float ellipse_normalized_distance,
        bool  apply_normalization,
        int FW, int FH)
    {
        // Edge/transition factor
        constexpr float T_START = 0.5f, T_END = 1.2f;
        float edge_factor;
        if (ellipse_normalized_distance < T_START) edge_factor = 0.0f;
        else if (ellipse_normalized_distance > T_END)   edge_factor = 1.0f;
        else {
            float p = (ellipse_normalized_distance - T_START) / (T_END - T_START);
            edge_factor = std::pow(p, 0.8f);
        }

        // Depth scale
        cv::Mat tv = tvec.reshape(1, 3);
        float face_depth = std::clamp(std::abs((float)tv.at<double>(2)), 20.0f, 150.0f);
        float depth_scale = 50.0f / face_depth;

        // Extract yaw/pitch from rotation matrix
        auto& R = rot_mat;
        double sy = std::sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) +
            R.at<double>(1, 0) * R.at<double>(1, 0));
        float yaw = (sy > 1e-6) ? (float)std::atan2(R.at<double>(1, 0), R.at<double>(0, 0)) : 0.0f;
        float pitch = (sy > 1e-6) ? (float)std::atan2(-R.at<double>(2, 0), sy) : 0.0f;

        // Viewing-angle blend
        float va = viewing_angle_from_depth(lm);
        pitch = pitch * 0.1f + va * 0.9f;

        // Head-position offsets from nose-tip
        float head_pitch_offset_deg = 0.0f, head_yaw_offset_deg = 0.0f;
        if (lm.size() > 1) {
            float nose_x = lm[1].x, nose_y = lm[1].y;
            head_pitch_offset_deg = (0.5f - nose_y) * 35.0f;
            head_yaw_offset_deg = (0.5f - nose_x) * 30.0f;
        }

        if (apply_normalization) {
            pitch += -va * 0.90f;
            pitch += head_pitch_offset_deg * (std::numbers::pi_v<float> / 180.0f);
            yaw += head_yaw_offset_deg * (std::numbers::pi_v<float> / 180.0f);
        }

        // Sensitivity with edge boost
        float edge_sensitivity = 1.0f + edge_factor * 2.5f;
        float depth_sens = apply_normalization
            ? std::clamp(face_depth / 50.0f, 0.7f, 1.5f) : 1.0f;

        float sens_x = 120.0f * edge_sensitivity * depth_sens;
        float sens_y = 180.0f * edge_sensitivity * depth_sens;

        float angle_x = -std::numbers::inv_pi_v<float> *yaw * sens_x * (180.0f / 90.0f);
        float angle_y = -std::numbers::inv_pi_v<float> *pitch * sens_y * (180.0f / 90.0f);

        // Iris fine-adjustment
        auto px = [&](int i) -> Eigen::Vector2f {
            return { lm[i].x * FW, lm[i].y * FH };
            };
        Eigen::Vector2f eye_inner = px(eye_idx.in_);
        Eigen::Vector2f eye_outer = px(eye_idx.out_);
        Eigen::Vector2f iris_pos = px(eye_idx.iris_);
        Eigen::Vector2f eye_dir = (eye_outer - eye_inner).normalized();
        Eigen::Vector2f eye_perp = { -eye_dir.y(), eye_dir.x() };
        Eigen::Vector2f eye_center = (eye_inner + eye_outer) * 0.5f;
        Eigen::Vector2f iris_off = iris_pos - eye_center;

        float iris_off_x = iris_off.dot(eye_dir);
        float iris_off_y = iris_off.dot(eye_perp);
        float iris_scale_x = 15.0f * edge_sensitivity;
        float iris_scale_y = 25.0f * edge_sensitivity;
        bool  is_left = (eye_idx.out_ == 33);
        angle_x += (is_left ? 1.0f : -1.0f) * iris_off_x * iris_scale_x / 100.0f;
        angle_y += iris_off_y * iris_scale_y / 100.0f;

        // Lighting compensation
        float lc = 1.0f;
        if (brightness < 0.35f) lc = (brightness > 0.2f) ? 0.85f + (brightness - 0.2f) / 0.15f * 0.15f : 0.85f;
        else if (brightness < 0.45f) lc = 0.95f + (brightness - 0.35f) / 0.1f * 0.05f;
        else if (brightness > 0.75f) lc = (0.6f - (brightness - 0.75f) / 0.25f * 0.2f) * 0.85f;
        else if (brightness > 0.65f) lc = 0.9f - (brightness - 0.65f) / 0.1f * 0.1f;
        angle_x *= lc; angle_y *= lc;
        if (brightness > 0.7f) { angle_x *= 0.85f; angle_y *= 0.85f; }

        // Damping for large movements
        float mag = std::sqrt(angle_x * angle_x + angle_y * angle_y);
        if (mag > 8.0f) {
            float damp = 1.0f - std::min((mag - 8.0f) / 15.0f, 0.35f);
            angle_x *= damp; angle_y *= damp;
        }

        return { angle_x, angle_y, edge_factor };
    }


    // ═══════════════════════════════════════════════════════════════════════════
    // 5. Ellipse-based eye gaze (iris position + quaternion projection)
    // ═══════════════════════════════════════════════════════════════════════════
    // Returns (angle_x, angle_y) in degrees.
    inline std::pair<float, float> eye_gaze_ellipse(
        std::span<const Landmark> lm,
        const EyeIndices& eye_idx,
        float brightness,
        float distance,       // user distance cm
        std::optional<float> plane_depth,
        const Config& cfg,
        bool  apply_normalization = true)
    {
        auto px = [&](int i) -> Eigen::Vector2f {
            return { lm[i].x * cfg.FW, lm[i].y * cfg.FH };
            };

        Eigen::Vector2f inner = px(eye_idx.in_);
        Eigen::Vector2f outer = px(eye_idx.out_);
        Eigen::Vector2f top = px(eye_idx.top_);
        Eigen::Vector2f bottom = px(eye_idx.bot_);
        Eigen::Vector2f iris = px(eye_idx.iris_);

        Eigen::Vector2f center = (inner + outer + top + bottom) * 0.25f;
        Eigen::Vector2f delta = iris - center;

        Eigen::Vector2f h_axis = (outer - inner).normalized();
        Eigen::Vector2f v_axis = (bottom - top).normalized();

        float delta_x = delta.dot(h_axis);
        float delta_y = delta.dot(v_axis);

        // Depth scaling
        float base_ellipse_a = 18.0f, base_ellipse_b = 10.0f;
        float raw_nx = delta_x / base_ellipse_a;
        float raw_ny = delta_y / base_ellipse_b;
        float raw_nd = std::sqrt(raw_nx * raw_nx + raw_ny * raw_ny);

        float blended_depth = plane_depth.has_value()
            ? std::clamp(*plane_depth * 0.7f + distance * 0.3f, 20.0f, 150.0f)
            : std::clamp(distance, 20.0f, 150.0f);
        float base_depth_scale = 50.0f / blended_depth;

        // Corneal vector-field correction
        float depth_correction = 1.0f, tx_corr = 0.0f, ty_corr = 0.0f;
        if (raw_nd > 1e-6f) {
            float ef = std::min(raw_nd / 1.5f, 1.0f);
            float raw_angle = std::atan2(raw_ny, raw_nx);
            float corner_angles[4] = { (float)std::numbers::pi / 4, 3 * (float)std::numbers::pi / 4,
                                       5 * (float)std::numbers::pi / 4, 7 * (float)std::numbers::pi / 4 };
            float min_cd = std::numbers::pi_v<float>;
            for (float ca : corner_angles) {
                float d = std::min(std::abs(raw_angle - ca),
                    2.0f * (float)std::numbers::pi - std::abs(raw_angle - ca));
                min_cd = std::min(min_cd, d);
            }
            float corner_f = std::cos(min_cd * 2.0f);
            float cs = ef * (1.0f + corner_f * 0.5f);
            depth_correction = 1.0f + cs * 0.15f;
            tx_corr = raw_nx * cs * 0.1f;
            ty_corr = raw_ny * cs * 0.1f;
        }

        float depth_scale = base_depth_scale * depth_correction;
        float ellipse_a = base_ellipse_a * depth_scale;
        float ellipse_b = base_ellipse_b * depth_scale;

        float cx_delta = delta_x + tx_corr * base_ellipse_a;
        float cy_delta = delta_y + ty_corr * base_ellipse_b;
        float norm_x = (std::abs(ellipse_a) > 1e-6f) ? cx_delta / ellipse_a : 0.0f;
        float norm_y = (std::abs(ellipse_b) > 1e-6f) ? cy_delta / ellipse_b : 0.0f;
        float ell_nd = std::sqrt(norm_x * norm_x + norm_y * norm_y);
        if (ell_nd > 3.0f) {
            float sf = 3.0f / ell_nd;
            norm_x *= sf; norm_y *= sf; ell_nd = 3.0f;
        }

        // Quaternion components
        float move_angle = std::atan2(norm_y, norm_x);
        float dist_sens = 1.0f;
        if (ell_nd > 1e-6f) {
            float exp_sens = std::pow(2.5f, ell_nd);
            float vert_exp = std::pow(3.0f, std::abs(norm_y));
            float edge_exp = std::pow(2.8f, std::min(ell_nd, 1.0f));
            dist_sens = exp_sens * vert_exp * edge_exp;
        }

        float qw = depth_scale * dist_sens;
        float qx = norm_x, qy = norm_y;

        float corner_angles4[4] = { (float)std::numbers::pi / 4, 3 * (float)std::numbers::pi / 4,
                                     5 * (float)std::numbers::pi / 4, 7 * (float)std::numbers::pi / 4 };
        float min_cd4 = std::numbers::pi_v<float>;
        for (float ca : corner_angles4) {
            float d = std::min(std::abs(move_angle - ca),
                2.0f * (float)std::numbers::pi - std::abs(move_angle - ca));
            min_cd4 = std::min(min_cd4, d);
        }
        float corner_f4 = std::cos(min_cd4 * 2.0f);
        float qz = corner_f4 * 0.6f + std::min(ell_nd, 1.0f) * 0.4f;

        float qmag = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
        if (qmag > 1e-6f) { qw /= qmag; qx /= qmag; qy /= qmag; qz /= qmag; }

        // Unwrapping boost
        float ep = std::min(ell_nd / 1.5f, 1.0f);
        float unwrap_exp = std::pow(3.2f, ep);
        float corner_exp = std::pow(4.0f, corner_f4);
        float corner_boost4 = corner_f4 * 3.5f;
        float vert_exp_uw = std::pow(3.5f, std::abs(norm_y));
        float vert_boost = std::abs(norm_y) * 3.0f;
        float horiz_boost = std::abs(norm_x) * 2.0f;
        float uw_base = 1.0f + corner_boost4 + vert_boost + horiz_boost * 0.5f;
        float uw_raw = uw_base * unwrap_exp * corner_exp * vert_exp_uw;

        // Clamp bias
        float uniform_boost = 1.0f + ep * 1.2f;
        float uw = (uw_raw > uniform_boost * 4.0f)
            ? uw_raw * (uniform_boost * 4.0f / uw_raw) : uw_raw;

        // Edge enhancement
        float ee = (1.0f + qz * 1.5f) * std::pow(1.8f, qz);

        float proj_x = qx * qw * ee * uw * ellipse_a;
        float proj_y = qy * qw * ee * uw * ellipse_b;

        // Sensitivity scales
        float DEG_X = (25.0f / 150.0f) * cfg.SENS;
        float DEG_Y = (25.0f / 100.0f) * cfg.SENS * 2.5f;

        float h_scale_raw = DEG_X * 2.2f * std::pow(1.5f, std::abs(norm_x));
        float v_scale_raw = DEG_Y * 2.2f * std::pow(2.2f, std::abs(norm_y))
            * (1.0f + std::abs(norm_y) * 1.5f);
        float v_scale = (v_scale_raw / std::max(h_scale_raw, 1e-6f) > 5.5f)
            ? h_scale_raw * 5.5f : v_scale_raw;
        float h_scale = h_scale_raw;

        float ax = proj_x * h_scale;
        float ay = proj_y * v_scale;

        // Depth normalization
        if (apply_normalization) {
            float ds = std::clamp(blended_depth / 50.0f, 0.7f, 1.5f);
            ax *= ds; ay *= ds;
        }

        // Head angle/position normalization
        if (apply_normalization) {
            float va = viewing_angle_from_depth(lm);
            ay += -std::numbers::inv_pi_v<float> *180.0f * va * 0.85f;
            if (lm.size() > 1) {
                ay += (0.5f - lm[1].y) * 35.0f;
                ax += (0.5f - lm[1].x) * 30.0f;
            }
        }

        return { ax, ay };
    }


    // ═══════════════════════════════════════════════════════════════════════════
    // 6. Mahalanobis corner mapping
    // ═══════════════════════════════════════════════════════════════════════════
    inline std::pair<float, float> mahalanobis_corner_mapping(
        float angle_x, float angle_y,
        float screen_x, float screen_y,
        int screen_w, int screen_h,
        float ellipse_nd)
    {
        float nx = screen_x / screen_w;
        float ny = screen_y / screen_h;

        float d_tl = std::sqrt(nx * nx + ny * ny);
        float d_tr = std::sqrt((1 - nx) * (1 - nx) + ny * ny);
        float d_bl = std::sqrt(nx * nx + (1 - ny) * (1 - ny));
        float d_br = std::sqrt((1 - nx) * (1 - nx) + (1 - ny) * (1 - ny));
        float min_cd = std::min({ d_tl, d_tr, d_bl, d_br });

        if (min_cd >= 0.3f || ellipse_nd < 0.7f)
            return { angle_x, angle_y };

        float corner_strength = std::pow(1.0f - min_cd / 0.3f, 0.8f);

        // Closest corner direction
        std::pair<float, float> corners[4] = {
            { std::atan2(ny, nx),       d_tl },
            { std::atan2(ny, 1.0f - nx),  d_tr },
            { std::atan2(1.0f - ny, nx),  d_bl },
            { std::atan2(1.0f - ny, 1 - nx),d_br }
        };
        auto [cc_angle, _] = *std::min_element(corners, corners + 4,
            [](auto& a, auto& b) { return a.second < b.second; });

        float angle_dir = std::atan2(angle_y, angle_x);
        float to_corner = std::abs(angle_dir - cc_angle);
        to_corner = std::min(to_corner, 2.0f * (float)std::numbers::pi - to_corner);
        float tc_norm = to_corner / ((float)std::numbers::pi / 2.0f);

        float var_along = 0.5f + tc_norm * 0.5f;
        float var_perp = 1.5f - tc_norm * 0.5f;
        float maha_scale = std::sqrt(var_along / var_perp);

        float amag = std::sqrt(angle_x * angle_x + angle_y * angle_y);
        if (amag < 1e-6f) return { angle_x, angle_y };

        float correction = 1.0f + corner_strength * (maha_scale - 1.0f);
        correction = std::clamp(correction, 0.7f, 1.4f);
        return { angle_x * correction, angle_y * correction };
    }


    // ═══════════════════════════════════════════════════════════════════════════
    // 7. Lighting normalisation (CLAHE + adaptive gamma in LAB)
    // ═══════════════════════════════════════════════════════════════════════════
    // Returns {normalised_frame, brightness [0,1]}.
    inline std::pair<cv::Mat, float> normalize_lighting(const cv::Mat& bgr)
    {
        cv::Mat lab;
        cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);

        std::vector<cv::Mat> ch;
        cv::split(lab, ch);

        float brightness = (float)cv::mean(ch[0])[0] / 255.0f;

        auto clahe = cv::createCLAHE(2.0, { 8, 8 });
        clahe->apply(ch[0], ch[0]);

        // Adaptive gamma on L channel
        if (brightness < 0.3f || brightness > 0.7f) {
            float gamma = (brightness < 0.3f)
                ? 1.0f + (0.3f - brightness) * 0.5f
                : 1.0f + (brightness - 0.7f) * 0.4f;
            float inv_g = (brightness < 0.3f) ? 1.0f / gamma : gamma;

            cv::Mat lf; ch[0].convertTo(lf, CV_32F, 1.0 / 255.0);
            cv::pow(lf, inv_g, lf);
            lf.convertTo(ch[0], CV_8U, 255.0);
        }

        cv::Mat merged;
        cv::merge(ch, merged);
        cv::Mat result;
        cv::cvtColor(merged, result, cv::COLOR_Lab2BGR);
        return { result, brightness };
    }

} // namespace eyetracker::gaze