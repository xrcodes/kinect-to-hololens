using UnityEngine;

// A class that creates Unity textures using the external methods of Plugin.cs.
// Through the methods, the Unity textures become wrappers of single channel Direct3D textures.
public class TextureGroup
{
    public int ColorWidth { get; private set; }
    public int ColorHeight { get; private set; }
    public int DepthWidth { get; private set; }
    public int DepthHeight { get; private set; }
    public Texture2D YTexture { get; private set; }
    public Texture2D UTexture { get; private set; }
    public Texture2D VTexture { get; private set; }
    public Texture2D DepthTexture { get; private set; }

    public TextureGroup()
    {
        ColorWidth = 1280;
        ColorHeight = 720;
        DepthWidth = 640;
        DepthHeight = 576;

        YTexture = Texture2D.CreateExternalTexture(ColorWidth,
                                                   ColorHeight,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_y_texture_view());
        UTexture = Texture2D.CreateExternalTexture(ColorWidth / 2,
                                                   ColorHeight / 2,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_u_texture_view());

        VTexture = Texture2D.CreateExternalTexture(ColorWidth / 2,
                                                   ColorHeight / 2,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_v_texture_view());

        DepthTexture = Texture2D.CreateExternalTexture(DepthWidth,
                                                       DepthHeight,
                                                       TextureFormat.R16,
                                                       false,
                                                       false,
                                                       Plugin.texture_group_get_depth_texture_view());
    }
}
