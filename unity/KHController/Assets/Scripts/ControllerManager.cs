using System.Net.Sockets;
using UnityEngine;
using ImGuiNET;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Diagnostics;

public class ControllerManager : MonoBehaviour
{
    private const int SENDER_PORT = 3773;
    private const float TIME_OUT_SEC = 5.0f;
    private TcpSocket tcpSocket;
    private List<ControllerServerSocket> serverSockets;
    private Dictionary<ControllerServerSocket, ViewerState> viewerStates;
    private Dictionary<ControllerServerSocket, ControllerScene> controllerScenes;
    private Dictionary<ControllerServerSocket, Stopwatch> socketTimers;

    private List<IPAddress> localIpAddresses;
    private string singleCameraAddress;
    private int singleCameraPort;
    private float singleCameraDistance;


    void Start()
    {
        ImGui.GetIO().SetIniFilename(null);

        tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        serverSockets = new List<ControllerServerSocket>();
        viewerStates = new Dictionary<ControllerServerSocket, ViewerState>();
        controllerScenes = new Dictionary<ControllerServerSocket, ControllerScene>();
        socketTimers = new Dictionary<ControllerServerSocket, Stopwatch>();

        localIpAddresses = new List<IPAddress>();
        var host = Dns.GetHostEntry(Dns.GetHostName());
        foreach (var ipAddress in host.AddressList)
        {
            if (ipAddress.AddressFamily == AddressFamily.InterNetwork)
            {
                localIpAddresses.Add(ipAddress);
            }
        }
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
                controllerScenes.Add(serverSocket, new ControllerScene());
                socketTimers.Add(serverSocket, Stopwatch.StartNew());
            }
        }
        catch (TcpSocketException e)
        {
            print(e.Message);
        }

        foreach (var serverSocket in serverSockets)
        {
            while (true)
            {
                var viewerState = serverSocket.ReceiveViewerState();
                if (viewerState == null)
                    break;

                viewerStates[serverSocket] = viewerState;
                socketTimers[serverSocket] = Stopwatch.StartNew();
            }
        }

        var timedOutSockets = new List<ControllerServerSocket>();
        foreach (var socketTimerPair in socketTimers)
        {
            if (socketTimerPair.Value.Elapsed.TotalSeconds > TIME_OUT_SEC)
                timedOutSockets.Add(socketTimerPair.Key);
        }

        foreach (var timedOutSocket in timedOutSockets)
        {
            serverSockets.Remove(timedOutSocket);
            viewerStates.Remove(timedOutSocket);
            controllerScenes.Remove(timedOutSocket);
            socketTimers.Remove(timedOutSocket);
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
        ImGui.SetNextWindowSize(new Vector2(Screen.width * 0.5f, Screen.height * 0.2f), ImGuiCond.FirstUseEver);
        ImGui.Begin("Local IP Addresses");
        foreach (var ipAddress in localIpAddresses)
        {
            ImGui.BulletText(ipAddress.ToString());
        }
        ImGui.End();

        ImGui.SetNextWindowPos(new Vector2(0.0f, Screen.height * 0.2f), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(Screen.width * 0.5f, Screen.height * 0.4f), ImGuiCond.FirstUseEver);
        ImGui.Begin("Viewer States");
        foreach (var viewerState in viewerStates.Values)
        {
            ImGui.Text($"Viewer (User ID: {viewerState.userId})");
            foreach (var receiverState in viewerState.receiverStates)
            {
                ImGui.BulletText($"Receiver (Session ID: {receiverState.sessionId})");
                ImGui.Indent();
                ImGui.Text($"  Sender End Point: {receiverState.senderAddress}:{receiverState.senderPort}");
                ImGui.Unindent();
            }
            ImGui.NewLine();
        }
        ImGui.End();

        ImGui.SetNextWindowPos(new Vector2(0.0f, Screen.height * 0.6f), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(Screen.width * 0.5f, Screen.height * 0.4f), ImGuiCond.FirstUseEver);
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
                    var node = new ControllerNode(singleCameraAddress,
                                                  singleCameraPort,
                                                  new Vector3(0.0f, 0.0f, singleCameraDistance),
                                                  Quaternion.identity);

                    var controllerScene = new ControllerScene();
                    controllerScene.nodes = new List<ControllerNode>();
                    controllerScene.nodes.Add(node);
                    controllerScenes[serverSocket] = controllerScene;
                }
            }
        }
        ImGui.EndTabBar();
        ImGui.End();

        ImGui.SetNextWindowPos(new Vector2(Screen.width * 0.5f, 0.0f), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(Screen.width * 0.5f, Screen.height), ImGuiCond.FirstUseEver);
        ImGui.Begin("Scene");
        ImGui.BeginTabBar("Scene TabBar");

        int kinectSenderElementIndex = 0;
        foreach (var viewerStatePair in viewerStates)
        {
            var serverSocket = viewerStatePair.Key;
            var viewerState = viewerStatePair.Value;
            var controllerScene = controllerScenes[serverSocket];
            if (ImGui.BeginTabItem(viewerState.userId.ToString()))
            {
                foreach (var kinectSenderElement in controllerScene.nodes.ToList())
                {
                    ImGui.InputText("Address##" + kinectSenderElementIndex, ref kinectSenderElement.address, 30);
                    ImGui.InputInt("Port##" + kinectSenderElementIndex, ref kinectSenderElement.port);
                    ImGui.InputFloat3("Position##" + kinectSenderElementIndex, ref kinectSenderElement.position);
                    var eulerAngles = kinectSenderElement.rotation.eulerAngles;
                    if(ImGui.InputFloat3("Rotation##" + kinectSenderElementIndex, ref eulerAngles))
                    {
                        kinectSenderElement.rotation = Quaternion.Euler(eulerAngles);
                    }
                    
                    if(ImGui.Button("Remove This Sender##" + kinectSenderElementIndex))
                    {
                        bool removeResult = controllerScene.nodes.Remove(kinectSenderElement);
                        print($"removeResult: {removeResult}");
                    }

                    ImGui.NewLine();

                    ++kinectSenderElementIndex;
                }

                if(ImGui.Button("Add Kinect Sender"))
                {
                    controllerScene.nodes.Add(new ControllerNode("127.0.0.1", SENDER_PORT, Vector3.zero, Quaternion.identity));
                }

                if(ImGui.Button("Update Scene"))
                {
                    string invalidIpAddress = null;
                    foreach (var kinectSenderElement in controllerScene.nodes)
                    {
                        if(!IsValidIpAddress(kinectSenderElement.address))
                        {
                            invalidIpAddress = kinectSenderElement.address;
                            break;
                        }
                    }

                    if (invalidIpAddress == null)
                    {
                        serverSocket.SendViewerScene(controllerScene);
                    }
                    else
                    {
                        print($"Scene not updated since an invalid IP address was found: {invalidIpAddress}");
                    }
                }
                ImGui.EndTabItem();
            }
        }
        ImGui.EndTabBar();
        ImGui.End();
    }

    private static bool IsValidIpAddress(string ipString)
    {
        IPAddress address;
        return IPAddress.TryParse(ipString, out address);
    }
}
