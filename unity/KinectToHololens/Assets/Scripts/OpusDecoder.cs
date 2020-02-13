using System;
using System.Runtime.InteropServices;

public class OpusDecoder
{
    private IntPtr ptr;

    public OpusDecoder(int sampleRate, int channels)
    {
        ptr = Plugin.create_opus_decoder(sampleRate, channels);
    }

    ~OpusDecoder()
    {
        Plugin.destroy_opus_decoder(ptr);
    }

    public OpusFrame Decode(byte[] packet, int startIndex, int packetSize, int frameSize, int channels)
    {
        IntPtr opusPacketBytes = Marshal.AllocHGlobal(packet.Length);
        Marshal.Copy(packet, startIndex, opusPacketBytes, packetSize);
        var opusFrame = new OpusFrame(Plugin.opus_decoder_decode(ptr, opusPacketBytes, packetSize, frameSize, channels));
        Marshal.FreeHGlobal(opusPacketBytes);

        return opusFrame;
    }
}
