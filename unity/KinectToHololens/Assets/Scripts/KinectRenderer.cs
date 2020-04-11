using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using UnityEngine;

public class KinectRenderer
{
    private UdpSocket udpSocket;
    private int sessionId;
    private IPEndPoint endPoint;

    private Material azureKinectScreenMaterial;

    private TextureGroup textureGroup;

    public int lastVideoFrameId;

    private Vp8Decoder colorDecoder;
    private TrvlDecoder depthDecoder;
    private bool prepared;

    private Dictionary<int, VideoSenderMessageData> videoMessages;
    private Stopwatch frameStopWatch;

    public KinectRenderer(Material azureKinectScreenMaterial, InitSenderPacketData initPacketData, UdpSocket udpSocket, int sessionId, IPEndPoint endPoint)
    {
        this.azureKinectScreenMaterial = azureKinectScreenMaterial;

        textureGroup = new TextureGroup(Plugin.texture_group_reset());

        lastVideoFrameId = -1;

        prepared = false;

        videoMessages = new Dictionary<int, VideoSenderMessageData>();
        frameStopWatch = Stopwatch.StartNew();

        textureGroup.SetWidth(initPacketData.depthWidth);
        textureGroup.SetHeight(initPacketData.depthHeight);
        PluginHelper.InitTextureGroup();

        colorDecoder = new Vp8Decoder();
        depthDecoder = new TrvlDecoder(initPacketData.depthWidth * initPacketData.depthHeight);

        this.udpSocket = udpSocket;
        this.sessionId = sessionId;
        this.endPoint = endPoint;
    }

    public void UpdateFrame(ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue)
    {
        // If texture is not created, create and assign them to quads.
        if (!prepared)
        {
            // Check whether the native plugin has Direct3D textures that
            // can be connected to Unity textures.
            if (textureGroup.IsInitialized())
            {
                // TextureGroup includes Y, U, V, and a depth texture.
                azureKinectScreenMaterial.SetTexture("_YTex", textureGroup.GetYTexture());
                azureKinectScreenMaterial.SetTexture("_UTex", textureGroup.GetUTexture());
                azureKinectScreenMaterial.SetTexture("_VTex", textureGroup.GetVTexture());
                azureKinectScreenMaterial.SetTexture("_DepthTex", textureGroup.GetDepthTexture());

                prepared = true;

                UnityEngine.Debug.Log("textureGroup intialized");
            }
        }

        {
            Tuple<int, VideoSenderMessageData> frameMessagePair;
            while (videoMessageQueue.TryDequeue(out frameMessagePair))
            {
                // C# Dictionary throws an error when you add an element with
                // a key that is already taken.
                if (videoMessages.ContainsKey(frameMessagePair.Item1))
                    continue;

                videoMessages.Add(frameMessagePair.Item1, frameMessagePair.Item2);
            }
        }

        if (videoMessages.Count == 0)
        {
            return;
        }

        int? beginFrameId = null;
        // If there is a key frame, use the most recent one.
        foreach (var frameMessagePair in videoMessages)
        {
            if (frameMessagePair.Key <= lastVideoFrameId)
                continue;

            if (frameMessagePair.Value.keyframe)
                beginFrameId = frameMessagePair.Key;
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!beginFrameId.HasValue)
        {
            if (videoMessages.ContainsKey(lastVideoFrameId + 1))
            {
                beginFrameId = lastVideoFrameId + 1;
            }
            else
            {
                // Wait for more frames if there is way to render without glitches.
                return;
            }
        }

        // ffmpegFrame and trvlFrame are guaranteed to be non-null
        // since the existence of beginIndex's value.
        FFmpegFrame ffmpegFrame = null;
        TrvlFrame trvlFrame = null;

        var decoderStopWatch = Stopwatch.StartNew();
        for (int i = beginFrameId.Value; ; ++i)
        {
            if (!videoMessages.ContainsKey(i))
                break;

            var frameMessage = videoMessages[i];
            lastVideoFrameId = i;

            var colorEncoderFrame = frameMessage.colorEncoderFrame;
            var depthEncoderFrame = frameMessage.depthEncoderFrame;

            ffmpegFrame = colorDecoder.Decode(colorEncoderFrame);
            trvlFrame = depthDecoder.Decode(depthEncoderFrame, frameMessage.keyframe);
        }

        decoderStopWatch.Stop();
        var decoderTime = decoderStopWatch.Elapsed;
        frameStopWatch.Stop();
        var frameTime = frameStopWatch.Elapsed;
        frameStopWatch = Stopwatch.StartNew();

        udpSocket.Send(PacketHelper.createReportReceiverPacketBytes(sessionId,
                                                                    lastVideoFrameId,
                                                                    (float)decoderTime.TotalMilliseconds,
                                                                    (float)frameTime.TotalMilliseconds), endPoint);

        // Invokes a function to be called in a render thread.
        if (prepared)
        {
            //Plugin.texture_group_set_ffmpeg_frame(textureGroup, ffmpegFrame.Ptr);
            textureGroup.SetFFmpegFrame(ffmpegFrame);
            //Plugin.texture_group_set_depth_pixels(textureGroup, trvlFrame.Ptr);
            textureGroup.SetTrvlFrame(trvlFrame);
            PluginHelper.UpdateTextureGroup();
        }

        // Remove frame messages before the rendered frame.
        var frameMessageKeys = new List<int>();
        foreach (int key in videoMessages.Keys)
        {
            frameMessageKeys.Add(key);
        }
        foreach (int key in frameMessageKeys)
        {
            if (key < lastVideoFrameId)
            {
                videoMessages.Remove(key);
            }
        }
    }
}
