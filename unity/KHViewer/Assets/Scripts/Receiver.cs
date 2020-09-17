using System.Collections.Generic;
using System.Diagnostics;
using System.Net;

public enum PrepareState
{
    Unprepared,
    Preparing,
    Prepared
}

public class Receiver
{
    private const float HEARTBEAT_INTERVAL_SEC = 1.0f;
    private const float HEARTBEAT_TIME_OUT_SEC = 5.0f;

    public int ReceiverId { get; private set; }
    public int SenderId { get; private set; }
    public IPEndPoint SenderEndPoint { get; private set; }
    public TextureSetUpdater TextureSetUpdater { get; private set; }
    private VideoMessageAssembler videoMessageAssembler;
    private AudioReceiver audioReceiver;
    private Stopwatch heartbeatStopWatch;
    private Stopwatch receivedAnyStopWatch;

    public Receiver(int receiverId, int senderId, IPEndPoint senderEndPoint)
    {
        ReceiverId = receiverId;
        SenderId = senderId;
        SenderEndPoint = senderEndPoint;
        TextureSetUpdater = new TextureSetUpdater(ReceiverId, SenderEndPoint);
        videoMessageAssembler = new VideoMessageAssembler(ReceiverId, SenderEndPoint);
        audioReceiver = new AudioReceiver();
        heartbeatStopWatch = Stopwatch.StartNew();
        receivedAnyStopWatch = Stopwatch.StartNew();
    }

    public bool UpdateFrame(UdpSocket udpSocket, SenderPacketInfo senderPacketSet, KinectOrigin kinectOrigin)
    {
        var videoMessages = new SortedDictionary<int, VideoSenderMessage>();
        try
        {
            if (heartbeatStopWatch.Elapsed.TotalSeconds > HEARTBEAT_INTERVAL_SEC)
            {
                udpSocket.Send(PacketUtils.createHeartbeatReceiverPacketBytes(ReceiverId).bytes, SenderEndPoint);
                heartbeatStopWatch = Stopwatch.StartNew();
            }

            if (senderPacketSet.ReceivedAny)
            {
                videoMessageAssembler.Assemble(udpSocket,
                                               senderPacketSet.VideoPackets,
                                               senderPacketSet.ParityPackets,
                                               TextureSetUpdater.lastFrameId,
                                               videoMessages);

                if (videoMessages.Count > 0)
                {
                    if (kinectOrigin.Screen.State == PrepareState.Unprepared)
                    {
                        foreach (var videoMessagePair in videoMessages)
                        {
                            kinectOrigin.Screen.StartPrepare(videoMessagePair.Value);
                            TextureSetUpdater.StartPrepare(kinectOrigin.Screen.Material, videoMessagePair.Value);
                            break;
                        }
                    }
                }

                audioReceiver.Receive(senderPacketSet.AudioPackets, kinectOrigin.Speaker.RingBuffer);
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

        if (kinectOrigin.Screen.State == PrepareState.Preparing)
        {
            kinectOrigin.SetProgressText(SenderEndPoint, kinectOrigin.screen.Progress);
            kinectOrigin.ProgressTextVisibility = true;
        }
        else if(kinectOrigin.Screen.State == PrepareState.Prepared)
        {
            kinectOrigin.ProgressTextVisibility = false;
        }

        kinectOrigin.UpdateFrame(videoMessages);
        TextureSetUpdater.UpdateFrame(udpSocket, videoMessages);

        return true;
    }
}
