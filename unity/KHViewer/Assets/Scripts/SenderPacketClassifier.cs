using System.Collections.Generic;
using System.Net;
using TelepresenceToolkit;

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
        foreach (var receiver in receivers)
            senderPacketInfos.Add(receiver.SenderId, new SenderPacketInfo());

        while (true)
        {
            var packet = udpSocket.Receive();
            if (packet == null)
                break;

            int senderId = PacketUtils.getSessionIdFromSenderPacketBytes(packet.Bytes);
            var packetType = PacketUtils.getPacketTypeFromSenderPacketBytes(packet.Bytes);

            if (packetType == SenderPacketType.Confirm)
            {
                confirmPacketInfos.Add(new ConfirmPacketInfo(packet.EndPoint, ConfirmSenderPacket.Create(packet.Bytes)));
                continue;
            }

            // Ignore packet if it is sent from a sender without a corresponding receiver.
            if (!senderPacketInfos.TryGetValue(senderId, out SenderPacketInfo senderPacketInfo))
                continue;

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
}