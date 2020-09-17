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

    public void SendViewerScene(ControllerScene controllerScene)
    {
        var controllerSceneJson = JsonUtility.ToJson(controllerScene);
        var controllerSceneBytes = Encoding.ASCII.GetBytes(controllerSceneJson);

        var ms = new MemoryStream();
        ms.Write(BitConverter.GetBytes(controllerSceneBytes.Length), 0, 4);
        ms.Write(controllerSceneBytes, 0, controllerSceneBytes.Length);
        tcpSocket.Send(ms.ToArray());
    }
}
