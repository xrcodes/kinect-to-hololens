using System;
using UnityEngine;

public class RingBuffer
{
    private float[] buffer;
    private int readCursor;
    private int writeCursor;

    public int FillSamples
    {
        get
        {
            int fillBytes = writeCursor - readCursor;
            if(fillBytes <= 0)
            {
                fillBytes += buffer.Length;
            }
            return fillBytes;
        }
    }

    public int FreeSamples
    {
        get
        {
            int freeBytes = readCursor - writeCursor;
            if(freeBytes < 0)
            {
                freeBytes += buffer.Length;
            }
            return freeBytes;
        }
    }

    public RingBuffer(int capacity)
    {
        buffer = new float[capacity];
        readCursor = 0;
        writeCursor = 0;
    }

    public void Read(float[] samples)
    {
        if (samples.Length > buffer.Length)
        {
            Debug.Log("Cannot write more samples than the capacity to a RingBuffer.");
            return;
        }

        if (samples.Length > FillSamples)
        {
            Debug.Log("Ringbuffer underflow...");
            return;
        }

        if (readCursor + samples.Length <= buffer.Length)
        {
            Array.Copy(buffer, readCursor, samples, 0, samples.Length);
            readCursor += samples.Length;
            return;
        }

        int leftSamples = samples.Length;
        int readLength = buffer.Length - readCursor;
        Array.Copy(buffer, readCursor, samples, 0, readLength);
        leftSamples -= readLength;
        Array.Copy(buffer, 0, samples, readLength, leftSamples);
        readCursor = leftSamples;
    }

    public void Write(float[] samples)
    {
        if(samples.Length > buffer.Length)
        {
            Debug.Log("Cannot write more samples than the capacity to a RingBuffer.");
            return;
        }

        if (samples.Length > FreeSamples)
        {
            Debug.Log("Ringbuffer overflow...");
            return;
        }

        if (writeCursor + samples.Length <= buffer.Length)
        {
            Array.Copy(samples, 0, buffer, writeCursor, samples.Length);
            writeCursor += samples.Length;
            return;
        }

        int leftSamples = samples.Length;
        int writeLength = buffer.Length - writeCursor;
        Array.Copy(samples, 0, buffer, writeCursor, writeLength);
        leftSamples -= writeLength;
        Array.Copy(samples, writeLength, buffer, 0, leftSamples);
        writeCursor = leftSamples;
    }
}
