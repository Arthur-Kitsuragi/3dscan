#ifndef STRUCTS_H
#define STRUCTS_H

#include <cstddef>
#include <linux/videodev2.h>

struct CamFrame {
    int index;
    std::size_t size;
};

enum class ImageFormat: uint32_t {
    MJPG = V4L2_PIX_FMT_MJPEG
};

#endif // STRUCTS_H