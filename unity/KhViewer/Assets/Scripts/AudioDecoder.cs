using System;
using System.Runtime.InteropServices;

public class AudioDecoder
{
    private IntPtr ptr;

    public AudioDecoder(int sampleRate, int channelCount)
    {
        ptr = Plugin.create_audio_decoder(sampleRate, channelCount);
    }

    ~AudioDecoder()
    {
        Plugin.destroy_audio_decoder(ptr);
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

            
            pcmFrameSize = Plugin.audio_decoder_decode(ptr, nativeOpusFrame, opusFrame.Length, nativePcm, frameSize);
            Marshal.Copy(nativePcm, pcm, 0, pcm.Length);
            
            Marshal.FreeHGlobal(nativeOpusFrame);
        }
        else
        {
            pcmFrameSize = Plugin.audio_decoder_decode(ptr, IntPtr.Zero, 0, nativePcm, frameSize);
        }
        Marshal.FreeHGlobal(nativePcm);

        return pcmFrameSize;
    }
}
