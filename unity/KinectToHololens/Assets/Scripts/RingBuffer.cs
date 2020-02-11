using System;
using UnityEngine;

public class RingBuffer
{
    private byte[] buffer;
    private int readCursor;
    private int writeCursor;

    public int FillBytes
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

    public int FreeBytes
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
        buffer = new byte[capacity];
        readCursor = 0;
        writeCursor = 0;
    }

    public void Read(byte[] bytes)
    {
        if (bytes.Length > buffer.Length)
        {
            Debug.Log("Cannot write more bytes than the capacity to a RingBuffer.");
            return;
        }

        if (bytes.Length > FillBytes)
        {
            Debug.Log("Ringbuffer underflow...");
            return;
        }

        if (readCursor + bytes.Length <= buffer.Length)
        {
            Array.Copy(buffer, readCursor, bytes, 0, bytes.Length);
            readCursor += bytes.Length;
            return;
        }

        int leftBytes = bytes.Length;
        int readLength = buffer.Length - readCursor;
        Array.Copy(buffer, readCursor, bytes, 0, readLength);
        leftBytes -= readLength;
        Array.Copy(buffer, 0, bytes, readLength, leftBytes);
        readCursor = leftBytes;
    }

    public void Write(byte[] bytes)
    {
        if(bytes.Length > buffer.Length)
        {
            Debug.Log("Cannot write more bytes than the capacity to a RingBuffer.");
            return;
        }

        if (bytes.Length > FreeBytes)
        {
            Debug.Log("Ringbuffer overflow...");
            return;
        }

        if (writeCursor + bytes.Length <= buffer.Length)
        {
            Array.Copy(bytes, 0, buffer, writeCursor, bytes.Length);
            writeCursor += bytes.Length;
            return;
        }

        int leftBytes = bytes.Length;
        int writeLength = buffer.Length - writeCursor;
        Array.Copy(bytes, 0, buffer, writeCursor, writeLength);
        leftBytes -= writeLength;
        Array.Copy(bytes, writeLength, buffer, 0, leftBytes);
        writeCursor = leftBytes;
    }
}
