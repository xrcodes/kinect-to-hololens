using System;
using System.Collections.Generic;
using System.Diagnostics;

class FramePacketCollection
{
    public int FrameId { get; private set; }
    public int PacketCount { get; private set; }
    public byte[][] Packets { get; private set; }
    private Stopwatch stopWatch;

    public FramePacketCollection(int frameId, int packetCount)
    {
        FrameId = frameId;
        PacketCount = packetCount;
        Packets = new byte[packetCount][];
        for (int i = 0; i < packetCount; ++i)
        {
            Packets[i] = new byte[0];
        }

        stopWatch = Stopwatch.StartNew();
    }

    public void AddPacket(int packetIndex, byte[] packet)
    {
        Packets[packetIndex] = packet;
    }

    public bool IsFull()
    {
        foreach (var packet in Packets)
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
        foreach (var packet in Packets)
        {
            messageSize += packet.Length - HEADER_SIZE;
        }

        byte[] message = new byte[messageSize];
        for (int i = 0; i < Packets.Length; ++i)
        {
            int cursor = (1500 - HEADER_SIZE) * i;
            Array.Copy(Packets[i], HEADER_SIZE, message, cursor, Packets[i].Length - HEADER_SIZE);
        }

        stopWatch.Stop();
        return FrameMessage.Create(FrameId, message, stopWatch.Elapsed);
    }

    public List<int> GetMissingPacketIds()
    {
        var missingPacketIds = new List<int>();
        for(int i = 0; i < Packets.Length; ++i)
        {
            if(Packets[i].Length == 0)
            {
                missingPacketIds.Add(i);
            }
        }
        return missingPacketIds;
    }
};