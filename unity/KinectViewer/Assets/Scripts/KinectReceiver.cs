using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;

public class KinectReceiver
{
    private const float HEARTBEAT_INTERVAL_SEC = 1.0f;
    private const float HEARTBEAT_TIME_OUT_SEC = 5.0f;

    public readonly int SessionId;
    public readonly IPEndPoint SenderEndPoint;
    private KinectOrigin kinectOrigin;
    private VideoMessageAssembler videoMessageAssembler;
    private AudioPacketReceiver audioPacketReceiver;
    private TextureGroupUpdater textureGroupUpdater;
    private Stopwatch heartbeatStopWatch;
    private Stopwatch receivedAnyStopWatch;

    public KinectReceiver(int sessionId, IPEndPoint senderEndPoint, KinectOrigin kinectOrigin, InitSenderPacketData initPacketData)
    {
        SessionId = sessionId;
        SenderEndPoint = senderEndPoint;
        this.kinectOrigin = kinectOrigin;
        videoMessageAssembler = new VideoMessageAssembler(sessionId, senderEndPoint);
        audioPacketReceiver = new AudioPacketReceiver();
        textureGroupUpdater = new TextureGroupUpdater(kinectOrigin.Screen.Material, initPacketData, sessionId, senderEndPoint);
        heartbeatStopWatch = Stopwatch.StartNew();
        receivedAnyStopWatch = Stopwatch.StartNew();
    }

    public bool UpdateFrame(UdpSocket udpSocket, SenderPacketSet senderPacketSet)
    {
        var videoMessageList = new List<Tuple<int, VideoSenderMessageData>>();
        try
        {
            if (heartbeatStopWatch.Elapsed.TotalSeconds > HEARTBEAT_INTERVAL_SEC)
            {
                udpSocket.Send(PacketHelper.createHeartbeatReceiverPacketBytes(SessionId), SenderEndPoint);
                heartbeatStopWatch = Stopwatch.StartNew();
            }

            if (senderPacketSet.ReceivedAny)
            {
                videoMessageAssembler.Assemble(udpSocket,
                                               senderPacketSet.VideoPacketDataList,
                                               senderPacketSet.FecPacketDataList,
                                               textureGroupUpdater.lastVideoFrameId,
                                               videoMessageList);
                audioPacketReceiver.Receive(senderPacketSet.AudioPacketDataList, kinectOrigin.Speaker.RingBuffer);
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

        textureGroupUpdater.UpdateFrame(udpSocket, videoMessageList);
        kinectOrigin.UpdateFrame(senderPacketSet.FloorPacketDataList);

        return true;
    }
}
