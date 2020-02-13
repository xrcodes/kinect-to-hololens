class XorPacketCollection
{
    public int FrameId { get; private set; }
    public int PacketCount { get; private set; }
    private byte[][] packets;
    public XorPacketCollection(int frameId, int packetCount)
    {
        FrameId = frameId;
        PacketCount = packetCount;
        packets = new byte[packetCount][];
        for (int i = 0; i < packetCount; ++i)
        {
            packets[i] = new byte[0];
        }
    }

    public void AddPacket(int packetIndex, byte[] packet)
    {
        packets[packetIndex] = packet;
    }

    public byte[] TryGetPacket(int packetIndex)
    {
        if (packets[packetIndex].Length == 0)
        {
            return null;
        }

        return packets[packetIndex];
    }
};
