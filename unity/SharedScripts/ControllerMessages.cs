using System;
using System.Collections.Generic;
using UnityEngine;

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
public class ControllerNode
{
    public string address;
    public int port;
    public Vector3 position;
    public Quaternion rotation;

    public ControllerNode(string address, int port, Vector3 position, Quaternion rotation)
    {
        this.address = address;
        this.port = port;
        this.position = position;
        this.rotation = rotation;
    }
}

public class ControllerScene
{
    public List<ControllerNode> nodes;

    public ControllerScene()
    {
        nodes = new List<ControllerNode>();
    }
}
