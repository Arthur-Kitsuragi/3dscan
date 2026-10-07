#ifndef SRC_H
#define SRC_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <linux/videodev2.h>
#include <array>
#include <cstring>
#include "spsc_q.h"
#include "mpp_allocator.h"
#include <thread>
#include <poll.h>
#include "constants.h"
#include "structs.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cerrno>
#include <string>

#define MAX_CAM_BUFS 8
#define POLL_TIME 100

class CameraSource {
public:

    ~CameraSource();
    CameraSource(const CameraSource&) = delete;
    CameraSource& operator=(const CameraSource&) = delete;
    CameraSource(CameraSource&&) = delete;
    CameraSource& operator= (CameraSource&&) = delete;
    CameraSource(const std::string& device, int width, int height, uint32_t pixel_format);
    int start(BufferPool& pool, SPSC_q<CamFrame, cam_queue_size>& to_decode, 
        SPSC_q<int, cam_queue_size>& cam_done);
    void captureLoop(std::stop_token st);
    int configure();
    std::size_t sizeImage() const;

private:
    int setFormat();
    int requestBuffers();
    int queueBuffer(int index);
    
    int width{0};
    int height{0};
    int v4l2_bytesperline{0};
    int v4l2_sizeimage{0};

    int fd{-1}; //descriptor for camera device
    std::string device{};
    uint32_t pixel_format{0};
    
    BufferPool* pool{nullptr};
    SPSC_q<CamFrame, cam_queue_size>* to_decode{nullptr};
    SPSC_q<int, cam_queue_size>* cam_done{nullptr};
};

#endif // SRC_H
