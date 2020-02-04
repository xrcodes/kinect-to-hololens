using System;
using System.Diagnostics;

class FramePacketCollection
{
    public int FrameId { get; private set; }
    public int PacketCount { get; private set; }
    private byte[][] packets;
    private Stopwatch stopWatch;

    public FramePacketCollection(int frameId, int packetCount)
    {
        FrameId = frameId;
        PacketCount = packetCount;
        packets = new byte[packetCount][];
        for (int i = 0; i < packetCount; ++i)
        {
            packets[i] = new byte[0];
        }

        stopWatch = Stopwatch.StartNew();
    }

    public void AddPacket(int packetIndex, byte[] packet)
    {
        packets[packetIndex] = packet;
    }

    public bool IsFull()
    {
        foreach (var packet in packets)
        {
            if (packet.Length == 0)
                return false;
        }

        return true;
    }

    public FrameMessage ToMessage()
    {
        const int HEADER_SIZE = 17;
        int messageSize = 0;
        foreach (var packet in packets)
        {
            messageSize += packet.Length - HEADER_SIZE;
        }

        byte[] message = new byte[messageSize];
        for (int i = 0; i < packets.Length; ++i)
        {
            int cursor = (1500 - HEADER_SIZE) * i;
            Array.Copy(packets[i], HEADER_SIZE, message, cursor, packets[i].Length - HEADER_SIZE);
        }

        stopWatch.Stop();
        return FrameMessage.Create(FrameId, message, stopWatch.Elapsed);
    }
};