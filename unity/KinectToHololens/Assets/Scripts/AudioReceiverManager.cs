using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class AudioReceiverManager : MonoBehaviour
{
    private const int KH_SAMPLE_RATE = 48000;
    private const int KH_CHANNEL_COUNT = 2;
    private const double KH_LATENCY_SECONDS = 0.2;
    private const int KH_SAMPLES_PER_FRAME = 960;
    private const int KH_BYTES_PER_SECOND = KH_SAMPLE_RATE * KH_CHANNEL_COUNT * sizeof(float);

    private UdpSocket udpSocket;
    private RingBuffer ringBuffer;
    private AudioDecoder audioDecoder;
    private int lastAudioFrameId;

    void Start()
    {
        IPAddress address = IPAddress.Parse("127.0.0.1");
        int port = 7777;

        Socket socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp)
        {
            ReceiveBufferSize = 1024 * 1024
        };

        ringBuffer = new RingBuffer((int)(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND / sizeof(float)));
        udpSocket = new UdpSocket(socket, new IPEndPoint(address, port));
        audioDecoder = new AudioDecoder(KH_SAMPLE_RATE, KH_CHANNEL_COUNT);
        lastAudioFrameId = -1;

        udpSocket.Send(PacketHelper.createPingReceiverPacketBytes());
    }

    private void Update()
    {
        var audioPacketDataSet = new List<AudioSenderPacketData>();
        SocketError error = SocketError.WouldBlock;
        while (true)
        {
            var packet = udpSocket.Receive(out error);
            if (packet == null)
                break;

            audioPacketDataSet.Add(AudioSenderPacketData.Parse(packet));
        }

        audioPacketDataSet.Sort((x, y) => x.frameId.CompareTo(y.frameId));

        float[] pcm = new float[KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT];
        int index = 0;
        while (ringBuffer.FreeSamples >= pcm.Length)
        {
            if (index >= audioPacketDataSet.Count)
                break;

            var audioPacketData = audioPacketDataSet[index++];
            if (audioPacketData.frameId <= lastAudioFrameId)
                continue;

            audioDecoder.Decode(audioPacketData.opusFrame, pcm, KH_SAMPLES_PER_FRAME);
            ringBuffer.Write(pcm);
            lastAudioFrameId = audioPacketData.frameId;
        }
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        const float AMPLIFIER = 4;

        ringBuffer.Read(data);
        for (int i = 0; i < data.Length; ++i)
            data[i] = data[i] * AMPLIFIER;
    }
}
