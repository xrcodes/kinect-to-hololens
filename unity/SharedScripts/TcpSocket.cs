using System;
using System.Net;
using System.Net.Sockets;

public class TcpSocketException : Exception
{
    public TcpSocketException(string message) : base(message)
    {
    }
}

public class TcpSocket
{
    private Socket socket;

    public TcpSocket(Socket socket)
    {
        socket.Blocking = false;
    }

    public void Listen(int port)
    {
        socket.Bind(new IPEndPoint(IPAddress.Any, port));
        socket.Listen(10);
    }

    public Tuple<int, SocketError> Receive(byte[] buffer)
    {
        return Receive(buffer, 0, buffer.Length);
    }

    public Tuple<int, SocketError> Receive(byte[] buffer, int offset, int length)
    {
        SocketError socketError;
        int size = socket.Receive(buffer, offset, length, SocketFlags.None, out socketError);
        return new Tuple<int, SocketError>(size, socketError);
    }

    public Tuple<int, SocketError> Send(byte[] buffer)
    {
        SocketError socketError;
        int size = socket.Send(buffer, 0, buffer.Length, SocketFlags.None, out socketError);
        return new Tuple<int, SocketError>(size, socketError);
    }
}