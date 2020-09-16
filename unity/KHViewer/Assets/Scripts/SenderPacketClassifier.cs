﻿using System.Collections.Generic;
using System.Net;

public class ConfirmPacketInfo
{
    public IPEndPoint SenderEndPoint { get; private set; }
    public ConfirmSenderPacket ConfirmPacketData { get; private set; }

    public ConfirmPacketInfo(IPEndPoint senderEndPoint, ConfirmSenderPacket confirmPacketData)
    {
        SenderEndPoint = senderEndPoint;
        ConfirmPacketData = confirmPacketData;
    }
}

public class SenderPacketSet
{
    public bool ReceivedAny { get; set; }
    public List<VideoSenderPacket> VideoPacketDataList { get; private set; }
    public List<ParitySenderPacketData> ParityPacketDataList { get; private set; }
    public List<AudioSenderPacket> AudioPacketDataList { get; private set; }

    public SenderPacketSet()
    {
        ReceivedAny = false;
        VideoPacketDataList = new List<VideoSenderPacket>();
        ParityPacketDataList = new List<ParitySenderPacketData>();
        AudioPacketDataList = new List<AudioSenderPacket>();
    }
}

public class SenderPacketCollection
{
    public List<ConfirmPacketInfo> ConfirmPacketInfoList { get; private set; }
    public Dictionary<int, SenderPacketSet> SenderPacketSets { get; private set; }

    public SenderPacketCollection()
    {
        ConfirmPacketInfoList = new List<ConfirmPacketInfo>();
        SenderPacketSets = new Dictionary<int, SenderPacketSet>();
    }
};

public static class SenderPacketClassifier
{
    public static SenderPacketCollection Classify(UdpSocket udpSocket, List<KinectReceiver> kinectReceivers)
    {
        var senderEndPoints = new List<IPEndPoint>();
        var senderPacketCollection = new SenderPacketCollection();
        foreach (var kinectReceiver in kinectReceivers)
        {
            senderEndPoints.Add(kinectReceiver.SenderEndPoint);
            senderPacketCollection.SenderPacketSets.Add(kinectReceiver.SenderId, new SenderPacketSet());
        }

        // During this loop, SocketExceptions will have endpoint information.
        foreach(var senderEndPoint in senderEndPoints)
        {
            while(true)
            {
                var packet = udpSocket.ReceiveFrom(senderEndPoint);
                if (packet == null)
                    break;

                CollectPacket(packet, senderPacketCollection);
            }
        }

        // During this loop, SocketExceptions won't have endpoint information but connection messages will be received.
        while(true)
        {
            var packet = udpSocket.Receive();
            if (packet == null)
                break;

            CollectPacket(packet, senderPacketCollection);
        }

        return senderPacketCollection;
    }

    private static void CollectPacket(UdpSocketPacket packet, SenderPacketCollection senderPacketCollection)
    {
        int senderSessionId = PacketUtils.getSessionIdFromSenderPacketBytes(packet.Bytes);
        SenderPacketType packetType = PacketUtils.getPacketTypeFromSenderPacketBytes(packet.Bytes);

        if (packetType == SenderPacketType.Confirm)
        {
            senderPacketCollection.ConfirmPacketInfoList.Add(new ConfirmPacketInfo(packet.EndPoint, ConfirmSenderPacket.Create(packet.Bytes)));
            return;
        }

        SenderPacketSet senderPacketSet;
        if (!senderPacketCollection.SenderPacketSets.TryGetValue(senderSessionId, out senderPacketSet))
            return;

        // Heartbeat packets turns on ReceivedAny.
        senderPacketSet.ReceivedAny = true;
        switch (packetType)
        {
            case SenderPacketType.Video:
                senderPacketSet.VideoPacketDataList.Add(VideoSenderPacket.Create(packet.Bytes));
                break;
            case SenderPacketType.Parity:
                senderPacketSet.ParityPacketDataList.Add(ParitySenderPacketData.Create(packet.Bytes));
                break;
            case SenderPacketType.Audio:
                senderPacketSet.AudioPacketDataList.Add(AudioSenderPacket.Create(packet.Bytes));
                break;
        }
    }
}