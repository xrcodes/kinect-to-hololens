using System.Net.Sockets;
using UnityEngine;

public class ControllerManager : MonoBehaviour
{
    private TcpSocket tcpSocket;

    void Start()
    {
        tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        tcpSocket.BindAndListen(ControllerMessages.PORT);
    }

    void Update()
    {
        try
        {
            var remoteSocket = tcpSocket.Accept();
            if(remoteSocket != null)
                print($"remoteSocket: {remoteSocket.Socket.RemoteEndPoint}");
        }
        catch(TcpSocketException e)
        {
            print(e.Message);
        }
    }

    void OnApplicationQuit()
    {
        tcpSocket = null;
    }
}
