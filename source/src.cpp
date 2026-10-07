#include "src.h"

int CameraSource::setFormat() {
    v4l2_format fmt{};
    memset(&fmt, 0, sizeof(fmt));
    
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) return -1; 

    width = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;
    v4l2_bytesperline = fmt.fmt.pix.bytesperline;
    v4l2_sizeimage = fmt.fmt.pix.sizeimage;

    return 0;
}

int CameraSource::requestBuffers() {
    //single plane API
    v4l2_requestbuffers reqbuf{};
    memset(&reqbuf, 0, sizeof (reqbuf));

    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_DMABUF;
    reqbuf.count = MAX_CAM_BUFS;

    int ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);

    if (ret || reqbuf.count < MAX_CAM_BUFS) {
        return -1;
    }

    return 0;
}

CameraSource::CameraSource(const std::string& device, int width, int height, uint32_t pixel_format) 
    : device(device), width(width), height(height), pixel_format(pixel_format) {
}

int CameraSource::configure() {
    fd = ::open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) return -1;
    if (setFormat()) return -1;
    return 0;
}


int CameraSource::start(BufferPool& pool, SPSC_q<CamFrame, cam_queue_size>& to_decode, 
    SPSC_q<int, cam_queue_size>& cam_done) {
    
    this->pool = &pool;
    this->to_decode = &to_decode;
    this->cam_done =  &cam_done;

    if (requestBuffers()) return -1;

    for (int i = 0; i < MAX_CAM_BUFS; ++i) {
        if (queueBuffer(i)) return -1;
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    return ioctl(fd, VIDIOC_STREAMON, &type);
}

CameraSource::~CameraSource() {

    if (fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        ::close(fd);
        fd = -1;
    }

}

int CameraSource::queueBuffer(int index) {

    if (index >= MAX_CAM_BUFS) return -1;
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.index = index;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.m.fd = pool->fd(index);

    errno = 0;
    int ret = ioctl(fd, VIDIOC_QBUF, &buf);

    return ret;
}

void CameraSource::captureLoop(std::stop_token st) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (!st.stop_requested()) {

        //first check if there are buffers to return back

        cam_done->consume(cam_queue_size, [this](int& id){queueBuffer(id);});
        int ret = poll(&pfd, 1, POLL_TIME);

        if (ret <= 0) {
            if (ret < 0 && errno != EINTR) break;
            continue;
        }
        
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_DMABUF;

        ret = ioctl(fd, VIDIOC_DQBUF, &buf);

        if (ret < 0) {
            if (errno == EAGAIN) continue;
            break;
        }

        to_decode->push({buf.index, buf.bytesused});
    }

    to_decode->close();
}

std::size_t CameraSource::sizeImage() const {
    return v4l2_sizeimage;
}