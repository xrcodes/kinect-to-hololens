using UnityEngine;

// A class that creates Unity textures using the external methods of Plugin.cs.
// Through the methods, the Unity textures become wrappers of single channel Direct3D textures.
public class TextureGroup
{
    public Texture YTexture { get; private set; }
    public Texture UTexture { get; private set; }
    public Texture VTexture { get; private set; }
    public Texture DepthTexture { get; private set; }

    public TextureGroup()
    {
        //const int COLOR_WIDTH = 960;
        //const int COLOR_HEIGHT = 540;
        //const int DEPTH_WIDTH = 512;
        //const int DEPTH_HEIGHT = 424;

        const int AZURE_KINECT_COLOR_WIDTH = 1280;
        const int AZURE_KINECT_COLOR_HEIGHT = 720;
        const int AZURE_KINECT_DEPTH_WIDTH = 640;
        const int AZURE_KINECT_DEPTH_HEIGHT = 576;

        YTexture = Texture2D.CreateExternalTexture(AZURE_KINECT_COLOR_WIDTH,
                                                   AZURE_KINECT_COLOR_HEIGHT,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_y_texture_view());
        UTexture = Texture2D.CreateExternalTexture(AZURE_KINECT_COLOR_WIDTH / 2,
                                                   AZURE_KINECT_COLOR_HEIGHT / 2,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_u_texture_view());

        VTexture = Texture2D.CreateExternalTexture(AZURE_KINECT_COLOR_WIDTH / 2,
                                                   AZURE_KINECT_COLOR_HEIGHT / 2,
                                                   TextureFormat.R8,
                                                   false,
                                                   false,
                                                   Plugin.texture_group_get_v_texture_view());

        DepthTexture = Texture2D.CreateExternalTexture(AZURE_KINECT_DEPTH_WIDTH,
                                                       AZURE_KINECT_DEPTH_HEIGHT,
                                                       TextureFormat.R16,
                                                       false,
                                                       false,
                                                       Plugin.texture_group_get_depth_texture_view());
    }
}
