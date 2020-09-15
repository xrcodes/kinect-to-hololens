using System;
using UnityEngine;

public class TextureSet
{
    private IntPtr ptr;

    public TextureSet()
    {
        ptr = TelepresenceToolkitPlugin.texture_set_create();
    }

    public int GetId()
    {
        return TelepresenceToolkitPlugin.texture_set_get_id(ptr);
    }

    public bool IsInitialized()
    {
        return TelepresenceToolkitPlugin.texture_set_get_y_texture_view(ptr).ToInt64() != 0;
    }

    public int GetWidth()
    {
        return TelepresenceToolkitPlugin.texture_set_get_width(ptr);
    }

    public void SetWidth(int width)
    {
        TelepresenceToolkitPlugin.texture_set_set_width(ptr, width);
    }

    public int GetHeight()
    {
        return TelepresenceToolkitPlugin.texture_set_get_height(ptr);
    }

    public void SetHeight(int height)
    {
        TelepresenceToolkitPlugin.texture_set_set_height(ptr, height);
    }

    public void SetAvFrame(AVFrame avFrame)
    {
        TelepresenceToolkitPlugin.texture_set_set_av_frame(ptr, avFrame.Ptr);
    }

    public void SetDepthPixels(DepthPixels depthPixels)
    {
        TelepresenceToolkitPlugin.texture_set_set_depth_pixels(ptr, depthPixels.Ptr);
    }

    public Texture2D GetYTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth(),
                                               GetHeight(),
                                               TextureFormat.R8,
                                               false,
                                               false,
                                               TelepresenceToolkitPlugin.texture_set_get_y_texture_view(ptr));
    }

    public Texture2D GetUvTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth() / 2,
                                               GetHeight() / 2,
                                               TextureFormat.RG16,
                                               false,
                                               false,
                                               TelepresenceToolkitPlugin.texture_set_get_uv_texture_view(ptr));
    }

    public Texture2D GetDepthTexture()
    {
        return Texture2D.CreateExternalTexture(GetWidth(),
                                               GetHeight(),
                                               TextureFormat.R16,
                                               false,
                                               false,
                                               TelepresenceToolkitPlugin.texture_set_get_depth_texture_view(ptr));
    }
}
