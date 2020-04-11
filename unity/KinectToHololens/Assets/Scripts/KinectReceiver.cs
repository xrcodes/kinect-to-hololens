using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;

public class KinectReceiver
{
    private const float HEARTBEAT_INTERVAL_SEC = 1.0f;
    private const float HEARTBEAT_TIME_OUT_SEC = 5.0f;

    private UdpSocket udpSocket;
    private int sessionId;
    private IPEndPoint endPoint;
    private VideoMessageAssembler videoMessageAssembler;
    private AudioPacketReceiver audioPacketReceiver;
    private Stopwatch heartbeatStopWatch;
    private Stopwatch receivedAnyStopWatch;

    public KinectReceiver(UdpSocket udpSocket, int sessionId, IPEndPoint endPoint, VideoMessageAssembler videoMessageAssembler, AudioPacketReceiver audioPacketReceiver)
    {
        this.udpSocket = udpSocket;
        this.sessionId = sessionId;
        this.endPoint = endPoint;
        this.videoMessageAssembler = videoMessageAssembler;
        this.audioPacketReceiver = audioPacketReceiver;
        heartbeatStopWatch = Stopwatch.StartNew();
        receivedAnyStopWatch = Stopwatch.StartNew();
    }

    public bool UpdateFrame(KinectRenderer kinectRenderer,
                            AzureKinectSpeaker azureKinectSpeaker,
                            ConcurrentQueue<Tuple<int, VideoSenderMessageData>> videoMessageQueue,
                            ConcurrentQueue<FloorSenderPacketData> floorPacketDataQueue)
    {
        try
        {
            if (heartbeatStopWatch.Elapsed.TotalSeconds > HEARTBEAT_INTERVAL_SEC)
            {
                udpSocket.Send(PacketHelper.createHeartbeatReceiverPacketBytes(sessionId), endPoint);
                heartbeatStopWatch = Stopwatch.StartNew();
            }

            var senderPacketSet = SenderPacketReceiver.Receive(udpSocket, floorPacketDataQueue);
            if (senderPacketSet.ReceivedAny)
            {
                videoMessageAssembler.Assemble(udpSocket,
                                               senderPacketSet.VideoPacketDataList,
                                               senderPacketSet.FecPacketDataList,
                                               kinectRenderer.lastVideoFrameId,
                                               videoMessageQueue);
                audioPacketReceiver.Receive(senderPacketSet.AudioPacketDataList, azureKinectSpeaker.RingBuffer);
                receivedAnyStopWatch = Stopwatch.StartNew();
            }
            else
            {
                if (receivedAnyStopWatch.Elapsed.TotalSeconds > HEARTBEAT_TIME_OUT_SEC)
                {
                    UnityEngine.Debug.Log($"Timed out after waiting for {HEARTBEAT_TIME_OUT_SEC} seconds without a received packet.");
                    //break;
                    return false;
                }
            }
        }
        catch (UdpSocketException e)
        {
            UnityEngine.Debug.Log($"UdpSocketRuntimeError: {e}");
            //break;
            return false;
        }

        return true;
    }
}
