using System;

public class AVFrame
{
    public IntPtr Ptr { get; private set; }

    public AVFrame(IntPtr ptr)
    {
        Ptr = ptr;
    }

    ~AVFrame()
    {
        TelepresenceToolkitPlugin.delete_av_frame(Ptr);
    }
}
