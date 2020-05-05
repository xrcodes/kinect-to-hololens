using System;
using System.Net;
using System.Net.Sockets;

public class UdpSocketPacket
{
    public byte[] bytes;
    public IPEndPoint endPoint;

    public UdpSocketPacket(byte[] bytes, IPEndPoint endPoint)
    {
        this.bytes = bytes;
        this.endPoint = endPoint;
    }
};

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

    public UdpSocketPacket Receive()
    {
        var bytes = new byte[PacketHelper.PACKET_SIZE];
        SocketError error = SocketError.Success;
        EndPoint endPoint = new IPEndPoint(IPAddress.Any, 0);
        int packetSize = 0;
        try
        {
            packetSize = socket.ReceiveFrom(bytes, 0, bytes.Length, SocketFlags.None, ref endPoint);
        }
        catch (SocketException e)
        {
            error = e.SocketErrorCode;
        }

        if (error == SocketError.WouldBlock)
        {
            return null;
        }

        if (error != SocketError.Success)
        {
            throw new UdpSocketException($"Failed to receive bytes: {error}");
        }

        if (packetSize != bytes.Length)
        {
            var resizedBytes = new byte[packetSize];
            Array.Copy(bytes, 0, resizedBytes, 0, packetSize);
            bytes = resizedBytes;
        }

        return new UdpSocketPacket(bytes, (IPEndPoint)endPoint);
    }

    public int Send(byte[] buffer, IPEndPoint endPoint)
    {
        return socket.SendTo(buffer, 0, buffer.Length, SocketFlags.None, endPoint);
    }
}
