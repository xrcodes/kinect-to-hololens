using System;
using System.Net;
using System.Net.Sockets;

public class UdpSocket
{
    private Socket socket;
    private IPEndPoint remoteEndPoint;

    public UdpSocket(Socket socket, IPEndPoint remoteEndPoint)
    {
        this.socket = socket;
        this.remoteEndPoint = remoteEndPoint;
        socket.Blocking = false;
    }

    public byte[] Receive(out SocketError error)
    {
        var bytes = new byte[PacketHelper.PACKET_SIZE];
        var packetSize = socket.Receive(bytes, 0, bytes.Length, SocketFlags.None, out error);

        if (error == SocketError.WouldBlock)
        {
            return null;
        }

        if (error != SocketError.WouldBlock)
        {
            UnityEngine.Debug.Log($"Failed to receive bytes: {error}");
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

    public int Send(byte[] buffer)
    {
        return socket.SendTo(buffer, 0, buffer.Length, SocketFlags.None, remoteEndPoint);
    }
}
