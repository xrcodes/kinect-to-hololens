public static class PacketHelper
{
    // Packet types.
    public const byte SENDER_INIT_PACKET = 0;
    public const byte SENDER_FRAME_PACKET = 1;
    public const byte SENDER_XOR_PACKET = 2;
    public const byte SENDER_AUDIO_PACKET = 3;

    public const int PACKET_SIZE = 1472;
    public const int PACKET_HEADER_SIZE = 17;
    public const int MAX_PACKET_CONTENT_SIZE = PACKET_SIZE - PACKET_HEADER_SIZE;
}
