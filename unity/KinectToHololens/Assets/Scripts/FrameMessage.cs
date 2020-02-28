using System;

class FrameMessage
{
    public int FrameId { get; private set; }
    private byte[] message;
    public float FrameTimeStamp { get; private set; }
    public bool Keyframe { get; private set; }
    public int ColorEncoderFrameSize { get; private set; }
    public int DepthEncoderFrameSize { get; private set; }

    private FrameMessage(int frameId, byte[] message, float frameTimeStamp,
                         bool keyframe, int colorEncoderFrameSize, int depthEncoderFrameSize)
    {
        FrameId = frameId;
        this.message = message;
        FrameTimeStamp = frameTimeStamp;
        Keyframe = keyframe;
        ColorEncoderFrameSize = colorEncoderFrameSize;
        DepthEncoderFrameSize = depthEncoderFrameSize;
    }

    public static FrameMessage Create(int frameId, byte[] message)
    {
        int cursor = 0;

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

        return new FrameMessage(frameId: frameId, message: message, frameTimeStamp: frameTimeStamp,
                                keyframe: keyframe, colorEncoderFrameSize: colorEncoderFrameSize,
                                depthEncoderFrameSize: depthEncoderFrameSize);
    }

    public byte[] GetColorEncoderFrame()
    {
        int cursor = 4 + 1 + 4;
        byte[] colorEncoderFrame = new byte[ColorEncoderFrameSize];
        Array.Copy(message, cursor, colorEncoderFrame, 0, ColorEncoderFrameSize);
        return colorEncoderFrame;
    }

    public byte[] GetDepthEncoderFrame()
    {
        int cursor = 4 + 1 + 4 + ColorEncoderFrameSize + 4;
        byte[] depthEncoderFrame = new byte[DepthEncoderFrameSize];
        Array.Copy(message, cursor, depthEncoderFrame, 0, DepthEncoderFrameSize);
        return depthEncoderFrame;
    }
};