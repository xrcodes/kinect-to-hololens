using System.Collections.Generic;

public class AudioReceiver
{
    private OpusDecoder opusDecoder;
    private int lastAudioFrameId;

    public AudioReceiver()
    {
        opusDecoder = new OpusDecoder(KinectSpeaker.KH_SAMPLE_RATE, KinectSpeaker.KH_CHANNEL_COUNT);
        lastAudioFrameId = -1;
    }

    public void Receive(List<AudioSenderPacket> audioPackets, RingBuffer ringBuffer)
    {
        audioPackets.Sort((x, y) => x.frameId.CompareTo(y.frameId));

        float[] pcm = new float[KinectSpeaker.KH_SAMPLES_PER_FRAME * KinectSpeaker.KH_CHANNEL_COUNT];
        int index = 0;
        while (ringBuffer.FreeSamples >= pcm.Length)
        {
            if (index >= audioPackets.Count)
                break;

            var audioPacketData = audioPackets[index++];
            if (audioPacketData.frameId <= lastAudioFrameId)
                continue;

            opusDecoder.Decode(audioPacketData.opusFrame, pcm, KinectSpeaker.KH_SAMPLES_PER_FRAME);
            ringBuffer.Write(pcm);
            lastAudioFrameId = audioPacketData.frameId;
        }
    }
}