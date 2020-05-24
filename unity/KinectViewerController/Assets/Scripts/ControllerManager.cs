using System.Net.Sockets;
using System.Text;
using UnityEngine;
using ImGuiNET;
using System.Collections.Generic;
using System.Linq;
using System;
using UnityEngine.Apple.TV;

public class ControllerManager : MonoBehaviour
{
    private TcpSocket tcpSocket;
    private MessageBuffer messageBuffer;
    private List<TcpSocket> remoteSockets;
    private Dictionary<TcpSocket, ViewerState> viewerStates;
    private Dictionary<TcpSocket, KinectSenderElement> kinectSenderElements;

    void Start()
    {
        tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        messageBuffer = new MessageBuffer();
        remoteSockets = new List<TcpSocket>();
        viewerStates = new Dictionary<TcpSocket, ViewerState>();
        kinectSenderElements = new Dictionary<TcpSocket, KinectSenderElement>();

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
                remoteSockets.Add(remoteSocket);
                kinectSenderElements.Add(remoteSocket, new KinectSenderElement("127.0.0.1", 3773));
            }
        }
        catch(TcpSocketException e)
        {
            print(e.Message);
        }

        foreach (var remoteSocket in remoteSockets)
        {
            byte[] message;
            while (messageBuffer.TryReceiveMessage(remoteSocket, out message))
            {
                var viewerStateJson = Encoding.ASCII.GetString(message);
                var viewerState = JsonUtility.FromJson<ViewerState>(viewerStateJson);

                viewerStates[remoteSocket] = viewerState;
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
        foreach (var remoteSocket in remoteSockets)
        {
            ViewerState viewerState;
            if (viewerStates.TryGetValue(remoteSocket, out viewerState))
            {
                ImGui.Text($"Kinect Sender of Viewer User ID {viewerState.userId}");
                ImGui.InputText("Address", ref kinectSenderElements[remoteSocket].address, 30);
                ImGui.InputInt("Port", ref kinectSenderElements[remoteSocket].port);
            }
        }
        if(ImGui.Button("Connect"))
        {
            var viewerScene = new ViewerScene(kinectSenderElements.Values.ToList());
            foreach (var remoteSocket in remoteSockets)
            {
                var viewerSceneJson = JsonUtility.ToJson(viewerScene);
                var viewerSceneBytes = Encoding.ASCII.GetBytes(viewerSceneJson);

                var ms = new System.IO.MemoryStream();
                ms.Write(BitConverter.GetBytes(viewerSceneBytes.Length), 0, 4);
                ms.Write(viewerSceneBytes, 0, viewerSceneBytes.Length);
                remoteSocket.Send(ms.ToArray());
            }
        }
        ImGui.End();
    }
}
