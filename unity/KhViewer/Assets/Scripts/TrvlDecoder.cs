using System;
using System.Runtime.InteropServices;

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

    public TrvlFrame Decode(byte[] frame, bool keyframe)
    {
        IntPtr bytes = Marshal.AllocHGlobal(frame.Length);
        Marshal.Copy(frame, 0, bytes, frame.Length);
        var trvlFrame = new TrvlFrame(Plugin.trvl_decoder_decode(ptr, bytes, frame.Length, keyframe));
        Marshal.FreeHGlobal(bytes);

        return trvlFrame;
    }
}
