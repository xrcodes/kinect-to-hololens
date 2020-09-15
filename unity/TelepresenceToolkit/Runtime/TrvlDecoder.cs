﻿using System;
using System.Runtime.InteropServices;

// A class that contains a pointer to a TrvlDecoder in KinectToHololensPlugin.dll.
public class TrvlDecoder
{
    private IntPtr ptr;

    public TrvlDecoder(int frameSize)
    {
        ptr = TelepresenceToolkitPlugin.create_trvl_decoder(frameSize);
    }

    ~TrvlDecoder()
    {
        TelepresenceToolkitPlugin.delete_trvl_decoder(ptr);
    }

    public TrvlFrame Decode(byte[] frame, bool keyframe)
    {
        //IntPtr bytes = Marshal.AllocHGlobal(frame.Length);
        //Marshal.Copy(frame, 0, bytes, frame.Length);
        //var trvlFrame = new TrvlFrame(TelepresenceToolkitPlugin.trvl_decoder_decode(ptr, bytes, frame.Length, keyframe));
        //Marshal.FreeHGlobal(bytes);

        GCHandle handle = GCHandle.Alloc(frame, GCHandleType.Pinned);
        IntPtr bytes = handle.AddrOfPinnedObject();
        var trvlFrame = new TrvlFrame(TelepresenceToolkitPlugin.trvl_decoder_decode(ptr, bytes, frame.Length, keyframe));
        handle.Free();

        return trvlFrame;
    }
}
