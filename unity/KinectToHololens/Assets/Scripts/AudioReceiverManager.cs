using System;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using UnityEngine;

public class AudioReceiverManager : MonoBehaviour
{
    private const int AUDIO_FRAME_SIZE = 960;
    private const int STEREO_CHANNEL_COUNT = 2;
    private Receiver receiver;
    private RingBuffer ringBuffer;
    private IntPtr opusDecoder;

    void Start()
    {
        receiver = new Receiver(1024 * 1024);
        ringBuffer = new RingBuffer(64 * 1024);
        opusDecoder = Plugin.create_opus_decoder(AUDIO_FRAME_SIZE, STEREO_CHANNEL_COUNT);

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

            print($"packet size: {packet.Length}");
            //ringBuffer.Write(packet);

            IntPtr opusPacketBytes = Marshal.AllocHGlobal(packet.Length);
            Marshal.Copy(packet, 0, opusPacketBytes, packet.Length);
            var opusFrame = Plugin.opus_decoder_decode(opusDecoder, opusPacketBytes, packet.Length, AUDIO_FRAME_SIZE, STEREO_CHANNEL_COUNT);
            Marshal.FreeHGlobal(opusPacketBytes);

            int opusFrameSize = Plugin.opus_frame_get_size(opusFrame);
            print($"opusFrameSize: {opusFrameSize}");
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
