#ifndef MPP_DECODER_H
#define MPP_DECODER_H

#include "spsc_q.h"
#include "mpp_allocator.h"
#include "constants.h"
#include "structs.h"
#include <thread>
#include <rockchip/rk_mpi.h>

class MppDecoder {
public:
    MppDecoder(BufferPool& decode_pool, BufferPool& camera_pool,
        SPSC_q<CamFrame, cam_queue_size>& to_decode, SPSC_q<int, decoder_queue_size>& decode_pool_free,
        SPSC_q<int, decoder_queue_size>& ready, SPSC_q<int, cam_queue_size>& cam_done);
    ~MppDecoder();
    MppDecoder(const MppDecoder&) = delete;
    MppDecoder(MppDecoder&&) = delete;
    MppDecoder& operator=(const MppDecoder&) = delete;
    MppDecoder& operator=(const MppDecoder&&) = delete;

    int init();
    void deinit();

    int decode(MppBuffer jpeg_buf, std::size_t jpeg_size, MppFrame out);
    void decodeLoop(std::stop_token st);

    int get_w() const;
    int get_h() const;
    int get_hor() const;
    int get_ver() const;

private:
    BufferPool* decode_pool{nullptr};
    BufferPool* camera_pool{nullptr};
    SPSC_q<CamFrame, cam_queue_size>* to_decode{nullptr}; //cam_pool
    SPSC_q<int, decoder_queue_size>* decode_pool_free{nullptr}; //cam done but for decoder, dec_pool
    SPSC_q<int, decoder_queue_size>* ready{nullptr}; //dec_pool
    SPSC_q<int, cam_queue_size>* cam_done{nullptr}; //cam_pool

    MppCtx ctx = nullptr;
    MppApi* mpi = nullptr;

    std::vector<MppFrame> frames{};

    int w{0};
    int h{0};
    int hor{0};
    int ver{0};

    bool success{false};
};

#endif // MPP_DECODER_H