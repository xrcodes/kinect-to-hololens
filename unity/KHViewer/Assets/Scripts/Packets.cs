using System;
using System.Collections.Generic;
using System.IO;

public enum SenderPacketType : int
{
    Confirm = 0,
    Heartbeat = 1,
    Video = 2,
    Parity = 3,
    Audio = 4,
}

public enum ReceiverPacketType : int
{
    Connect = 0,
    Heartbeat = 1,
    Report = 2,
    Request = 3,
}

public class Packet
{
    public byte[] bytes;
    
    public Packet(byte[] bytes)
    {
        this.bytes = bytes;
    }
}

public class ConfirmSenderPacket
{
    public int senderId;
    public SenderPacketType type;
    public int receiverId;

    public static ConfirmSenderPacket Create(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        var confirmSenderPacket = new ConfirmSenderPacket();
        confirmSenderPacket.senderId = reader.ReadInt32();
        confirmSenderPacket.type = (SenderPacketType)reader.ReadInt32();
        confirmSenderPacket.receiverId = reader.ReadInt32();

        return confirmSenderPacket;
    }
}

public class VideoSenderMessage
{
    public float frameTimeStamp;
    public bool keyframe;

    public int width;
    public int height;
    public KinectIntrinsics intrinsics;

    public byte[] colorEncoderFrame;
    public byte[] depthEncoderFrame;
    public float[] floor;

    public static VideoSenderMessage Create(byte[] messageBytes)
    {
        var reader = new BinaryReader(new MemoryStream(messageBytes));

        var videoSenderMessageData = new VideoSenderMessage();
        videoSenderMessageData.frameTimeStamp = reader.ReadSingle();
        videoSenderMessageData.keyframe = reader.ReadBoolean();

        videoSenderMessageData.width = reader.ReadInt32();
        videoSenderMessageData.height = reader.ReadInt32();

        var depthIntrinsics = new KinectIntrinsics();
        depthIntrinsics.cx = reader.ReadSingle();
        depthIntrinsics.cy = reader.ReadSingle();
        depthIntrinsics.fx = reader.ReadSingle();
        depthIntrinsics.fy = reader.ReadSingle();
        depthIntrinsics.k1 = reader.ReadSingle();
        depthIntrinsics.k2 = reader.ReadSingle();
        depthIntrinsics.k3 = reader.ReadSingle();
        depthIntrinsics.k4 = reader.ReadSingle();
        depthIntrinsics.k5 = reader.ReadSingle();
        depthIntrinsics.k6 = reader.ReadSingle();
        depthIntrinsics.codx = reader.ReadSingle();
        depthIntrinsics.cody = reader.ReadSingle();
        depthIntrinsics.p1 = reader.ReadSingle();
        depthIntrinsics.p2 = reader.ReadSingle();
        depthIntrinsics.maxRadiusForProjection = reader.ReadSingle();
        videoSenderMessageData.intrinsics = depthIntrinsics;

        int colorEncoderFrameSize = reader.ReadInt32();
        videoSenderMessageData.colorEncoderFrame = reader.ReadBytes(colorEncoderFrameSize);

        int depthEncoderFrameSize = reader.ReadInt32();
        videoSenderMessageData.depthEncoderFrame = reader.ReadBytes(depthEncoderFrameSize);

        bool hasFloor = reader.ReadBoolean();
        if(hasFloor)
        {
            var floor = new float[4];
            floor[0] = reader.ReadSingle();
            floor[1] = reader.ReadSingle();
            floor[2] = reader.ReadSingle();
            floor[3] = reader.ReadSingle();
            videoSenderMessageData.floor = floor;
        }
        else
        {
            videoSenderMessageData.floor = null;
        }

        return videoSenderMessageData;
    }
}

public class VideoSenderPacket
{
    public int senderId;
    public SenderPacketType type;
    public int frameId;
    public int packetIndex;
    public int packetCount;
    public byte[] messageData;

    public static VideoSenderPacket Create(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        var videoSenderPacket = new VideoSenderPacket();
        videoSenderPacket.senderId = reader.ReadInt32();
        videoSenderPacket.type = (SenderPacketType)reader.ReadInt32();
        videoSenderPacket.frameId = reader.ReadInt32();
        videoSenderPacket.packetIndex = reader.ReadInt32();
        videoSenderPacket.packetCount = reader.ReadInt32();

        videoSenderPacket.messageData = reader.ReadBytes(packetBytes.Length - (int)reader.BaseStream.Position);

        return videoSenderPacket;
    }
}

