using System.Collections.Generic;

public class SenderPacketSet
{
    public bool ReceivedAny { get; set; }
    public List<ConfirmSenderPacketData> ConfirmPacketDataList { get; private set; }
    public List<VideoInitSenderPacketData> InitPacketDataList { get; private set; }
    public List<VideoSenderPacketData> VideoPacketDataList { get; private set; }
    public List<ParitySenderPacketData> FecPacketDataList { get; private set; }
    public List<AudioSenderPacketData> AudioPacketDataList { get; private set; }
    public List<FloorSenderPacketData> FloorPacketDataList { get; private set; }

    public SenderPacketSet()
    {
        ReceivedAny = false;
        ConfirmPacketDataList = new List<ConfirmSenderPacketData>();
        InitPacketDataList = new List<VideoInitSenderPacketData>();
        VideoPacketDataList = new List<VideoSenderPacketData>();
        FecPacketDataList = new List<ParitySenderPacketData>();
        AudioPacketDataList = new List<AudioSenderPacketData>();
        FloorPacketDataList = new List<FloorSenderPacketData>();
    }
}

public static class SenderPacketReceiver
{
    public static SenderPacketSet Receive(UdpSocket udpSocket)
    {
        var senderPacketSet = new SenderPacketSet();
        while (true)
        {
            var packet = udpSocket.Receive();
            if (packet == null)
                break;

            //int sessionId = PacketHelper.getSessionIdFromSenderPacketBytes(packet);
            // Heartbeat packets turns on ReceivedAny.
            senderPacketSet.ReceivedAny = true;
            switch (PacketHelper.getPacketTypeFromSenderPacketBytes(packet.bytes))
            {
                case SenderPacketType.Confirm:
                    senderPacketSet.ConfirmPacketDataList.Add(ConfirmSenderPacketData.Parse(packet.bytes));
                    break;
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

        return senderPacketSet;
    }
}