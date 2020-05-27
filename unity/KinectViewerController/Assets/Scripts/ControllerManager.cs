using System.Net.Sockets;
using UnityEngine;
using ImGuiNET;
using System.Collections.Generic;
using System.Linq;

public class ControllerManager : MonoBehaviour
{
    private const int SENDER_PORT = 3773;
    private TcpSocket tcpSocket;
    private List<ControllerServerSocket> serverSockets;
    private Dictionary<ControllerServerSocket, ViewerState> viewerStates;
    private Dictionary<ControllerServerSocket, ViewerScene> viewerScenes;

    private string singleCameraAddress;
    private int singleCameraPort;
    private float singleCameraDistance;

    void Start()
    {
        ImGui.GetIO().SetIniFilename(null);

        tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        serverSockets = new List<ControllerServerSocket>();
        viewerStates = new Dictionary<ControllerServerSocket, ViewerState>();
        viewerScenes = new Dictionary<ControllerServerSocket, ViewerScene>();

        singleCameraAddress = "127.0.0.1";
        singleCameraPort = SENDER_PORT;
        singleCameraDistance = 0.0f;

        tcpSocket.BindAndListen(ControllerMessages.PORT);
    }
    
    void Update()
    {
        try
        {
            var remoteSocket = tcpSocket.Accept();
            if (remoteSocket != null)
            {
                print($"remoteSocket: {remoteSocket.RemoteEndPoint}");
                var serverSocket = new ControllerServerSocket(remoteSocket);
                serverSockets.Add(serverSocket);
                viewerScenes.Add(serverSocket, new ViewerScene());
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
        ImGui.SetNextWindowPos(new Vector2(0.0f, 0.0f), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(Screen.width * 0.4f, Screen.height * 0.4f), ImGuiCond.FirstUseEver);
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

        ImGui.SetNextWindowPos(new Vector2(0.0f, Screen.height * 0.4f), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(Screen.width * 0.4f, Screen.height * 0.6f), ImGuiCond.FirstUseEver);
        ImGui.Begin("Scene Templates");
        ImGui.BeginTabBar("Scene Templates TabBar");
        if (ImGui.BeginTabItem("Default"))
        {
            foreach (var viewerStatePair in viewerStates)
            {
            }
            if(ImGui.Button("Set Scene"))
            {
            }
            ImGui.EndTabItem();
        }
        if (ImGui.BeginTabItem("Single Camera"))
        {
            ImGui.InputText("Address", ref singleCameraAddress, 30);
            ImGui.InputInt("Port", ref singleCameraPort);
            ImGui.InputFloat("Distance", ref singleCameraDistance);

            if(ImGui.Button("Set Scene"))
            {
                foreach(var serverSocket in serverSockets)
                {
                    var kinectSenderElement = new KinectSenderElement(singleCameraAddress,
                                                                      singleCameraPort,
                                                                      new Vector3(0.0f, 0.0f, singleCameraDistance),
                                                                      Quaternion.identity);

                    var viewerScene = new ViewerScene();
                    viewerScene.kinectSenderElements = new List<KinectSenderElement>();
                    viewerScene.kinectSenderElements.Add(kinectSenderElement);
                    viewerScenes[serverSocket] = viewerScene;
                }
            }
        }
        ImGui.EndTabBar();
        ImGui.End();

        ImGui.SetNextWindowPos(new Vector2(Screen.width * 0.4f, 0.0f), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(Screen.width * 0.6f, Screen.height), ImGuiCond.FirstUseEver);
        ImGui.Begin("Scene");
        ImGui.BeginTabBar("Scene TabBar");
        foreach (var viewerStatePair in viewerStates)
        {
            var serverSocket = viewerStatePair.Key;
            var viewerState = viewerStatePair.Value;
            var viewerScene = viewerScenes[serverSocket];
            if (ImGui.BeginTabItem(viewerState.userId.ToString()))
            {
                foreach (var kinectSenderElement in viewerScene.kinectSenderElements.ToList())
                {
                    ImGui.InputText("Address", ref kinectSenderElement.address, 30);
                    ImGui.InputInt("Port", ref kinectSenderElement.port);
                    ImGui.InputFloat3("Position", ref kinectSenderElement.position);
                    var eulerAngles = kinectSenderElement.rotation.eulerAngles;
                    if(ImGui.InputFloat3("Rotation", ref eulerAngles))
                    {
                        kinectSenderElement.rotation = Quaternion.Euler(eulerAngles);
                    }
                    
                    if(ImGui.Button("Remove This Sender"))
                    {
                        viewerScene.kinectSenderElements.Remove(kinectSenderElement);
                    }

                    ImGui.NewLine();
                }

                if(ImGui.Button("Add Kinect Sender"))
                {
                    viewerScene.kinectSenderElements.Add(new KinectSenderElement("127.0.0.1", SENDER_PORT, Vector3.zero, Quaternion.identity));
                }

                if(ImGui.Button("Update Scene"))
                {
                    serverSocket.SendViewerScene(viewerScene);
                }
                ImGui.EndTabItem();
            }
        }
        ImGui.EndTabBar();
        ImGui.End();
    }
}
