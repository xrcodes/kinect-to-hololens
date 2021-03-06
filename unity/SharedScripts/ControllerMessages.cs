﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using UnityEngine;

public class ControllerMessages
{
    public const int PORT = 3798;
}

[Serializable]
public class ReceiverState
{
    public int receiverId;
    public int senderId;
    public string senderAddress;
    public int senderPort;

    public ReceiverState(int receiverId, int senderId, IPEndPoint senderEndPoint)
    {
        this.receiverId = receiverId;
        this.senderId = senderId;
        senderAddress = senderEndPoint.Address.ToString();
        senderPort = senderEndPoint.Port;
    }
}

public class ViewerState
{
    public int viewerId;
    public List<ReceiverState> receiverStates;

    public ViewerState(int viewerId, List<ReceiverState> receiverStates)
    {
        this.viewerId = viewerId;
        this.receiverStates = receiverStates;
    }
}

[Serializable]
public class ControllerNode
{
    public string senderAddress;
    public int senderPort;
    public Vector3 position;
    public Quaternion rotation;

    public ControllerNode(string address, int port, Vector3 position, Quaternion rotation)
    {
        senderAddress = address;
        senderPort = port;
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

    public ControllerNode FindNode(IPEndPoint endPoint)
    {
        return nodes.FirstOrDefault(x => x.senderAddress == endPoint.Address.ToString() && x.senderPort == endPoint.Port);
    }

}
