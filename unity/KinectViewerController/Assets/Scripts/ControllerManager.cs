using System.IO;
using System.Net.Sockets;
using System.Text;
using UnityEngine;

public class ControllerManager : MonoBehaviour
{
    private TcpSocket tcpSocket;
    private MessageBuffer messageBuffer;
    private TcpSocket remoteSocket;

    void Start()
    {
        tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        messageBuffer = new MessageBuffer();
        tcpSocket.BindAndListen(ControllerMessages.PORT);
    }

    void Update()
    {
        try
        {
            var remoteSocket = tcpSocket.Accept();
            if (remoteSocket != null)
            {
                print($"remoteSocket: {remoteSocket.Socket.RemoteEndPoint}");
                this.remoteSocket = remoteSocket;
            }
        }
        catch(TcpSocketException e)
        {
            print(e.Message);
        }

        if (this.remoteSocket != null)
        {
            byte[] message;
            while (messageBuffer.TryReceiveMessage(this.remoteSocket, out message))
            {
                var viewerStateJson = Encoding.ASCII.GetString(message);
                var viewerState = JsonUtility.FromJson<ViewerState>(viewerStateJson);

                print($"viewerStateJson: {viewerStateJson}");
            }
        }
    }

    void OnApplicationQuit()
    {
        tcpSocket = null;
    }
}
