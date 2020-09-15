using System;

public class DepthPixels
{
    public IntPtr Ptr { get; private set; }

    public DepthPixels(IntPtr ptr)
    {
        Ptr = ptr;
    }

    ~DepthPixels()
    {
        TelepresenceToolkitPlugin.delete_depth_pixels(Ptr);
    }
}
