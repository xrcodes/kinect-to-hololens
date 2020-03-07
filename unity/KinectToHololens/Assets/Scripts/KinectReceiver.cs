using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net.Sockets;
using System.IO;
using System.Threading;
using UnityEngine;

public class KinectReceiver
{
    private const int KH_SAMPLE_RATE = 48000;
    private const int KH_CHANNEL_COUNT = 2;
    private const double KH_LATENCY_SECONDS = 0.2;
    private const int KH_SAMPLES_PER_FRAME = 960;
    private const int KH_BYTES_PER_SECOND = KH_SAMPLE_RATE * KH_CHANNEL_COUNT * sizeof(float);

    private Material azureKinectScreenMaterial;
    private AzureKinectScreen azureKinectScreen;

    private TextureGroup textureGroup;

    private UdpSocket udpSocket;
    private bool receiverStopped;
    private ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue;
    private int lastVideoFrameId;

    private Vp8Decoder colorDecoder;
    private TrvlDecoder depthDecoder;
    private bool preapared;

    private RingBuffer ringBuffer;
    private AudioDecoder audioDecoder;
    private int lastAudioFrameId;

    private Dictionary<int, VideoSenderMessageData> videoMessages;
    private Stopwatch frameStopWatch;

    public RingBuffer RingBuffer
    {
        get
        {
            return ringBuffer;
        }
    }

    public KinectReceiver(Material azureKinectScreenMaterial, AzureKinectScreen azureKinectScreen)
    {
        this.azureKinectScreenMaterial = azureKinectScreenMaterial;
        this.azureKinectScreen = azureKinectScreen;

        textureGroup = new TextureGroup(Plugin.texture_group_reset());

        udpSocket = null;
        receiverStopped = false;
        videoMessageQueue = new ConcurrentQueue<Tuple<int, VideoSenderMessageData>>();
        lastVideoFrameId = -1;

        colorDecoder = null;
        depthDecoder = null;
        preapared = false;

        ringBuffer = new RingBuffer((int)(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND / sizeof(float)));
        audioDecoder = new AudioDecoder(KH_SAMPLE_RATE, KH_CHANNEL_COUNT);
        lastAudioFrameId = -1;

        videoMessages = new Dictionary<int, VideoSenderMessageData>();
        frameStopWatch = Stopwatch.StartNew();
    }

