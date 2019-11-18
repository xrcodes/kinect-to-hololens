using System;
using System.Net;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.XR.WSA.Input;

// The main script for the HololensDemo scene.
public class HololensDemoManager : MonoBehaviour
{
    private enum InputState
    {
        IpAddress, Port
    }

    // The main camera's Transform.
    public Transform cameraTransform;
    // The TextMesh placed above user's head.
    public TextMesh statusText;
    // The root of the scene that includes everything else except the main camera.
    // This provides a convenient way to place everything in front of the camera.
    public Transform scenceRootTransform;
    // TextMeshes for the UI.
    public TextMesh ipAddressText;
    public TextMesh ipAddressInputField;
    public TextMesh portText;
    public TextMesh portInputField;
    public TextMesh instructionText;
    // For rendering the Kinect pixels in 3D.
    public Material azureKinectScreenMaterial;
    public AzureKinectScreen azureKinectScreen;

    // To recognize when the user taps.
    private GestureRecognizer gestureRecognizer;
    // Varaibles that represent states of the scene.
    private InputState inputState;
    private TextureGroup textureGroup;
    // The Receiver which receives Kinect data over the network.
    private Receiver receiver;
    // Decodes Kinect frames that were encoded before being sent over the network.
    private Vp8Decoder decoder;

    public TextMesh ActiveInputField
    {
        get
        {
            return inputState == InputState.IpAddress ? ipAddressInputField : portInputField;
        }
    }

    public bool UiVisibility
    {
        set
        {
            ipAddressText.gameObject.SetActive(value);
            ipAddressInputField.gameObject.SetActive(value);
            portText.gameObject.SetActive(value);
            portInputField.gameObject.SetActive(value);
            instructionText.gameObject.SetActive(value);
        }
    }

    void Awake()
    {
        gestureRecognizer = new GestureRecognizer();
        textureGroup = null;
        UiVisibility = true;
        SetInputState(InputState.IpAddress);

        Plugin.texture_group_set_color_width(1280);
        Plugin.texture_group_set_color_height(720);
        Plugin.texture_group_set_depth_width(640);
        Plugin.texture_group_set_depth_height(576);
        PluginHelper.InitTextureGroup();

        // Prepare a GestureRecognizer to recognize taps.
        gestureRecognizer.Tapped += OnTapped;
        gestureRecognizer.StartCapturingGestures();

        statusText.text = "Waiting for user input.";
    }

    void Update()
    {
        // Space key resets the scene to be placed in front of the camera.
        if(Input.GetKeyDown(KeyCode.Space))
        {
            ResetView();
        }

        // Sends virtual keyboards strokes to the TextMeshes for the IP address and the port.
        AbsorbInput();

        // If texture is not created, create and assign them to quads.
        if(textureGroup == null)
        {
            // Check whether the native plugin has Direct3D textures that
            // can be connected to Unity textures.
            if(Plugin.texture_group_get_y_texture_view().ToInt64() == 0)
                return;

            // TextureGroup includes Y, U, V, and a depth texture.
            textureGroup = new TextureGroup();
            azureKinectScreenMaterial.SetTexture("_YTex", textureGroup.YTexture);
            azureKinectScreenMaterial.SetTexture("_UTex", textureGroup.UTexture);
            azureKinectScreenMaterial.SetTexture("_VTex", textureGroup.VTexture);
            azureKinectScreenMaterial.SetTexture("_DepthTex", textureGroup.DepthTexture);
        }

        // Do not continue if there is no Receiever connected to a Sender.
        if(receiver == null)
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
        if(message == null)
            return;

        // Prepare the ScreenRenderer with calibration information of the Kinect.
        if(message[0] == 0)
        {
            int depthCompressionType;
            AzureKinectCalibration calibration;
            ReadAzureKinectCalibrationFromMessage(message, out depthCompressionType, out calibration);

            Plugin.texture_group_init_depth_encoder(depthCompressionType);

            azureKinectScreen.Setup(calibration);

            if((textureGroup.ColorWidth != calibration.ColorCamera.Width) ||
               (textureGroup.ColorHeight != calibration.ColorCamera.Height) ||
               (textureGroup.DepthWidth != calibration.DepthCamera.Width) ||
               (textureGroup.DepthHeight != calibration.DepthCamera.Height))
            {
                Debug.LogError("Dimensions of the textures do not match calibration information.");
            }

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
            //Plugin.texture_group_set_rvl_frame(rvlFrameBytes, rvlFrameSize);
            Plugin.texture_group_set_depth_encoder_frame(depthEncoderFrameBytes, depthEncoderFrameSize);
            Marshal.FreeHGlobal(depthEncoderFrameBytes);

            if (frameId % 100 == 0)
            {
                string logString = $"Received frame {frameId} (vp8FrameSize: {vp8FrameSize}, rvlFrameSize: {depthEncoderFrameSize})";
                Debug.Log(logString);
                statusText.text = logString;
            }

            // Invokes a function to be called in a render thread.
            PluginHelper.UpdateTextureGroup();
        }
    }

