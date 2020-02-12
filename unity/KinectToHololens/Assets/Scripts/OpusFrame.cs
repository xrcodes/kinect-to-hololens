using System;
using System.Runtime.InteropServices;

public class OpusFrame
{
    public IntPtr ptr;
    
    public OpusFrame(IntPtr ptr)
    {
        this.ptr = ptr;
    }

    ~OpusFrame()
    {
        Plugin.delete_opus_frame(ptr);
    }

    public float[] GetArray()
    {
        IntPtr data = Plugin.opus_frame_get_data(ptr);
        int size = Plugin.opus_frame_get_size(ptr);

        float[] array = new float[size];
        Marshal.Copy(data, array, 0, size);
        return array;
    }
}
