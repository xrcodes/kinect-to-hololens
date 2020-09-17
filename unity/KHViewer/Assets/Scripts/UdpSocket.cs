using System;
using System.Net;
using System.Net.Sockets;
using TelepresenceToolkit;

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

    public UdpSocketException(string message, SocketError error) : base(message)
    {
        Error = error;
    }
}

public class UdpSocket
{
    private Socket socket;

    public UdpSocket(int receiveBufferSize)
    {
        socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = receiveBufferSize, Blocking = false };
        socket.Bind(new IPEndPoint(IPAddress.Any, 0));
    }

    public UdpSocketPacket Receive()
    {
        byte[] bytes = new byte[PacketUtils.PACKET_SIZE];
        SocketError error = SocketError.Success;
        EndPoint endPoint = new IPEndPoint(IPAddress.Any, 0);
        int packetSize = 0;
        try
        {
            /*
            Note 1.
                The current .NET implementation of Socket.ReceiveFrom() guarantees endPoint to end up as an IPEndPoint when it handed in as an IPEndPoint.
                Therefore, there is no need to test whether endPoint is IPEndPoint when casting endPoint to IPEndPoint.
            
            Note 2.
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
            throw new UdpSocketException($"Failed to receive bytes: {error}", error);
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
