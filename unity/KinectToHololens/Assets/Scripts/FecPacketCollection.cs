class FecPacketCollection
{
    public int FrameId { get; private set; }
    public int PacketCount { get; private set; }
    private FecSenderPacketData[] packetDataSet;
    public FecPacketCollection(int frameId, int packetCount)
    {
        FrameId = frameId;
        PacketCount = packetCount;
        packetDataSet = new FecSenderPacketData[packetCount];
    }

    public void AddPacket(int packetIndex, FecSenderPacketData packetData)
    {
        packetDataSet[packetIndex] = packetData;
    }

    public FecSenderPacketData GetPacketData(int packetIndex)
    {
        return packetDataSet[packetIndex];
    }
};
