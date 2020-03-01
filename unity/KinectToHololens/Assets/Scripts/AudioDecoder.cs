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

    public int Decode(byte[] opusFrame, ref IntPtr pcm, int frameSize)
    {
        if (opusFrame != null)
        {
            IntPtr opusFrameBytes = Marshal.AllocHGlobal(opusFrame.Length);
            Marshal.Copy(opusFrame, 0, opusFrameBytes, opusFrame.Length);
            int pcmFrameSize = Plugin.audio_decoder_decode(ptr, opusFrameBytes, opusFrame.Length, pcm, frameSize);
            Marshal.FreeHGlobal(opusFrameBytes);
            return pcmFrameSize;
        }
        else
        {
            int pcmFrameSize = Plugin.audio_decoder_decode(ptr, IntPtr.Zero, 0, pcm, frameSize);
            return pcmFrameSize;
        }
    }
}
