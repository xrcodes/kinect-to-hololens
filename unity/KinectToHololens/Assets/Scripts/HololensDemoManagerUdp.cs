using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using UnityEngine;


class FrameMessage
{
    private byte[] message;
    public int FrameId { get; private set; }
    public float FrameTimeStamp { get; private set; }
    public bool Keyframe { get; private set; }
    public int ColorEncoderFrameSize { get; private set; }
    public int DepthEncoderFrameSize { get; private set; }

    private FrameMessage(byte[] message, int frameId, float frameTimeStamp,
                         bool keyframe, int colorEncoderFrameSize, int depthEncoderFrameSize)
    {
        this.message = message;
        FrameId = frameId;
        FrameTimeStamp = frameTimeStamp;
        Keyframe = keyframe;
        ColorEncoderFrameSize = colorEncoderFrameSize;
        DepthEncoderFrameSize = depthEncoderFrameSize;
    }

    public static FrameMessage Create(byte[] message)
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
                                depthEncoderFrameSize: depthEncoderFrameSize);
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

class FramePacketCollection
{
    public int FrameId { get; private set; }
    public int PacketCount { get; private set; }
    private byte[][] packets;

    public FramePacketCollection(int frameId, int packetCount)
    {
        FrameId = frameId;
        PacketCount = packetCount;
        packets = new byte[packetCount][];
        for(int i = 0; i < packetCount; ++i)
        {
            packets[i] = new byte[0];
        }
    }

    public void AddPacket(int packetIndex, byte[] packet)
    {
        packets[packetIndex] = packet;
    }

    public bool IsFull()
    {
        foreach (var packet in packets)
        {
            if (packet.Length == 0)
                return false;
        }

        return true;
    }

    public FrameMessage ToMessage()
    {
        int messageSize = 0;
        foreach (var packet in packets)
        {
            messageSize += packet.Length - 13;
        }

        byte[] message = new byte[messageSize];
        for (int i = 0; i < packets.Length; ++i)
        {
            int cursor = (1500 - 13) * i;
            Array.Copy(packets[i], 13, message, cursor, packets[i].Length - 13);
        }

        return FrameMessage.Create(message);
    }
};

public class HololensDemoManagerUdp : MonoBehaviour
{
    private UdpSocket udpSocket;
    private Dictionary<int, FramePacketCollection> framePacketCollections;
    private List<FrameMessage> frameMessages;
    private int lastFrameId;

    void Awake()
    {
        udpSocket = new UdpSocket(1024 * 1024);
        framePacketCollections = new Dictionary<int, FramePacketCollection>();
        frameMessages = new List<FrameMessage>();
        lastFrameId = -1;

        var address = IPAddress.Parse("127.0.0.1");
        int port = 7777;
        var bytes = new byte[1];
        bytes[0] = 1;

        udpSocket.SendTo(bytes, address, port);
    }

    void Update()
    {
        float start = Time.time;
        var packets = new List<byte[]>();
        while (udpSocket.Available > 0)
        {
            var packet = new byte[1500];
            var receiveResult = udpSocket.Receive(packet);
            int packetSize = receiveResult.Item1;
            var error = receiveResult.Item2;

            if(error != SocketError.Success)
            {
                break;
            }

            if (packetSize != packet.Length)
            {
                var resizedPacket = new byte[packetSize];
                Array.Copy(packet, 0, resizedPacket, 0, packetSize);
                packets.Add(resizedPacket);
            }
            else
            {
                packets.Add(packet);
            }
        }

        foreach (var packet in packets)
        {
            var packetType = packet[0];
            if (packetType == 0)
            {
                print("Received calibration packet.");
            }
            else if (packetType == 1)
            {
                int frameId = BitConverter.ToInt32(packet, 1);
                int packetIndex = BitConverter.ToInt32(packet, 5);
                int packetCount = BitConverter.ToInt32(packet, 9);

                if (!framePacketCollections.ContainsKey(frameId))
                {
                    framePacketCollections[frameId] = new FramePacketCollection(frameId, packetCount);
                }

                framePacketCollections[frameId].AddPacket(packetIndex, packet);
            }
        }

        // Find all full collections and their frame_ids.
        var fullFrameIds = new List<int>();
        foreach (var collectionPair in framePacketCollections)
        {
            if (collectionPair.Value.IsFull())
            {
                int frameId = collectionPair.Key;
                fullFrameIds.Add(frameId);
            }
        }

        // Extract messages from the full collections.
        foreach (int fullFrameId in fullFrameIds)
        {
            frameMessages.Add(framePacketCollections[fullFrameId].ToMessage());
            framePacketCollections.Remove(fullFrameId);
        }

        frameMessages.Sort((x, y) => x.FrameId.CompareTo(y.FrameId));

        if(frameMessages.Count == 0)
        {
            return;
        }

        int? beginIndex = null;
        // If there is a key frame, use the most recent one.
        for (int i = frameMessages.Count - 1; i >= 0; --i)
        {
            if (frameMessages[i].Keyframe)
            {
                beginIndex = i;
                break;
            }
        }

        // When there is no key frame, go through all the frames if the first
        // FrameMessage is the one right after the previously rendered one.
        if (!beginIndex.HasValue)
        {
            if (frameMessages[0].FrameId == lastFrameId + 1)
            {
                beginIndex = 0;
            }
            else
            {
                // Wait for more frames if there is way to render without glitches.
                return;
            }
        }

        print($"frameMessages.Count: {frameMessages.Count}");
        // Reset frame_messages after they are displayed.
        frameMessages = new List<FrameMessage>();
    }

    void OnApplicationQuit()
    {
        udpSocket.Dispose();
    }

    int CompareFramePacketCollection(FramePacketCollection lhs, FramePacketCollection rhs)
    {
        return lhs.FrameId.CompareTo(rhs.FrameId);
    }
}