    public void UpdateFrame()
    {
        // If texture is not created, create and assign them to quads.
        if (!preapared)
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

                preapared = true;

                UnityEngine.Debug.Log("textureGroup intialized");
            }
        }

        if (udpSocket == null)
            return;

        UpdateTextureGroup();
    }

    public void Stop()
    {
        receiverStopped = true;
    }

    public void Ping(UdpSocket udpSocket)
    {
        int senderSessionId = -1;
        int pingCount = 0;
        while (true)
        {
            bool initialized = false;
            udpSocket.Send(PacketHelper.createPingReceiverPacketBytes());
            ++pingCount;
            UnityEngine.Debug.Log("Sent ping");

            //Thread.Sleep(100);
            Thread.Sleep(300);

            SocketError error = SocketError.WouldBlock;
            while (true)
            {
                var packet = udpSocket.Receive(out error);
                if (packet == null)
                    break;

                int cursor = 0;
                int sessionId = BitConverter.ToInt32(packet, cursor);
                cursor += 4;

                var packetType = (SenderPacketType)packet[cursor];
                cursor += 1;
                if (packetType != SenderPacketType.Init)
                {
                    UnityEngine.Debug.Log($"A different kind of a packet received before an init packet: {packetType}");
                    continue;
                }

                senderSessionId = sessionId;

                var initSenderPacketData = InitSenderPacketData.Parse(packet);

                textureGroup.SetWidth(initSenderPacketData.depthWidth);
                textureGroup.SetHeight(initSenderPacketData.depthHeight);
                PluginHelper.InitTextureGroup();

                colorDecoder = new Vp8Decoder();
                depthDecoder = new TrvlDecoder(initSenderPacketData.depthWidth * initSenderPacketData.depthHeight);

                azureKinectScreen.Setup(initSenderPacketData);

                initialized = true;
                break;
            }
            if (initialized)
                break;

            if (pingCount == 10)
            {
                UnityEngine.Debug.Log("Tried pinging 10 times and failed to received an init packet...\n");
                return;
            }
        }

        this.udpSocket = udpSocket;
        var videoPacketDataQueue = new ConcurrentQueue<VideoSenderPacketData>();
        var fecPacketDataQueue = new ConcurrentQueue<FecSenderPacketData>();
        var audioPacketDataQueue = new ConcurrentQueue<AudioSenderPacketData>();

        var taskThread = new Thread(() =>
        {
            var receiveSenderPacketTask = new ReceiveSenderPacketTask();
            var reassembleVideoMessageTask = new ReassembleVideoMessageTask();
            var consumeAudioPacketTask = new ConsumeAudioPacketTask();

            while(!receiverStopped)
            {
                receiveSenderPacketTask.Run(this,
                                            senderSessionId,
                                            videoPacketDataQueue,
                                            fecPacketDataQueue,
                                            audioPacketDataQueue);
                reassembleVideoMessageTask.Run(this, videoPacketDataQueue, fecPacketDataQueue);
                consumeAudioPacketTask.Run(this, audioPacketDataQueue);
            }

            receiverStopped = true;
        });

        taskThread.Start();
    }

    class ReceiveSenderPacketTask
    {
        public void Run(KinectReceiver kinectReceiver,
                        int senderSessionId,
                        ConcurrentQueue<VideoSenderPacketData> videoPacketDataQueue,
                        ConcurrentQueue<FecSenderPacketData> fecPacketDataQueue,
                        ConcurrentQueue<AudioSenderPacketData> audioPacketDataQueue)
        {
            SocketError error = SocketError.WouldBlock;
            while (true)
            {
                var packet = kinectReceiver.udpSocket.Receive(out error);
                if (packet == null)
                    break;

                int sessionId = PacketHelper.getSessionIdFromSenderPacketBytes(packet);
                var packetType = PacketHelper.getPacketTypeFromSenderPacketBytes(packet);

                if (sessionId != senderSessionId)
                    continue;

                if (packetType == SenderPacketType.Frame)
                {
                    videoPacketDataQueue.Enqueue(VideoSenderPacketData.Parse(packet));
                }
                else if (packetType == SenderPacketType.Fec)
                {
                    fecPacketDataQueue.Enqueue(FecSenderPacketData.Parse(packet));
                }
                else if (packetType == SenderPacketType.Audio)
                {
                    audioPacketDataQueue.Enqueue(AudioSenderPacketData.Parse(packet));
                }
            }

            if (error != SocketError.WouldBlock)
            {
                UnityEngine.Debug.Log($"Error from receiving packets: {error.ToString()}");
            }
        }
    }

    class ReassembleVideoMessageTask
    {
        private const int XOR_MAX_GROUP_SIZE = 5;
        private Dictionary<int, VideoSenderPacketData[]> videoPacketCollections = new Dictionary<int, VideoSenderPacketData[]>();
        private Dictionary<int, FecSenderPacketData[]> fecPacketCollections = new Dictionary<int, FecSenderPacketData[]>();

        public void Run(KinectReceiver kinectReceiver,
                        ConcurrentQueue<VideoSenderPacketData> videoPacketDataQueue,
                        ConcurrentQueue<FecSenderPacketData> fecPacketDataQueue)
        {
            // The logic for XOR FEC packets are almost the same to frame packets.
            // The operations for XOR FEC packets should happen before the frame packets
            // so that frame packet can be created with XOR FEC packets when a missing
            // frame packet is detected.
            FecSenderPacketData fecSenderPacketData;
            while (fecPacketDataQueue.TryDequeue(out fecSenderPacketData))
            {
                if (fecSenderPacketData.frameId <= kinectReceiver.lastVideoFrameId)
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
                if (videoSenderPacketData.frameId <= kinectReceiver.lastVideoFrameId)
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

                            kinectReceiver.udpSocket.Send(PacketHelper.createRequestReceiverPacketBytes(collectionPair.Key, fecFailedPacketIndices));
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
                kinectReceiver.videoMessageQueue.Enqueue(new Tuple<int, VideoSenderMessageData>(fullFrameId, videoMessageData));

                videoPacketCollections.Remove(fullFrameId);
            }

            // Clean up frame_packet_collections.
            var obsoleteFrameIds = new List<int>();
            foreach (var collectionPair in videoPacketCollections)
            {
                if (collectionPair.Key <= kinectReceiver.lastVideoFrameId)
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

    class ConsumeAudioPacketTask
    {
        public void Run(KinectReceiver kinectReceiver,
                        ConcurrentQueue<AudioSenderPacketData> audioPacketDataQueue)
        {
            var audioPacketDataSet = new List<AudioSenderPacketData>();
            {
                AudioSenderPacketData audioPacketData;
                while (audioPacketDataQueue.TryDequeue(out audioPacketData))
                {
                    audioPacketDataSet.Add(audioPacketData);
                }
            }

            audioPacketDataSet.Sort((x, y) => x.frameId.CompareTo(y.frameId));

            float[] pcm = new float[KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT];
            int index = 0;
            while (kinectReceiver.ringBuffer.FreeSamples >= pcm.Length)
            {
                if (index >= audioPacketDataSet.Count)
                    break;

                var audioPacketData = audioPacketDataSet[index++];
                if (audioPacketData.frameId <= kinectReceiver.lastAudioFrameId)
                    continue;

                kinectReceiver.audioDecoder.Decode(audioPacketData.opusFrame, pcm, KH_SAMPLES_PER_FRAME);
                kinectReceiver.ringBuffer.Write(pcm);
                kinectReceiver.lastAudioFrameId = audioPacketData.frameId;
            }
        }
    }

    private void UpdateTextureGroup()
    {
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
        //for (int i = beginIndex.Value; i < frameMessages.Count; ++i)
        //{
        //    var frameMessage = frameMessages[i];
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

        //print($"id: {lastFrameId}, packet collection time: {packetCollectionTime.TotalMilliseconds}, " +
        //      $"decoder time: {decoderTime.TotalMilliseconds}, frame time: {frameTime.TotalMilliseconds}");

        udpSocket.Send(PacketHelper.createReportReceiverPacketBytes(lastVideoFrameId,
                                                                    (float)decoderTime.TotalMilliseconds,
                                                                    (float)frameTime.TotalMilliseconds));

        // Invokes a function to be called in a render thread.
        if (preapared)
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
