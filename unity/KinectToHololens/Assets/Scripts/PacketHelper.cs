public static class PacketHelper
{
    public const int PACKET_SIZE = 1472;
    public const int PACKET_HEADER_SIZE = 17;
    public const int MAX_PACKET_CONTENT_SIZE = PACKET_SIZE - PACKET_HEADER_SIZE;
}
