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

    public UdpSocket(int receiveBufferSize)
    {
        socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = receiveBufferSize, Blocking = false };
        socket.Bind(new IPEndPoint(IPAddress.Any, 0));
    }

    // Though it is actually an IPEndPoint, endPoint needs to be an EndPoint as ref parameter should exactly match type,
    // since they can be assigned inside the function--Socket.ReceiveFrom() in our case.
    public UdpSocketPacket ReceiveFrom(EndPoint endPoint)
    {
        var bytes = new byte[PacketUtils.PACKET_SIZE];
        SocketError error = SocketError.Success;
        int packetSize = 0;
        try
        {
            /*
            Note 1.
                There are two cases for the resulting value of endPoint:
                (1) When there is no exception, endPoint becomes the actual IPEndPoint of the socket that sent the packet.
                (2) When there is an exception, endPoint does not change.

                This means, when there is a SocketException while the endPoint is {IPAddress.Any, 0},
                we cannot know the actual IPEndPoint that caused the SocketException.
                This gives a motivation to directly use UdpSocket.ReceiveFrom with the actual IPEndPoint, not {IPAddress.Any, 0}.

            Note 2.
                The current .NET implementation of Socket.ReceiveFrom() guarantees endPoint to end up as an IPEndPoint when it handed in as an IPEndPoint.
                Therefore, there is no need to test whether endPoint is IPEndPoint when casting endPoint to IPEndPoint.
            
            Note 3.
                When C# scripts are transpiled to C++ through IL2CPP, running in debug mode,
                whenever an exception corresponding to a C# exception is thrown, the exception leaves a message on the ouput window of Visual Studio.
                This includes the SocketExceptions thrown from Socket.ReceiveFrom(), even when it is due to a SocketError.WouldBlock.
                I do not know how to get rid of these meaningless messages.
            */
            packetSize = socket.ReceiveFrom(bytes, bytes.Length, SocketFlags.None, ref endPoint);
        }
        catch (SocketException e)
        {
            // When ReceiveFrom throws a SocketException, endPoint is not set.
            error = e.SocketErrorCode;
        }

        if (error == SocketError.WouldBlock)
            return null;

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

    public UdpSocketPacket ReceiveFromAny()
    {
        return ReceiveFrom(new IPEndPoint(IPAddress.Any, 0));
    }

    public int Send(byte[] buffer, IPEndPoint endPoint)
    {
        return socket.SendTo(buffer, 0, buffer.Length, SocketFlags.None, endPoint);
    }
}
