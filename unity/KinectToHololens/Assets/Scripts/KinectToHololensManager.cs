using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using System.Threading;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.XR.WSA.Input;

public class KinectToHololensManager : MonoBehaviour
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
    private Receiver receiver;
    private Vp8Decoder colorDecoder;
    private TrvlDecoder depthDecoder;

    private Dictionary<int, FramePacketCollection> framePacketCollections;
    private List<FrameMessage> frameMessages;
    private int lastFrameId;
    private Stopwatch frameStopWatch;
    private int? serverSessionId;
    private int summaryPacketCount;

    private bool stopReceiverThread;
    private ConcurrentQueue<byte[]> packetQueue;

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
        get
        {
            return ipAddressText.gameObject.activeSelf;
        }
    }

    void Awake()
    {
        gestureRecognizer = new GestureRecognizer();
        receiver = null;
        textureGroup = null;
        UiVisibility = true;
        SetInputState(InputState.IpAddress);

        framePacketCollections = new Dictionary<int, FramePacketCollection>();
        frameMessages = new List<FrameMessage>();
        lastFrameId = -1;
        frameStopWatch = Stopwatch.StartNew();
        serverSessionId = null;
        summaryPacketCount = 0;

        stopReceiverThread = false;
        packetQueue = new ConcurrentQueue<byte[]>();

        // Prepare a GestureRecognizer to recognize taps.
        gestureRecognizer.Tapped += OnTapped;
        gestureRecognizer.StartCapturingGestures();

        statusText.text = "Waiting for user input.";

        Plugin.texture_group_reset();
    }

    void Update()
    {
        // Space key resets the scene to be placed in front of the camera.
        if (Input.GetKeyDown(KeyCode.Space))
        {
            ResetView();
        }

        // Sends virtual keyboards strokes to the TextMeshes for the IP address and the port.
        AbsorbInput();

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

                azureKinectScreenMaterial.SetTexture("_YTex", textureGroup.YTexture);
                azureKinectScreenMaterial.SetTexture("_UTex", textureGroup.UTexture);
                azureKinectScreenMaterial.SetTexture("_VTex", textureGroup.VTexture);
                azureKinectScreenMaterial.SetTexture("_DepthTex", textureGroup.DepthTexture);

                print("textureGroup intialized");
            }
        }

        if (receiver == null)
            return;

        //var packets = new List<byte[]>();
        //while (true)
        //{
        //    var packet = receiver.Receive();
        //    if (packet == null)
        //        break;

        //    packets.Add(packet);
        //}
        //if (packets.Count == 0)
        //    return;

        //summaryPacketCount += packets.Count;

        //foreach (var packet in packets)
        while(packetQueue.TryDequeue(out byte[] packet))
        {
            int cursor = 0;
            int sessionId = BitConverter.ToInt32(packet, cursor);
            cursor += 4;

            var packetType = packet[cursor];
            cursor += 1;
            // For an initialization packet.
            if (packetType == 0)
            {
                // No need to have UI for connection after initialization.
                UiVisibility = false;

                serverSessionId = sessionId;

                var calibration = ManagerHelper.ReadAzureKinectCalibrationFromMessage(packet, cursor);

                Plugin.texture_group_set_width(calibration.DepthCamera.Width);
                Plugin.texture_group_set_height(calibration.DepthCamera.Height);
                PluginHelper.InitTextureGroup();

                colorDecoder = new Vp8Decoder();
                depthDecoder = new TrvlDecoder(calibration.DepthCamera.Width * calibration.DepthCamera.Height);

                azureKinectScreen.Setup(calibration);
            }

            // Block packets from before initilization and from a sender that
            // was not the one that initialized this receiver.
            if (!serverSessionId.HasValue || sessionId != serverSessionId)
                continue;

            if (packetType == 1)
            {
                int frameId = BitConverter.ToInt32(packet, cursor);
                cursor += 4;

                // No need to collect packets for previous frames.
                if (frameId <= lastFrameId)
                    continue;
                
                int packetIndex = BitConverter.ToInt32(packet, cursor);
                cursor += 4;
                int packetCount = BitConverter.ToInt32(packet, cursor);
                cursor += 4;

                if (!framePacketCollections.ContainsKey(frameId))
                {
                    framePacketCollections[frameId] = new FramePacketCollection(frameId, packetCount);
                }

                framePacketCollections[frameId].AddPacket(packetIndex, packet);
            }
        }

        // Find all full collections and their frame_ids.
        var fullFrameIds = new List<int>();
        foreach (var collectionPair in framePacketCollections)
        {
            if (collectionPair.Value.IsFull())
            {
                int frameId = collectionPair.Key;
                fullFrameIds.Add(frameId);
            }
        }

        // Extract messages from the full collections.
        foreach (int fullFrameId in fullFrameIds)
        {
            frameMessages.Add(framePacketCollections[fullFrameId].ToMessage());
            framePacketCollections.Remove(fullFrameId);
        }

        frameMessages.Sort((x, y) => x.FrameId.CompareTo(y.FrameId));
        
        if (frameMessages.Count == 0)
        {
            return;
        }

        int? beginIndex = null;
        // If there is a key frame, use the most recent one.
        for (int i = frameMessages.Count - 1; i >= 0; --i)
        {
            if (frameMessages[i].Keyframe)
            {
                beginIndex = i;
                break;
            }
        }

        // When there is no key frame, go through all the frames if the first
        // FrameMessage is the one right after the previously rendered one.
        if (!beginIndex.HasValue)
        {
            if (frameMessages[0].FrameId == lastFrameId + 1)
            {
                beginIndex = 0;
            }
            else
            {
                // Wait for more frames if there is way to render without glitches.
                return;
            }
        }

        // ffmpegFrame and trvlFrame are guaranteed to be non-null
        // since the existence of beginIndex's value.
        FFmpegFrame ffmpegFrame = null;
        TrvlFrame trvlFrame = null;
        TimeSpan packetCollectionTime;

        var decoderStopWatch = Stopwatch.StartNew();
        for (int i = beginIndex.Value; i < frameMessages.Count; ++i)
        {
            var frameMessage = frameMessages[i];
            lastFrameId = frameMessage.FrameId;

            packetCollectionTime = frameMessage.PacketCollectionTime;

            var colorEncoderFrame = frameMessage.GetColorEncoderFrame();
            var depthEncoderFrame = frameMessage.GetDepthEncoderFrame();

            IntPtr colorEncoderFrameBytes = Marshal.AllocHGlobal(colorEncoderFrame.Length);
            Marshal.Copy(colorEncoderFrame, 0, colorEncoderFrameBytes, colorEncoderFrame.Length);
            ffmpegFrame = colorDecoder.Decode(colorEncoderFrameBytes, colorEncoderFrame.Length);
            Marshal.FreeHGlobal(colorEncoderFrameBytes);


            IntPtr depthEncoderFrameBytes = Marshal.AllocHGlobal(depthEncoderFrame.Length);
            Marshal.Copy(depthEncoderFrame, 0, depthEncoderFrameBytes, depthEncoderFrame.Length);
            trvlFrame = depthDecoder.Decode(depthEncoderFrameBytes, frameMessage.Keyframe);
            Marshal.FreeHGlobal(depthEncoderFrameBytes);

        }
        decoderStopWatch.Stop();
        var decoderTime = decoderStopWatch.Elapsed;
        frameStopWatch.Stop();
        var frameTime = frameStopWatch.Elapsed;
        frameStopWatch = Stopwatch.StartNew();

        print($"id: {lastFrameId}, packet collection time: {packetCollectionTime.TotalMilliseconds}," +
              $"decoder time: {decoderTime.TotalMilliseconds}, frame time: {frameTime.TotalMilliseconds}");

        receiver.Send(lastFrameId, (float) packetCollectionTime.TotalMilliseconds, (float) decoderTime.TotalMilliseconds,
            (float) frameTime.TotalMilliseconds, summaryPacketCount);
        summaryPacketCount = 0;

        // Invokes a function to be called in a render thread.
        if (textureGroup != null)
        {
            Plugin.texture_group_set_ffmpeg_frame(ffmpegFrame.Ptr);
            Plugin.texture_group_set_depth_pixels(trvlFrame.Ptr);
            PluginHelper.UpdateTextureGroup();
        }

        // Clean up frame_packet_collections.
        int endFrameId = frameMessages[frameMessages.Count - 1].FrameId;
        var obsoleteFrameIds = new List<int>();
        foreach (var collectionPair in framePacketCollections)
        {
            if(collectionPair.Key <= endFrameId)
            {
                obsoleteFrameIds.Add(collectionPair.Key);
            }
        }

        foreach (int obsoleteFrameId in obsoleteFrameIds)
        {
            framePacketCollections.Remove(obsoleteFrameId);
        }

        frameMessages = new List<FrameMessage>();
    }

    void OnDestroy()
    {
        stopReceiverThread = true;
    }

    private void SetInputState(InputState inputState)
    {
        if (inputState == InputState.IpAddress)
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
        if (Input.GetKeyDown(KeyCode.Backspace))
        {
            var text = ActiveInputField.text;
            if (text.Length > 0)
            {
                ActiveInputField.text = text.Substring(0, text.Length - 1);
            }
        }
        if (Input.GetKeyDown(KeyCode.Return) || Input.GetKeyDown(KeyCode.KeypadEnter) || Input.GetKeyDown("enter"))
        {
            Ping();
        }
    }

    // A helper method for AbsorbInput().
    private void AbsorbKeyCode(KeyCode keyCode, char c)
    {
        if (Input.GetKeyDown(keyCode))
        {
            ActiveInputField.text += c;
        }
    }

    // To copy the c++ receiver, for easier development,
    // there should be only one chance to send a ping.
    private void Ping()
    {
        if(!UiVisibility)
        {
            print("No more than one ping at a time.");
            return;
        }

        UiVisibility = false;

        // The default IP address is 127.0.0.1.
        string ipAddressText = ipAddressInputField.text;
        if (ipAddressText.Length == 0)
            ipAddressText = "127.0.0.1";

        // The default port is 7777.
        string portString = portInputField.text;
        int port = portString.Length != 0 ? int.Parse(portString) : 7777;

        string logString = $"Try connecting to {ipAddressText}:{port}...";
        print(logString);
        statusText.text = logString;

        var ipAddress = IPAddress.Parse(ipAddressText);
        receiver = new Receiver(1024 * 1024);
        receiver.Ping(ipAddress, port);

        Thread receiverThread = new Thread(RunReceiverThread);
        receiverThread.Start();
    }

    private void RunReceiverThread()
    {
        print("Start Receiver Thread");
        while (!stopReceiverThread)
        {
            while (true)
            {
                var packet = receiver.Receive();
                if (packet == null)
                    break;

                packetQueue.Enqueue(packet);
                ++summaryPacketCount;
            }
        }
        print("Receiver Thread Dead");
    }
}
