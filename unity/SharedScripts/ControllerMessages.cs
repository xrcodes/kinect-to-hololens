using System;
using System.Collections.Generic;

public class ControllerMessages
{
    public const int PORT = 3798;
}

[Serializable]
public class ReceiverState
{
    public string senderAddress;
    public int senderPort;
    public int sessionId;

    public ReceiverState(string senderAddress, int senderPort, int sessionId)
    {
        this.senderAddress = senderAddress;
        this.senderPort = senderPort;
        this.sessionId = sessionId;
    }
}

public class ViewerState
{
    public int userId;
    public List<ReceiverState> receiverStates;

    public ViewerState(int userId, List<ReceiverState> receiverStates)
    {
        this.userId = userId;
        this.receiverStates = receiverStates;
    }
}

[Serializable]
public class KinectSenderElement
{
    public string address;
    public int port;

    public KinectSenderElement(string address, int port)
    {
        this.address = address;
        this.port = port;
    }
}

public class ViewerScene
{
    public List<KinectSenderElement> kinectSenderElements;

    public ViewerScene(List<KinectSenderElement> kinectSenderElements)
    {
        this.kinectSenderElements = kinectSenderElements;
    }
}
