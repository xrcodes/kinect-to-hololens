using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Text;
using UnityEngine;

public class ControllerClientSocket
{
    public readonly int UserId;
    private TcpSocket tcpSocket;
    private MessageBuffer messageBuffer;

    public IPEndPoint RemoteEndPoint
    {
        get
        {
            return tcpSocket.RemoteEndPoint;
        }
    }

    public ControllerClientSocket(int userId, TcpSocket tcpSocket)
    {
        UserId = userId;
        this.tcpSocket = tcpSocket;
        messageBuffer = new MessageBuffer();
    }

    public ControllerScene ReceiveViewerScene()
    {
        byte[] message;
        if (!messageBuffer.TryReceiveMessage(tcpSocket, out message))
            return null;

        var viewerSceneJson = Encoding.ASCII.GetString(message);
        var viewerScene = JsonUtility.FromJson<ControllerScene>(viewerSceneJson);

        return viewerScene;
    }

    public void SendViewerState(List<ReceiverState> receiverStates)
    {
        var viewerState = new ViewerState(UserId, receiverStates);
        var viewerStateJson = JsonUtility.ToJson(viewerState);
        var viewerStateBytes = Encoding.ASCII.GetBytes(viewerStateJson);

        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(viewerStateBytes.Length), 0, 4);
        ms.Write(viewerStateBytes, 0, viewerStateBytes.Length);
        tcpSocket.Send(ms.ToArray());
    }
}
