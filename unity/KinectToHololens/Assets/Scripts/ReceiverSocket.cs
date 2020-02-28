using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;

public class ReceiverSocket
{
    private UdpSocket socket;
    public IPAddress Address { get; private set; }
    public int Port { get; private set; }

    public ReceiverSocket(int receiveBufferSize)
    {
        socket = new UdpSocket(receiveBufferSize);
    }

    public void Ping(IPAddress address, int port)
    {
        Address = address;
        Port = port;

        var bytes = new byte[1];
        bytes[0] = 0;
        socket.SendTo(bytes, address, port);
    }

    public byte[] Receive(out SocketError error)
    {
        var packet = new byte[PacketHelper.PACKET_SIZE];
        var receiveResult = socket.Receive(packet);
        int packetSize = receiveResult.Item1;
        error = receiveResult.Item2;

        if (error != SocketError.Success)
        {
            return null;
        }

        if (packetSize != packet.Length)
        {
            var resizedPacket = new byte[packetSize];
            Array.Copy(packet, 0, resizedPacket, 0, packetSize);
            return resizedPacket;
        }
        else
        {
            return packet;
        }
    }

    // Sends a message to the Sender that a Kinect frame was received.
    public void Send(int frameId, float decoderMs, float frameMs, int packetCount)
    {
        var ms = new MemoryStream();
        ms.WriteByte((byte) ReceiverPacketType.Report);
        ms.Write(BitConverter.GetBytes(frameId), 0, 4);
        ms.Write(BitConverter.GetBytes(decoderMs), 0, 4);
        ms.Write(BitConverter.GetBytes(frameMs), 0, 4);
        ms.Write(BitConverter.GetBytes(packetCount), 0, 4);
        socket.SendTo(ms.ToArray(), Address, Port);
    }

    public void Send(int frameId, List<int> missingPacketIds)
    {
        var ms = new MemoryStream();
        ms.WriteByte((byte) ReceiverPacketType.Request);
        ms.Write(BitConverter.GetBytes(frameId), 0, 4);
        foreach(int missingPacketId in missingPacketIds)
        {
            ms.Write(BitConverter.GetBytes(missingPacketId), 0, 4);
        }
        socket.SendTo(ms.ToArray(), Address, Port);
    }
}
