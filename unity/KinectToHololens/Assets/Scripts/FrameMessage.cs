using System;

class FrameMessage
{
    private byte[] message;
    public int FrameId { get; private set; }
    public float FrameTimeStamp { get; private set; }
    public bool Keyframe { get; private set; }
    public int ColorEncoderFrameSize { get; private set; }
    public int DepthEncoderFrameSize { get; private set; }
    public TimeSpan PacketCollectionTime { get; private set; }

    private FrameMessage(byte[] message, int frameId, float frameTimeStamp,
                         bool keyframe, int colorEncoderFrameSize, int depthEncoderFrameSize,
                         TimeSpan packetCollectionTime)
    {
        this.message = message;
        FrameId = frameId;
        FrameTimeStamp = frameTimeStamp;
        Keyframe = keyframe;
        ColorEncoderFrameSize = colorEncoderFrameSize;
        DepthEncoderFrameSize = depthEncoderFrameSize;
        PacketCollectionTime = packetCollectionTime;
    }

    public static FrameMessage Create(byte[] message, TimeSpan packetCollectionTime)
    {
        int cursor = 0;
        int frameId = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        float frameTimeStamp = BitConverter.ToSingle(message, cursor);
        cursor += 4;

        bool keyframe = Convert.ToBoolean(message[cursor]);
        cursor += 1;

        // Parsing the bytes of the message into the VP8 and TRVL frames.
        int colorEncoderFrameSize = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        // Bytes of the color_encoder_frame.
        cursor += colorEncoderFrameSize;

        int depthEncoderFrameSize = BitConverter.ToInt32(message, cursor);

        return new FrameMessage(message: message, frameId: frameId, frameTimeStamp: frameTimeStamp,
                                keyframe: keyframe, colorEncoderFrameSize: colorEncoderFrameSize,
                                depthEncoderFrameSize: depthEncoderFrameSize,
                                packetCollectionTime: packetCollectionTime);
    }

    public byte[] GetColorEncoderFrame()
    {
        int cursor = 4 + 4 + 1 + 4;
        byte[] colorEncoderFrame = new byte[ColorEncoderFrameSize];
        Array.Copy(message, cursor, colorEncoderFrame, 0, ColorEncoderFrameSize);
        return colorEncoderFrame;
    }

    public byte[] GetDepthEncoderFrame()
    {
        int cursor = 4 + 4 + 1 + 4 + ColorEncoderFrameSize + 4;
        byte[] depthEncoderFrame = new byte[DepthEncoderFrameSize];
        Array.Copy(message, cursor, depthEncoderFrame, 0, DepthEncoderFrameSize);
        return depthEncoderFrame;
    }
};