using System;
using UnityEngine;

public class TextureGroup
{
    private IntPtr ptr;

    public TextureGroup()
    {
        ptr = Plugin.texture_group_create();
    }

    public int GetId()
    {
        return Plugin.texture_group_get_id(ptr);
    }

    public bool IsInitialized()
    {
        return Plugin.texture_group_get_y_texture_view(ptr).ToInt64() != 0;
    }

    public int GetWidth()
    {
        return Plugin.texture_group_get_width(ptr);
    }

    public void SetWidth(int width)
    {
        Plugin.texture_group_set_width(ptr, width);
    }

    public int GetHeight()
    {
        return Plugin.texture_group_get_height(ptr);
    }

    public void SetHeight(int height)
    {
        Plugin.texture_group_set_height(ptr, height);
    }

    public void SetFFmpegFrame(FFmpegFrame ffmpegFrame)
    {
        Plugin.texture_group_set_ffmpeg_frame(ptr, ffmpegFrame.Ptr);
    }

    public void SetTrvlFrame(TrvlFrame trvlFrame)
    {
        Plugin.texture_group_set_depth_pixels(ptr, trvlFrame.Ptr);
    }

    public Texture2D GetYTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth(),
                                               GetHeight(),
                                               TextureFormat.R8,
                                               false,
                                               false,
                                               Plugin.texture_group_get_y_texture_view(ptr));
    }

    public Texture2D GetUvTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth() / 2,
                                               GetHeight() / 2,
                                               TextureFormat.RG16,
                                               false,
                                               false,
                                               Plugin.texture_group_get_uv_texture_view(ptr));
    }

    public Texture2D GetDepthTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth(),
                                               GetHeight(),
                                               TextureFormat.R16,
                                               false,
                                               false,
                                               Plugin.texture_group_get_depth_texture_view(ptr));
    }
}
