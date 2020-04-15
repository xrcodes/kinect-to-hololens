using System;
using System.Net;
using System.Net.Sockets;

public class UdpSocketException : Exception
{
    public UdpSocketException(string message) : base(message)
    {
    }
}

public class UdpSocket
{
    private Socket socket;

    public UdpSocket(Socket socket)
    {
        this.socket = socket;
        socket.Blocking = false;
    }

    public byte[] Receive()
    {
        var bytes = new byte[PacketHelper.PACKET_SIZE];
        SocketError error;
        var packetSize = socket.Receive(bytes, 0, bytes.Length, SocketFlags.None, out error);

        if (error == SocketError.WouldBlock)
        {
            return null;
        }

        if (error != SocketError.Success)
        {
            throw new UdpSocketException($"Failed to receive bytes: { error }");
        }

        if (packetSize != bytes.Length)
        {
            var resizedBytes = new byte[packetSize];
            Array.Copy(bytes, 0, resizedBytes, 0, packetSize);
            return resizedBytes;
        }
        else
        {
            return bytes;
        }
    }

    public int Send(byte[] buffer, IPEndPoint endPoint)
    {
        return socket.SendTo(buffer, 0, buffer.Length, SocketFlags.None, endPoint);
    }
}
