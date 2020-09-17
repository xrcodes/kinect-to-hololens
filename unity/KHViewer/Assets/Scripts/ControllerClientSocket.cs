using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Text;
using UnityEngine;

public class ControllerClientSocket
{
    public readonly int ViewerId;
    private TcpSocket tcpSocket;
    private MessageBuffer messageBuffer;

    public IPEndPoint RemoteEndPoint
    {
        get
        {
            return tcpSocket.RemoteEndPoint;
        }
    }

    public ControllerClientSocket(int viewerId, TcpSocket tcpSocket)
    {
        ViewerId = viewerId;
        this.tcpSocket = tcpSocket;
        messageBuffer = new MessageBuffer();
    }

    public ControllerScene ReceiveControllerScene()
    {
        byte[] message;
        if (!messageBuffer.TryReceiveMessage(tcpSocket, out message))
            return null;

        var controllerSceneJson = Encoding.ASCII.GetString(message);
        return JsonUtility.FromJson<ControllerScene>(controllerSceneJson);
    }

    public void SendViewerState(List<ReceiverState> receiverStates)
    {
        var viewerState = new ViewerState(ViewerId, receiverStates);
        var viewerStateJson = JsonUtility.ToJson(viewerState);
        var viewerStateBytes = Encoding.ASCII.GetBytes(viewerStateJson);

        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(viewerStateBytes.Length), 0, 4);
        ms.Write(viewerStateBytes, 0, viewerStateBytes.Length);
        tcpSocket.Send(ms.ToArray());
    }
}
