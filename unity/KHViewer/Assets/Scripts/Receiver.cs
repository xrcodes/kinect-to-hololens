using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using TelepresenceToolkit;

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
    public SortedDictionary<int, VideoSenderMessage> VideoMessages { get; private set; }

    private TextureSetUpdater textureSetUpdater;
    private VideoMessageAssembler videoMessageAssembler;
    private AudioReceiver audioReceiver;

    private Stopwatch heartbeatStopWatch;
    private Stopwatch receivedAnyStopWatch;

    public Receiver(int receiverId, int senderId, IPEndPoint senderEndPoint)
    {
        ReceiverId = receiverId;
        SenderId = senderId;
        SenderEndPoint = senderEndPoint;
        VideoMessages = new SortedDictionary<int, VideoSenderMessage>();

        textureSetUpdater = new TextureSetUpdater(ReceiverId, SenderEndPoint);
        videoMessageAssembler = new VideoMessageAssembler(ReceiverId, SenderEndPoint);
        audioReceiver = new AudioReceiver();

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

    public void ReceivePackets(UdpSocket udpSocket, SenderPacketInfo senderPacketSet, KinectNode kinectNode)
    {
        if (senderPacketSet.ReceivedAny)
            receivedAnyStopWatch = Stopwatch.StartNew();

        videoMessageAssembler.Assemble(udpSocket,
                                       senderPacketSet.VideoPackets,
                                       senderPacketSet.ParityPackets,
                                       textureSetUpdater.lastFrameId,
                                       VideoMessages);

        audioReceiver.Receive(senderPacketSet.AudioPackets, kinectNode.Speaker.RingBuffer);

        if (textureSetUpdater.State == PrepareState.Unprepared && VideoMessages.Count > 0)
        {
            foreach (var videoMessage in VideoMessages.Values)
            {
                CoroutineRunner.Run(textureSetUpdater.Prepare(kinectNode.KinectRenderer.Material, videoMessage));
                break;
            }
        }
    }

    public void UpdateFrame(UdpSocket udpSocket)
    {
        textureSetUpdater.UpdateFrame(udpSocket, VideoMessages);

        // Remove obsolete video messages.
        foreach (var videoMessageFrameId in VideoMessages.Keys.ToList())
        {
            if(videoMessageFrameId <= textureSetUpdater.lastFrameId)
                VideoMessages.Remove(videoMessageFrameId);
        }
    }

    public bool IsTimedOut()
    {
        return receivedAnyStopWatch.Elapsed.TotalSeconds > HEARTBEAT_TIME_OUT_SEC;
    }
}
