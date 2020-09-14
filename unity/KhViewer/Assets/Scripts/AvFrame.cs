using System;

// A class that contains a pointer to a FFmpegFrame in KinectToHololensPlugin.dll.
public class AVFrame
{
    public IntPtr Ptr { get; private set; }

    public AVFrame(IntPtr ptr)
    {
        Ptr = ptr;
    }

    ~AVFrame()
    {
        Plugin.delete_av_frame(Ptr);
    }
}
