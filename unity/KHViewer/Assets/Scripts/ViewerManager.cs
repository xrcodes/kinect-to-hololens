using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class ViewerManager : MonoBehaviour
{
    private const int SENDER_DEFAULT_PORT = 3773;

    // The main camera's Transform.
    public Transform mainCameraTransform;
    // TextMeshes for the UI.
    public ConnectionWindow connectionWindow;
    public ConnectedControllerWindow connectedControllerWindow;
    // The root of the scene that includes everything else except the main camera.
    // This provides a convenient way to place everything in front of the camera.
    public SharedSpaceAnchor sharedSpaceAnchor;

    private UdpSocket udpSocket;
    private ControllerClientSocket controllerClientSocket;

    private ControllerScene controllerScene;
    private Dictionary<int, Receiver> receivers;
    private int connectingCount;

    void Start()
    {
        TelepresenceToolkitPlugin.texture_set_reset();

        var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 };
        socket.Bind(new IPEndPoint(IPAddress.Any, 0));
        udpSocket = new UdpSocket(socket);

        controllerScene = null;
        receivers = new Dictionary<int, Receiver>();
        connectingCount = 0;

        sharedSpaceAnchor.GizmoVisibility = false;

        UpdateUiWindows();
    }

    void Update()
    {
        UpdateUiWindows();

        if (connectionWindow.Visibility)
        {
            if (Input.GetKeyDown(KeyCode.Return))
            {
                if (connectionWindow.ConnectionTarget == ConnectionTarget.Controller)
                {
                    string ipAddressText = connectionWindow.IpAddress;
                    if (ipAddressText.Length == 0)
                        ipAddressText = "127.0.0.1";

                    TryConnectToController(ipAddressText, ControllerMessages.PORT);
                }
                else
                {
                    string ipAddressText = connectionWindow.IpAddress;
                    if (ipAddressText.Length == 0)
                        ipAddressText = "127.0.0.1";


                    if (!IPAddress.TryParse(ipAddressText, out IPAddress ipAddress))
                    {
                        TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");
                    }

                    StartCoroutine(TryConnectToKinectSender(new IPEndPoint(ipAddress, SENDER_DEFAULT_PORT)));
                }
            }
        }

        // Sets the anchor's position and rotation using the current position of the HoloLens device.
        if (Input.GetKeyDown(KeyCode.Space))
        {
            sharedSpaceAnchor.SetPositionAndRotation(mainCameraTransform.position, mainCameraTransform.rotation);
        }

        if (Input.GetKeyDown(KeyCode.V))
        {
            sharedSpaceAnchor.GizmoVisibility = !sharedSpaceAnchor.GizmoVisibility;
        }

        if (Input.GetKeyDown(KeyCode.F))
        {
            FpsCounter.Toast();
        }

            if (controllerClientSocket != null)
        {
            UpdateControllerClient();
        }

        UpdateReceivers();
    }

    private void UpdateUiWindows()
    {
        connectionWindow.Visibility = controllerClientSocket == null && receivers.Count == 0 && connectingCount == 0;

        if (controllerClientSocket != null && receivers.Count == 0)
        {
            if (controllerClientSocket.RemoteEndPoint != null)
            {
                connectedControllerWindow.IpAddress = controllerClientSocket.RemoteEndPoint.Address.ToString();
            }
            else
            {
                connectedControllerWindow.IpAddress = "N/A";
            }
            connectedControllerWindow.UserId = controllerClientSocket.UserId.ToString();
            connectedControllerWindow.Visibility = true;
        }
        else
        {
            connectedControllerWindow.Visibility = false;
        }
    }

    private void UpdateControllerClient()
    {
        ControllerScene viewerScene = null;
        try
        {
            viewerScene = controllerClientSocket.ReceiveViewerScene();
        }
        catch (TcpSocketException e)
        {
            print($"TcpSocketException while receiving: {e}");
            controllerClientSocket = null;
            return;
        }

        if (viewerScene != null)
        {
            foreach (var node in viewerScene.nodes)
            {
                if (!IPAddress.TryParse(node.address, out IPAddress ipAddress))
                {
                    TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");
                }

                var endPoint = new IPEndPoint(ipAddress, node.port);
                print($"endPoint: {endPoint}");
                foreach (var receiverPair in receivers)
                {
                    print($"receiver.SenderEndPoint: {receiverPair.Value.SenderEndPoint}");
                }

                if (receivers.Values.FirstOrDefault(x => x.SenderEndPoint.Equals(endPoint)) != null)
                    continue;

                StartCoroutine(TryConnectToKinectSender(endPoint));
            }

            foreach(var receiverPair in receivers)
            {
                var node = viewerScene.nodes.FirstOrDefault(x => x.address == receiverPair.Value.SenderEndPoint.Address.ToString()
                                                              && x.port == receiverPair.Value.SenderEndPoint.Port);

                if (node != null)
                {
                    var kinectOrigin = sharedSpaceAnchor.GetKinectOrigin(receiverPair.Key);
                    if (kinectOrigin != null)
                    {
                        kinectOrigin.transform.localPosition = node.position;
                        kinectOrigin.transform.localRotation = node.rotation;
                    }
                }
            }

            this.controllerScene = viewerScene;
        }

        var receiverStates = new List<ReceiverState>();
        foreach (var receiver in receivers.Values)
        {
            var receiverState = new ReceiverState(receiver.SenderEndPoint.Address.ToString(),
                                                  receiver.SenderEndPoint.Port,
                                                  receiver.ReceiverId);
            receiverStates.Add(receiverState);
        }

        try
        {
            controllerClientSocket.SendViewerState(receiverStates);
        }
        catch (TcpSocketException e)
        {
            print($"TcpSocketException while connecting: {e}");
            controllerClientSocket = null;
        }
    }

    private void UpdateReceivers()
    {
        try
        {
            SenderPacketClassifier.Classify(udpSocket, receivers.Values,
                                            out var confirmPacketInfos,
                                            out var senderPacketInfos);
            foreach (var confirmPacketInfo in confirmPacketInfos)
            {
                int receiverId = confirmPacketInfo.ConfirmPacket.receiverId;
                // Ignore if there is already a receiver with the receiver ID.
                if (receivers.ContainsKey(receiverId))
                    continue;

                var kinectOrigin = sharedSpaceAnchor.AddKinectOrigin(receiverId);

                var receiver = new Receiver(receiverId, confirmPacketInfo.ConfirmPacket.senderId, confirmPacketInfo.SenderEndPoint);
                receivers.Add(confirmPacketInfo.ConfirmPacket.receiverId, receiver);

                // Apply transformation of kinectSenderElement if there is a corresponding one.
                if (controllerScene != null)
                {
                    var node = controllerScene.nodes.FirstOrDefault(x => x.address == receiver.SenderEndPoint.Address.ToString()
                                                                  && x.port == receiver.SenderEndPoint.Port);
                    if (node != null)
                    {
                        kinectOrigin.transform.localPosition = node.position;
                        kinectOrigin.transform.localRotation = node.rotation;
                    }
                }
            }

            // Send heartbeat packets to senders.
            foreach (var receiver in receivers.Values)
                receiver.SendHeartBeat(udpSocket);

            // Using a copy of remoteSenders through ToList() as this allows removal of elements from remoteSenders.
            foreach (var senderPacketInfoPair in senderPacketInfos)
            {
                // The keys of senderPacketInfos are sender IDs, not receiver IDs like other collections.
                int senderId = senderPacketInfoPair.Key;
                // Since senderPacketInfos were built based on receivers, there should be a corresponding receiver.
                var receiver = receivers.Values.First(x => x.SenderId == senderId);

                var kinectOrigin = sharedSpaceAnchor.GetKinectOrigin(receiver.ReceiverId);
                receiver.ReceivePackets(udpSocket, senderPacketInfoPair.Value, kinectOrigin);

                if (kinectOrigin.Screen.State == PrepareState.Unprepared)
                {
                    if (receiver.VideoMessages.Count > 0)
                    {
                        foreach (var videoMessage in receiver.VideoMessages.Values)
                        {
                            kinectOrigin.Screen.StartPrepare(videoMessage);
                            break;
                        }
                    }
                }
                else if (kinectOrigin.Screen.State == PrepareState.Preparing)
                {
                    kinectOrigin.SetProgressText(receiver.SenderEndPoint, kinectOrigin.screen.Progress);
                    kinectOrigin.ProgressTextVisibility = true;
                }
                else if (kinectOrigin.Screen.State == PrepareState.Prepared)
                {
                    kinectOrigin.ProgressTextVisibility = false;
                }

                kinectOrigin.UpdateFrame(receiver.VideoMessages);

                receiver.UpdateFrame(udpSocket);
            }

            foreach (var receiver in receivers.Values)
            {
                if (receiver.IsTimedOut())
                {
                    print($"Receiver {receiver.ReceiverId} timed out.");
                    receivers.Remove(receiver.ReceiverId);
                    sharedSpaceAnchor.RemoveKinectOrigin(receiver.ReceiverId);
                    connectionWindow.Visibility = true;
                }
            }
        }
        catch (UdpSocketException e)
        {
            print($"UdpSocketException: {e}");
            var receiver = receivers.Values.FirstOrDefault(x => x.SenderEndPoint == e.EndPoint);
            if (receiver != null)
            {
                receivers.Remove(receiver.ReceiverId);
                sharedSpaceAnchor.RemoveKinectOrigin(receiver.ReceiverId);
                connectionWindow.Visibility = true;
            }
            else
            {
                print("Failed to find the KinectReceiver to remove...");
            }
        }
    }

    private async void TryConnectToController(string ipAddress, int port)
    {
        if (controllerClientSocket != null)
        {
            TextToaster.Toast("Cannot connect to multiple controllers at once.");
            return;
        }

        ++connectingCount;

        var random = new System.Random();
        int userId = random.Next();

        if (!IPAddress.TryParse(ipAddress, out IPAddress controllerIpAddress))
        {
            TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");
            --connectingCount;
            return;
        }

        var controllerEndPoint = new IPEndPoint(controllerIpAddress, port);

        var tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        try
        {
            if (await tcpSocket.ConnectAsync(controllerEndPoint))
            {
                controllerClientSocket = new ControllerClientSocket(userId, tcpSocket);
            }
        }
        catch (TcpSocketException e)
        {
            TextToaster.Toast("Failed not connect to the controller.");
            print($"An TcpSocketException while connecting to the controller: {e.Message}");
        }

        --connectingCount;
    }

    // Nudge a sender with a connect packet five times.
    private IEnumerator TryConnectToKinectSender(IPEndPoint senderEndPoint)
    {
        ++connectingCount;
        TextToaster.Toast($"Try Connecting to a Sender: {senderEndPoint}");
        print($"Try Connecting to a Sender: {senderEndPoint}");
        var random = new System.Random();
        int receiverId = random.Next();

        for (int i = 0; i < 5; ++i)
        {
            udpSocket.Send(PacketUtils.createConnectReceiverPacketBytes(receiverId, true, true).bytes, senderEndPoint);
            yield return new WaitForSeconds(0.3f);
        }

        --connectingCount;
    }
}
