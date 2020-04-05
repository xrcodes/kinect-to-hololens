using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net.Sockets;

class SenderPacketSet
{
    public List<InitSenderPacketData> InitPacketDataList { get; private set; }
    public List<VideoSenderPacketData> VideoPacketDataList { get; private set; }
    public List<ParitySenderPacketData> FecPacketDataList { get; private set; }
    public List<AudioSenderPacketData> AudioPacketDataList { get; private set; }

    public SenderPacketSet()
    {
        InitPacketDataList = new List<InitSenderPacketData>();
        VideoPacketDataList = new List<VideoSenderPacketData>();
        FecPacketDataList = new List<ParitySenderPacketData>();
        AudioPacketDataList = new List<AudioSenderPacketData>();
    }
}

class SenderPacketReceiver
{
    public static SenderPacketSet Receive(UdpSocket udpSocket, ConcurrentQueue<FloorSenderPacketData> floorPacketDataQueue)
    {
        var senderPacketSet = new SenderPacketSet();
        while (true)
        {
            var packet = udpSocket.Receive();
            if (packet == null)
                break;

            //int sessionId = PacketHelper.getSessionIdFromSenderPacketBytes(packet);

            switch (PacketHelper.getPacketTypeFromSenderPacketBytes(packet))
            {
                case SenderPacketType.Init:
                    senderPacketSet.InitPacketDataList.Add(InitSenderPacketData.Parse(packet));
                    break;
                case SenderPacketType.Frame:
                    senderPacketSet.VideoPacketDataList.Add(VideoSenderPacketData.Parse(packet));
                    break;
                case SenderPacketType.Parity:
                    senderPacketSet.FecPacketDataList.Add(ParitySenderPacketData.Parse(packet));
                    break;
                case SenderPacketType.Audio:
                    senderPacketSet.AudioPacketDataList.Add(AudioSenderPacketData.Parse(packet));
                    break;
                case SenderPacketType.Floor:
                    floorPacketDataQueue.Enqueue(FloorSenderPacketData.Parse(packet));
                    break;
            }
        }

        return senderPacketSet;
    }
}