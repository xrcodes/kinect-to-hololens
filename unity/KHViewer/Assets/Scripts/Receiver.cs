using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
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
    public SortedDictionary<int, VideoSenderMessage> VideoMessages { get; private set; }
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
        VideoMessages = new SortedDictionary<int, VideoSenderMessage>(); ;

        heartbeatStopWatch = Stopwatch.StartNew();
        receivedAnyStopWatch = Stopwatch.StartNew();
    }

    public void SendHeartBeat(UdpSocket udpSocket)
    {
        try
        {
            if (heartbeatStopWatch.Elapsed.TotalSeconds > HEARTBEAT_INTERVAL_SEC)
            {
                udpSocket.Send(PacketUtils.createHeartbeatReceiverPacketBytes(ReceiverId).bytes, SenderEndPoint);
                heartbeatStopWatch = Stopwatch.StartNew();
            }
        }
        catch (UdpSocketException e)
        {
            UnityEngine.Debug.Log($"UdpSocketRuntimeError: {e}");
        }
    }

    public void ReceivePackets(UdpSocket udpSocket, SenderPacketInfo senderPacketSet, KinectOrigin kinectOrigin)
    {
        if (senderPacketSet.ReceivedAny)
            receivedAnyStopWatch = Stopwatch.StartNew();

        videoMessageAssembler.Assemble(udpSocket,
                                       senderPacketSet.VideoPackets,
                                       senderPacketSet.ParityPackets,
                                       TextureSetUpdater.lastFrameId,
                                       VideoMessages);

        audioReceiver.Receive(senderPacketSet.AudioPackets, kinectOrigin.Speaker.RingBuffer);

        if (TextureSetUpdater.State == PrepareState.Unprepared && VideoMessages.Count > 0)
        {
            foreach (var videoMessage in VideoMessages.Values)
            {
                TextureSetUpdater.StartPrepare(kinectOrigin.Screen.Material, videoMessage);
                break;
            }
        }
    }

    public void UpdateFrame(UdpSocket udpSocket)
    {
        TextureSetUpdater.UpdateFrame(udpSocket, VideoMessages);

        // Remove obsolete video messages.
        foreach (var videoMessageFrameId in VideoMessages.Keys.ToList())
        {
            if(videoMessageFrameId <= TextureSetUpdater.lastFrameId)
                VideoMessages.Remove(videoMessageFrameId);
        }
    }

    public bool IsTimedOut()
    {
        return receivedAnyStopWatch.Elapsed.TotalSeconds > HEARTBEAT_TIME_OUT_SEC;
    }
}