public class ParitySenderPacket
{
    public int senderId;
    public SenderPacketType type;
    public int frameId;
    public int packetIndex;
    public int videoPacketCount;
    public byte[] bytes;

    public static ParitySenderPacket Create(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        var paritySenderPacket = new ParitySenderPacket();
        paritySenderPacket.senderId = reader.ReadInt32();
        paritySenderPacket.type = (SenderPacketType)reader.ReadInt32();
        paritySenderPacket.frameId = reader.ReadInt32();
        paritySenderPacket.packetIndex = reader.ReadInt32();
        paritySenderPacket.videoPacketCount = reader.ReadInt32();

        paritySenderPacket.bytes =  reader.ReadBytes(packetBytes.Length - (int) reader.BaseStream.Position);

        return paritySenderPacket;
    }
}

public class AudioSenderPacket
{
    public int senderId;
    public SenderPacketType type;
    public int frameId;
    public byte[] opusFrame;

    public static AudioSenderPacket Create(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        var audioSenderPacket = new AudioSenderPacket();
        audioSenderPacket.senderId = reader.ReadInt32();
        audioSenderPacket.type = (SenderPacketType)reader.ReadInt32();
        audioSenderPacket.frameId = reader.ReadInt32();

        audioSenderPacket.opusFrame = reader.ReadBytes(packetBytes.Length - (int)reader.BaseStream.Position);

        return audioSenderPacket;
    }
}

public static class PacketUtils
{
    public const int PACKET_SIZE = 508;

    public static int getSessionIdFromSenderPacketBytes(byte[] packetBytes)
    {
        return BitConverter.ToInt32(packetBytes, 0);
    }

    public static SenderPacketType getPacketTypeFromSenderPacketBytes(byte[] packetBytes)
    {
        return (SenderPacketType)BitConverter.ToInt32(packetBytes, 4);
    }

    public static Packet createConnectReceiverPacketBytes(int receiverId,
                                                          bool videoRequested,
                                                          bool audioRequested)
    {
        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(receiverId), 0, 4);
        ms.Write(BitConverter.GetBytes((int)ReceiverPacketType.Connect), 0, 4);
        // bools need to be converted to bytes since C# bools are 4 bytes each, different from the 1-byte C++ bools.
        ms.WriteByte(Convert.ToByte(videoRequested));
        ms.WriteByte(Convert.ToByte(audioRequested));
        
        return new Packet(ms.ToArray());
    }

    public static Packet createHeartbeatReceiverPacketBytes(int receiverId)
    {
        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(receiverId), 0, 4);
        ms.Write(BitConverter.GetBytes((int)ReceiverPacketType.Heartbeat), 0, 4);
        
        return new Packet(ms.ToArray());
    }

    public static Packet createReportReceiverPacketBytes(int receiverId, int frameId)
    {
        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(receiverId), 0, 4);
        ms.Write(BitConverter.GetBytes((int)ReceiverPacketType.Report), 0, 4);
        ms.Write(BitConverter.GetBytes(frameId), 0, 4);
        
        return new Packet(ms.ToArray());
    }

    public static Packet createRequestReceiverPacketBytes(int receiverId, int frameId, bool allPackets, List<int> videoPacketIndices, List<int> parityPacketIndices)
    {
        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(receiverId), 0, 4);
        ms.Write(BitConverter.GetBytes((int)ReceiverPacketType.Request), 0, 4);
        ms.Write(BitConverter.GetBytes(frameId), 0, 4);
        ms.WriteByte(Convert.ToByte(allPackets));
        ms.Write(BitConverter.GetBytes(videoPacketIndices.Count), 0, 4);
        ms.Write(BitConverter.GetBytes(parityPacketIndices.Count), 0, 4);
        foreach (int index in videoPacketIndices)
        {
            ms.Write(BitConverter.GetBytes(index), 0, 4);
        }
        foreach (int index in parityPacketIndices)
        {
            ms.Write(BitConverter.GetBytes(index), 0, 4);
        }

        return new Packet(ms.ToArray());
    }
}
