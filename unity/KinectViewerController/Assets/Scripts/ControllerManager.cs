using System.Net.Sockets;
using UnityEngine;
using ImGuiNET;
using System.Collections.Generic;
using System.Linq;

public class ControllerManager : MonoBehaviour
{
    private TcpSocket tcpSocket;
    private List<ControllerServerSocket> serverSockets;
    private Dictionary<ControllerServerSocket, ViewerState> viewerStates;
    private Dictionary<ControllerServerSocket, KinectSenderElement> kinectSenderElements;

    void Start()
    {
        tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        serverSockets = new List<ControllerServerSocket>();
        viewerStates = new Dictionary<ControllerServerSocket, ViewerState>();
        kinectSenderElements = new Dictionary<ControllerServerSocket, KinectSenderElement>();

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
                var serverSocket = new ControllerServerSocket(remoteSocket);
                serverSockets.Add(serverSocket);
                kinectSenderElements.Add(serverSocket, new KinectSenderElement("127.0.0.1", 3773));
            }
        }
        catch(TcpSocketException e)
        {
            print(e.Message);
        }

        foreach (var serverSocket in serverSockets)
        {
            var viewerState = serverSocket.ReceiveViewerState();
            if(viewerState != null)
            {
                viewerStates[serverSocket] = viewerState;
            }
        }
    }

    void OnApplicationQuit()
    {
        tcpSocket = null;
    }

    void OnEnable()
    {
        ImGuiUn.Layout += OnLayout;
    }

    void OnDisable()
    {
        ImGuiUn.Layout -= OnLayout;
    }

    void OnLayout()
    {
        ImGui.Begin("Viewer States");
        foreach (var viewerState in viewerStates.Values)
        {
            ImGui.Text($"Viewer (User ID: {viewerState.userId})");
            foreach (var receiverState in viewerState.receiverStates)
            {
                ImGui.BulletText($"Receiver (Session ID: {receiverState.sessionId})");
                ImGui.Indent();
                ImGui.Text($"  Sender End Point: {receiverState.senderAddress}:{receiverState.senderPort}");
            }
            ImGui.NewLine();
        }
        ImGui.End();

        ImGui.Begin("Scene");
        foreach (var serverSocket in serverSockets)
        {
            ViewerState viewerState;
            if (viewerStates.TryGetValue(serverSocket, out viewerState))
            {
                ImGui.Text($"Kinect Sender of Viewer User ID {viewerState.userId}");
                ImGui.InputText("Address", ref kinectSenderElements[serverSocket].address, 30);
                ImGui.InputInt("Port", ref kinectSenderElements[serverSocket].port);
            }
        }
        if(ImGui.Button("Connect"))
        {
            var viewerScene = new ViewerScene(kinectSenderElements.Values.ToList());
            foreach (var serverSocket in serverSockets)
            {
                serverSocket.SendViewerState(viewerScene);
            }
        }
        ImGui.End();
    }
}
