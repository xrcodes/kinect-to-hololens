using UnityEngine;

// A class that creates Unity textures using the external methods of Plugin.cs.
// Through the methods, the Unity textures become wrappers of single channel Direct3D textures.
public class UnityTextureGroup
{
    public int Width { get; private set; }
    public int Height { get; private set; }
    public Texture2D YTexture { get; private set; }
    public Texture2D UTexture { get; private set; }
    public Texture2D VTexture { get; private set; }
    public Texture2D DepthTexture { get; private set; }

    public UnityTextureGroup(int width, int height)
    {
        Width = width;
        Height = height;

        YTexture = Texture2D.CreateExternalTexture(Width,
                                                   Height,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_y_texture_view());
        UTexture = Texture2D.CreateExternalTexture(Width / 2,
                                                   Height / 2,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_u_texture_view());

        VTexture = Texture2D.CreateExternalTexture(Width / 2,
                                                   Height / 2,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_v_texture_view());

        DepthTexture = Texture2D.CreateExternalTexture(Width,
                                                       Height,
                                                       TextureFormat.R16,
                                                       false,
                                                       false,
                                                       Plugin.texture_group_get_depth_texture_view());
    }
}
