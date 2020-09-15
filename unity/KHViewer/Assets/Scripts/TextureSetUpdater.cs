﻿using System;
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

    public int lastVideoFrameId;

    private Vp8Decoder colorDecoder;
    private TrvlDecoder depthDecoder;

    private Dictionary<int, VideoSenderMessageData> videoMessages;

    public PrepareState State => state;

    public TextureSetUpdater(Material azureKinectScreenMaterial, int sessionId, IPEndPoint endPoint)
    {
        state = PrepareState.Unprepared;

        this.azureKinectScreenMaterial = azureKinectScreenMaterial;
        
        textureSet = new TextureSet();
        UnityEngine.Debug.Log($"textureGroup id: {textureSet.GetId()}");

        lastVideoFrameId = -1;

        videoMessages = new Dictionary<int, VideoSenderMessageData>();

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

    public void UpdateFrame(UdpSocket udpSocket, List<Tuple<int, VideoSenderMessageData>> videoMessageList)
    {
        if (state != PrepareState.Prepared)
        {
            UnityEngine.Debug.Log("TextureGroupUpdater is not prepared yet...");
            return;
        }

        foreach (var frameMessagePair in videoMessageList)
        {
            // C# Dictionary throws an error when you add an element with
            // a key that is already taken.
            if (videoMessages.ContainsKey(frameMessagePair.Item1))
                continue;

            videoMessages.Add(frameMessagePair.Item1, frameMessagePair.Item2);
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
        AVFrame avFrame = null;
        DepthPixels depthPixels = null;

        for (int i = beginFrameId.Value; ; ++i)
        {
            if (!videoMessages.ContainsKey(i))
                break;

            var frameMessage = videoMessages[i];
            lastVideoFrameId = i;

            var colorEncoderFrame = frameMessage.colorEncoderFrame;
            var depthEncoderFrame = frameMessage.depthEncoderFrame;

            avFrame = colorDecoder.Decode(colorEncoderFrame);
            depthPixels = depthDecoder.Decode(depthEncoderFrame, frameMessage.keyframe);
        }

        udpSocket.Send(PacketHelper.createReportReceiverPacketBytes(sessionId, lastVideoFrameId), endPoint);

        //Plugin.texture_group_set_ffmpeg_frame(textureGroup, ffmpegFrame.Ptr);
        textureSet.SetAvFrame(avFrame);
        //Plugin.texture_group_set_depth_pixels(textureGroup, trvlFrame.Ptr);
        textureSet.SetDepthPixels(depthPixels);
        TelepresenceToolkitPlugin.UpdateTextureGroup(textureSet.GetId());

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