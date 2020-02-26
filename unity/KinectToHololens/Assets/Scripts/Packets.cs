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
        colorIntrinsics.Cx = reader.ReadSingle();
        colorIntrinsics.Cy = reader.ReadSingle();
        colorIntrinsics.Fx = reader.ReadSingle();
        colorIntrinsics.Fy = reader.ReadSingle();
        colorIntrinsics.K1 = reader.ReadSingle();
        colorIntrinsics.K2 = reader.ReadSingle();
        colorIntrinsics.K3 = reader.ReadSingle();
        colorIntrinsics.K4 = reader.ReadSingle();
        colorIntrinsics.K5 = reader.ReadSingle();
        colorIntrinsics.K6 = reader.ReadSingle();
        colorIntrinsics.Codx = reader.ReadSingle();
        colorIntrinsics.Cody = reader.ReadSingle();
        colorIntrinsics.P2 = reader.ReadSingle();
        colorIntrinsics.P1 = reader.ReadSingle();
        colorIntrinsics.MetricRadius = reader.ReadSingle();

        initSenderPacketData.colorIntrinsics = colorIntrinsics;

        initSenderPacketData.colorMetricRadius = reader.ReadSingle();

        UnityEngine.Debug.LogFormat($"color: {colorIntrinsics.MetricRadius} / {initSenderPacketData.colorMetricRadius}");

        var depthIntrinsics = new AzureKinectCalibration.Intrinsics();
        depthIntrinsics.Cx = reader.ReadSingle();
        depthIntrinsics.Cy = reader.ReadSingle();
        depthIntrinsics.Fx = reader.ReadSingle();
        depthIntrinsics.Fy = reader.ReadSingle();
        depthIntrinsics.K1 = reader.ReadSingle();
        depthIntrinsics.K2 = reader.ReadSingle();
        depthIntrinsics.K3 = reader.ReadSingle();
        depthIntrinsics.K4 = reader.ReadSingle();
        depthIntrinsics.K5 = reader.ReadSingle();
        depthIntrinsics.K6 = reader.ReadSingle();
        depthIntrinsics.Codx = reader.ReadSingle();
        depthIntrinsics.Cody = reader.ReadSingle();
        depthIntrinsics.P2 = reader.ReadSingle();
        depthIntrinsics.P1 = reader.ReadSingle();
        depthIntrinsics.MetricRadius = reader.ReadSingle();
        initSenderPacketData.depthIntrinsics = depthIntrinsics;

        initSenderPacketData.depthMetricRadius = reader.ReadSingle();

        UnityEngine.Debug.LogFormat($"depth: {depthIntrinsics.MetricRadius} / {initSenderPacketData.depthMetricRadius}");

        var depthToColorExtrinsics = new AzureKinectCalibration.Extrinsics();
        for (int i = 0; i < depthToColorExtrinsics.Rotation.Length; ++i)
            depthToColorExtrinsics.Rotation[i] = reader.ReadSingle();
        for (int i = 0; i < depthToColorExtrinsics.Translation.Length; ++i)
            depthToColorExtrinsics.Translation[i] = reader.ReadSingle();
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
