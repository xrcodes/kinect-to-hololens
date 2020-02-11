using System;
using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class AudioReceiverManager : MonoBehaviour
{
    private Receiver receiver;
    private RingBuffer ringBuffer;

    void Start()
    {
        receiver = new Receiver(1024 * 1024);
        ringBuffer = new RingBuffer(64 * 1024);

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

            ringBuffer.Write(packet);
        }
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        int readSize = data.Length * 4;
        byte[] readBuffer = new byte[readSize];
        ringBuffer.Read(readBuffer);

        for(int i = 0; i < data.Length; ++i)
        {
            data[i] = BitConverter.ToSingle(readBuffer, i * 4);
        }
    }
}
