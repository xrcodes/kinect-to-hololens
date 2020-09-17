﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Net;

public class VideoMessageAssembler
{
    private const int FEC_GROUP_SIZE = 2;
    private int sessionId;
    private IPEndPoint remoteEndPoint;
    private Dictionary<int, VideoSenderPacket[]> videoPacketCollections;
    private Dictionary<int, ParitySenderPacket[]> parityPacketCollections;

    public VideoMessageAssembler(int sessionId, IPEndPoint remoteEndPoint)
    {
        this.sessionId = sessionId;
        this.remoteEndPoint = remoteEndPoint;
        videoPacketCollections = new Dictionary<int, VideoSenderPacket[]>();
        parityPacketCollections = new Dictionary<int, ParitySenderPacket[]>();
    }

    public void Assemble(UdpSocket udpSocket,
                         List<VideoSenderPacket> videoPacketDataList,
                         List<ParitySenderPacket> parityPacketDataList,
                         int lastVideoFrameId,
                         IDictionary<int, VideoSenderMessage> videoMessages)
    {
        int? addedFrameId = null;
        // Collect the received video packets.
        foreach (var videoSenderPacketData in videoPacketDataList)
        {
            if (videoSenderPacketData.frameId <= lastVideoFrameId)
                continue;

            if(!videoPacketCollections.ContainsKey(videoSenderPacketData.frameId))
            {
                videoPacketCollections[videoSenderPacketData.frameId] = new VideoSenderPacket[videoSenderPacketData.packetCount];
                // Assign the largest new frame_id.
                if (!addedFrameId.HasValue || addedFrameId < videoSenderPacketData.frameId)
                    addedFrameId = videoSenderPacketData.frameId;
            }

            videoPacketCollections[videoSenderPacketData.frameId][videoSenderPacketData.packetIndex] = videoSenderPacketData;
        }

        // Collect the received parity packets.
        foreach (var paritySenderPacketData in parityPacketDataList)
        {
            if (paritySenderPacketData.frameId <= lastVideoFrameId)
                continue;

            if (!parityPacketCollections.ContainsKey(paritySenderPacketData.frameId))
            {
                int parityPacketCount = (paritySenderPacketData.videoPacketCount - 1) / FEC_GROUP_SIZE + 1;
                parityPacketCollections[paritySenderPacketData.frameId] = new ParitySenderPacket[parityPacketCount];
            }

            parityPacketCollections[paritySenderPacketData.frameId][paritySenderPacketData.packetIndex] = paritySenderPacketData;
        }

        if (addedFrameId.HasValue)
        {
            foreach (var videoPacketCollection in videoPacketCollections)
            {
                int frameId = videoPacketCollection.Key;
                VideoSenderPacket[] videoPackets = videoPacketCollection.Value;

                // Skip the frame that just got added or even newer.
                if (frameId >= addedFrameId)
                    continue;

                // Find the parity packet collection corresponding to the video packet collection.
                // Skip if there is no parity packet collection for the video frame.
                if (!parityPacketCollections.ContainsKey(frameId))
                    continue;

                ParitySenderPacket[] parityPackets = parityPacketCollections[frameId];

                // Loop per each parity packet.
                // Collect video packet indices to request.
                List<int> videoPacketIndiecsToRequest = new List<int>();
                List<int> parityPacketIndiecsToRequest = new List<int>();

                for (int parityPacketIndex = 0; parityPacketIndex < parityPackets.Length; ++parityPacketIndex)
                {
                    // Range of the video packets that correspond to the parity packet.
                    int videoPacketStartIndex = parityPacketIndex * FEC_GROUP_SIZE;
                    // Pick the end index with the end of video packet indices in mind (i.e., prevent overflow).
                    int videoPacketEndIndex = Math.Min(videoPacketStartIndex + FEC_GROUP_SIZE, videoPackets.Length);

                    // If the parity packet is missing, request all missing video packets and skip the FEC process.
                    // Also request the parity packet if there is a relevant missing video packet.
                    if (parityPackets[parityPacketIndex] == null)
                    {
                        bool parityPacketNeeded = false;
                        for (int videoPacketIndex = videoPacketStartIndex; videoPacketIndex < videoPacketEndIndex; ++videoPacketIndex)
                        {
                            if (videoPackets[videoPacketIndex] == null)
                            {
                                videoPacketIndiecsToRequest.Add(videoPacketIndex);
                                parityPacketNeeded = true;
                            }
                        }
                        if (parityPacketNeeded)
                            parityPacketIndiecsToRequest.Add(parityPacketIndex);
                        continue;
                    }

                    // Find if there is existing video packets and missing video packet indices.
                    // Check all video packets that relates to this parity packet.
                    var existingVideoPackets = new List<VideoSenderPacket>();
                    var missingVideoPacketIndices = new List<int>();
                    for (int videoPacketIndex = videoPacketStartIndex; videoPacketIndex < videoPacketEndIndex; ++videoPacketIndex)
                    {
                        if (videoPackets[videoPacketIndex] != null)
                        {
                            existingVideoPackets.Add(videoPackets[videoPacketIndex]);
                        }
                        else
                        {
                            missingVideoPacketIndices.Add(videoPacketIndex);
                        }
                    }

                    // Skip if there all video packets already exist.
                    if (missingVideoPacketIndices.Count == 0)
                        continue;

                    // XOR based FEC only works for a single missing packet.
                    if (missingVideoPacketIndices.Count > 1)
                    {
                        foreach (int missingIndex in missingVideoPacketIndices)
                        {
                            // Add the missing video packet indices for the vector to request them.
                            videoPacketIndiecsToRequest.Add(missingIndex);
                        }
                        continue;
                    }

                    // The missing video packet index.
                    int missingVideoPacketIndex = missingVideoPacketIndices[0];

                    // Reconstruct the missing video packet.
                    VideoSenderPacket fecVideoPacketData = new VideoSenderPacket();
                    fecVideoPacketData.frameId = frameId;
                    fecVideoPacketData.packetIndex = missingVideoPacketIndex;
                    fecVideoPacketData.packetCount = videoPackets.Length;
                    // Assign from the parity packet here since other video packets will be XOR'ed in the below loop.
                    fecVideoPacketData.messageData = parityPackets[parityPacketIndex].bytes;

                    foreach (var existingVideoPacket in existingVideoPackets)
                    {
                        for (int i = 0; i < fecVideoPacketData.messageData.Length; ++i)
                            fecVideoPacketData.messageData[i] ^= existingVideoPacket.messageData[i];
                    }

                    // Insert the reconstructed packet.
                    videoPacketCollections[frameId][missingVideoPacketIndex] = fecVideoPacketData;
                }
                // Request the video packets that FEC was not enough to fix.
                udpSocket.Send(PacketUtils.createRequestReceiverPacketBytes(sessionId, frameId, false, videoPacketIndiecsToRequest, parityPacketIndiecsToRequest).bytes, remoteEndPoint);
            }
        }

        // Find all full collections and their frame_ids.
        var fullFrameIds = new List<int>();
        foreach (var collectionPair in videoPacketCollections)
        {
            bool full = true;
            foreach (var packetData in collectionPair.Value)
            {
                if (packetData == null)
                {
                    full = false;
                    break;
                }
            }

            if (full)
            {
                int frameId = collectionPair.Key;
                fullFrameIds.Add(frameId);
            }
        }

        // Extract messages from the full collections.
        foreach (int fullFrameId in fullFrameIds)
        {
            var ms = new MemoryStream();
            foreach (var packetData in videoPacketCollections[fullFrameId])
            {
                ms.Write(packetData.messageData, 0, packetData.messageData.Length);
            }

            var videoMessageData = VideoSenderMessage.Create(ms.ToArray());
            videoMessages.Add(fullFrameId, videoMessageData);

            videoPacketCollections.Remove(fullFrameId);
        }

        // Clean up frame_packet_collections.
        var obsoleteFrameIds = new List<int>();
        foreach (var collectionPair in videoPacketCollections)
        {
            if (collectionPair.Key <= lastVideoFrameId)
            {
                obsoleteFrameIds.Add(collectionPair.Key);
            }
        }

        foreach (int obsoleteFrameId in obsoleteFrameIds)
        {
            videoPacketCollections.Remove(obsoleteFrameId);
        }
    }
}