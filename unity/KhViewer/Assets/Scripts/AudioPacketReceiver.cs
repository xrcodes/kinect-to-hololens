using System.Collections.Generic;

public class AudioPacketReceiver
{
    private AudioDecoder audioDecoder;
    private int lastAudioFrameId;

    public AudioPacketReceiver()
    {
        audioDecoder = new AudioDecoder(KinectSpeaker.KH_SAMPLE_RATE, KinectSpeaker.KH_CHANNEL_COUNT);
        lastAudioFrameId = -1;
    }

    public void Receive(List<AudioSenderPacketData> audioPacketDataList, RingBuffer ringBuffer)
    {
        audioPacketDataList.Sort((x, y) => x.frameId.CompareTo(y.frameId));

        float[] pcm = new float[KinectSpeaker.KH_SAMPLES_PER_FRAME * KinectSpeaker.KH_CHANNEL_COUNT];
        int index = 0;
        while (ringBuffer.FreeSamples >= pcm.Length)
        {
            if (index >= audioPacketDataList.Count)
                break;

            var audioPacketData = audioPacketDataList[index++];
            if (audioPacketData.frameId <= lastAudioFrameId)
                continue;

            audioDecoder.Decode(audioPacketData.opusFrame, pcm, KinectSpeaker.KH_SAMPLES_PER_FRAME);
            ringBuffer.Write(pcm);
            lastAudioFrameId = audioPacketData.frameId;
        }
    }
}