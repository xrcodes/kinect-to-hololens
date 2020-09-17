using System.Collections.Generic;
using System.Net;

public class ConfirmPacketInfo
{
    public IPEndPoint SenderEndPoint { get; private set; }
    public ConfirmSenderPacket ConfirmPacket { get; private set; }

    public ConfirmPacketInfo(IPEndPoint senderEndPoint, ConfirmSenderPacket confirmPacket)
    {
        SenderEndPoint = senderEndPoint;
        ConfirmPacket = confirmPacket;
    }
}

public class SenderPacketInfo
{
    public bool ReceivedAny { get; set; }
    public List<VideoSenderPacket> VideoPackets { get; private set; }
    public List<ParitySenderPacket> ParityPackets { get; private set; }
    public List<AudioSenderPacket> AudioPackets { get; private set; }

    public SenderPacketInfo()
    {
        ReceivedAny = false;
        VideoPackets = new List<VideoSenderPacket>();
        ParityPackets = new List<ParitySenderPacket>();
        AudioPackets = new List<AudioSenderPacket>();
    }
}

public static class SenderPacketClassifier
{
    public static void Classify(UdpSocket udpSocket, ICollection<Receiver> kinectReceivers,
                                out List<ConfirmPacketInfo> confirmPacketInfos,
                                out Dictionary<int, SenderPacketInfo> senderPacketInfos)
    {
        confirmPacketInfos = new List<ConfirmPacketInfo>();
        senderPacketInfos = new Dictionary<int, SenderPacketInfo>();

        var senderEndPoints = new List<IPEndPoint>();
        foreach (var kinectReceiver in kinectReceivers)
        {
            senderEndPoints.Add(kinectReceiver.SenderEndPoint);
            senderPacketInfos.Add(kinectReceiver.SenderId, new SenderPacketInfo());
        }

        // During this loop, SocketExceptions will have endpoint information.
        foreach(var senderEndPoint in senderEndPoints)
        {
            while(true)
            {
                var packet = udpSocket.ReceiveFrom(senderEndPoint);
                if (packet == null)
                    break;

                CollectPacket(packet, confirmPacketInfos, senderPacketInfos);
            }
        }

        // During this loop, SocketExceptions won't have endpoint information but connection messages will be received.
        while(true)
        {
            var packet = udpSocket.Receive();
            if (packet == null)
                break;

            CollectPacket(packet, confirmPacketInfos, senderPacketInfos);
        }
    }

    private static void CollectPacket(UdpSocketPacket packet,
                                      List<ConfirmPacketInfo> confirmPacketInfos,
                                      Dictionary<int, SenderPacketInfo> senderPacketInfos)
    {
        int senderSessionId = PacketUtils.getSessionIdFromSenderPacketBytes(packet.Bytes);
        var packetType = PacketUtils.getPacketTypeFromSenderPacketBytes(packet.Bytes);

        if (packetType == SenderPacketType.Confirm)
        {
            confirmPacketInfos.Add(new ConfirmPacketInfo(packet.EndPoint, ConfirmSenderPacket.Create(packet.Bytes)));
            return;
        }

        if (!senderPacketInfos.TryGetValue(senderSessionId, out SenderPacketInfo senderPacketInfo))
            return;

        // Heartbeat packets turns on ReceivedAny.
        senderPacketInfo.ReceivedAny = true;
        switch (packetType)
        {
            case SenderPacketType.Video:
                senderPacketInfo.VideoPackets.Add(VideoSenderPacket.Create(packet.Bytes));
                break;
            case SenderPacketType.Parity:
                senderPacketInfo.ParityPackets.Add(ParitySenderPacket.Create(packet.Bytes));
                break;
            case SenderPacketType.Audio:
                senderPacketInfo.AudioPackets.Add(AudioSenderPacket.Create(packet.Bytes));
                break;
        }
    }
}