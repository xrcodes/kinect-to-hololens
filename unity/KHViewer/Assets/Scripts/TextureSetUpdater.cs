using System;
using System.Collections;
using System.Collections.Generic;
using System.Net;
using UnityEngine;

public class TextureSetUpdater
{
    private PrepareState state;
    private int sessionId;
    private IPEndPoint endPoint;

    private Material azureKinectScreenMaterial;

    private TextureSet textureSet;

    public int lastFrameId;

    private Vp8Decoder colorDecoder;
    private TrvlDecoder depthDecoder;

    public PrepareState State => state;

    public TextureSetUpdater(Material azureKinectScreenMaterial, int sessionId, IPEndPoint endPoint)
    {
        state = PrepareState.Unprepared;

        this.azureKinectScreenMaterial = azureKinectScreenMaterial;
        
        textureSet = new TextureSet();
        UnityEngine.Debug.Log($"textureGroup id: {textureSet.GetId()}");

        lastFrameId = -1;

        //textureGroup.SetWidth(initPacketData.depthWidth);
        //textureGroup.SetHeight(initPacketData.depthHeight);
        //PluginHelper.InitTextureGroup(textureGroup.GetId());

        colorDecoder = new Vp8Decoder();
        //depthDecoder = new TrvlDecoder(initPacketData.depthWidth * initPacketData.depthHeight);

        this.sessionId = sessionId;
        this.endPoint = endPoint;
    }

    public void StartPrepare(MonoBehaviour monoBehaviour, VideoSenderMessageData videoMessageData)
    {
        monoBehaviour.StartCoroutine(SetupTextureGroup(videoMessageData));
    }

    public IEnumerator SetupTextureGroup(VideoSenderMessageData videoMessageData)
    {
        if(state != PrepareState.Unprepared)
            throw new Exception("State has to be Unprepared to prepare TextureGroupUpdater.");

        state = PrepareState.Preparing;

        textureSet.SetWidth(videoMessageData.width);
        textureSet.SetHeight(videoMessageData.height);
        TelepresenceToolkitPlugin.InitTextureGroup(textureSet.GetId());

        depthDecoder = new TrvlDecoder(videoMessageData.width * videoMessageData.height);

        state = PrepareState.Prepared;

        while (!textureSet.IsInitialized())
            yield return null;

        // TextureGroup includes Y, U, V, and a depth texture.
        azureKinectScreenMaterial.SetTexture("_YTex", textureSet.GetYTexture());
        azureKinectScreenMaterial.SetTexture("_UvTex", textureSet.GetUvTexture());
        azureKinectScreenMaterial.SetTexture("_DepthTex", textureSet.GetDepthTexture());

        state = PrepareState.Prepared;
    }

    public void UpdateFrame(UdpSocket udpSocket, SortedDictionary<int, VideoSenderMessageData> videoMessages)
    {
        if (state != PrepareState.Prepared)
        {
            UnityEngine.Debug.Log("TextureGroupUpdater is not prepared yet...");
            return;
        }

        int? frameIdToRender = null;
        if (lastFrameId == -1)
        {
            // For the first frame, find a keyframe.
            foreach (var videoMessagePair in videoMessages)
            {
                if (videoMessagePair.Value.keyframe)
                {
                    frameIdToRender = videoMessagePair.Key;
                    break;
                }
            }
        }
        else
        {
            // If there is a key frame, use the most recent one.
            foreach (var videoMessagePair in videoMessages)
            {
                if (videoMessagePair.Key <= lastFrameId)
                    continue;

                if (videoMessagePair.Value.keyframe)
                    frameIdToRender = videoMessagePair.Key;
            }

            // Find if there is the next frame.
            if (!frameIdToRender.HasValue)
            {
                if (videoMessages.ContainsKey(lastFrameId + 1))
                    frameIdToRender = lastFrameId + 1;
            }
        }

        if (!frameIdToRender.HasValue)
        {
            return;
        }

        var videoMessage = videoMessages[frameIdToRender.Value];
        AVFrame avFrame = colorDecoder.Decode(videoMessage.colorEncoderFrame);
        DepthPixels depthPixels = depthDecoder.Decode(videoMessage.depthEncoderFrame, videoMessage.keyframe);

        lastFrameId = frameIdToRender.Value;

        udpSocket.Send(PacketHelper.createReportReceiverPacketBytes(sessionId, lastFrameId), endPoint);

        //Plugin.texture_group_set_ffmpeg_frame(textureGroup, ffmpegFrame.Ptr);
        textureSet.SetAvFrame(avFrame);
        //Plugin.texture_group_set_depth_pixels(textureGroup, trvlFrame.Ptr);
        textureSet.SetDepthPixels(depthPixels);
        TelepresenceToolkitPlugin.UpdateTextureGroup(textureSet.GetId());
    }
}
