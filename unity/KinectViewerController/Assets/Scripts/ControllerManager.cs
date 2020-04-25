using System.Net.Sockets;
using System.Text;
using UnityEngine;
using UnityEngine.UI;

public class ControllerManager : MonoBehaviour
{
    public Text viewerStateText;
    public InputField kinectAddressInputField;
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

                //print($"this.remoteSocket.Socket.RemoteEndPoint: {this.remoteSocket.Socket.RemoteEndPoint}");

                viewerStateText.text = $"end point: {this.remoteSocket.Socket.RemoteEndPoint}\nuser ID: {viewerState.userId}";
            }
        }
    }

    void OnApplicationQuit()
    {
        tcpSocket = null;
    }

    public void OnConnect()
    {
        print($"kinectAddressInputField: {kinectAddressInputField.text}");
    }
}