    private void OnTapped(TappedEventArgs args)
    {
        // Place the scene in front of the camera when the user taps.
        ResetView();
    }

    // Places everything in front of the camera by positing and turning a root transform for
    // everything else except the camera.
    private void ResetView()
    {
        scenceRootTransform.localPosition = cameraTransform.localPosition;
        scenceRootTransform.localRotation = cameraTransform.localRotation;
    }

    // Sends keystrokes of the virtual keyboard to TextMeshes.
    // Try connecting the Receiver to a Sender when the user pressed the enter key.
    private void AbsorbInput()
    {
        if (Input.GetKeyDown(KeyCode.UpArrow) || Input.GetKeyDown(KeyCode.DownArrow) || Input.GetKeyDown(KeyCode.Tab))
        {
            SetInputState(inputState != InputState.IpAddress ? InputState.IpAddress : InputState.Port);
        }
        AbsorbKeyCode(KeyCode.Alpha0, '0');
        AbsorbKeyCode(KeyCode.Keypad0, '0');
        AbsorbKeyCode(KeyCode.Alpha1, '1');
        AbsorbKeyCode(KeyCode.Keypad1, '1');
        AbsorbKeyCode(KeyCode.Alpha2, '2');
        AbsorbKeyCode(KeyCode.Keypad2, '2');
        AbsorbKeyCode(KeyCode.Alpha3, '3');
        AbsorbKeyCode(KeyCode.Keypad3, '3');
        AbsorbKeyCode(KeyCode.Alpha4, '4');
        AbsorbKeyCode(KeyCode.Keypad4, '4');
        AbsorbKeyCode(KeyCode.Alpha5, '5');
        AbsorbKeyCode(KeyCode.Keypad5, '5');
        AbsorbKeyCode(KeyCode.Alpha6, '6');
        AbsorbKeyCode(KeyCode.Keypad6, '6');
        AbsorbKeyCode(KeyCode.Alpha7, '7');
        AbsorbKeyCode(KeyCode.Keypad7, '7');
        AbsorbKeyCode(KeyCode.Alpha8, '8');
        AbsorbKeyCode(KeyCode.Keypad8, '8');
        AbsorbKeyCode(KeyCode.Alpha9, '9');
        AbsorbKeyCode(KeyCode.Keypad9, '9');
        if (inputState == InputState.IpAddress)
        {
            AbsorbKeyCode(KeyCode.Period, '.');
            AbsorbKeyCode(KeyCode.KeypadPeriod, '.');
        }
        if(Input.GetKeyDown(KeyCode.Backspace))
        {
            var text = ActiveInputField.text;
            if(text.Length > 0)
            {
                ActiveInputField.text = text.Substring(0, text.Length - 1);
            }
        }
        if(Input.GetKeyDown(KeyCode.Return) || Input.GetKeyDown(KeyCode.KeypadEnter) || Input.GetKeyDown("enter"))
        {
            Connect();
        }
    }

    // A helper method for AbsorbInput().
    private void AbsorbKeyCode(KeyCode keyCode, char c)
    {
        if(Input.GetKeyDown(keyCode))
        {
            ActiveInputField.text += c;
        }
    }

