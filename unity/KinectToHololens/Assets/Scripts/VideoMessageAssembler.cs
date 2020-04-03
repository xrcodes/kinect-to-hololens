using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;

class VideoMessageAssembler
{
    private const int XOR_MAX_GROUP_SIZE = 5;
    private int sessionId;
    private Dictionary<int, VideoSenderPacketData[]> videoPacketCollections;
    private Dictionary<int, FecSenderPacketData[]> fecPacketCollections;

    public VideoMessageAssembler(int sessionId)
    {
        this.sessionId = sessionId;
        videoPacketCollections = new Dictionary<int, VideoSenderPacketData[]>();
        fecPacketCollections = new Dictionary<int, FecSenderPacketData[]>();
    }

    public void Reassemble(UdpSocket udpSocket,
                           ConcurrentQueue<VideoSenderPacketData> videoPacketDataQueue,
                           ConcurrentQueue<FecSenderPacketData> fecPacketDataQueue,
                           int lastVideoFrameId,
                           ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue)
    {
        // The logic for XOR FEC packets are almost the same to frame packets.
        // The operations for XOR FEC packets should happen before the frame packets
        // so that frame packet can be created with XOR FEC packets when a missing
        // frame packet is detected.
        FecSenderPacketData fecSenderPacketData;
        while (fecPacketDataQueue.TryDequeue(out fecSenderPacketData))
        {
            if (fecSenderPacketData.frameId <= lastVideoFrameId)
                continue;

            if (!fecPacketCollections.ContainsKey(fecSenderPacketData.frameId))
            {
                fecPacketCollections[fecSenderPacketData.frameId] = new FecSenderPacketData[fecSenderPacketData.packetCount];
            }

            fecPacketCollections[fecSenderPacketData.frameId][fecSenderPacketData.packetIndex] = fecSenderPacketData;
        }

        VideoSenderPacketData videoSenderPacketData;
        while (videoPacketDataQueue.TryDequeue(out videoSenderPacketData))
        {
            if (videoSenderPacketData.frameId <= lastVideoFrameId)
                continue;

            // If there is a packet for a new frame, check the previous frames, and if
            // there is a frame with missing packets, try to create them using xor packets.
            // If using the xor packets fails, request the sender to retransmit the packets.
            if (!videoPacketCollections.ContainsKey(videoSenderPacketData.frameId))
            {
                videoPacketCollections[videoSenderPacketData.frameId] = new VideoSenderPacketData[videoSenderPacketData.packetCount];

                ///////////////////////////////////
                // Forward Error Correction Start//
                ///////////////////////////////////
                // Request missing packets of the previous frames.
                foreach (var collectionPair in videoPacketCollections)
                {
                    if (collectionPair.Key < videoSenderPacketData.frameId)
                    {
                        int missingFrameId = collectionPair.Key;
                        //var missingPacketIndices = collectionPair.Value.GetMissingPacketIds();

                        var missingPacketIndices = new List<int>();
                        for (int i = 0; i < collectionPair.Value.Length; ++i)
                        {
                            if (collectionPair.Value[i] == null)
                            {
                                missingPacketIndices.Add(i);
                            }
                        }

                        // Try correction using XOR FEC packets.
                        var fecFailedPacketIndices = new List<int>();
                        var fecPacketIndices = new List<int>();

                        // missing_packet_index cannot get error corrected if there is another missing_packet_index
                        // that belongs to the same XOR FEC packet...
                        foreach (int i in missingPacketIndices)
                        {
                            bool found = false;
                            foreach (int j in missingPacketIndices)
                            {
                                if (i == j)
                                    continue;

                                if ((i / XOR_MAX_GROUP_SIZE) == (j / XOR_MAX_GROUP_SIZE))
                                {
                                    found = true;
                                    break;
                                }
                            }
                            if (found)
                            {
                                fecFailedPacketIndices.Add(i);
                            }
                            else
                            {
                                fecPacketIndices.Add(i);
                            }
                        }

                        foreach (int fecPacketIndex in fecPacketIndices)
                        {
                            // Try getting the XOR FEC packet for correction.
                            int xorPacketIndex = fecPacketIndex / XOR_MAX_GROUP_SIZE;

                            if (!fecPacketCollections.ContainsKey(missingFrameId))
                            {
                                fecFailedPacketIndices.Add(fecPacketIndex);
                                continue;
                            }

                            var fecPacketData = fecPacketCollections[missingFrameId][xorPacketIndex];
                            // Give up if there is no xor packet yet.
                            if (fecPacketData == null)
                            {
                                fecFailedPacketIndices.Add(fecPacketIndex);
                                continue;
                            }

                            var fecVideoPacketData = new VideoSenderPacketData();

                            fecVideoPacketData.frameId = missingFrameId;
                            fecVideoPacketData.packetIndex = videoSenderPacketData.packetIndex;
                            fecVideoPacketData.packetCount = videoSenderPacketData.packetCount;
                            fecVideoPacketData.messageData = fecPacketData.bytes;

                            int beginFramePacketIndex = xorPacketIndex * XOR_MAX_GROUP_SIZE;
                            int endFramePacketIndex = Math.Min(beginFramePacketIndex + XOR_MAX_GROUP_SIZE, collectionPair.Value.Length);

                            // Run bitwise XOR with all other packets belonging to the same XOR FEC packet.
                            for (int i = beginFramePacketIndex; i < endFramePacketIndex; ++i)
                            {
                                if (i == fecPacketIndex)
                                    continue;

                                for (int j = 0; j < fecVideoPacketData.messageData.Length; ++j)
                                    fecVideoPacketData.messageData[j] ^= collectionPair.Value[i].messageData[j];
                            }

                            videoPacketCollections[missingFrameId][fecPacketIndex] = fecVideoPacketData;
                        } // end of foreach (int missingPacketIndex in missingPacketIndices)

                        udpSocket.Send(PacketHelper.createRequestReceiverPacketBytes(sessionId, collectionPair.Key, fecFailedPacketIndices));
                    }
                }
                /////////////////////////////////
                // Forward Error Correction End//
                /////////////////////////////////
            }
            // End of if (frame_packet_collections.find(frame_id) == frame_packet_collections.end())
            // which was for reacting to a packet for a new frame.

            videoPacketCollections[videoSenderPacketData.frameId][videoSenderPacketData.packetIndex] = videoSenderPacketData;
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

            var videoMessageData = VideoSenderMessageData.Parse(ms.ToArray());
            videoMessageQueue.Enqueue(new Tuple<int, VideoSenderMessageData>(fullFrameId, videoMessageData));

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