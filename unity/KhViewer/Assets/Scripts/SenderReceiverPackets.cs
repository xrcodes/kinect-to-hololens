using System;
using System.Collections.Generic;
using System.IO;

public enum SenderPacketType : int
{
    Confirm = 0,
    Heartbeat = 1,
    Frame = 2,
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

public static class PacketHelper
{
    public const int PACKET_SIZE = 1472;

    public static int getSessionIdFromSenderPacketBytes(byte[] packetBytes)
    {
        return BitConverter.ToInt32(packetBytes, 0);
    }

    public static SenderPacketType getPacketTypeFromSenderPacketBytes(byte[] packetBytes)
    {
        return (SenderPacketType)BitConverter.ToInt32(packetBytes, 4);
    }

    public static byte[] createConnectReceiverPacketBytes(int sessionId,
                                                          bool videoRequested,
                                                          bool audioRequested)
    {
        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(sessionId), 0, 4);
        ms.Write(BitConverter.GetBytes((int)ReceiverPacketType.Connect), 0, 4);
        // bools need to be converted to bytes since C# bools are 4 bytes each, different from the 1-byte C++ bools.
        ms.WriteByte(Convert.ToByte(videoRequested));
        ms.WriteByte(Convert.ToByte(audioRequested));
        return ms.ToArray();
    }

    public static byte[] createHeartbeatReceiverPacketBytes(int sessionId)
    {
        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(sessionId), 0, 4);
        ms.Write(BitConverter.GetBytes((int)ReceiverPacketType.Heartbeat), 0, 4);
        return ms.ToArray();
    }

    public static byte[] createReportReceiverPacketBytes(int sessionId, int frameId)
    {
        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(sessionId), 0, 4);
        ms.Write(BitConverter.GetBytes((int)ReceiverPacketType.Report), 0, 4);
        ms.Write(BitConverter.GetBytes(frameId), 0, 4);
        return ms.ToArray();
    }

    public static byte[] createRequestReceiverPacketBytes(int sessionId, int frameId, bool allPackets, List<int> videoPacketIndices, List<int> parityPacketIndices)
    {
        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(sessionId), 0, 4);
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
        return ms.ToArray();
    }
}

public class ConfirmSenderPacketData
{
    public int receiverSessionId;

    public static ConfirmSenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 8;

        var confirmSenderPacketData = new ConfirmSenderPacketData();
        confirmSenderPacketData.receiverSessionId = reader.ReadInt32();

        return confirmSenderPacketData;
    }
}

public class VideoSenderMessageData
{
    public float frameTimeStamp;
    public bool keyframe;

    public int width;
    public int height;
    public KinectCalibration.Intrinsics intrinsics;

    public byte[] colorEncoderFrame;
    public byte[] depthEncoderFrame;
    public float[] floor;

    public static VideoSenderMessageData Parse(byte[] messageBytes)
    {
        var reader = new BinaryReader(new MemoryStream(messageBytes));

        var videoSenderMessageData = new VideoSenderMessageData();
        videoSenderMessageData.frameTimeStamp = reader.ReadSingle();
        videoSenderMessageData.keyframe = reader.ReadBoolean();

        videoSenderMessageData.width = reader.ReadInt32();
        videoSenderMessageData.height = reader.ReadInt32();

        var depthIntrinsics = new KinectCalibration.Intrinsics();
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

public class VideoSenderPacketData
{
    public int frameId;
    public int packetIndex;
    public int packetCount;
    public byte[] messageData;

    public static VideoSenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 8;

        var videoSenderPacketData = new VideoSenderPacketData();
        videoSenderPacketData.frameId = reader.ReadInt32();
        videoSenderPacketData.packetIndex = reader.ReadInt32();
        videoSenderPacketData.packetCount = reader.ReadInt32();

        videoSenderPacketData.messageData = reader.ReadBytes(packetBytes.Length - (int)reader.BaseStream.Position);

        return videoSenderPacketData;
    }
}

public class ParitySenderPacketData
{
    public int frameId;
    public int packetIndex;
    public int videoPacketCount;
    public byte[] bytes;

    public static ParitySenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 8;

        var fecSenderPacketData = new ParitySenderPacketData();
        fecSenderPacketData.frameId = reader.ReadInt32();
        fecSenderPacketData.packetIndex = reader.ReadInt32();
        fecSenderPacketData.videoPacketCount = reader.ReadInt32();

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
        reader.BaseStream.Position = 8;

        var audioSenderPacketData = new AudioSenderPacketData();
        audioSenderPacketData.frameId = reader.ReadInt32();

        audioSenderPacketData.opusFrame = reader.ReadBytes(packetBytes.Length - (int)reader.BaseStream.Position);

        return audioSenderPacketData;
    }
}
