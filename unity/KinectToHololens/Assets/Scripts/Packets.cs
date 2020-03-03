using System;
using System.Collections.Generic;
using System.IO;

public enum SenderPacketType : byte
{
    Init = 0,
    Frame = 1,
    Fec = 2,
    Audio = 3,
}

public enum ReceiverPacketType : byte
{
    Ping = 0,
    Report = 1,
    Request = 2,
}

public static class PacketHelper
{
    public const int PACKET_SIZE = 1472;
    public const int PACKET_HEADER_SIZE = 17;
    public const int MAX_PACKET_CONTENT_SIZE = PACKET_SIZE - PACKET_HEADER_SIZE;

    public static int getSessionIdFromSenderPacketBytes(byte[] packetBytes)
    {
        return BitConverter.ToInt32(packetBytes, 0);
    }

    public static SenderPacketType getPacketTypeFromSenderPacketBytes(byte[] packetBytes)
    {
        return (SenderPacketType)packetBytes[4];
    }

    public static byte[] createPingReceiverPacketBytes()
    {
        var bytes = new byte[1];
        bytes[0] = (byte)ReceiverPacketType.Ping;
        return bytes;
    }

    public static byte[] createReportReceiverPacketBytes(int frameId, float decoderMs, float frameMs)
    {
        var ms = new MemoryStream();
        ms.WriteByte((byte)ReceiverPacketType.Report);
        ms.Write(BitConverter.GetBytes(frameId), 0, 4);
        ms.Write(BitConverter.GetBytes(decoderMs), 0, 4);
        ms.Write(BitConverter.GetBytes(frameMs), 0, 4);
        return ms.ToArray();
    }

    public static byte[] createRequestReceiverPacketBytes(int frameId, List<int> missingPacketIds)
    {
        var ms = new MemoryStream();
        ms.WriteByte((byte)ReceiverPacketType.Request);
        ms.Write(BitConverter.GetBytes(frameId), 0, 4);
        foreach (int missingPacketId in missingPacketIds)
        {
            ms.Write(BitConverter.GetBytes(missingPacketId), 0, 4);
        }
        return ms.ToArray();
    }
}

public class InitSenderPacketData
{
    public int depthWidth;
    public int depthHeight;
    public AzureKinectCalibration.Intrinsics depthIntrinsics;
    public float depthMetricRadius;

    public static InitSenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 5;

        var initSenderPacketData = new InitSenderPacketData();
        initSenderPacketData.depthWidth = reader.ReadInt32();
        initSenderPacketData.depthHeight = reader.ReadInt32();

        var depthIntrinsics = new AzureKinectCalibration.Intrinsics();
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
        depthIntrinsics.p2 = reader.ReadSingle();
        depthIntrinsics.p1 = reader.ReadSingle();
        depthIntrinsics.metricRadius = reader.ReadSingle();
        initSenderPacketData.depthIntrinsics = depthIntrinsics;

        initSenderPacketData.depthMetricRadius = reader.ReadSingle();

        return initSenderPacketData;
    }
}

public class VideoSenderMessageData
{
    public float frameTimeStamp;
    public bool keyframe;
    public byte[] colorEncoderFrame;
    public byte[] depthEncoderFrame;

    public static VideoSenderMessageData Parse(byte[] messageBytes)
    {
        var reader = new BinaryReader(new MemoryStream(messageBytes));

        var videoSenderMessageData = new VideoSenderMessageData();
        videoSenderMessageData.frameTimeStamp = reader.ReadSingle();
        videoSenderMessageData.keyframe = reader.ReadBoolean();

        int colorEncoderFrameSize = reader.ReadInt32();
        int depthEncoderFrameSize = reader.ReadInt32();

        videoSenderMessageData.colorEncoderFrame = reader.ReadBytes(colorEncoderFrameSize);
        videoSenderMessageData.depthEncoderFrame = reader.ReadBytes(depthEncoderFrameSize);

        return videoSenderMessageData;
    }
}

public class VideoSenderPacketData
{
    public int frameId;
    public int packetIndex;
    public int packetCount;
    public byte[] messageData;

    public static VideoSenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 5;

        var videoSenderPacketData = new VideoSenderPacketData();
        videoSenderPacketData.frameId = reader.ReadInt32();
        videoSenderPacketData.packetIndex = reader.ReadInt32();
        videoSenderPacketData.packetCount = reader.ReadInt32();

        videoSenderPacketData.messageData = reader.ReadBytes(packetBytes.Length - (int)reader.BaseStream.Position);

        return videoSenderPacketData;
    }
}

public class FecSenderPacketData
{
    public int frameId;
    public int packetIndex;
    public int packetCount;
    public byte[] bytes;

    public static FecSenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 5;

        var fecSenderPacketData = new FecSenderPacketData();
        fecSenderPacketData.frameId = reader.ReadInt32();
        fecSenderPacketData.packetIndex = reader.ReadInt32();
        fecSenderPacketData.packetCount = reader.ReadInt32();

        fecSenderPacketData.bytes =  reader.ReadBytes(packetBytes.Length - (int) reader.BaseStream.Position);

        return fecSenderPacketData;
    }
}

public class AudioSenderPacketData
{
    public int frameId;
    public byte[] opusFrame;

    public static AudioSenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 5;

        var audioSenderPacketData = new AudioSenderPacketData();
        audioSenderPacketData.frameId = reader.ReadInt32();

        audioSenderPacketData.opusFrame = reader.ReadBytes(packetBytes.Length - (int)reader.BaseStream.Position);

        return audioSenderPacketData;
    }
}
