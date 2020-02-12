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

    public OpusFrame Decode(byte[] packet, int frameSize, int channels)
    {
        IntPtr opusPacketBytes = Marshal.AllocHGlobal(packet.Length);
        Marshal.Copy(packet, 0, opusPacketBytes, packet.Length);
        var opusFrame = new OpusFrame(Plugin.opus_decoder_decode(ptr, opusPacketBytes, packet.Length, frameSize, channels));
        Marshal.FreeHGlobal(opusPacketBytes);

        return opusFrame;
    }
}
