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

    public IntPtr Decode(IntPtr framePtr)
    {
        return Plugin.trvl_decoder_decode(ptr, framePtr);
    }
}
