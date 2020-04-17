using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class TcpTest : MonoBehaviour
{
    private TcpSocket tcpSocket;

    // Start is called before the first frame update
    async void Start()
    {
        tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        if(await tcpSocket.ConnectAsync(IPAddress.Loopback, 1234))
        {
            print("connected");
        }
        else
        {
            print("not connected");
        }
    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
