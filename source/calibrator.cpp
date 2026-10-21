#include "calibrator.h"

int Calibrator::load_stereo_maps(const fs::path& path) {
    if (!fs::is_regular_file(path)) return -1;

    cv::FileStorage fs(path.string(), cv::FileStorage::READ);
    cv::FileNode node = fs["baseline_mm"];

    fs["baseline_mm"] >> baseline_mm;
    // fs["image_width"]  >> image_width;
    // fs["image_height"] >> image_height;
    fs["K1"] >> K1;
    fs["D1"] >> D1;
    fs["K2"] >> K2;
    fs["D2"] >> D2;
    fs["R1"] >> R1;
    fs["R2"] >> R2;
    fs["P1"] >> P1;
    fs["P2"] >> P2;
    fs["Q"]  >> Q;

    cv::Size size(cfg.width / 2, cfg.height);
    cv::initUndistortRectifyMap(K1, D1, R1, P1, size, CV_32FC1, map1x, map1y);
    cv::initUndistortRectifyMap(K2, D2, R2, P2, size, CV_32FC1, map2x, map2y);

    return 0;
}

Calibrator::Calibrator(const std::string& path, BufferPool& calibrator_pool, BufferPool& decode_pool, 
    SPSC_q<int, decoder_queue_size>& to_calibrate, SPSC_q<int, calibrator_queue_size>& calibrate_pool_free,
    SPSC_q<int, calibrator_queue_size>& ready, ImageConfig& cfg
): path(path), calibrator_pool(&calibrator_pool), decode_pool(&decode_pool), to_calibrate(&to_calibrate),
    calibrate_pool_free(&calibrate_pool_free), ready(&ready), cfg(cfg) {
}

int Calibrator::init() {
    return load_stereo_maps(path);
}

void Calibrator::calibrateLoop(std::stop_token st) {

    while (!st.stop_requested()) {

        int idx{};

        if (!to_calibrate->wait_pop([&](int& index){
            idx = std::move(index);
        })) {
            break;
        }

        uint8_t* base = static_cast<uint8_t*>(decode_pool->ptr(idx));
        cv::Mat y(cfg.height, cfg.width, CV_8UC1, base, cfg.hor);

        cv::Mat left = y(cv::Rect(0, 0, cfg.width / 2, cfg.height));
        cv::Mat right = y(cv::Rect(cfg.width/2, 0, cfg.width/2, cfg.height));

        int idx_{};

        if (!calibrate_pool_free->pop([&](int& index){
            idx_ = std::move(index);
        })) {
            continue;
        }

        uint8_t* out_ptr = static_cast<uint8_t*>(calibrator_pool->ptr(idx_));
        cv::Mat left_r(cfg.height, cfg.width / 2, CV_8UC1, out_ptr, cfg.width);
        cv::Mat right_r(cfg.height, cfg.width / 2, CV_8UC1, out_ptr + cfg.width / 2, cfg.width);
        cv::remap(left, left_r, map1x, map1y, cv::INTER_LINEAR);
        cv::remap(right, right_r, map2x, map2y, cv::INTER_LINEAR);

        ready->push(idx_);
    }
}