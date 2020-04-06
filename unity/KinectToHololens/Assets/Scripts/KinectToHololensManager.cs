using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;
using UnityEngine.XR.WSA.Input;

public enum InputState
{
    IpAddress, Port
}

public class KinectToHololensManager : MonoBehaviour
{
    public const int KH_SAMPLE_RATE = 48000;
    public const int KH_CHANNEL_COUNT = 2;
    public const double KH_LATENCY_SECONDS = 0.2;
    public const int KH_SAMPLES_PER_FRAME = 960;
    public const int KH_BYTES_PER_SECOND = KH_SAMPLE_RATE * KH_CHANNEL_COUNT * sizeof(float);

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
    public Transform floorPlaneTransform;

    // To recognize when the user taps.
    private GestureRecognizer gestureRecognizer;
    // Varaibles that represent states of the scene.
    private InputState inputState;

    private int sessionId;
    private bool stopped;
    private RingBuffer ringBuffer;
    private ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue;
    private ConcurrentQueue<FloorSenderPacketData> floorPacketDataQueue;

    private KinectRenderer kinectRenderer;

    private InputState InputState
    {
        set
        {
            if (value == InputState.IpAddress)
            {
                ipAddressText.color = Color.yellow;
                portText.color = Color.white;
            }
            else
            {
                ipAddressText.color = Color.white;
                portText.color = Color.yellow;
            }

            inputState = value;
        }
    }

    private TextMesh ActiveInputField
    {
        get
        {
            return inputState == InputState.IpAddress ? ipAddressInputField : portInputField;
        }
    }

    private bool UiVisibility
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
        InputState = InputState.IpAddress;
        UiVisibility = true;

        gestureRecognizer = new GestureRecognizer();

        var random = new System.Random();
        sessionId = random.Next();
        stopped = false;
        ringBuffer = new RingBuffer((int)(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND / sizeof(float)));
        videoMessageQueue = new ConcurrentQueue<Tuple<int, VideoSenderMessageData>>();
        floorPacketDataQueue = new ConcurrentQueue<FloorSenderPacketData>();


        // Prepare a GestureRecognizer to recognize taps.
        gestureRecognizer.Tapped += OnTapped;
        gestureRecognizer.StartCapturingGestures();

        statusText.text = "Waiting for user input.";
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

        if(kinectRenderer != null)
            kinectRenderer.UpdateFrame(videoMessageQueue, floorPacketDataQueue);
    }

    void OnDestroy()
    {
        stopped = true;
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        const float AMPLIFIER = 8.0f;

        ringBuffer.Read(data);
        for (int i = 0; i < data.Length; ++i)
            data[i] = data[i] * AMPLIFIER;
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
            InputState = inputState != InputState.IpAddress ? InputState.IpAddress : InputState.Port;
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
            StartCoroutine(Ping());
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
    private IEnumerator Ping()
    {
        if(!UiVisibility)
        {
            print("No more than one ping at a time.");
            yield break;
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
        var udpSocket = new UdpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 });
        var endPoint = new IPEndPoint(ipAddress, port);

        InitSenderPacketData initPacketData;
        int pingCount = 0;
        while (true)
        {
            udpSocket.Send(PacketHelper.createConnectReceiverPacketBytes(sessionId), endPoint);
            ++pingCount;
            UnityEngine.Debug.Log("Sent ping");

            //Thread.Sleep(100);
            Thread.Sleep(300);

            var senderPacketSet = SenderPacketReceiver.Receive(udpSocket, floorPacketDataQueue);
            if (senderPacketSet.InitPacketDataList.Count > 0)
            {
                initPacketData = senderPacketSet.InitPacketDataList[0];
                break;
            }

            if (pingCount == 10)
            {
                UnityEngine.Debug.Log("Tried pinging 10 times and failed to received an init packet...\n");
                UiVisibility = true;
                yield break;
            }
        }

        kinectRenderer = new KinectRenderer(azureKinectScreenMaterial, azureKinectScreen, floorPlaneTransform,
            initPacketData, udpSocket, endPoint, sessionId);

        var taskThread = new Thread(() =>
        {
            var videoMessageAssembler = new VideoMessageAssembler(sessionId, endPoint);
            var audioPacketReceiver = new AudioPacketReceiver();

            while (!stopped)
            {
                var senderPacketSet = SenderPacketReceiver.Receive(udpSocket, floorPacketDataQueue);
                videoMessageAssembler.Assemble(udpSocket, senderPacketSet.VideoPacketDataList,
                    senderPacketSet.FecPacketDataList, kinectRenderer.lastVideoFrameId, videoMessageQueue);
                audioPacketReceiver.Receive(senderPacketSet.AudioPacketDataList, ringBuffer);
            }

            stopped = true;
        });

        taskThread.Start();
    }
}
