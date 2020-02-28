using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
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

    private ReceiverSocket receiver;
    private bool stopReceiverThread;
    private ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue;
    private int lastFrameId;
    private int summaryPacketCount;

    private Vp8Decoder colorDecoder;
    private TrvlDecoder depthDecoder;
    private TextureGroup textureGroup;

    private Dictionary<int, VideoSenderMessageData> videoMessages;
    private Stopwatch frameStopWatch;

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

        receiver = null;
        stopReceiverThread = false;
        videoMessageQueue = new ConcurrentQueue<Tuple<int, VideoSenderMessageData>>();
        lastFrameId = -1;
        summaryPacketCount = 0;

        colorDecoder = null;
        depthDecoder = null;
        textureGroup = null;

        videoMessages = new Dictionary<int, VideoSenderMessageData>();
        frameStopWatch = Stopwatch.StartNew();

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

        UpdateTextureGroup();
    }

    void OnDestroy()
    {
        stopReceiverThread = true;
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

        Socket socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp)
        {
            ReceiveBufferSize = 1024 * 1024
        };

        var receiver = new ReceiverSocket(socket, new IPEndPoint(ipAddress, port));
        int senderSessionId = -1;
        int pingCount = 0;
        while (true)
        {
            bool initialized = false;
            receiver.Send(PacketHelper.createPingReceiverPacketBytes());
            ++pingCount;
            print($"Sent ping to {ipAddress.ToString()}:{port}.");

            yield return new WaitForSeconds(0.1f);

            SocketError error = SocketError.WouldBlock;
            while (true)
            {
                var packet = receiver.Receive(out error);
                if (packet == null)
                    break;

                int cursor = 0;
                int sessionId = BitConverter.ToInt32(packet, cursor);
                cursor += 4;

                var packetType = (SenderPacketType) packet[cursor];
                cursor += 1;
                if (packetType != SenderPacketType.Init)
                {
                    print($"A different kind of a packet received before an init packet: {packetType}");
                    continue;
                }

                senderSessionId = sessionId;

                var initSenderPacketData = InitSenderPacketData.Parse(packet);

                Plugin.texture_group_set_width(initSenderPacketData.depthWidth);
                Plugin.texture_group_set_height(initSenderPacketData.depthHeight);
                PluginHelper.InitTextureGroup();

                colorDecoder = new Vp8Decoder();
                depthDecoder = new TrvlDecoder(initSenderPacketData.depthWidth * initSenderPacketData.depthHeight);

                azureKinectScreen.Setup(initSenderPacketData);

                initialized = true;
                break;
            }
            if (initialized)
                break;

            if(pingCount == 10)
            {
                print("Tried pinging 10 times and failed to received an init packet...\n");
                yield break;
            }
        }

        this.receiver = receiver;

        Thread receiverThread = new Thread(() => RunReceiverThread(senderSessionId));
        receiverThread.Start();
    }

    private void RunReceiverThread(int senderSessionId)
    {
        const int XOR_MAX_GROUP_SIZE = 5;

        //int? senderSessionId = null;
        var videoPacketCollections = new Dictionary<int, VideoPacketCollection>();
        var fecPacketCollections = new Dictionary<int, FecPacketCollection>();
        print("Start Receiver Thread");
        while (!stopReceiverThread)
        {
            var videoPacketDataSet = new List<VideoSenderPacketData>();
            var fecPacketDataSet = new List<FecSenderPacketData>();
            SocketError error = SocketError.WouldBlock;
            while (true)
            {
                var packet = receiver.Receive(out error);
                if (packet == null)
                    break;

                ++summaryPacketCount;

                int sessionId = PacketHelper.getSessionIdFromSenderPacketBytes(packet);
                var packetType = PacketHelper.getPacketTypeFromSenderPacketBytes(packet);

                if (sessionId != senderSessionId)
                    continue;

                if (packetType == SenderPacketType.Frame)
                {
                    videoPacketDataSet.Add(VideoSenderPacketData.Parse(packet));
                }
                else if (packetType == SenderPacketType.Fec)
                {
                    fecPacketDataSet.Add(FecSenderPacketData.Parse(packet));
                }
            }

            if(error != SocketError.WouldBlock)
            {
                print($"Error from receiving packets: {error.ToString()}");
            }

            // The logic for XOR FEC packets are almost the same to frame packets.
            // The operations for XOR FEC packets should happen before the frame packets
            // so that frame packet can be created with XOR FEC packets when a missing
            // frame packet is detected.
            foreach (var fecSenderPacketData in fecPacketDataSet)
            {
                if (fecSenderPacketData.frameId <= lastFrameId)
                    continue;

                if (!fecPacketCollections.ContainsKey(fecSenderPacketData.frameId))
                {
                    fecPacketCollections[fecSenderPacketData.frameId] = new FecPacketCollection(fecSenderPacketData.frameId,
                                                                                                fecSenderPacketData.packetCount);
                }

                fecPacketCollections[fecSenderPacketData.frameId].AddPacket(fecSenderPacketData.packetIndex, fecSenderPacketData);
            }

            foreach (var videoSenderPacketData in videoPacketDataSet)
            {
                if (videoSenderPacketData.frameId <= lastFrameId)
                    continue;

                // If there is a packet for a new frame, check the previous frames, and if
                // there is a frame with missing packets, try to create them using xor packets.
                // If using the xor packets fails, request the sender to retransmit the packets.
                if (!videoPacketCollections.ContainsKey(videoSenderPacketData.frameId))
                {
                    videoPacketCollections[videoSenderPacketData.frameId] = new VideoPacketCollection(videoSenderPacketData.frameId,
                                                                                                      videoSenderPacketData.packetCount);

                    ///////////////////////////////////
                    // Forward Error Correction Start//
                    ///////////////////////////////////
                    // Request missing packets of the previous frames.
                    foreach (var collectionPair in videoPacketCollections)
                    {
                        if(collectionPair.Key < videoSenderPacketData.frameId)
                        {
                            int missingFrameId = collectionPair.Key;
                            var missingPacketIndices = collectionPair.Value.GetMissingPacketIds();

                            // Try correction using XOR FEC packets.
                            var fecFailedPacketIndices = new List<int>();
                            var fecPacketIndices = new List<int>();

                            // missing_packet_index cannot get error corrected if there is another missing_packet_index
                            // that belongs to the same XOR FEC packet...
                            foreach (int i in missingPacketIndices)
                            {
                                bool found = false;
                                foreach(int j in missingPacketIndices)
                                {
                                    if (i == j)
                                        continue;

                                    if ((i / XOR_MAX_GROUP_SIZE) == (j / XOR_MAX_GROUP_SIZE))
                                    {
                                        found = true;
                                        break;
                                    }
                                }
                                if(found)
                                {
                                    fecFailedPacketIndices.Add(i);
                                }
                                else
                                {
                                    fecPacketIndices.Add(i);
                                }
                            }

                            foreach (int fecPacketIndex in fecPacketIndices)
                            {
                                // Try getting the XOR FEC packet for correction.
                                int xorPacketIndex = fecPacketIndex / XOR_MAX_GROUP_SIZE;

                                if(!fecPacketCollections.ContainsKey(missingFrameId))
                                {
                                    fecFailedPacketIndices.Add(fecPacketIndex);
                                    continue;
                                }

                                var fecPacketData = fecPacketCollections[missingFrameId].GetPacketData(xorPacketIndex);
                                // Give up if there is no xor packet yet.
                                if (fecPacketData == null)
                                {
                                    fecFailedPacketIndices.Add(fecPacketIndex);
                                    continue;
                                }
                                
                                var fecVideoPacketData = new VideoSenderPacketData();

                                fecVideoPacketData.frameId = missingFrameId;
                                fecVideoPacketData.packetIndex = videoSenderPacketData.packetIndex;
                                fecVideoPacketData.packetCount = videoSenderPacketData.packetCount;
                                fecVideoPacketData.messageData = fecPacketData.bytes;

                                int beginFramePacketIndex = xorPacketIndex * XOR_MAX_GROUP_SIZE;
                                int endFramePacketIndex = Math.Min(beginFramePacketIndex + XOR_MAX_GROUP_SIZE, collectionPair.Value.PacketCount);

                                // Run bitwise XOR with all other packets belonging to the same XOR FEC packet.
                                for (int i = beginFramePacketIndex; i < endFramePacketIndex; ++i)
                                {
                                    if (i == fecPacketIndex)
                                        continue;

                                    //for (int j = PacketHelper.PACKET_HEADER_SIZE; j < PacketHelper.PACKET_SIZE; ++j)
                                    //    fecFramePacket[j] ^= collectionPair.Value.Packets[i][j];
                                    for (int j = 0; j < fecVideoPacketData.messageData.Length; ++j)
                                        fecVideoPacketData.messageData[j] ^= collectionPair.Value.PacketDataSet[i].messageData[j];
                                }

                                //framePacketCollections[missingFrameId].AddPacket(fecPacketIndex, fecFramePacket);
                                videoPacketCollections[missingFrameId].AddPacketData(fecPacketIndex, fecVideoPacketData);
                            } // end of foreach (int missingPacketIndex in missingPacketIndices)

                            receiver.Send(PacketHelper.createRequestReceiverPacketBytes(collectionPair.Key, fecFailedPacketIndices));
                        }
                    }
                    /////////////////////////////////
                    // Forward Error Correction End//
                    /////////////////////////////////
                }
                // End of if (frame_packet_collections.find(frame_id) == frame_packet_collections.end())
                // which was for reacting to a packet for a new frame.

                videoPacketCollections[videoSenderPacketData.frameId].AddPacketData(videoSenderPacketData.packetIndex, videoSenderPacketData);
            }

            // Find all full collections and their frame_ids.
            var fullFrameIds = new List<int>();
            foreach (var collectionPair in videoPacketCollections)
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
                //frameMessageQueue.Enqueue(videoPacketCollections[fullFrameId].ToMessage());
                var ms = new MemoryStream();
                foreach (var packetData in videoPacketCollections[fullFrameId].PacketDataSet)
                {
                    ms.Write(packetData.messageData, 0, packetData.messageData.Length);
                }

                var videoMessageData = VideoSenderMessageData.Parse(ms.ToArray());
                videoMessageQueue.Enqueue(new Tuple<int, VideoSenderMessageData>(fullFrameId, videoMessageData));

                videoPacketCollections.Remove(fullFrameId);
            }

            // Clean up frame_packet_collections.
            var obsoleteFrameIds = new List<int>();
            foreach (var collectionPair in videoPacketCollections)
            {
                if (collectionPair.Key <= lastFrameId)
                {
                    obsoleteFrameIds.Add(collectionPair.Key);
                }
            }

            foreach (int obsoleteFrameId in obsoleteFrameIds)
            {
                videoPacketCollections.Remove(obsoleteFrameId);
            }
        }
        print("Receiver Thread Dead");
    }

    private void UpdateTextureGroup()
    {
        while (videoMessageQueue.TryDequeue(out Tuple<int, VideoSenderMessageData> frameMessagePair))
        {
            // C# Dictionary throws an error when you add an element with
            // a key that is already taken.
            if (videoMessages.ContainsKey(frameMessagePair.Item1))
                continue;

            videoMessages.Add(frameMessagePair.Item1, frameMessagePair.Item2);
        }

        if (videoMessages.Count == 0)
        {
            return;
        }

        int? beginFrameId = null;
        // If there is a key frame, use the most recent one.
        foreach (var frameMessagePair in videoMessages)
        {
            if (frameMessagePair.Key <= lastFrameId)
                continue;

            if (frameMessagePair.Value.keyframe)
                beginFrameId = frameMessagePair.Key;
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!beginFrameId.HasValue)
        {
            if(videoMessages.ContainsKey(lastFrameId + 1))
            {
                beginFrameId = lastFrameId + 1;
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

        var decoderStopWatch = Stopwatch.StartNew();
        //for (int i = beginIndex.Value; i < frameMessages.Count; ++i)
        //{
        //    var frameMessage = frameMessages[i];
        for (int i = beginFrameId.Value; ; ++i)
        {
            if(!videoMessages.ContainsKey(i))
                break;
            
            var frameMessage = videoMessages[i];
            lastFrameId = i;

            var colorEncoderFrame = frameMessage.colorEncoderFrame;
            var depthEncoderFrame = frameMessage.depthEncoderFrame;

            ffmpegFrame = colorDecoder.Decode(colorEncoderFrame);
            trvlFrame = depthDecoder.Decode(depthEncoderFrame, frameMessage.keyframe);
        }

        decoderStopWatch.Stop();
        var decoderTime = decoderStopWatch.Elapsed;
        frameStopWatch.Stop();
        var frameTime = frameStopWatch.Elapsed;
        frameStopWatch = Stopwatch.StartNew();

        //print($"id: {lastFrameId}, packet collection time: {packetCollectionTime.TotalMilliseconds}, " +
        //      $"decoder time: {decoderTime.TotalMilliseconds}, frame time: {frameTime.TotalMilliseconds}");

        receiver.Send(PacketHelper.createReportReceiverPacketBytes(lastFrameId, (float)decoderTime.TotalMilliseconds,
                                                                   (float)frameTime.TotalMilliseconds, summaryPacketCount));
        summaryPacketCount = 0;

        // Invokes a function to be called in a render thread.
        if (textureGroup != null)
        {
            Plugin.texture_group_set_ffmpeg_frame(ffmpegFrame.Ptr);
            Plugin.texture_group_set_depth_pixels(trvlFrame.Ptr);
            PluginHelper.UpdateTextureGroup();
        }

        // Remove frame messages before the rendered frame.
        var frameMessageKeys = new List<int>();
        foreach (int key in videoMessages.Keys)
        {
            frameMessageKeys.Add(key);
        }
        foreach (int key in frameMessageKeys)
        {
            if(key < lastFrameId)
            {
                videoMessages.Remove(key);
            }
        }
    }
}
