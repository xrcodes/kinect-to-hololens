using System;
using System.Runtime.InteropServices;

public class OpusDecoder
{
    private IntPtr ptr;

    public OpusDecoder(int sampleRate, int channelCount)
    {
        ptr = Plugin.create_opus_decoder(sampleRate, channelCount);
    }

    ~OpusDecoder()
    {
        Plugin.delete_opus_decoder(ptr);
    }

    //public int Decode(byte[] opusFrame, ref IntPtr pcm, int frameSize)
    public int Decode(byte[] opusFrame, float[] pcm, int frameSize)
    {
        IntPtr nativePcm = Marshal.AllocHGlobal(sizeof(float) * pcm.Length);
        int pcmFrameSize;
        if (opusFrame != null)
        {
            IntPtr nativeOpusFrame = Marshal.AllocHGlobal(opusFrame.Length);
            Marshal.Copy(opusFrame, 0, nativeOpusFrame, opusFrame.Length);

            
            pcmFrameSize = Plugin.opus_decoder_decode(ptr, nativeOpusFrame, opusFrame.Length, nativePcm, frameSize);
            Marshal.Copy(nativePcm, pcm, 0, pcm.Length);
            
            Marshal.FreeHGlobal(nativeOpusFrame);
        }
        else
        {
            pcmFrameSize = Plugin.opus_decoder_decode(ptr, IntPtr.Zero, 0, nativePcm, frameSize);
        }
        Marshal.FreeHGlobal(nativePcm);

        return pcmFrameSize;
    }
}
