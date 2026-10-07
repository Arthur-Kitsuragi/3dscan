#include "mpp_decoder.h"

MppDecoder::MppDecoder(BufferPool& decode_pool, BufferPool& camera_pool,
    SPSC_q<CamFrame, cam_queue_size>& to_decode, SPSC_q<int, decoder_queue_size>& decode_pool_free,
    SPSC_q<int, decoder_queue_size>& ready, SPSC_q<int, cam_queue_size>& cam_done) :
    decode_pool(&decode_pool), camera_pool(&camera_pool), to_decode(&to_decode),
    decode_pool_free(&decode_pool_free), ready(&ready), cam_done(&cam_done) {
}
    
int MppDecoder::init() {

    MPP_RET ret = mpp_create(&ctx, &mpi);

    if (ret != MPP_OK) {
        return -1;
    }

    ret = mpp_init(ctx, MPP_CTX_DEC, MPP_VIDEO_CodingMJPEG);

    if (ret != MPP_OK) {
        deinit();
        return -1;
    }

    MppFrameFormat out_fmt = MPP_FMT_YUV420SP;
    ret = mpi->control(ctx, MPP_DEC_SET_OUTPUT_FORMAT, &out_fmt);

    if (ret != MPP_OK) {
        deinit();
        return -1;
    }

    frames.resize(decode_pool->count(), nullptr);

    for (int i = 0; i < decode_pool->count(); ++i) {
        MPP_RET ret = mpp_frame_init(&frames[i]);
        if (ret != MPP_OK) return -1;

        mpp_frame_set_buffer(frames[i], decode_pool->buffer(i));
    }

    for (int i = 0; i < decode_pool->count(); ++i) {
        decode_pool_free->push(i);
    }

    return 0;
}

void MppDecoder::deinit() {
    if (ctx) {
        mpp_destroy(ctx);
        ctx = nullptr;
        mpi = nullptr;

        for (int i = 0; i < decode_pool->count(); ++i) {
            mpp_frame_deinit(&frames[i]);
            frames[i] = nullptr;
        }
    }
}

MppDecoder::~MppDecoder() {
    deinit();
}

int MppDecoder::decode(MppBuffer jpeg_buf, std::size_t jpeg_size, MppFrame out) {

    MppPacket packet = nullptr;
    MppFrame frame_ret = nullptr;
    MppPacket packet_ret = nullptr;

    MPP_RET ret = mpp_packet_init_with_buffer(&packet, jpeg_buf);

    if (ret) {
        return -1;
    }

    mpp_packet_set_length(packet, jpeg_size);

    MppMeta meta = mpp_packet_get_meta(packet);

    if (meta)
        mpp_meta_set_frame(meta, KEY_OUTPUT_FRAME, out);

    ret = mpi->decode_put_packet(ctx, packet);

    if (ret) 
        goto DONE;

    ret = mpi->decode_get_frame(ctx, &frame_ret);

    if (ret || !frame_ret) {
        goto DONE;
    }

    if (!success) {
        w = mpp_frame_get_width(frame_ret);
        h = mpp_frame_get_height(frame_ret);
        hor = mpp_frame_get_hor_stride(frame_ret);
        ver = mpp_frame_get_ver_stride(frame_ret);
        success = true;
    }

    meta = mpp_frame_get_meta(out);
    
    if (meta) {
        ret = mpp_meta_get_packet(meta, KEY_INPUT_PACKET, &packet_ret);
        if (ret || !packet_ret || packet_ret != packet) {
            goto DONE;
        }
    }

DONE:
    mpp_packet_deinit(&packet);

    return ret;
}

void MppDecoder::decodeLoop(std::stop_token st) {

    while (!st.stop_requested()) {

        CamFrame frame{};

        if (!to_decode->wait_pop([&](CamFrame& f){
            frame = std::move(f);
        })) {
            break;
        }
        
        int idx{0};

        if (!decode_pool_free->pop([&](int& i){
            idx = std::move(i);
        })) {
            cam_done->push(frame.index);
            continue;
        }

        if (decode(camera_pool->buffer(frame.index), frame.size, frames[idx])) {
            cam_done->push(frame.index);
            decode_pool_free->push(idx);
            continue;
        }

        cam_done->push(frame.index);
        ready->push(idx);
    }

    //close?
}

int MppDecoder::get_w() const {
    return w;
}

int MppDecoder::get_h() const {
    return h;
}

int MppDecoder::get_hor() const {
    return hor;
}

int MppDecoder::get_ver() const {
    return ver;
}