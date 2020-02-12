using System;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using UnityEngine;

public class AudioReceiverManager : MonoBehaviour
{
    private const int AZURE_KINECT_SAMPLE_RATE = 48000;
    private const int AUDIO_FRAME_SIZE = 960;
    private const int STEREO_CHANNEL_COUNT = 2;
    private Receiver receiver;
    private RingBuffer ringBuffer;
    private IntPtr opusDecoder;

    void Start()
    {
        receiver = new Receiver(1024 * 1024);
        ringBuffer = new RingBuffer(64 * 1024);
        opusDecoder = Plugin.create_opus_decoder(AZURE_KINECT_SAMPLE_RATE, STEREO_CHANNEL_COUNT);

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

            IntPtr opusPacketBytes = Marshal.AllocHGlobal(packet.Length);
            Marshal.Copy(packet, 0, opusPacketBytes, packet.Length);
            var nativeOpusFrame = Plugin.opus_decoder_decode(opusDecoder, opusPacketBytes, packet.Length, AUDIO_FRAME_SIZE, STEREO_CHANNEL_COUNT);
            Marshal.FreeHGlobal(opusPacketBytes);

            int opusFrameSize = Plugin.opus_frame_get_size(nativeOpusFrame);
            print($"opusFrameSize: {opusFrameSize}");

            float[] opusFrame = new float[opusFrameSize];
            IntPtr opusFrameData = Plugin.opus_frame_get_data(nativeOpusFrame);
            Marshal.Copy(opusFrameData, opusFrame, 0, opusFrameSize);
            Plugin.delete_opus_frame(nativeOpusFrame);

            print($"opusFrame[0]: {opusFrame[0]}");

            ringBuffer.Write(opusFrame);
        }
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        ringBuffer.Read(data);
    }
}
