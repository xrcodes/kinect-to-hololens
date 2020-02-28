using System;
using UnityEngine;

public class TextureGroup
{
    public IntPtr Ptr { get; private set; }

    public TextureGroup(IntPtr ptr)
    {
        Ptr = ptr;
    }

    public bool IsInitialized()
    {
        return Plugin.texture_group_get_y_texture_view(Ptr).ToInt64() != 0;
    }

    public int GetWidth()
    {
        return Plugin.texture_group_get_width(Ptr);
    }

    public void SetWidth(int width)
    {
        Plugin.texture_group_set_width(Ptr, width);
    }

    public int GetHeight()
    {
        return Plugin.texture_group_get_height(Ptr);
    }

    public void SetHeight(int height)
    {
        Plugin.texture_group_set_height(Ptr, height);
    }

    public void SetFFmpegFrame(FFmpegFrame ffmpegFrame)
    {
        Plugin.texture_group_set_ffmpeg_frame(Ptr, ffmpegFrame.Ptr);
    }

    public void SetTrvlFrame(TrvlFrame trvlFrame)
    {
        Plugin.texture_group_set_depth_pixels(Ptr, trvlFrame.Ptr);
    }

    public Texture2D GetYTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth(),
                                               GetHeight(),
                                               TextureFormat.R8,
                                               false,
                                               false,
                                               Plugin.texture_group_get_y_texture_view(Ptr));
    }

    public Texture2D GetUTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth() / 2,
                                               GetHeight() / 2,
                                               TextureFormat.R8,
                                               false,
                                               false,
                                               Plugin.texture_group_get_u_texture_view(Ptr));
    }

    public Texture2D GetVTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth() / 2,
                                               GetHeight() / 2,
                                               TextureFormat.R8,
                                               false,
                                               false,
                                               Plugin.texture_group_get_v_texture_view(Ptr));
    }

    public Texture2D GetDepthTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth(),
                                               GetHeight(),
                                               TextureFormat.R16,
                                               false,
                                               false,
                                               Plugin.texture_group_get_depth_texture_view(Ptr));
    }
}
