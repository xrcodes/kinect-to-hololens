#include "kh_vp8_encoder2.h"

static std::vector<std::byte> encode_frame(AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt)
{
    int ret;

    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %3d\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    std::vector<std::byte> bytes;
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            //return;
            exit(1);
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }

        if (!bytes.empty())
            throw std::exception("Multiple packets found from a Vp8 frame in Vp8Encoder2...");

        printf("Write packet %3d (size=%5d)\n", pkt->pts, pkt->size);

        bytes.resize(pkt->size);
        memcpy(bytes.data(), (std::byte*)pkt->data, pkt->size);

        av_packet_unref(pkt);
    }
}

namespace kh
{
Vp8Encoder2::Vp8Encoder2(int width, int height)
    : frame_index_{0}
{
    auto codec = avcodec_find_encoder(AV_CODEC_ID_VP8);
    if (!codec) {
        fprintf(stderr, "Codec VP8 not found");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);

    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = width;
    c->height = height;
    /* frames per second */
    c->time_base = AVRational{1, 25};
    c->framerate = AVRational{25, 1};

    /* open it */
    auto ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        //fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }
}

Vp8Encoder2::~Vp8Encoder2()
{
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
}

std::vector<std::byte> Vp8Encoder2::encode(const YuvFrame& yuv_image, bool keyframe)
{
    /* make sure the frame data is writable */
    auto ret = av_frame_make_writable(frame);
    if (ret < 0)
        exit(1);

    /* prepare a dummy image */
    /* Y */
    for (int y = 0; y < c->height; y++) {
        for (int x = 0; x < c->width; x++) {
            frame->data[0][y * frame->linesize[0] + x] = x + y + frame_index_ * 3;
        }
    }

    /* Cb and Cr */
    for (int y = 0; y < c->height / 2; y++) {
        for (int x = 0; x < c->width / 2; x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128 + y + frame_index_ * 2;
            frame->data[2][y * frame->linesize[2] + x] = 64 + x + frame_index_ * 5;
        }
    }

    frame->pts = frame_index_++;

    /* encode the image */
    return encode_frame(c, frame, pkt);

    /* flush the encoder */
    //encode(c, NULL, pkt, f);
}
}
