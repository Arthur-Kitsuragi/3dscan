#include "constants.h"
#include "mpp_allocator.h"
#include "mpp_decoder.h"
#include "spsc_q.h"
#include "src.h"
#include "structs.h"
#include "worker.h"
#include "func.h"
#include <opencv2/opencv.hpp>

int main() {

    ImageConfig cfg{480, 1280, ImageFormat::MJPG, MPP_ALIGN(1280,16), MPP_ALIGN(480,16)};

    MppAllocator* allocator{nullptr};
    int ret = MppAllocator::instance(&allocator);

    CameraSource cam("/dev/video1", cfg);

    cam.configure();

    allocator->alloc("camera", {cam.sizeImage(), MAX_CAM_BUFS});
    allocator->alloc("decode", {static_cast<std::size_t>(cfg.hor) * cfg.ver * 4,
        MAX_CAM_BUFS});
    allocator->alloc("calibrate", {static_cast<std::size_t>(cfg.hor) * cfg.ver, MAX_CAM_BUFS});

    SPSC_q<CamFrame, cam_queue_size> to_decode{};
    SPSC_q<int, cam_queue_size> cam_done{};
    SPSC_q<int, decoder_queue_size> decode_pool_free{};
    SPSC_q<int, decoder_queue_size> to_calibrate{};
    SPSC_q<int, calibrator_queue_size> calibrate_pool_free{};
    SPSC_q<int, calibrator_queue_size> ready{};
    
    int s = cam.start(*allocator->get("camera"), to_decode, cam_done); //init

    MppDecoder dec(*allocator->get("decode"), *allocator->get("camera"),
        to_decode, decode_pool_free, to_calibrate, cam_done);

    int dec_ret = dec.init();

    Calibrator calibrator("assets/calib.yml", *allocator->get("calibrate"), 
        *allocator->get("decode"), to_calibrate, calibrate_pool_free, ready, cfg);
    
    calibrator.init();
    worker cam_worker([&](std::stop_token st){cam.captureLoop(st);});
    worker dec_worker([&](std::stop_token st){dec.decodeLoop(st);});
    worker calib_worker([&](std::stop_token st){calibrator.calibrateLoop(st);});

    /*
        debug imshow worker 
    */

    BufferPool* calibrate_pool = allocator->get("calibrate");

    while (true) {
        int idx{-1};
        ready.wait_pop([&](int& i){idx = i;});
        
        auto* base = static_cast<uint8_t*>(calibrate_pool->ptr(idx));

        cv::Mat frame(cfg.height, cfg.width, CV_8UC1, base, cfg.width);

        cv::imshow("frame", frame);
        cv::waitKey(1);

        decode_pool_free.push(idx);
    }
}

//3840x1080