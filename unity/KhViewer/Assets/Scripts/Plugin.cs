﻿using System;
using System.Runtime.InteropServices;
using UnityEngine;

// A class with helper static methods for methods of Plugin.cs.
public static class PluginHelper
{
    private const int COMMAND_COUNT = 2;
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
        GL.IssuePluginEvent(Plugin.get_render_event_function_pointer(), renderEvent);
    }
}

// Class with static methods that are bridging to the external functions of KinectToHololensPlugin.dll.
public static class Plugin
{
    private const string DllName = "KinectToHololensUnity";

    [DllImport(DllName)]
    public static extern bool has_unity_interfaces();

    [DllImport(DllName)]
    public static extern bool has_unity_graphics();

    [DllImport(DllName)]
    public static extern bool has_d3d11_device();

    [DllImport(DllName)]
    public static extern IntPtr get_render_event_function_pointer();

    [DllImport(DllName)]
    public static extern void texture_group_reset();

    [DllImport(DllName)]
    public static extern IntPtr texture_group_create();

    [DllImport(DllName)]
    public static extern int texture_group_get_id(IntPtr textureGroup);

    [DllImport(DllName)]
    public static extern IntPtr texture_group_get_y_texture_view(IntPtr textureGroup);

    [DllImport(DllName)]
    public static extern IntPtr texture_group_get_uv_texture_view(IntPtr textureGroup);

    [DllImport(DllName)]
    public static extern IntPtr texture_group_get_depth_texture_view(IntPtr textureGroup);

    [DllImport(DllName)]
    public static extern int texture_group_get_width(IntPtr textureGroup);

    [DllImport(DllName)]
    public static extern void texture_group_set_width(IntPtr textureGroup, int width);

    [DllImport(DllName)]
    public static extern int texture_group_get_height(IntPtr textureGroup);

    [DllImport(DllName)]
    public static extern void texture_group_set_height(IntPtr textureGroup, int height);

    [DllImport(DllName)]
    public static extern void texture_group_set_ffmpeg_frame(IntPtr textureGroup, IntPtr ffmpeg_frame_ptr);

    [DllImport(DllName)]
    public static extern void texture_group_set_depth_pixels(IntPtr textureGroup, IntPtr depth_pixels_ptr);

    [DllImport(DllName)]
    public static extern IntPtr create_vp8_decoder();

    [DllImport(DllName)]
    public static extern void delete_vp8_decoder(IntPtr ptr);

    [DllImport(DllName)]
    public static extern IntPtr vp8_decoder_decode(IntPtr decoder_ptr, IntPtr frame_ptr, int frame_size);

    [DllImport(DllName)]
    public static extern void delete_ffmpeg_frame(IntPtr ptr);

    [DllImport(DllName)]
    public static extern IntPtr create_trvl_decoder(int frame_size);

    [DllImport(DllName)]
    public static extern void delete_trvl_decoder(IntPtr ptr);

    [DllImport(DllName)]
    public static extern IntPtr trvl_decoder_decode(IntPtr decoder_ptr, IntPtr frame_ptr, int frame_size, bool keyframe);

    [DllImport(DllName)]
    public static extern void delete_depth_pixels(IntPtr ptr);

    [DllImport(DllName)]
    public static extern IntPtr create_audio_decoder(int sample_rate, int channel_count);

    [DllImport(DllName)]
    public static extern void destroy_audio_decoder(IntPtr ptr);

    [DllImport(DllName)]
    public static extern int audio_decoder_decode(IntPtr decoder_ptr, IntPtr opus_frame_data, int opus_frame_size, IntPtr pcm_data, int frame_size);
}
