using UnityEngine;

public class AzureKinectSpeaker : MonoBehaviour
{
    public const int KH_SAMPLE_RATE = 48000;
    public const int KH_CHANNEL_COUNT = 2;
    public const double KH_LATENCY_SECONDS = 0.2;
    public const int KH_SAMPLES_PER_FRAME = 960;
    public const int KH_BYTES_PER_SECOND = KH_SAMPLE_RATE * KH_CHANNEL_COUNT * sizeof(float);

    public RingBuffer RingBuffer { get; set; }

    public void Init()
    {
        RingBuffer = new RingBuffer((int)(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND / sizeof(float)));
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        const float AMPLIFIER = 8.0f;

        RingBuffer.Read(data);
        for (int i = 0; i < data.Length; ++i)
            data[i] = data[i] * AMPLIFIER;
    }
}
