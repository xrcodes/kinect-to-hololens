using System;

// A wrapper of a C++ vector for depth pixels.
public class TrvlFrame
{
    public IntPtr Ptr { get; private set; }

    public TrvlFrame(IntPtr ptr)
    {
        Ptr = ptr;
    }

    ~TrvlFrame()
    {
        Plugin.delete_depth_pixels(Ptr);
    }
}
