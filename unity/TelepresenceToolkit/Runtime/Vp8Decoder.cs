using System;
using System.Runtime.InteropServices;

// A class that contains a pointer to a Vp8Decoder in KinectToHololensPlugin.dll.
public class Vp8Decoder
{
    private IntPtr ptr;

    public Vp8Decoder()
    {
        ptr = TelepresenceToolkitPlugin.create_vp8_decoder();
    }

    ~Vp8Decoder()
    {
        TelepresenceToolkitPlugin.delete_vp8_decoder(ptr);
    }

    public AVFrame Decode(byte[] frame)
    {
        GCHandle handle = GCHandle.Alloc(frame, GCHandleType.Pinned);
        IntPtr bytes = handle.AddrOfPinnedObject();
        var avFrame = new AVFrame(TelepresenceToolkitPlugin.vp8_decoder_decode(ptr, bytes, frame.Length));
        handle.Free();

        return avFrame;
    }
}
