using System;
using System.IO;
using System.Text;
using UnityEngine;

public class ControllerServerSocket
{
    private TcpSocket tcpSocket;
    private MessageBuffer messageBuffer;

    public ControllerServerSocket(TcpSocket tcpSocket)
    {
        this.tcpSocket = tcpSocket;
        messageBuffer = new MessageBuffer();
    }

    public ViewerState ReceiveViewerState()
    {
        byte[] message;
        if (!messageBuffer.TryReceiveMessage(tcpSocket, out message))
            return null;

        var viewerStateJson = Encoding.ASCII.GetString(message);
        var viewerState = JsonUtility.FromJson<ViewerState>(viewerStateJson);

        return viewerState;
    }

    public void SendViewerState(ViewerScene viewerScene)
    {
        var viewerSceneJson = JsonUtility.ToJson(viewerScene);
        var viewerSceneBytes = Encoding.ASCII.GetBytes(viewerSceneJson);

        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(viewerSceneBytes.Length), 0, 4);
        ms.Write(viewerSceneBytes, 0, viewerSceneBytes.Length);
        tcpSocket.Send(ms.ToArray());
    }
}
