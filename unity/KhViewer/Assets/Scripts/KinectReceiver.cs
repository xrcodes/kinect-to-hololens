using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using UnityEngine;
using UnityEngine.Assertions;

public enum PrepareState
{
    Unprepared,
    Preparing,
    Prepared
}

public class KinectReceiver
{
    private const float HEARTBEAT_INTERVAL_SEC = 1.0f;
    private const float HEARTBEAT_TIME_OUT_SEC = 5.0f;

    public int ReceiverSessionId { get; private set; }
    public IPEndPoint SenderEndPoint { get; private set; }
    public PrepareState State { get; private set; }
    
    public KinectOrigin KinectOrigin { get; private set; }
    public TextureGroupUpdater TextureGroupUpdater { get; private set; }
    private VideoMessageAssembler videoMessageAssembler;
    private AudioPacketReceiver audioPacketReceiver;
    private Stopwatch heartbeatStopWatch;
    private Stopwatch receivedAnyStopWatch;

    public KinectReceiver(int receiverSessionId, IPEndPoint senderEndPoint)
    {
        ReceiverSessionId = receiverSessionId;
        SenderEndPoint = senderEndPoint;
        State = PrepareState.Unprepared;
    }

    public void Prepare(KinectOrigin kinectOrigin)
    {
        State = PrepareState.Prepared;
        KinectOrigin = kinectOrigin;
        TextureGroupUpdater = new TextureGroupUpdater(kinectOrigin.Screen.Material, ReceiverSessionId, SenderEndPoint);
        videoMessageAssembler = new VideoMessageAssembler(ReceiverSessionId, SenderEndPoint);
        audioPacketReceiver = new AudioPacketReceiver();
        heartbeatStopWatch = Stopwatch.StartNew();
        receivedAnyStopWatch = Stopwatch.StartNew();
    }

    public bool UpdateFrame(MonoBehaviour monoBehaviour, UdpSocket udpSocket, SenderPacketSet senderPacketSet)
    {
        // UpdateFrame() should not be called before Prepare().
        Assert.AreEqual(PrepareState.Prepared, State);

        var videoMessageList = new List<Tuple<int, VideoSenderMessageData>>();
        try
        {
            if (heartbeatStopWatch.Elapsed.TotalSeconds > HEARTBEAT_INTERVAL_SEC)
            {
                udpSocket.Send(PacketHelper.createHeartbeatReceiverPacketBytes(ReceiverSessionId), SenderEndPoint);
                heartbeatStopWatch = Stopwatch.StartNew();
            }

            if (senderPacketSet.ReceivedAny)
            {
                // Use init packet to prepare rendering video messages.
                //if (senderPacketSet.InitPacketDataList.Count > 0)
                //{
                //    if (KinectOrigin.Screen.State == PrepareState.Unprepared)
                //    {
                //        KinectOrigin.Screen.StartPrepare(senderPacketSet.InitPacketDataList[0]);
                //        TextureGroupUpdater.StartPrepare(monoBehaviour, senderPacketSet.InitPacketDataList[0]);
                //    }
                //}

                videoMessageAssembler.Assemble(udpSocket,
                                               senderPacketSet.VideoPacketDataList,
                                               senderPacketSet.FecPacketDataList,
                                               TextureGroupUpdater.lastVideoFrameId,
                                               videoMessageList);

                if (videoMessageList.Count > 0)
                {
                    if (KinectOrigin.Screen.State == PrepareState.Unprepared)
                    {
                        KinectOrigin.Screen.StartPrepare(videoMessageList[0].Item2);
                        TextureGroupUpdater.StartPrepare(monoBehaviour, videoMessageList[0].Item2);
                    }
                }

                audioPacketReceiver.Receive(senderPacketSet.AudioPacketDataList, KinectOrigin.Speaker.RingBuffer);
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

        if (KinectOrigin.Screen.State == PrepareState.Preparing)
        {
            KinectOrigin.SetProgressText(SenderEndPoint, KinectOrigin.screen.Progress);
            KinectOrigin.ProgressTextVisibility = true;
        }
        else if(KinectOrigin.Screen.State == PrepareState.Prepared)
        {
            KinectOrigin.ProgressTextVisibility = false;
        }

        KinectOrigin.UpdateFrame(videoMessageList);
        TextureGroupUpdater.UpdateFrame(udpSocket, videoMessageList);

        return true;
    }
}
