using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class ControllerManager : MonoBehaviour
{
    private Socket socket;

    void Start()
    {
        socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        socket.Blocking = false;
        socket.Bind(new IPEndPoint(IPAddress.Loopback, 1234));
        socket.Listen(10);
    }

    void Update()
    {
        try
        {
            var remoteSocket = socket.Accept();
            print($"remoteSocket: {remoteSocket.RemoteEndPoint}");
        }
        catch(SocketException e)
        {
            print($"e: {e.ErrorCode}: {e.Message}");
        }
    }

    void OnApplicationQuit()
    {
        socket = null;
    }
}
