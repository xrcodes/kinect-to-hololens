#include "kh_vp8.h"

#include <iostream>
#include <libavformat/avformat.h>

namespace kh
{
class Vp8Decoder::CodecContext
{
public:
    CodecContext(AVCodec* codec)
        : codec_context_{avcodec_alloc_context3(codec)}
    {
        if (!codec_context_)
            throw std::exception("avcodec_alloc_context3 failed.");
    }
    CodecContext(const CodecContext&) = delete;
    CodecContext& operator=(const CodecContext&) = delete;

    ~CodecContext()
    {
        if (codec_context_)
            avcodec_free_context(&codec_context_);
    }

    AVCodecContext* get() noexcept { return codec_context_; }

private:
    AVCodecContext* codec_context_;
};

class Vp8Decoder::CodecParserContext
{
public:
    CodecParserContext(int codec_id)
        : codec_parser_context_{av_parser_init(codec_id)}
    {
        if (!codec_parser_context_)
            throw std::exception("av_parser_init failed from CodecParserContext::CodecParserContext");
    }
    CodecParserContext(const CodecParserContext&) = delete;
    CodecParserContext& operator=(const CodecParserContext&) = delete;

    ~CodecParserContext()
    {
        if (codec_parser_context_)
            av_parser_close(codec_parser_context_);
    }

    AVCodecParserContext* get() noexcept { return codec_parser_context_; }

private:
    AVCodecParserContext* codec_parser_context_;
};

class Vp8Decoder::Packet
{
public:
    Packet()
        : packet_{av_packet_alloc()}
    {
        if (!packet_)
            throw std::exception("av_packet_alloc failed.");
    }
    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;

    ~Packet()
    {
        if (packet_)
            av_packet_free(&packet_);
    }

    AVPacket* get() noexcept { return packet_; }

private:
    AVPacket* packet_;
};

namespace
{
AVCodec* find_codec(AVCodecID id)
{
    auto codec = avcodec_find_decoder(AV_CODEC_ID_VP8);
    if (!codec)
        throw std::exception("avcodec_find_decoder failed.");
    return codec;
}

// A helper function for Vp8Decoder::decode() that feeds frames of packet into decoder_frames.
void decode_packet(std::vector<FFmpegFrame>& decoder_frames, AVCodecContext* codec_context, AVPacket* packet)
{
    if (avcodec_send_packet(codec_context, packet) < 0)
        throw std::exception("Error from avcodec_send_packet.");

    while (true) {
        auto av_frame = av_frame_alloc();
        if (!av_frame)
            throw std::exception("Error from av_frame_alloc.");

        av_frame->format = AV_PIX_FMT_YUV420P;

        int receive_frame_result = avcodec_receive_frame(codec_context, av_frame);
        if (receive_frame_result == AVERROR(EAGAIN) || receive_frame_result == AVERROR_EOF) {
            return;
        } else if (receive_frame_result < 0) {
            throw std::exception("Error from avcodec_send_packet.");
        }

        fflush(stdout);
        decoder_frames.emplace_back(av_frame);
    }

    return;
}
}

Vp8Decoder::Vp8Decoder()
    : codec_context_{std::make_shared<CodecContext>(find_codec(AV_CODEC_ID_VP8))}
    , codec_parser_context_{std::make_shared<CodecParserContext>(codec_context_->get()->codec->id)}
    , packet_{std::make_shared<Packet>()}
{
    if (avcodec_open2(codec_context_->get(), codec_context_->get()->codec, nullptr) < 0)
        throw std::exception("avcodec_open2 failed.");
}


// Decode frames in vp8_frame_data.
FFmpegFrame Vp8Decoder::decode(gsl::span<const std::byte> vp8_frame)
{
    std::vector<FFmpegFrame> decoder_frames;
    /* use the parser to split the data into frames */
    size_t data_size = vp8_frame.size();
    // Adding buffer padding is important!
    // Removing this will result in crashes in some cases.
    // When the crash happens, it happens in av_parser_parse2().
    std::unique_ptr<uint8_t> padded_data(new uint8_t[data_size + AV_INPUT_BUFFER_PADDING_SIZE]);
    memcpy(padded_data.get(), vp8_frame.data(), data_size);
    memset(padded_data.get() + data_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    uint8_t* data = padded_data.get();

    while (data_size > 0) {
        // Returns the number of bytes used.
        const int size = av_parser_parse2(codec_parser_context_->get(),
            codec_context_->get(), &packet_->get()->data, &packet_->get()->size,
            data, static_cast<int>(data_size), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

        if (size < 0)
            throw std::exception("An error from av_parser_parse2.");
        
        data += size;
        data_size -= size;

        if (packet_->get()->size)
            decode_packet(decoder_frames, codec_context_->get(), packet_->get());
    }

    if (decoder_frames.size() != 1)
        throw std::exception("More or less than one frame found in Vp8Decoder::decode.");

    return std::move(decoder_frames[0]);
}
}