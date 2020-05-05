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
        IPEndPoint ipEndPoint = new IPEndPoint(IPAddress.Any, 0);
        EndPoint endPoint = ipEndPoint;
        SocketError error = SocketError.Success;
        int packetSize = 0;
        try
        {
            packetSize = socket.ReceiveFrom(bytes, ref endPoint);
        }
        catch (SocketException e)
        {
            error = e.SocketErrorCode;
        }

        // ReceiveFrom throws SocketError.InvalidArgument before the socket got connected to any remote ones.
        if (error == SocketError.WouldBlock || error == SocketError.InvalidArgument)
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
            return new UdpSocketPacket(resizedBytes, ipEndPoint);
        }
        else
        {
            return new UdpSocketPacket(bytes, ipEndPoint);
        }
    }

    public int Send(byte[] buffer, IPEndPoint endPoint)
    {
        return socket.SendTo(buffer, 0, buffer.Length, SocketFlags.None, endPoint);
    }
}
