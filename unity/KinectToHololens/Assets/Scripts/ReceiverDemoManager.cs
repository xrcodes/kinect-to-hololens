using System;
using System.Net;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;

// The main script for the ReceiverDemo scene.
public class ReceiverDemoManager : MonoBehaviour
{
    // UI instances for connection to a Sender.
    public InputField ipAddressInputField;
    public InputField portInputField;
    public Button connectButton;
    // Quads for rendering the texutures of received pixels.
    public MeshRenderer yQuad;
    public MeshRenderer uQuad;
    public MeshRenderer vQuad;
    public MeshRenderer colorQuad;
    public MeshRenderer depthQuad;

    private TextureGroup textureGroup;
    private Receiver receiver;
    private Vp8Decoder decoder;

    public bool UiVisibility
    {
        set
        {
            ipAddressInputField.gameObject.SetActive(value);
            portInputField.gameObject.SetActive(value);
            connectButton.gameObject.SetActive(value);
        }
    }

    public bool QuadVisibility
    {
        set
        {
            yQuad.gameObject.SetActive(value);
            uQuad.gameObject.SetActive(value);
            vQuad.gameObject.SetActive(value);
            colorQuad.gameObject.SetActive(value);
            depthQuad.gameObject.SetActive(value);
        }
    }

    void Awake()
    {
        textureGroup = null;
        UiVisibility = true;
        QuadVisibility = false;

        Plugin.texture_group_reset();
    }

    void Update()
    {
        // If texture is not created, create and assign them to quads.
        if (textureGroup == null)
        {
            // Check whether the native plugin has Direct3D textures that
            // can be connected to Unity textures.
            if (Plugin.texture_group_get_y_texture_view().ToInt64() != 0)
            {
                // TextureGroup includes Y, U, V, and a depth texture.
                textureGroup = new TextureGroup(Plugin.texture_group_get_width(),
                                                Plugin.texture_group_get_height());

                yQuad.material.mainTexture = textureGroup.YTexture;
                uQuad.material.mainTexture = textureGroup.UTexture;
                vQuad.material.mainTexture = textureGroup.VTexture;
                
                colorQuad.material.SetTexture("_YTex", textureGroup.YTexture);
                colorQuad.material.SetTexture("_UTex", textureGroup.UTexture);
                colorQuad.material.SetTexture("_VTex", textureGroup.VTexture);
                
                depthQuad.material.mainTexture = textureGroup.DepthTexture;
            }
        }

        // Do not continue if there is no Receiever connected to a Sender.
        if (receiver == null)
            return;

        // Try receiving a message.
        byte[] message;
        try
        {
            message = receiver.Receive();
        }
        catch (Exception e)
        {
            Debug.Log(e.Message);
            receiver = null;
            return;
        }

        // Continue only if there is a message.
        if (message == null)
            return;

        if (message[0] == 0)
        {
            DemoManagerHelper.ReadAzureKinectCalibrationFromMessage(message,
                out int depthCompressionType, out AzureKinectCalibration calibration);

            Plugin.texture_group_set_width(calibration.DepthCamera.Width);
            Plugin.texture_group_set_height(calibration.DepthCamera.Height);
            PluginHelper.InitTextureGroup();

            Plugin.texture_group_init_depth_encoder(depthCompressionType);
        }
        // When a Kinect frame got received.
        else if (message[0] == 1)
        {
            int cursor = 1;
            int frameId = BitConverter.ToInt32(message, cursor);
            cursor += 4;

            // Notice the Sender that the frame was received through the Receiver.
            receiver.Send(frameId);

            int vp8FrameSize = BitConverter.ToInt32(message, cursor);
            cursor += 4;

            // Marshal.AllocHGlobal, Marshal.Copy, and Marshal.FreeHGlobal are like
            // malloc, memcpy, and free of C.
            // This is required since vp8FrameBytes gets sent to a Vp8Decoder
            // inside the native plugin.
            IntPtr vp8FrameBytes = Marshal.AllocHGlobal(vp8FrameSize);
            Marshal.Copy(message, cursor, vp8FrameBytes, vp8FrameSize);
            var ffmpegFrame = decoder.Decode(vp8FrameBytes, vp8FrameSize);
            Plugin.texture_group_set_ffmpeg_frame(ffmpegFrame.Ptr);
            Marshal.FreeHGlobal(vp8FrameBytes);
            cursor += vp8FrameSize;

            int depthEncoderFrameSize = BitConverter.ToInt32(message, cursor);
            cursor += 4;

            // Marshal.AllocHGlobal, Marshal.Copy, and Marshal.FreeHGlobal are like
            // malloc, memcpy, and free of C.
            // This is required since rvlFrameBytes gets sent to the native plugin.
            IntPtr depthEncoderFrameBytes = Marshal.AllocHGlobal(depthEncoderFrameSize);
            Marshal.Copy(message, cursor, depthEncoderFrameBytes, depthEncoderFrameSize);
            Plugin.texture_group_decode_depth_encoder_frame(depthEncoderFrameBytes, depthEncoderFrameSize);
            Marshal.FreeHGlobal(depthEncoderFrameBytes);

            if (frameId % 100 == 0)
            {
                print($"Received frame {frameId} (vp8FrameSize: {vp8FrameSize}, rvlFrameSize: {depthEncoderFrameSize}).");
            }

            // Invokes a function to be called in a render thread.
            if (textureGroup != null)
                PluginHelper.UpdateTextureGroup();
        }
    }

    public async void OnConnectButtonClicked()
    {
        UiVisibility = false;

        // The default IP address is 127.0.0.1.
        string ipAddress = ipAddressInputField.text;
        if (ipAddress.Length == 0)
            ipAddress = "127.0.0.1";

        // The default port is 7777.
        string portString = portInputField.text;
        int port = portString.Length != 0 ? int.Parse(portString) : 7777;

        print($"Try connecting to {ipAddress}:{port}.");
        var receiver = new Receiver();
        if (await receiver.ConnectAsync(new IPEndPoint(IPAddress.Parse(ipAddress), port)))
        {
            QuadVisibility = true;
            this.receiver = receiver;
            decoder = new Vp8Decoder();
        }
        else
        {
            UiVisibility = true;
        }
    }
}
