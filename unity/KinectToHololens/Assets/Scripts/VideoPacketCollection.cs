using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

class VideoPacketCollection
{
    public int FrameId { get; private set; }
    public int PacketCount { get; private set; }
    public VideoSenderPacketData[] PacketDataSet { get; private set; }

    public VideoPacketCollection(int frameId, int packetCount)
    {
        FrameId = frameId;
        PacketCount = packetCount;
        PacketDataSet = new VideoSenderPacketData[packetCount];
    }

    public void AddPacketData(int packetIndex, VideoSenderPacketData packetData)
    {
        PacketDataSet[packetIndex] = packetData;
    }

    public bool IsFull()
    {
        foreach (var packetData in PacketDataSet)
        {
            if (packetData == null)
                return false;
        }

        return true;
    }

    //public FrameMessage ToMessage()
    //{
    //    int messageSize = 0;
    //    foreach (var packetData in PacketDataSet)
    //        messageSize += packetData.messageData.Length;

    //    byte[] message = new byte[messageSize];
    //    int cursor = 0;
    //    foreach (var packetData in PacketDataSet)
    //    {
    //        Array.Copy(packetData.messageData, 0, message, cursor, packetData.messageData.Length);
    //        cursor += packetData.messageData.Length;
    //    }

    //    return FrameMessage.Create(FrameId, message);
    //}

    public List<int> GetMissingPacketIds()
    {
        var missingPacketIds = new List<int>();
        for(int i = 0; i < PacketDataSet.Length; ++i)
        {
            if(PacketDataSet[i] == null)
            {
                missingPacketIds.Add(i);
            }
        }
        return missingPacketIds;
    }
};