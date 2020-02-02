using System;

// A class that contains a pointer to a TrvlDecoder in KinectToHololensPlugin.dll.
public class TrvlDecoder
{
    private IntPtr ptr;

    public TrvlDecoder(int frameSize)
    {
        ptr = Plugin.create_trvl_decoder(frameSize);
    }

    ~TrvlDecoder()
    {
        Plugin.delete_trvl_decoder(ptr);
    }

    public TrvlFrame Decode(IntPtr framePtr, bool keyframe)
    {
        return new TrvlFrame(Plugin.trvl_decoder_decode(ptr, framePtr, keyframe));
    }
}
