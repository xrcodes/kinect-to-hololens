using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;
using UnityEngine.XR.WSA.Input;

public class KinectToHololensManager : MonoBehaviour
{
    private const int PORT = 47498;

    private const float HEARTBEAT_INTERVAL_SEC = 1.0f;
    private const float HEARTBEAT_TIME_OUT_SEC = 5.0f;

    // The main camera's Transform.
    public Transform cameraTransform;
    // The TextMesh placed above user's head.
    public TextMesh statusText;
    // TextMeshes for the UI.
    public TextMesh ipAddressText;
    public TextMesh ipAddressInputField;
    public TextMesh instructionText;
    public TextMesh localIpAddressListText;
    // The root of the scene that includes everything else except the main camera.
    // This provides a convenient way to place everything in front of the camera.
    public AzureKinectRoot azureKinectRoot;
    public AzureKinectScreen azureKinectScreen;
    public AzureKinectSpeaker azureKinectSpeaker;
    public Transform floorPlaneTransform;

    // To recognize when the user taps.
    private GestureRecognizer gestureRecognizer;

    private int sessionId;
    private bool stopped;
    private ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue;
    private ConcurrentQueue<FloorSenderPacketData> floorPacketDataQueue;

    private KinectRenderer kinectRenderer;

    private bool UiVisibility
    {
        set
        {
            ipAddressText.gameObject.SetActive(value);
            ipAddressInputField.gameObject.SetActive(value);
            instructionText.gameObject.SetActive(value);
            localIpAddressListText.gameObject.SetActive(value);
        }
        get
        {
            return ipAddressText.gameObject.activeSelf;
        }
    }

    void Awake()
    {
        UiVisibility = true;

        gestureRecognizer = new GestureRecognizer();

        var random = new System.Random();
        sessionId = random.Next();
        stopped = false;
        azureKinectSpeaker.Init();
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

        if (kinectRenderer == null)
            return;

        kinectRenderer.UpdateFrame(videoMessageQueue);
        azureKinectRoot.UpdateFrame(floorPacketDataQueue);

        //FloorSenderPacketData floorSenderPacketData;
        //while (floorPacketDataQueue.TryDequeue(out floorSenderPacketData))
        //{
        //    //Vector3 upVector = new Vector3(floorSenderPacketData.a, floorSenderPacketData.b, floorSenderPacketData.c);
        //    // y component is fliped since the coordinate system of unity and azure kinect is different.
        //    Vector3 upVector = new Vector3(floorSenderPacketData.a, -floorSenderPacketData.b, floorSenderPacketData.c);
        //    floorPlaneTransform.localPosition = upVector * floorSenderPacketData.d;
        //    floorPlaneTransform.localRotation = Quaternion.FromToRotation(Vector3.up, upVector);
        //}
    }

    void OnDestroy()
    {
        stopped = true;
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
        azureKinectRoot.SetRootTransform(cameraTransform.position, cameraTransform.rotation);
    }

    // Sends keystrokes of the virtual keyboard to TextMeshes.
    // Try connecting the Receiver to a Sender when the user pressed the enter key.
    private void AbsorbInput()
    {
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
        AbsorbKeyCode(KeyCode.Period, '.');
        AbsorbKeyCode(KeyCode.KeypadPeriod, '.');
        if (Input.GetKeyDown(KeyCode.Backspace))
        {
            var text = ipAddressInputField.text;
            if (ipAddressInputField.text.Length > 0)
            {
                ipAddressInputField.text = text.Substring(0, text.Length - 1);
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
            ipAddressInputField.text += c;
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

        string logString = $"Try connecting to {ipAddressText}...";
        print(logString);
        statusText.text = logString;

        var ipAddress = IPAddress.Parse(ipAddressText);
        var udpSocket = new UdpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 });
        var endPoint = new IPEndPoint(ipAddress, PORT);

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

        azureKinectScreen.Setup(initPacketData);
        kinectRenderer = new KinectRenderer(azureKinectScreen.Material, initPacketData, udpSocket, endPoint, sessionId);

        var heartbeatStopWatch = Stopwatch.StartNew();
        var receivedAnyStopWatch = Stopwatch.StartNew();

        var taskThread = new Thread(() =>
        {
            var videoMessageAssembler = new VideoMessageAssembler(sessionId, endPoint);
            var audioPacketReceiver = new AudioPacketReceiver();

            while (!stopped)
            {
                try
                {
                    if (heartbeatStopWatch.Elapsed.TotalSeconds > HEARTBEAT_INTERVAL_SEC)
                    {
                        udpSocket.Send(PacketHelper.createHeartbeatReceiverPacketBytes(sessionId), endPoint);
                        heartbeatStopWatch = Stopwatch.StartNew();
                    }

                    var senderPacketSet = SenderPacketReceiver.Receive(udpSocket, floorPacketDataQueue);
                    if (senderPacketSet.ReceivedAny)
                    {
                        videoMessageAssembler.Assemble(udpSocket, 
                                                       senderPacketSet.VideoPacketDataList,
                                                       senderPacketSet.FecPacketDataList,
                                                       kinectRenderer.lastVideoFrameId,
                                                       videoMessageQueue);
                        audioPacketReceiver.Receive(senderPacketSet.AudioPacketDataList, azureKinectSpeaker.RingBuffer);
                        receivedAnyStopWatch = Stopwatch.StartNew();
                    }
                    else
                    {
                        if(receivedAnyStopWatch.Elapsed.TotalSeconds > HEARTBEAT_TIME_OUT_SEC)
                        {
                            print($"Timed out after waiting for {HEARTBEAT_TIME_OUT_SEC} seconds without a received packet.");
                            break;
                        }
                    }
                }
                catch (UdpSocketException e)
                {
                    print($"UdpSocketRuntimeError: {e}");
                    break;
                }
            }

            stopped = true;
        });

        taskThread.Start();
    }
}
