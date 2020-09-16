using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
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

    public int ReceiverId { get; private set; }
    public int SenderId { get; private set; }
    public IPEndPoint SenderEndPoint { get; private set; }
    public PrepareState State { get; private set; }
    
    public KinectOrigin KinectOrigin { get; private set; }
    public TextureSetUpdater TextureGroupUpdater { get; private set; }
    private VideoMessageAssembler videoMessageAssembler;
    private AudioReceiver audioPacketReceiver;
    private Stopwatch heartbeatStopWatch;
    private Stopwatch receivedAnyStopWatch;

    public KinectReceiver(int receiverId, int senderId, IPEndPoint senderEndPoint)
    {
        ReceiverId = receiverId;
        SenderId = senderId;
        SenderEndPoint = senderEndPoint;
        State = PrepareState.Unprepared;
    }

    public void Prepare(KinectOrigin kinectOrigin)
    {
        State = PrepareState.Prepared;
        KinectOrigin = kinectOrigin;
        TextureGroupUpdater = new TextureSetUpdater(kinectOrigin.Screen.Material, ReceiverId, SenderEndPoint);
        videoMessageAssembler = new VideoMessageAssembler(ReceiverId, SenderEndPoint);
        audioPacketReceiver = new AudioReceiver();
        heartbeatStopWatch = Stopwatch.StartNew();
        receivedAnyStopWatch = Stopwatch.StartNew();
    }

    public bool UpdateFrame(UdpSocket udpSocket, SenderPacketSet senderPacketSet)
    {
        // UpdateFrame() should not be called before Prepare().
        Assert.AreEqual(PrepareState.Prepared, State);

        var videoMessages = new SortedDictionary<int, VideoSenderMessage>();
        try
        {
            if (heartbeatStopWatch.Elapsed.TotalSeconds > HEARTBEAT_INTERVAL_SEC)
            {
                udpSocket.Send(PacketUtils.createHeartbeatReceiverPacketBytes(ReceiverId), SenderEndPoint);
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
                                               senderPacketSet.ParityPacketDataList,
                                               TextureGroupUpdater.lastFrameId,
                                               videoMessages);

                if (videoMessages.Count > 0)
                {
                    if (KinectOrigin.Screen.State == PrepareState.Unprepared)
                    {
                        foreach (var videoMessagePair in videoMessages)
                        {
                            KinectOrigin.Screen.StartPrepare(videoMessagePair.Value);
                            TextureGroupUpdater.StartPrepare(videoMessagePair.Value);
                            break;
                        }
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

        KinectOrigin.UpdateFrame(videoMessages);
        TextureGroupUpdater.UpdateFrame(udpSocket, videoMessages);

        return true;
    }
}
