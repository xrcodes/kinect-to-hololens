using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEngine;

public class ControllerClient
{
    public readonly int UserId;
    private TcpSocket tcpSocket;

    public ControllerClient(int userId, TcpSocket tcpSocket)
    {
        UserId = userId;
        this.tcpSocket = tcpSocket;
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
