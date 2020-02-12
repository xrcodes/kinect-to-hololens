using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class AudioReceiverManager : MonoBehaviour
{
    private const int AZURE_KINECT_SAMPLE_RATE = 48000;
    private const int AUDIO_FRAME_SIZE = 960;
    private const int STEREO_CHANNEL_COUNT = 2;
    private Receiver receiver;
    private RingBuffer ringBuffer;
    private OpusDecoder opusDecoder;

    void Start()
    {
        receiver = new Receiver(1024 * 1024);
        ringBuffer = new RingBuffer(64 * 1024);
        opusDecoder = new OpusDecoder(AZURE_KINECT_SAMPLE_RATE, STEREO_CHANNEL_COUNT);

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

            OpusFrame opusFrame = opusDecoder.Decode(packet, AUDIO_FRAME_SIZE, STEREO_CHANNEL_COUNT);
            ringBuffer.Write(opusFrame.GetArray());
        }
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        ringBuffer.Read(data);
    }
}
