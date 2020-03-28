using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;

public class KinectReceiver
{
    public const int KH_SAMPLE_RATE = 48000;
    public const int KH_CHANNEL_COUNT = 2;
    public const double KH_LATENCY_SECONDS = 0.2;
    public const int KH_SAMPLES_PER_FRAME = 960;
    public const int KH_BYTES_PER_SECOND = KH_SAMPLE_RATE * KH_CHANNEL_COUNT * sizeof(float);

    private Material azureKinectScreenMaterial;
    private AzureKinectScreen azureKinectScreen;

    private TextureGroup textureGroup;

    private UdpSocket udpSocket;
    private bool receiverStopped;
    private ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue;
    private int lastVideoFrameId;

    private Vp8Decoder colorDecoder;
    private TrvlDecoder depthDecoder;
    private bool preapared;

    private RingBuffer ringBuffer;

    private Dictionary<int, VideoSenderMessageData> videoMessages;
    private Stopwatch frameStopWatch;

    public RingBuffer RingBuffer
    {
        get
        {
            return ringBuffer;
        }
    }

    public KinectReceiver(Material azureKinectScreenMaterial, AzureKinectScreen azureKinectScreen)
    {
        this.azureKinectScreenMaterial = azureKinectScreenMaterial;
        this.azureKinectScreen = azureKinectScreen;

        textureGroup = new TextureGroup(Plugin.texture_group_reset());

        udpSocket = null;
        receiverStopped = false;
        videoMessageQueue = new ConcurrentQueue<Tuple<int, VideoSenderMessageData>>();
        lastVideoFrameId = -1;

        colorDecoder = null;
        depthDecoder = null;
        preapared = false;

        ringBuffer = new RingBuffer((int)(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND / sizeof(float)));

        videoMessages = new Dictionary<int, VideoSenderMessageData>();
        frameStopWatch = Stopwatch.StartNew();
    }

    public void UpdateFrame()
    {
        // If texture is not created, create and assign them to quads.
        if (!preapared)
        {
            // Check whether the native plugin has Direct3D textures that
            // can be connected to Unity textures.
            if (textureGroup.IsInitialized())
            {
                // TextureGroup includes Y, U, V, and a depth texture.
                azureKinectScreenMaterial.SetTexture("_YTex", textureGroup.GetYTexture());
                azureKinectScreenMaterial.SetTexture("_UTex", textureGroup.GetUTexture());
                azureKinectScreenMaterial.SetTexture("_VTex", textureGroup.GetVTexture());
                azureKinectScreenMaterial.SetTexture("_DepthTex", textureGroup.GetDepthTexture());

                preapared = true;

                UnityEngine.Debug.Log("textureGroup intialized");
            }
        }

        if (udpSocket == null)
            return;

        UpdateTextureGroup();
    }

    public void Stop()
    {
        receiverStopped = true;
    }

    public void Ping(UdpSocket udpSocket)
    {
        int senderSessionId = -1;
        int pingCount = 0;
        while (true)
        {
            bool initialized = false;
            udpSocket.Send(PacketHelper.createPingReceiverPacketBytes());
            ++pingCount;
            UnityEngine.Debug.Log("Sent ping");

            //Thread.Sleep(100);
            Thread.Sleep(300);

            SocketError error = SocketError.WouldBlock;
            while (true)
            {
                var packet = udpSocket.Receive(out error);
                if (packet == null)
                    break;

                int cursor = 0;
                int sessionId = BitConverter.ToInt32(packet, cursor);
                cursor += 4;

                var packetType = (SenderPacketType)packet[cursor];
                cursor += 1;
                if (packetType != SenderPacketType.Init)
                {
                    UnityEngine.Debug.Log($"A different kind of a packet received before an init packet: {packetType}");
                    continue;
                }

                senderSessionId = sessionId;

                var initSenderPacketData = InitSenderPacketData.Parse(packet);

                textureGroup.SetWidth(initSenderPacketData.depthWidth);
                textureGroup.SetHeight(initSenderPacketData.depthHeight);
                PluginHelper.InitTextureGroup();

                colorDecoder = new Vp8Decoder();
                depthDecoder = new TrvlDecoder(initSenderPacketData.depthWidth * initSenderPacketData.depthHeight);

                azureKinectScreen.Setup(initSenderPacketData);

                initialized = true;
                break;
            }
            if (initialized)
                break;

            if (pingCount == 10)
            {
                UnityEngine.Debug.Log("Tried pinging 10 times and failed to received an init packet...\n");
                return;
            }
        }

        this.udpSocket = udpSocket;

        var taskThread = new Thread(() =>
        {
            var senderPacketReceiver = new SenderPacketReceiver();
            var videoMessageReassembler = new VideoMessageReassembler();
            var audioPacketCollector = new AudioPacketCollector();

            while(!receiverStopped)
            {
                senderPacketReceiver.Receive(udpSocket, senderSessionId);
                videoMessageReassembler.Reassemble(udpSocket, senderPacketReceiver.VideoPacketDataQueue,
                    senderPacketReceiver.FecPacketDataQueue, lastVideoFrameId, videoMessageQueue);
                audioPacketCollector.Collect(senderPacketReceiver.AudioPacketDataQueue, ringBuffer);
            }

            receiverStopped = true;
        });

        taskThread.Start();
    }

    private void UpdateTextureGroup()
    {
        {
            Tuple<int, VideoSenderMessageData> frameMessagePair;
            while (videoMessageQueue.TryDequeue(out frameMessagePair))
            {
                // C# Dictionary throws an error when you add an element with
                // a key that is already taken.
                if (videoMessages.ContainsKey(frameMessagePair.Item1))
                    continue;

                videoMessages.Add(frameMessagePair.Item1, frameMessagePair.Item2);
            }
        }

        if (videoMessages.Count == 0)
        {
            return;
        }

        int? beginFrameId = null;
        // If there is a key frame, use the most recent one.
        foreach (var frameMessagePair in videoMessages)
        {
            if (frameMessagePair.Key <= lastVideoFrameId)
                continue;

            if (frameMessagePair.Value.keyframe)
                beginFrameId = frameMessagePair.Key;
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!beginFrameId.HasValue)
        {
            if (videoMessages.ContainsKey(lastVideoFrameId + 1))
            {
                beginFrameId = lastVideoFrameId + 1;
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
            if (!videoMessages.ContainsKey(i))
                break;

            var frameMessage = videoMessages[i];
            lastVideoFrameId = i;

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

        udpSocket.Send(PacketHelper.createReportReceiverPacketBytes(lastVideoFrameId,
                                                                    (float)decoderTime.TotalMilliseconds,
                                                                    (float)frameTime.TotalMilliseconds));

        // Invokes a function to be called in a render thread.
        if (preapared)
        {
            //Plugin.texture_group_set_ffmpeg_frame(textureGroup, ffmpegFrame.Ptr);
            textureGroup.SetFFmpegFrame(ffmpegFrame);
            //Plugin.texture_group_set_depth_pixels(textureGroup, trvlFrame.Ptr);
            textureGroup.SetTrvlFrame(trvlFrame);
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
            if (key < lastVideoFrameId)
            {
                videoMessages.Remove(key);
            }
        }
    }
}
