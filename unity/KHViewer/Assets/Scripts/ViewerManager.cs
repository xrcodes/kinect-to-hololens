using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;
using TelepresenceToolkit;

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
    public SharedSpaceScene sharedSpaceScene;

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

        sharedSpaceScene.GizmoVisibility = false;

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
                        TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");

                    StartCoroutine(TryConnectToKinectSender(new IPEndPoint(ipAddress, SENDER_DEFAULT_PORT)));
                }
            }
        }

        // Sets the anchor's position and rotation using the current position of the HoloLens device.
        if (Input.GetKeyDown(KeyCode.Space))
            sharedSpaceScene.SetPositionAndRotation(mainCameraTransform.position, mainCameraTransform.rotation);

        if (Input.GetKeyDown(KeyCode.V))
            sharedSpaceScene.GizmoVisibility = !sharedSpaceScene.GizmoVisibility;

        if (Input.GetKeyDown(KeyCode.F))
            FpsCounter.Toast();

        if (controllerClientSocket != null)
            UpdateControllerClient();

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
            print($"TcpSocketException while receiving from controller: {e}");
            controllerClientSocket = null;
            return;
        }

        if (controllerScene != null)
        {
            foreach (var controllerNode in controllerScene.nodes)
            {
                if (!IPAddress.TryParse(controllerNode.senderAddress, out IPAddress ipAddress))
                {
                    TextToaster.Toast($"Failed to parse an IP address ({ipAddress}) from controller.");
                    continue;
                }

                var senderEndPoint = new IPEndPoint(ipAddress, controllerNode.senderPort);
                if (receivers.Values.FirstOrDefault(x => x.SenderEndPoint.Equals(senderEndPoint)) != null)
                    continue;

                StartCoroutine(TryConnectToKinectSender(senderEndPoint));
            }

            foreach(var receiverPair in receivers)
            {
                var controllerNode = controllerScene.FindNode(receiverPair.Value.SenderEndPoint);
                if (controllerNode != null)
                {
                    var kinectNode = sharedSpaceScene.GetKinectNode(receiverPair.Key);
                    if (kinectNode != null)
                    {
                        kinectNode.transform.localPosition = controllerNode.position;
                        kinectNode.transform.localRotation = controllerNode.rotation;
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
            }
            else
            {
                // Remove the receiver whose connected sender caused the UdpSocketException.
                var receiver = receivers.Values.FirstOrDefault(x => x.SenderEndPoint == e.EndPoint);
                if (receiver != null)
                {
                    receivers.Remove(receiver.ReceiverId);
                    sharedSpaceScene.RemoveKinectNode(receiver.ReceiverId);
                }
                else
                {
                    TextToaster.Toast($"Failed to remove the receiver whose sender's end point is {e.EndPoint}.");
                }
            }
        }

        foreach (var confirmPacketInfo in confirmPacketInfos)
        {
            int receiverId = confirmPacketInfo.ConfirmPacket.receiverId;
            // Ignore if there is already a receiver with the receiver ID.
            if (receivers.ContainsKey(receiverId))
                continue;

            print("confirm packet: " + receiverId);
            // Receiver and KinectOrigin gets created together.
            // When destroying any of them, the other of the pair should also be destroyed.
            var receiver = new Receiver(receiverId, confirmPacketInfo.ConfirmPacket.senderId, confirmPacketInfo.SenderEndPoint);
            receivers.Add(confirmPacketInfo.ConfirmPacket.receiverId, receiver);
            var kinectOrigin = sharedSpaceScene.AddKinectNode(receiverId);

            // Apply transformation of kinectSenderElement if there is a corresponding one.
            if (controllerScene != null)
            {
                var controllerNode = controllerScene.FindNode(receiver.SenderEndPoint);
                if (controllerNode != null)
                {
                    kinectOrigin.transform.localPosition = controllerNode.position;
                    kinectOrigin.transform.localRotation = controllerNode.rotation;
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
            var kinectNode = sharedSpaceScene.GetKinectNode(receiver.ReceiverId);
            receiver.ReceivePackets(udpSocket, senderPacketInfoPair.Value, kinectNode.KinectRenderer.Material, kinectNode.Speaker.RingBuffer);

            print("sender packet: " + receiver.ReceiverId);
            // Remove receivers that did not receive any packet for too long.
            if (receiver.IsTimedOut())
            {
                TextToaster.Toast($"Receiver {receiver.ReceiverId} timed out.");
                receivers.Remove(receiver.ReceiverId);
                sharedSpaceScene.RemoveKinectNode(receiver.ReceiverId);
                continue;
            }

            if (kinectNode.KinectRenderer.State == PrepareState.Unprepared)
            {
                if (receiver.VideoMessages.Count > 0)
                {
                    foreach (var videoMessage in receiver.VideoMessages.Values)
                    {
                        kinectNode.KinectRenderer.StartPrepare(videoMessage);
                        break;
                    }
                }
            }
            else if (kinectNode.KinectRenderer.State == PrepareState.Preparing)
            {
                kinectNode.SetProgressText(receiver.SenderEndPoint, kinectNode.kinectRenderer.Progress);
                kinectNode.ProgressTextVisibility = true;
            }
            else if (kinectNode.KinectRenderer.State == PrepareState.Prepared)
            {
                kinectNode.ProgressTextVisibility = false;
            }

            var floorVideoMessage = receiver.VideoMessages.Values.FirstOrDefault(x => x.floor != null);
            if (floorVideoMessage != null)
                kinectNode.UpdateFrame(floorVideoMessage.floor);

            receiver.UpdateFrame(udpSocket);
        }
    }

    private async void TryConnectToController(string ipAddress, int port)
    {
        if (controllerClientSocket != null)
        {
            TextToaster.Toast("Cannot connect to multiple controllers at once.");
            return;
        }

        if (!IPAddress.TryParse(ipAddress, out IPAddress controllerIpAddress))
        {
            TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");
            return;
        }

        Interlocked.Increment(ref connectingCount);
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

        Interlocked.Decrement(ref connectingCount);
    }

    // Nudge a sender with a connect packet five times.
    private IEnumerator TryConnectToKinectSender(IPEndPoint senderEndPoint)
    {
        Interlocked.Increment(ref connectingCount);

        TextToaster.Toast($"Try Connecting to a Sender: {senderEndPoint}");

        var random = new System.Random();
        int receiverId = random.Next();

        for (int i = 0; i < 5; ++i)
        {
            udpSocket.Send(PacketUtils.createConnectReceiverPacketBytes(receiverId, true, true).bytes, senderEndPoint);
            yield return new WaitForSeconds(0.3f);
        }

        Interlocked.Decrement(ref connectingCount);
    }
}
