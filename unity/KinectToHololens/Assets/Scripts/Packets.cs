using System.IO;

public class InitSenderPacketData
{
    public int colorWidth;
    public int colorHeight;
    public int depthWidth;
    public int depthHeight;
    public AzureKinectCalibration.Intrinsics colorIntrinsics;
    public float colorMetricRadius;
    public AzureKinectCalibration.Intrinsics depthIntrinsics;
    public float depthMetricRadius;
    public AzureKinectCalibration.Extrinsics depthToColorExtrinsics;

    public static InitSenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 5;

        var initSenderPacketData = new InitSenderPacketData();
        initSenderPacketData.colorWidth = reader.ReadInt32();
        initSenderPacketData.colorHeight = reader.ReadInt32();
        initSenderPacketData.depthWidth = reader.ReadInt32();
        initSenderPacketData.depthHeight = reader.ReadInt32();

        var colorIntrinsics = new AzureKinectCalibration.Intrinsics();
        colorIntrinsics.cx = reader.ReadSingle();
        colorIntrinsics.cy = reader.ReadSingle();
        colorIntrinsics.fx = reader.ReadSingle();
        colorIntrinsics.fy = reader.ReadSingle();
        colorIntrinsics.k1 = reader.ReadSingle();
        colorIntrinsics.k2 = reader.ReadSingle();
        colorIntrinsics.k3 = reader.ReadSingle();
        colorIntrinsics.k4 = reader.ReadSingle();
        colorIntrinsics.k5 = reader.ReadSingle();
        colorIntrinsics.k6 = reader.ReadSingle();
        colorIntrinsics.codx = reader.ReadSingle();
        colorIntrinsics.cody = reader.ReadSingle();
        colorIntrinsics.p2 = reader.ReadSingle();
        colorIntrinsics.p1 = reader.ReadSingle();
        colorIntrinsics.metricRadius = reader.ReadSingle();

        initSenderPacketData.colorIntrinsics = colorIntrinsics;

        initSenderPacketData.colorMetricRadius = reader.ReadSingle();

        UnityEngine.Debug.LogFormat($"color: {colorIntrinsics.metricRadius} / {initSenderPacketData.colorMetricRadius}");

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

        UnityEngine.Debug.LogFormat($"depth: {depthIntrinsics.metricRadius} / {initSenderPacketData.depthMetricRadius}");

        var depthToColorExtrinsics = new AzureKinectCalibration.Extrinsics();
        for (int i = 0; i < depthToColorExtrinsics.rotation.Length; ++i)
            depthToColorExtrinsics.rotation[i] = reader.ReadSingle();
        for (int i = 0; i < depthToColorExtrinsics.translation.Length; ++i)
            depthToColorExtrinsics.translation[i] = reader.ReadSingle();
        initSenderPacketData.depthToColorExtrinsics = depthToColorExtrinsics;

        return initSenderPacketData;
    }
}

public class VideoSenderPacketData
{
    public int frameId;
    public int packetIndex;
    public int packetCount;
    public byte[] bytes;

    public static VideoSenderPacketData Parse(byte[] packetBytes)
    {
        var reader = new BinaryReader(new MemoryStream(packetBytes));
        reader.BaseStream.Position = 5;

        var videoSenderPacketData = new VideoSenderPacketData();
        videoSenderPacketData.frameId = reader.ReadInt32();
        videoSenderPacketData.packetIndex = reader.ReadInt32();
        videoSenderPacketData.packetCount = reader.ReadInt32();

        videoSenderPacketData.bytes = reader.ReadBytes(packetBytes.Length - (int)reader.BaseStream.Position);

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
