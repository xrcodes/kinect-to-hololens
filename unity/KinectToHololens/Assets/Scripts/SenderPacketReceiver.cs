using System.Collections.Concurrent;
using System.Net.Sockets;

class SenderPacketReceiver
{
    public ConcurrentQueue<InitSenderPacketData> InitPacketDataQueue { get; private set; }
    public ConcurrentQueue<VideoSenderPacketData> VideoPacketDataQueue { get; private set; }
    public ConcurrentQueue<FecSenderPacketData> FecPacketDataQueue { get; private set; }
    public ConcurrentQueue<AudioSenderPacketData> AudioPacketDataQueue { get; private set; }

    public SenderPacketReceiver()
    {
        InitPacketDataQueue = new ConcurrentQueue<InitSenderPacketData>();
        VideoPacketDataQueue = new ConcurrentQueue<VideoSenderPacketData>();
        FecPacketDataQueue = new ConcurrentQueue<FecSenderPacketData>();
        AudioPacketDataQueue = new ConcurrentQueue<AudioSenderPacketData>();
    }

    public void Receive(UdpSocket udpSocket, ConcurrentQueue<FloorSenderPacketData> floorPacketDataQueue)
    {
        SocketError error = SocketError.WouldBlock;
        while (true)
        {
            var packet = udpSocket.Receive(out error);
            if (packet == null)
                break;

            //int sessionId = PacketHelper.getSessionIdFromSenderPacketBytes(packet);

            switch (PacketHelper.getPacketTypeFromSenderPacketBytes(packet))
            {
                case SenderPacketType.Init:
                    InitPacketDataQueue.Enqueue(InitSenderPacketData.Parse(packet));
                    break;
                case SenderPacketType.Frame:
                    VideoPacketDataQueue.Enqueue(VideoSenderPacketData.Parse(packet));
                    break;
                case SenderPacketType.Fec:
                    FecPacketDataQueue.Enqueue(FecSenderPacketData.Parse(packet));
                    break;
                case SenderPacketType.Audio:
                    AudioPacketDataQueue.Enqueue(AudioSenderPacketData.Parse(packet));
                    break;
                case SenderPacketType.Floor:
                    floorPacketDataQueue.Enqueue(FloorSenderPacketData.Parse(packet));
                    break;
            }
        }
    }
}