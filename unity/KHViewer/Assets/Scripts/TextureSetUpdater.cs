﻿using System;
using System.Collections;
using System.Collections.Generic;
using System.Net;
using UnityEngine;
using TelepresenceToolkit;

public class TextureSetUpdater
{
    public PrepareState State { get; private set; }
    private int receiverId;
    private IPEndPoint senderEndPoint;
    private TextureSet textureSet;
    public int lastFrameId;
    private Vp8Decoder colorDecoder;
    private TrvlDecoder depthDecoder;


    public TextureSetUpdater(int receiverId, IPEndPoint senderEndPoint)
    {
        State = PrepareState.Unprepared;
        this.receiverId = receiverId;
        this.senderEndPoint = senderEndPoint;
        textureSet = new TextureSet();
        Debug.Log($"textureSet ID: {textureSet.GetId()}");
        lastFrameId = -1;
    }

    public IEnumerator Prepare(Material azureKinectScreenMaterial, VideoSenderMessage videoMessage)
    {
        if(State != PrepareState.Unprepared)
            throw new Exception("State has to be Unprepared to prepare TextureGroupUpdater.");

        State = PrepareState.Preparing;

        textureSet.SetWidth(videoMessage.width);
        textureSet.SetHeight(videoMessage.height);
        TelepresenceToolkitPlugin.InitTextureGroup(textureSet.GetId());

        colorDecoder = new Vp8Decoder();
        depthDecoder = new TrvlDecoder(videoMessage.width * videoMessage.height);

        State = PrepareState.Prepared;

        while (!textureSet.IsInitialized())
            yield return null;

        // TextureGroup includes Y, U, V, and a depth texture.
        azureKinectScreenMaterial.SetTexture("_YTex", textureSet.GetYTexture());
        azureKinectScreenMaterial.SetTexture("_UvTex", textureSet.GetUvTexture());
        azureKinectScreenMaterial.SetTexture("_DepthTex", textureSet.GetDepthTexture());

        State = PrepareState.Prepared;
    }

    public void UpdateFrame(UdpSocket udpSocket, SortedDictionary<int, VideoSenderMessage> videoMessages)
    {
        if (State != PrepareState.Prepared)
        {
            Debug.Log("TextureGroupUpdater is not prepared yet...");
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

        udpSocket.Send(PacketUtils.createReportReceiverPacketBytes(receiverId, lastFrameId).bytes, senderEndPoint);

        textureSet.SetAvFrame(avFrame);
        textureSet.SetDepthPixels(depthPixels);
        TelepresenceToolkitPlugin.UpdateTextureGroup(textureSet.GetId());
    }
}
