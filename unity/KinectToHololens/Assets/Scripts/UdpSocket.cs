using System;
using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class UdpSocket
{
    public bool Disposed { get; private set; }
    private Socket socket;

    public int Available
    {
        get
        {
            return socket.Available;
        }
    }

    public UdpSocket(int receiveBufferSize)
    {
        Disposed = false;
        socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp)
        {
            Blocking = false,
            ReceiveBufferSize = receiveBufferSize
        };
    }

    public void Dispose()
    {
        if (!Disposed)
        {
            Disposed = true;
            try
            {
                socket.Shutdown(SocketShutdown.Both);
            }
            catch (SocketException e)
            {
                if (e.SocketErrorCode == SocketError.NotConnected)
                {
                    // When the socket is not connected, calling socket.Dispose results in a C++ exception in IL2CPP
                    // which can't be caught in a try-catch statement in C#.
                    // In brief, calling socket.Dispose() should be avoided.
                    Debug.Log("Failed to shutdown a UdpSocket since it was not connected...");
                    return;
                }
                else
                {
                    Debug.Log("SocketException from UdpSocket.Dispose calling Shutdown: " + e.SocketErrorCode);
                    throw e;
                }
            }
            socket.Dispose();
        }
    }

    public Tuple<int, SocketError> Receive(byte[] buffer)
    {
        return Receive(buffer, 0, buffer.Length);
    }

    public Tuple<int, SocketError> Receive(byte[] buffer, int offset, int length)
    {
        int size = socket.Receive(buffer, offset, length, SocketFlags.None, out SocketError socketError);
        return new Tuple<int, SocketError>(size, socketError);
    }

    public int SendTo(byte[] buffer, int offset, int size, IPAddress address, int port)
    {
        EndPoint remoteEP = new IPEndPoint(address, port);
        return socket.SendTo(buffer, offset, size, SocketFlags.None, remoteEP);
    }

    public int SendTo(byte[] buffer, IPAddress address, int port)
    {
        return SendTo(buffer, 0, buffer.Length, address, port);
    }
}