using System;
using System.Runtime.InteropServices;
using UnityEngine;

// Class with static methods that are bridging to the external functions of KinectToHololensPlugin.dll.
public static class TelepresenceToolkitPlugin
{
    private const string DLL_NAME = "TelepresenceToolkitUnity";
    private const int COMMAND_COUNT = 2;

    [DllImport(DLL_NAME)]
    public static extern bool has_unity_interfaces();

    [DllImport(DLL_NAME)]
    public static extern bool has_unity_graphics();

    [DllImport(DLL_NAME)]
    public static extern bool has_d3d11_device();

    [DllImport(DLL_NAME)]
    public static extern IntPtr get_render_event_function_pointer();

    [DllImport(DLL_NAME)]
    public static extern void texture_group_reset();

    [DllImport(DLL_NAME)]
    public static extern IntPtr texture_group_create();

    [DllImport(DLL_NAME)]
    public static extern int texture_group_get_id(IntPtr textureGroup);

    [DllImport(DLL_NAME)]
    public static extern IntPtr texture_group_get_y_texture_view(IntPtr textureGroup);

    [DllImport(DLL_NAME)]
    public static extern IntPtr texture_group_get_uv_texture_view(IntPtr textureGroup);

    [DllImport(DLL_NAME)]
    public static extern IntPtr texture_group_get_depth_texture_view(IntPtr textureGroup);

    [DllImport(DLL_NAME)]
    public static extern int texture_group_get_width(IntPtr textureGroup);

    [DllImport(DLL_NAME)]
    public static extern void texture_group_set_width(IntPtr textureGroup, int width);

    [DllImport(DLL_NAME)]
    public static extern int texture_group_get_height(IntPtr textureGroup);

    [DllImport(DLL_NAME)]
    public static extern void texture_group_set_height(IntPtr textureGroup, int height);

    [DllImport(DLL_NAME)]
    public static extern void texture_group_set_av_frame(IntPtr textureGroup, IntPtr av_frame_ptr);

    [DllImport(DLL_NAME)]
    public static extern void texture_group_set_depth_pixels(IntPtr textureGroup, IntPtr depth_pixels_ptr);

    [DllImport(DLL_NAME)]
    public static extern IntPtr create_vp8_decoder();

    [DllImport(DLL_NAME)]
    public static extern void delete_vp8_decoder(IntPtr ptr);

    [DllImport(DLL_NAME)]
    public static extern IntPtr vp8_decoder_decode(IntPtr decoder_ptr, IntPtr frame_ptr, int frame_size);

    [DllImport(DLL_NAME)]
    public static extern void delete_av_frame(IntPtr ptr);

    [DllImport(DLL_NAME)]
    public static extern IntPtr create_trvl_decoder(int frame_size);

    [DllImport(DLL_NAME)]
    public static extern void delete_trvl_decoder(IntPtr ptr);

    [DllImport(DLL_NAME)]
    public static extern IntPtr trvl_decoder_decode(IntPtr decoder_ptr, IntPtr frame_ptr, int frame_size, bool keyframe);

    [DllImport(DLL_NAME)]
    public static extern void delete_depth_pixels(IntPtr ptr);

    [DllImport(DLL_NAME)]
    public static extern IntPtr create_opus_decoder(int sample_rate, int channel_count);

    [DllImport(DLL_NAME)]
    public static extern void delete_opus_decoder(IntPtr ptr);

    [DllImport(DLL_NAME)]
    public static extern int opus_decoder_decode(IntPtr decoder_ptr, IntPtr opus_frame_data, int opus_frame_size, IntPtr pcm_data, int frame_size);

    public static void InitTextureGroup(int textureGroupId)
    {
        InvokeRenderEvent(textureGroupId * COMMAND_COUNT + 0);
    }

    public static void UpdateTextureGroup(int textureGroupId)
    {
        InvokeRenderEvent(textureGroupId * COMMAND_COUNT + 1);
    }

    private static void InvokeRenderEvent(int renderEvent)
    {
        GL.IssuePluginEvent(get_render_event_function_pointer(), renderEvent);
    }
}
