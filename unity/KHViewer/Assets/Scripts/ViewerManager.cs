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

    private ViewerScene viewerScene;
    private Dictionary<int, Receiver> receivers;
    private int connectingCount;

    void Start()
    {
        TelepresenceToolkitPlugin.texture_set_reset();

        var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 };
        socket.Bind(new IPEndPoint(IPAddress.Any, 0));
        udpSocket = new UdpSocket(socket);

        viewerScene = null;
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
        ViewerScene viewerScene = null;
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
            foreach (var kinectSenderElement in viewerScene.kinectSenderElements)
            {
                if (!IPAddress.TryParse(kinectSenderElement.address, out IPAddress ipAddress))
                {
                    TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");
                }

                var endPoint = new IPEndPoint(ipAddress, kinectSenderElement.port);
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
                var kinectSenderElement = viewerScene.kinectSenderElements.FirstOrDefault(x => x.address == receiverPair.Value.SenderEndPoint.Address.ToString()
                                                                                            && x.port == receiverPair.Value.SenderEndPoint.Port);

                if (kinectSenderElement != null)
                {
                    var kinectOrigin = sharedSpaceAnchor.GetKinectOrigin(receiverPair.Key);
                    if (kinectOrigin != null)
                    {
                        kinectOrigin.transform.localPosition = kinectSenderElement.position;
                        kinectOrigin.transform.localRotation = kinectSenderElement.rotation;
                    }
                }
            }

            this.viewerScene = viewerScene;
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
                if (viewerScene != null)
                {
                    var kinectSenderElement = viewerScene.kinectSenderElements.FirstOrDefault(x => x.address == receiver.SenderEndPoint.Address.ToString()
                                                                                                && x.port == receiver.SenderEndPoint.Port);
                    if (kinectSenderElement != null)
                    {
                        kinectOrigin.transform.localPosition = kinectSenderElement.position;
                        kinectOrigin.transform.localRotation = kinectSenderElement.rotation;
                    }
                }
            }

            // Using a copy of remoteSenders through ToList() as this allows removal of elements from remoteSenders.
            foreach (var senderPacketInfoPair in senderPacketInfos)
            {
                int senderId = senderPacketInfoPair.Key;
                var receiver = receivers.Values.FirstOrDefault(x => x.SenderId == senderId);
                if (receiver == null)
                    continue;

                int receiverId = receiver.ReceiverId;
                var kinectOrigin = sharedSpaceAnchor.GetKinectOrigin(receiverId);
                if (!receiver.UpdateFrame(udpSocket, senderPacketInfoPair.Value, kinectOrigin))
                {
                    receivers.Remove(receiverId);
                    sharedSpaceAnchor.RemoveKinectOrigin(receiverId);
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
        print($"Try Connecting to a Sender: {senderEndPoint}");
        TextToaster.Toast($"Try Connecting to a Sender: {senderEndPoint}");
        var random = new System.Random();
        int receiverId = random.Next();

        for (int i = 0; i < 5; ++i)
        {
            print($"Send connect packet #{i}");
            udpSocket.Send(PacketUtils.createConnectReceiverPacketBytes(receiverId, true, true).bytes, senderEndPoint);
            yield return new WaitForSeconds(0.3f);
        }

        --connectingCount;
    }
}
