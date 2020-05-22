using System.Collections.Generic;

public class ConfirmPacketInfo
{
    public int SenderSessionId { get; private set; }
    public ConfirmSenderPacketData ConfirmPacketData { get; private set; }

    public ConfirmPacketInfo(int senderSessionId, ConfirmSenderPacketData confirmPacketData)
    {
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
    public static SenderPacketCollection Receive(UdpSocket udpSocket, List<int> senderSessionIds)
    {
        var senderPacketCollection = new SenderPacketCollection();
        foreach (int senderSessionId in senderSessionIds)
            senderPacketCollection.SenderPacketSets.Add(senderSessionId, new SenderPacketSet());

        while (true)
        {
            var packet = udpSocket.Receive();
            if (packet == null)
                break;

            int senderSessionId = PacketHelper.getSessionIdFromSenderPacketBytes(packet.bytes);
            SenderPacketType packetType = PacketHelper.getPacketTypeFromSenderPacketBytes(packet.bytes);

            if(packetType == SenderPacketType.Confirm)
            {
                senderPacketCollection.ConfirmPacketInfoList.Add(new ConfirmPacketInfo(senderSessionId, ConfirmSenderPacketData.Parse(packet.bytes)));
                continue;
            }

            SenderPacketSet senderPacketSet;
            if (!senderPacketCollection.SenderPacketSets.TryGetValue(senderSessionId, out senderPacketSet))
                continue;

            // Heartbeat packets turns on ReceivedAny.
            senderPacketSet.ReceivedAny = true;
            switch (packetType)
            {
                case SenderPacketType.VideoInit:
                    senderPacketSet.InitPacketDataList.Add(VideoInitSenderPacketData.Parse(packet.bytes));
                    break;
                case SenderPacketType.Frame:
                    senderPacketSet.VideoPacketDataList.Add(VideoSenderPacketData.Parse(packet.bytes));
                    break;
                case SenderPacketType.Parity:
                    senderPacketSet.FecPacketDataList.Add(ParitySenderPacketData.Parse(packet.bytes));
                    break;
                case SenderPacketType.Audio:
                    senderPacketSet.AudioPacketDataList.Add(AudioSenderPacketData.Parse(packet.bytes));
                    break;
                case SenderPacketType.Floor:
                    senderPacketSet.FloorPacketDataList.Add(FloorSenderPacketData.Parse(packet.bytes));
                    break;
            }
        }

        return senderPacketCollection;
    }
}