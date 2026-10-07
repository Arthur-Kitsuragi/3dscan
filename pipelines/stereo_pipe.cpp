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

    constexpr int W = 1280;
    constexpr int H = 480;
    bool success{false};

    MppAllocator* allocator{nullptr};
    int ret = MppAllocator::instance(&allocator);

    CameraSource cam("/dev/video1", W, H, 
        static_cast<uint32_t>(ImageFormat::MJPG));

    cam.configure();
    
    RK_U32 hor_stride = MPP_ALIGN(W, 16);
    RK_U32 ver_stride = MPP_ALIGN(H, 16);

    allocator->alloc("camera", {cam.sizeImage(), MAX_CAM_BUFS});
    allocator->alloc("decode", {static_cast<std::size_t>(hor_stride) * ver_stride * 4,
        MAX_CAM_BUFS});

    SPSC_q<CamFrame, cam_queue_size> to_decode{};
    SPSC_q<int, cam_queue_size> cam_done{};
    SPSC_q<int, decoder_queue_size> decode_pool_free{};
    SPSC_q<int, decoder_queue_size> ready{};
    
    int s = cam.start(*allocator->get("camera"), to_decode, cam_done);

    MppDecoder dec(*allocator->get("decode"), *allocator->get("camera"),
        to_decode, decode_pool_free, ready, cam_done);

    int dec_ret = dec.init();

    worker cam_worker([&](std::stop_token st){cam.captureLoop(st);});
    worker dec_worker([&](std::stop_token st){dec.decodeLoop(st);});


    /*
        debug imshow worker 
    */

    BufferPool* decode_pool = allocator->get("decode");

    while (true) {
        int idx{-1};
        ready.wait_pop([&](int& i){idx = i;});
        
        auto* base = static_cast<uint8_t*>(decode_pool->ptr(idx));

        int w = dec.get_w(), h = dec.get_h();
        int hor = dec.get_hor(), ver = dec.get_ver();

        //fprintf(stderr, "w=%d h=%d hor=%d ver=%d\n", w, h, hor, ver);

        cv::Mat y(h, w, CV_8UC1, base, hor);
        cv::Mat uv(h / 2, w / 2, CV_8UC2, base + hor * ver, hor);

        cv::Mat bgr;
        cv::cvtColorTwoPlane(y, uv, bgr, cv::COLOR_YUV2BGR_NV12);

        //cv::imwrite("/tmp/frame.png", bgr);

        cv::imshow("frame", bgr);
        cv::waitKey(1);

        decode_pool_free.push(idx);
    }
}

//3840x1080