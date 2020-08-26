using System;
using System.Runtime.InteropServices;

// A class that contains a pointer to a Vp8Decoder in KinectToHololensPlugin.dll.
public class Vp8Decoder
{
    private IntPtr ptr;

    public Vp8Decoder()
    {
        ptr = Plugin.create_vp8_decoder();
    }

    ~Vp8Decoder()
    {
        Plugin.delete_vp8_decoder(ptr);
    }

    public FFmpegFrame Decode(byte[] frame)
    {
        IntPtr bytes = Marshal.AllocHGlobal(frame.Length);
        Marshal.Copy(frame, 0, bytes, frame.Length);
        var ffmpegFrame =  new FFmpegFrame(Plugin.vp8_decoder_decode(ptr, bytes, frame.Length));
        Marshal.FreeHGlobal(bytes);

        return ffmpegFrame;
    }
}
