using System;
using System.Net;
using System.Net.Sockets;

public class ReceiverSocket
{
    private Socket socket;
    private IPEndPoint remoteEndPoint;

    public ReceiverSocket(Socket socket, IPEndPoint remoteEndPoint)
    {
        this.socket = socket;
        this.remoteEndPoint = remoteEndPoint;
        socket.Blocking = false;
    }

    //public void Ping()
    //{
    //    socket.SendTo(PacketHelper.createPingReceiverPacketBytes(), remoteEndPoint);
    //}

    public byte[] Receive(out SocketError error)
    {
        var packet = new byte[PacketHelper.PACKET_SIZE];
        var packetSize = socket.Receive(packet, 0, packet.Length, SocketFlags.None, out error);

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

    public int Send(byte[] buffer)
    {
        return socket.SendTo(buffer, 0, buffer.Length, SocketFlags.None, remoteEndPoint);
    }
}
