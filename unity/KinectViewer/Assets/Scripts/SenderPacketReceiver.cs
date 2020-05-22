using System.Collections.Generic;
using System.Net;

public class ConfirmPacketInfo
{
    public IPEndPoint SenderEndPoint { get; private set; }
    public int SenderSessionId { get; private set; }
    public ConfirmSenderPacketData ConfirmPacketData { get; private set; }

    public ConfirmPacketInfo(IPEndPoint senderEndPoint, int senderSessionId, ConfirmSenderPacketData confirmPacketData)
    {
        SenderEndPoint = senderEndPoint;
        SenderSessionId = senderSessionId;
        ConfirmPacketData = confirmPacketData;
    }
}

public class SenderPacketSet
{
    public bool ReceivedAny { get; set; }
    public List<VideoInitSenderPacketData> InitPacketDataList { get; private set; }
    public List<VideoSenderPacketData> VideoPacketDataList { get; private set; }
    public List<ParitySenderPacketData> FecPacketDataList { get; private set; }
    public List<AudioSenderPacketData> AudioPacketDataList { get; private set; }
    public List<FloorSenderPacketData> FloorPacketDataList { get; private set; }

    public SenderPacketSet()
    {
        ReceivedAny = false;
        InitPacketDataList = new List<VideoInitSenderPacketData>();
        VideoPacketDataList = new List<VideoSenderPacketData>();
        FecPacketDataList = new List<ParitySenderPacketData>();
        AudioPacketDataList = new List<AudioSenderPacketData>();
        FloorPacketDataList = new List<FloorSenderPacketData>();
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

public static class SenderPacketReceiver
{
    public static SenderPacketCollection Receive(UdpSocket udpSocket, List<RemoteSender> remoteSenders)
    {
        var senderEndPoints = new List<IPEndPoint>();
        var senderPacketCollection = new SenderPacketCollection();
        foreach (var remoteSender in remoteSenders)
        {
            senderEndPoints.Add(remoteSender.SenderEndPoint);
            senderPacketCollection.SenderPacketSets.Add(remoteSender.SenderSessionId, new SenderPacketSet());
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
        int senderSessionId = PacketHelper.getSessionIdFromSenderPacketBytes(packet.Bytes);
        SenderPacketType packetType = PacketHelper.getPacketTypeFromSenderPacketBytes(packet.Bytes);

        if (packetType == SenderPacketType.Confirm)
        {
            senderPacketCollection.ConfirmPacketInfoList.Add(new ConfirmPacketInfo(packet.EndPoint, senderSessionId, ConfirmSenderPacketData.Parse(packet.Bytes)));
            return;
        }

        SenderPacketSet senderPacketSet;
        if (!senderPacketCollection.SenderPacketSets.TryGetValue(senderSessionId, out senderPacketSet))
            return;

        // Heartbeat packets turns on ReceivedAny.
        senderPacketSet.ReceivedAny = true;
        switch (packetType)
        {
            case SenderPacketType.VideoInit:
                senderPacketSet.InitPacketDataList.Add(VideoInitSenderPacketData.Parse(packet.Bytes));
                break;
            case SenderPacketType.Frame:
                senderPacketSet.VideoPacketDataList.Add(VideoSenderPacketData.Parse(packet.Bytes));
                break;
            case SenderPacketType.Parity:
                senderPacketSet.FecPacketDataList.Add(ParitySenderPacketData.Parse(packet.Bytes));
                break;
            case SenderPacketType.Audio:
                senderPacketSet.AudioPacketDataList.Add(AudioSenderPacketData.Parse(packet.Bytes));
                break;
            case SenderPacketType.Floor:
                senderPacketSet.FloorPacketDataList.Add(FloorSenderPacketData.Parse(packet.Bytes));
                break;
        }
    }
}