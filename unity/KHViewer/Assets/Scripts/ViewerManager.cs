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

    private int viewerId;
    private UdpSocket udpSocket;
    private ControllerClientSocket controllerClientSocket;

    private ControllerScene controllerScene;
    private Dictionary<int, Receiver> receivers;
    private int connectingCount;

    void Start()
    {
        TelepresenceToolkitPlugin.texture_set_reset();

        var random = new System.Random();
        viewerId = random.Next();

        udpSocket = new UdpSocket(1024 * 1024);

        controllerScene = null;
        receivers = new Dictionary<int, Receiver>();
        connectingCount = 0;

        sharedSpaceAnchor.GizmoVisibility = false;

        UpdateUI();
    }

    void Update()
    {
        UpdateUI();

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

    private void UpdateUI()
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
            connectedControllerWindow.ViewerId = controllerClientSocket.ViewerId.ToString();
            connectedControllerWindow.Visibility = true;
        }
        else
        {
            connectedControllerWindow.Visibility = false;
        }
    }

    private void UpdateControllerClient()
    {
        ControllerScene controllerScene = null;
        try
        {
            controllerScene = controllerClientSocket.ReceiveControllerScene();
        }
        catch (TcpSocketException e)
        {
            print($"TcpSocketException while receiving: {e}");
            controllerClientSocket = null;
            return;
        }

        if (controllerScene != null)
        {
            foreach (var node in controllerScene.nodes)
            {
                if (!IPAddress.TryParse(node.senderAddress, out IPAddress ipAddress))
                {
                    TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");
                }

                var senderEndPoint = new IPEndPoint(ipAddress, node.senderPort);
                if (receivers.Values.FirstOrDefault(x => x.SenderEndPoint.Equals(senderEndPoint)) != null)
                    continue;

                StartCoroutine(TryConnectToKinectSender(senderEndPoint));
            }

            foreach(var receiverPair in receivers)
            {
                var node = controllerScene.FindNode(receiverPair.Value.SenderEndPoint);
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

            this.controllerScene = controllerScene;
        }

        var receiverStates = new List<ReceiverState>();
        foreach (var receiver in receivers.Values)
        {
            var receiverState = new ReceiverState(receiver.ReceiverId, receiver.SenderId, receiver.SenderEndPoint);
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
        var confirmPacketInfos = new List<ConfirmPacketInfo>();
        var senderPacketInfos = new Dictionary<int, SenderPacketInfo>();

        try
        {
            SenderPacketClassifier.Classify(udpSocket, receivers.Values, confirmPacketInfos, senderPacketInfos);
        }
        catch (UdpSocketException e)
        {
            // Ignore if there is no end point information.
            if (e.EndPoint.Address == IPAddress.Any)
            {
                TextToaster.Toast("Error from SenderPacketClassifier.Classify() while receiving packets from IPAddress.Any.");
                print("Error from SenderPacketClassifier.Classify() while receiving packets from IPAddress.Any.");
            }
            else
            {
                // Remove the receiver whose connected sender caused the UdpSocketException.
                var receiver = receivers.Values.FirstOrDefault(x => x.SenderEndPoint == e.EndPoint);
                if (receiver != null)
                {
                    receivers.Remove(receiver.ReceiverId);
                    sharedSpaceAnchor.RemoveKinectOrigin(receiver.ReceiverId);
                }
                else
                {
                    TextToaster.Toast($"Failed to remove the receiver whose sender's end point is {e.EndPoint}.");
                    print($"Failed to remove the receiver whose sender's end point is {e.EndPoint}.");
                }
            }
        }

        foreach (var confirmPacketInfo in confirmPacketInfos)
        {
            int receiverId = confirmPacketInfo.ConfirmPacket.receiverId;
            // Ignore if there is already a receiver with the receiver ID.
            if (receivers.ContainsKey(receiverId))
                continue;

            // Receiver and KinectOrigin gets created together.
            // When destroying any of them, the other of the pair should also be destroyed.
            var receiver = new Receiver(receiverId, confirmPacketInfo.ConfirmPacket.senderId, confirmPacketInfo.SenderEndPoint);
            receivers.Add(confirmPacketInfo.ConfirmPacket.receiverId, receiver);
            var kinectOrigin = sharedSpaceAnchor.AddKinectOrigin(receiverId);

            // Apply transformation of kinectSenderElement if there is a corresponding one.
            if (controllerScene != null)
            {
                var node = controllerScene.FindNode(receiver.SenderEndPoint);
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

            var floorVideoMessage = receiver.VideoMessages.Values.FirstOrDefault(x => x.floor != null);
            if (floorVideoMessage != null)
                kinectOrigin.UpdateFrame(floorVideoMessage.floor);

            receiver.UpdateFrame(udpSocket);
        }

        foreach (var receiver in receivers.Values.ToList())
        {
            if (receiver.IsTimedOut())
            {
                print($"Receiver {receiver.ReceiverId} timed out.");
                receivers.Remove(receiver.ReceiverId);
                sharedSpaceAnchor.RemoveKinectOrigin(receiver.ReceiverId);
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
                controllerClientSocket = new ControllerClientSocket(viewerId, tcpSocket);
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
