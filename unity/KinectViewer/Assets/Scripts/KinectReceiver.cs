using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;

public class KinectReceiver
{
    private const float HEARTBEAT_INTERVAL_SEC = 1.0f;
    private const float HEARTBEAT_TIME_OUT_SEC = 5.0f;

    private AzureKinectRoot azureKinectRoot;
    private UdpSocket udpSocket;
    private int sessionId;
    private IPEndPoint endPoint;
    private VideoMessageAssembler videoMessageAssembler;
    private AudioPacketReceiver audioPacketReceiver;
    private TextureGroupUpdater textureGroupUpdater;
    private Stopwatch heartbeatStopWatch;
    private Stopwatch receivedAnyStopWatch;
    private ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue;
    private ConcurrentQueue<FloorSenderPacketData> floorPacketDataQueue;

    public KinectReceiver(AzureKinectRoot azureKinectRoot, UdpSocket udpSocket, int sessionId, IPEndPoint endPoint, InitSenderPacketData initPacketData)
    {
        this.azureKinectRoot = azureKinectRoot;
        this.udpSocket = udpSocket;
        this.sessionId = sessionId;
        this.endPoint = endPoint;
        videoMessageAssembler = new VideoMessageAssembler(sessionId, endPoint);
        audioPacketReceiver = new AudioPacketReceiver();
        textureGroupUpdater = new TextureGroupUpdater(azureKinectRoot.Screen.Material, initPacketData, udpSocket, sessionId, endPoint);
        heartbeatStopWatch = Stopwatch.StartNew();
        receivedAnyStopWatch = Stopwatch.StartNew();
        videoMessageQueue = new ConcurrentQueue<Tuple<int, VideoSenderMessageData>>();
        floorPacketDataQueue = new ConcurrentQueue<FloorSenderPacketData>();
    }

    public bool UpdateFrame()
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
                                               textureGroupUpdater.lastVideoFrameId,
                                               videoMessageQueue);
                audioPacketReceiver.Receive(senderPacketSet.AudioPacketDataList, azureKinectRoot.Speaker.RingBuffer);
                receivedAnyStopWatch = Stopwatch.StartNew();
            }
            else
            {
                if (receivedAnyStopWatch.Elapsed.TotalSeconds > HEARTBEAT_TIME_OUT_SEC)
                {
                    UnityEngine.Debug.Log($"Timed out after waiting for {HEARTBEAT_TIME_OUT_SEC} seconds without a received packet.");
                    return false;
                }
            }
        }
        catch (UdpSocketException e)
        {
            UnityEngine.Debug.Log($"UdpSocketRuntimeError: {e}");
            return false;
        }

        textureGroupUpdater.UpdateFrame(videoMessageQueue);
        azureKinectRoot.UpdateFrame(floorPacketDataQueue);

        return true;
    }
}
