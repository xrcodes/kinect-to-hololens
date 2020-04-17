using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class TcpTest : MonoBehaviour
{
    private Socket socket;

    // Start is called before the first frame update
    async void Start()
    {
        socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        socket.Blocking = false;
        try
        {
            await socket.ConnectAsync(IPAddress.Loopback, 1234);
            print("connected");
        }
        catch (SocketException e)
        {
            print($"e: {e.Message}");
        }
    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
