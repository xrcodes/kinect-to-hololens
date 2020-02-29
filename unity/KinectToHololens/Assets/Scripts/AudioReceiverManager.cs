using System;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using UnityEngine;

public class AudioReceiverManager : MonoBehaviour
{
    private const int KINECT_MICROPHONE_SAMPLE_RATE = 48000;
    private const int STEREO_CHANNEL_COUNT = 2;
    private const int AUDIO_FRAME_SIZE = 960;
    private UdpSocket udpSocket;
    private RingBuffer ringBuffer;
    private AudioDecoder audioDecoder;

    void Start()
    {
        IPAddress address = IPAddress.Parse("127.0.0.1");
        int port = 7777;

        Socket socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp)
        {
            ReceiveBufferSize = 1024 * 1024
        };

        udpSocket = new UdpSocket(socket, new IPEndPoint(address, port));
        ringBuffer = new RingBuffer(64 * 1024);
        audioDecoder = new AudioDecoder(KINECT_MICROPHONE_SAMPLE_RATE, STEREO_CHANNEL_COUNT);

        udpSocket.Send(PacketHelper.createPingReceiverPacketBytes());
    }

    private void Update()
    {
        SocketError error = SocketError.WouldBlock;
        while (true)
        {
            var packet = udpSocket.Receive(out error);
            if (packet == null)
                break;

            int cursor = 5;
            int frameId = BitConverter.ToInt32(packet, cursor);
            cursor += 4;

            var opusFrame = new ArraySegment<byte>(packet, cursor, packet.Length - cursor);
            IntPtr nativePcm = Marshal.AllocHGlobal(sizeof(float) * AUDIO_FRAME_SIZE * STEREO_CHANNEL_COUNT);
            int pcmFrameSize = audioDecoder.Decode(opusFrame, ref nativePcm, AUDIO_FRAME_SIZE);
            float[] pcm = new float[AUDIO_FRAME_SIZE * STEREO_CHANNEL_COUNT];
            Marshal.Copy(nativePcm, pcm, 0, pcm.Length);
            Marshal.FreeHGlobal(nativePcm);
            ringBuffer.Write(pcm);

            print($"frameId: {frameId}, pcmFrameSize: {pcmFrameSize}");
        }
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        ringBuffer.Read(data);
    }
}
