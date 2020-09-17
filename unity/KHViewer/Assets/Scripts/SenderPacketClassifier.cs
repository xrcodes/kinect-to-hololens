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
    public static void Classify(UdpSocket udpSocket, ICollection<Receiver> receivers,
                                List<ConfirmPacketInfo> confirmPacketInfos,
                                Dictionary<int, SenderPacketInfo> senderPacketInfos)
    {
        // Loop for receiving packets with with specific end points.
        foreach(var receiver in receivers)
        {
            senderPacketInfos.Add(receiver.SenderId, new SenderPacketInfo());
            while (true)
            {
                var packet = udpSocket.ReceiveFrom(receiver.SenderEndPoint);
                if (packet == null)
                    break;

                CollectPacket(packet, confirmPacketInfos, senderPacketInfos);
            }
        }

        // During this loop, SocketExceptions won't have endpoint information but connection messages will be received.
        while(true)
        {
            var packet = udpSocket.ReceiveFromAny();
            if (packet == null)
                break;

            CollectPacket(packet, confirmPacketInfos, senderPacketInfos);
        }
    }

    private static void CollectPacket(UdpSocketPacket packet,
                                      List<ConfirmPacketInfo> confirmPacketInfos,
                                      Dictionary<int, SenderPacketInfo> senderPacketInfos)
    {
        int senderId = PacketUtils.getSessionIdFromSenderPacketBytes(packet.Bytes);
        var packetType = PacketUtils.getPacketTypeFromSenderPacketBytes(packet.Bytes);

        if (packetType == SenderPacketType.Confirm)
        {
            confirmPacketInfos.Add(new ConfirmPacketInfo(packet.EndPoint, ConfirmSenderPacket.Create(packet.Bytes)));
            return;
        }

        // Ignore packet if it is sent from a sender without a corresponding receiver.
        if (!senderPacketInfos.TryGetValue(senderId, out SenderPacketInfo senderPacketInfo))
            return;

        senderPacketInfo.ReceivedAny = true;
        switch (packetType)
        {
            case SenderPacketType.Heartbeat:
                break;
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