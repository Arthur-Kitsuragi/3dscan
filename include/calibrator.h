#ifndef CALIBRATOR_H
#define CALIBRATOR_H

#include <opencv2/opencv.hpp>
#include <string>
#include "mpp_allocator.h"
#include <filesystem>
#include <utility>
#include "spsc_q.h"
#include "structs.h"
#include "constants.h"
#include <thread>

namespace fs = std::filesystem;

class Calibrator {
public:
    Calibrator(const std::string& path, BufferPool& calibrator_pool, BufferPool& decode_pool, 
        SPSC_q<int, decoder_queue_size>& to_calibrate, SPSC_q<int, calibrator_queue_size>& calibrate_pool_free,
        SPSC_q<int, calibrator_queue_size>& ready, ImageConfig& cfg
    );
    ~Calibrator() = default;
private:
    int load_stereo_maps(const fs::path& path);
    int init();

    std::string path{};
    double baseline_mm{0};
    ImageConfig& cfg;
    // int image_width{0};
    // int image_height{0};

    cv::Mat K1{}, D1{}, K2{}, D2{};
    cv::Mat R1{}, R2{}, P1{}, P2{}, Q{};
    cv::Mat map1x{}, map1y{}, map2x{}, map2y{};

    BufferPool* calibrator_pool{nullptr};
    BufferPool* decode_pool{nullptr};

    SPSC_q<int, decoder_queue_size>* to_calibrate{nullptr}; 
    SPSC_q<int, calibrator_queue_size>* decode_pool_free{nullptr}; 
    SPSC_q<int, calibrator_queue_size>* ready{nullptr}; 

    void calibrateLoop(std::stop_token st);
};

#endif // CALIBRATOR_H