using System;
using System.IO;
using System.Net;
using System.Net.Sockets;

public class ReceiverUdp
{
    private UdpSocket socket;
    private IPAddress address;
    private int port;

    public ReceiverUdp(int receiveBufferSize)
    {
        socket = new UdpSocket(receiveBufferSize);
    }

    public void Ping(IPAddress address, int port)
    {
        this.address = address;
        this.port = port;

        var bytes = new byte[1];
        bytes[0] = 1;
        socket.SendTo(bytes, address, port);
    }

    public byte[] Receive()
    {
        // Not sure this is the best way but
        // even when the socket is non-blocking,
        // it takes to much time to receive then
        // leave by a WouldBlock error.
        if(socket.Available == 0)
        {
            return null;
        }

        var packet = new byte[1500];
        var receiveResult = socket.Receive(packet);
        int packetSize = receiveResult.Item1;
        var error = receiveResult.Item2;

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
    public void Send(int frameId)
    {
        var ms = new MemoryStream();
        ms.WriteByte(0);
        ms.Write(BitConverter.GetBytes(frameId), 0, 4);
        socket.SendTo(ms.ToArray(), address, port);
    }
}
