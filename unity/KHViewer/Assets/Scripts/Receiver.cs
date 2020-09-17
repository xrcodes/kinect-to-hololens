using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using UnityEngine;
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

    private VideoMessageAssembler videoMessageAssembler;
    private TextureSetUpdater textureSetUpdater;
    private AudioReceiver audioReceiver;

    private Stopwatch heartbeatStopWatch;
    private Stopwatch receivedAnyStopWatch;

    public Receiver(int receiverId, int senderId, IPEndPoint senderEndPoint)
    {
        ReceiverId = receiverId;
        SenderId = senderId;
        SenderEndPoint = senderEndPoint;
        VideoMessages = new SortedDictionary<int, VideoSenderMessage>();

        videoMessageAssembler = new VideoMessageAssembler(ReceiverId, SenderEndPoint);
        textureSetUpdater = new TextureSetUpdater(ReceiverId, SenderEndPoint);
        audioReceiver = new AudioReceiver();

        heartbeatStopWatch = Stopwatch.StartNew();
        receivedAnyStopWatch = Stopwatch.StartNew();
    }

    public void SendHeartBeat(UdpSocket udpSocket)
    {
        if (heartbeatStopWatch.Elapsed.TotalSeconds > HEARTBEAT_INTERVAL_SEC)
        {
            udpSocket.Send(PacketUtils.createHeartbeatReceiverPacketBytes(ReceiverId).bytes, SenderEndPoint);
            heartbeatStopWatch = Stopwatch.StartNew();
        }
    }

    public void ReceivePackets(UdpSocket udpSocket, SenderPacketInfo senderPacketSet, Material material, RingBuffer ringBuffer)
    {
        videoMessageAssembler.Assemble(udpSocket,
                                       senderPacketSet.VideoPackets,
                                       senderPacketSet.ParityPackets,
                                       textureSetUpdater.lastFrameId,
                                       VideoMessages);

        if (textureSetUpdater.State == PrepareState.Unprepared && VideoMessages.Count > 0)
        {
            foreach (var videoMessage in VideoMessages.Values)
            {
                CoroutineRunner.Run(textureSetUpdater.Prepare(material, videoMessage));
                break;
            }
        }

        audioReceiver.Receive(senderPacketSet.AudioPackets, ringBuffer);

        if (senderPacketSet.ReceivedAny)
            receivedAnyStopWatch = Stopwatch.StartNew();
    }

    public bool IsTimedOut()
    {
        return receivedAnyStopWatch.Elapsed.TotalSeconds > HEARTBEAT_TIME_OUT_SEC;
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
}
