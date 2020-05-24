using System;
using System.Net;
using System.Net.Sockets;

public class UdpSocketPacket
{
    public byte[] Bytes { get; private set; }
    public IPEndPoint EndPoint { get; private set; }

    public UdpSocketPacket(byte[] bytes, IPEndPoint endPoint)
    {
        Bytes = bytes;
        EndPoint = endPoint;
    }
};

public class UdpSocketException : Exception
{
    public SocketError Error { get; private set; }
    public IPEndPoint EndPoint { get; private set; }

    public UdpSocketException(string message, SocketError error, IPEndPoint endPoint) : base(message)
    {
        Error = error;
        EndPoint = endPoint;
    }
}

// Socket.ReceiveFrom(), after IL2CPP translation, prints exceptions to the console even when the exception is caught...
public class UdpSocket
{
    private Socket socket;

    public UdpSocket(Socket socket)
    {
        this.socket = socket;
        socket.Blocking = false;
    }

    // Receive and ReceiveFrom has been split to figure out where the endpoint of SocketExceptions is.
    // Unfortunately, when there is an error from the native side, .Net throws an exception without
    // updating the endpoint parameter...
    public UdpSocketPacket Receive()
    {
        EndPoint endPoint = new IPEndPoint(IPAddress.Any, 0);
        return ReceiveFrom(endPoint);
    }

    public UdpSocketPacket ReceiveFrom(EndPoint endPoint)
    {
        var bytes = new byte[PacketHelper.PACKET_SIZE];
        SocketError error = SocketError.Success;
        int packetSize = 0;
        try
        {
            packetSize = socket.ReceiveFrom(bytes, bytes.Length, SocketFlags.None, ref endPoint);
        }
        catch (SocketException e)
        {
            // When ReceiveFrom throws a SocketException, endPoint is not set.
            error = e.SocketErrorCode;
        }

        if (error == SocketError.WouldBlock)
        {
            return null;
        }

        if (error != SocketError.Success)
        {
            throw new UdpSocketException($"Failed to receive bytes from {endPoint}: {error}", error, (IPEndPoint)endPoint);
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