    private async void Connect()
    {
        UiVisibility = false;

        // The default IP address is 127.0.0.1.
        string ipAddress = ipAddressInputField.text;
        if (ipAddress.Length == 0)
            ipAddress = "127.0.0.1";

        // The default port is 7777.
        string portString = portInputField.text;
        int port = portString.Length != 0 ? int.Parse(portString) : 7777;

        string logString = $"Try connecting to {ipAddress}:{port}...";
        Debug.Log(logString);
        statusText.text = logString;
        var receiver = new Receiver();
        if (await receiver.ConnectAsync(new IPEndPoint(IPAddress.Parse(ipAddress), port)))
        {
            this.receiver = receiver;
            decoder = new Vp8Decoder();
            statusText.text = $"Connected to {ipAddress}:{port}!";
        }
        else
        {
            UiVisibility = true;
            statusText.text = $"Failed to connect to {ipAddress}:{port}.";
        }
    }

    private void SetInputState(InputState inputState)
    {
        if(inputState == InputState.IpAddress)
        {
            ipAddressText.color = Color.yellow;
            portText.color = Color.white;
        }
        else
        {
            ipAddressText.color = Color.white;
            portText.color = Color.yellow;
        }

        this.inputState = inputState;
    }

    private static void ReadAzureKinectCalibrationFromMessage(byte[] message,
        out int depthCompressionType, out AzureKinectCalibration calibraiton)
    {
        int cursor = 1;

        depthCompressionType = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        AzureKinectCalibration.Intrinsics depthIntrinsics;
        {
            float cx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float cy = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float fx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float fy = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k1 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k2 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k3 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k4 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k5 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k6 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float codx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float cody = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float p2 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float p1 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float metricRadius = BitConverter.ToSingle(message, cursor);
            cursor += 4;

            depthIntrinsics = new AzureKinectCalibration.Intrinsics(cx: cx,
                                                                    cy: cy,
                                                                    fx: fx,
                                                                    fy: fy,
                                                                    k1: k1,
                                                                    k2: k2,
                                                                    k3: k3,
                                                                    k4: k4,
                                                                    k5: k5,
                                                                    k6: k6,
                                                                    codx: codx,
                                                                    cody: cody,
                                                                    p2: p2,
                                                                    p1: p1,
                                                                    metricRadius: metricRadius);
        }

        int depthWidth = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        int depthHeight = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        float depthMetricRadius = BitConverter.ToSingle(message, cursor);
        cursor += 4;

        AzureKinectCalibration.Intrinsics colorIntrinsics;
        {
            float cx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float cy = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float fx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float fy = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k1 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k2 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k3 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k4 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k5 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k6 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float codx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float cody = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float p2 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float p1 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float metricRadius = BitConverter.ToSingle(message, cursor);
            cursor += 4;

            colorIntrinsics = new AzureKinectCalibration.Intrinsics(cx: cx,
                                                                    cy: cy,
                                                                    fx: fx,
                                                                    fy: fy,
                                                                    k1: k1,
                                                                    k2: k2,
                                                                    k3: k3,
                                                                    k4: k4,
                                                                    k5: k5,
                                                                    k6: k6,
                                                                    codx: codx,
                                                                    cody: cody,
                                                                    p2: p2,
                                                                    p1: p1,
                                                                    metricRadius: metricRadius);
        }

        int colorWidth = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        int colorHeight = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        float colorMetricRadius = BitConverter.ToSingle(message, cursor);
        cursor += 4;

        AzureKinectCalibration.Extrinsics depthToColorExtrinsics;
        {
            float[] rotation = new float[9];
            for(int i = 0; i < 9; ++i)
            {
                rotation[i] = BitConverter.ToSingle(message, cursor);
                cursor += 4;
            }

            float[] translation = new float[3];
            for(int i = 0; i < 3; ++i)
            {
                translation[i] = BitConverter.ToSingle(message, cursor);
                cursor += 4;
            }

            depthToColorExtrinsics = new AzureKinectCalibration.Extrinsics(rotation, translation);
        }

        var depthCamera = new AzureKinectCalibration.Camera(depthIntrinsics, depthWidth, depthHeight, depthMetricRadius);
        var colorCamera = new AzureKinectCalibration.Camera(colorIntrinsics, colorWidth, colorHeight, colorMetricRadius);

        calibraiton = new AzureKinectCalibration(depthCamera: depthCamera,
                                                 colorCamera: colorCamera,
                                                 depthToColorExtrinsics: depthToColorExtrinsics);
    }
}
