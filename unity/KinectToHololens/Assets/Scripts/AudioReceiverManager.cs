using System;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class AudioReceiverManager : MonoBehaviour
{
    private Receiver receiver;
    private ConcurrentQueue<byte[]> packets;

    void Start()
    {
        receiver = new Receiver(1024 * 1024);
        packets = new ConcurrentQueue<byte[]>();

        IPAddress address = IPAddress.Parse("127.0.0.1");
        int port = 7777;

        receiver.Ping(address, port);
    }

    private void Update()
    {
        SocketError error = SocketError.WouldBlock;
        while (true)
        {
            var packet = receiver.Receive(out error);
            if (packet == null)
                break;

            packets.Enqueue(packet);
        }
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        int cursor = 0;
        int leftBytes = data.Length;
        while(leftBytes > 0 && packets.TryDequeue(out byte[] packet))
        {
            int copySize = Math.Min(leftBytes, packet.Length);
            Array.Copy(packet, 0, data, cursor, copySize);

            cursor += copySize;
            leftBytes -= copySize;
        }
    }
}
