using UnityEngine;

public class AudioDecoderTest : MonoBehaviour
{
    private const int KH_SAMPLE_RATE = 48000;
    private const int KH_CHANNEL_COUNT = 2;

    void Start()
    {
        print("before audio decoder ctor");
        AudioDecoder audioDecoder = new AudioDecoder(KH_SAMPLE_RATE, KH_CHANNEL_COUNT);
        print("after audio decoder ctor");
    }

    void Update()
    {
        print("update");
    }
}
