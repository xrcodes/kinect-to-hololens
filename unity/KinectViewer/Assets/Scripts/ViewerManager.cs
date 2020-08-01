using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading.Tasks;
using UnityEngine;

public class ViewerManager : MonoBehaviour
{
    private const int SENDER_PORT = 3773;

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
    private List<KinectReceiver> kinectReceivers;
    private List<RemoteSender> remoteSenders;
    private int connectingCount;

    void Start()
    {
        Plugin.texture_group_reset();

        var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 };
        socket.Bind(new IPEndPoint(IPAddress.Any, 0));
        udpSocket = new UdpSocket(socket);

        viewerScene = null;
        kinectReceivers = new List<KinectReceiver>();
        remoteSenders = new List<RemoteSender>();
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


                    IPAddress ipAddress;
                    if (!IPAddress.TryParse(ipAddressText, out ipAddress))
                    {
                        TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");
                    }

                    TryConnectToKinectSender(new IPEndPoint(ipAddress, SENDER_PORT));
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

        if (controllerClientSocket != null)
        {
            UpdateControllerClient();
        }

        UpdateReceivers();
    }

    private void UpdateUiWindows()
    {
        connectionWindow.Visibility = controllerClientSocket == null && kinectReceivers.Count == 0 && connectingCount == 0;

        if (controllerClientSocket != null && kinectReceivers.Count == 0)
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
                IPAddress ipAddress;
                if (!IPAddress.TryParse(kinectSenderElement.address, out ipAddress))
                {
                    TextToaster.Toast($"Failed to parse {ipAddress} as an IP address.");
                }

                var endPoint = new IPEndPoint(ipAddress, kinectSenderElement.port);
                if (kinectReceivers.FirstOrDefault(x => x.SenderEndPoint == endPoint) != null)
                    continue;

                TryConnectToKinectSender(endPoint);
            }

            foreach(var kinectReceiver in kinectReceivers)
            {
                var kinectSenderElement = viewerScene.kinectSenderElements.FirstOrDefault(x => x.address == kinectReceiver.SenderEndPoint.Address.ToString()
                                                                                            && x.port == kinectReceiver.SenderEndPoint.Port);

                if (kinectSenderElement != null)
                {
                    var kinectOrigin = kinectReceiver.KinectOrigin;
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
        foreach (var receiver in kinectReceivers)
        {
            var receiverState = new ReceiverState(receiver.SenderEndPoint.Address.ToString(),
                                                  receiver.SenderEndPoint.Port,
                                                  receiver.ReceiverSessionId);
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
            var senderPacketCollection = SenderPacketReceiver.Receive(udpSocket, remoteSenders);
            foreach (var confirmPacketInfo in senderPacketCollection.ConfirmPacketInfoList)
            {
                if (remoteSenders.Exists(x => x.SenderSessionId == confirmPacketInfo.SenderSessionId))
                    continue;

                // There should be a receiver trying to connect that the confirmation matches.
                var kinectReceiver = kinectReceivers.FirstOrDefault(x => x.ReceiverSessionId == confirmPacketInfo.ConfirmPacketData.receiverSessionId);
                if (kinectReceiver == null)
                    continue;

                // Also, the receiver should not have been prepared with a ConfirmSenderPacket yet.
                if (kinectReceiver.State != PrepareState.Unprepared)
                    continue;

                var kinectOrigin = sharedSpaceAnchor.AddKinectOrigin();

                // viewerScene may not exist if connection through sender did not happen through a controller.
                if (viewerScene != null)
                {
                    var kinectSenderElement = viewerScene.kinectSenderElements.FirstOrDefault(x => x.address == kinectReceiver.SenderEndPoint.Address.ToString()
                                                                                                && x.port == kinectReceiver.SenderEndPoint.Port);

                    if (kinectSenderElement != null)
                    {
                        kinectOrigin.transform.localPosition = kinectSenderElement.position;
                        kinectOrigin.transform.localRotation = kinectSenderElement.rotation;
                    }
                }

                kinectReceiver.Prepare(kinectOrigin);
                kinectReceiver.KinectOrigin.Speaker.Setup();

                print($"Sender {confirmPacketInfo.SenderSessionId} connected.");

                remoteSenders.Add(new RemoteSender(confirmPacketInfo.SenderEndPoint,
                                                   confirmPacketInfo.SenderSessionId,
                                                   confirmPacketInfo.ConfirmPacketData.receiverSessionId));
            }

            // Using a copy of remoteSenders through ToList() as this allows removal of elements from remoteSenders.
            foreach (var remoteSender in remoteSenders.ToList())
            {
                SenderPacketSet senderPacketSet;
                if (!senderPacketCollection.SenderPacketSets.TryGetValue(remoteSender.SenderSessionId, out senderPacketSet))
                    continue;

                var kinectReceiver = kinectReceivers.FirstOrDefault(x => x.ReceiverSessionId == remoteSender.ReceiverSessionId);
                if (kinectReceiver == null)
                    continue;

                if (kinectReceiver.State == PrepareState.Unprepared)
                    continue;

                if (!kinectReceiver.UpdateFrame(this, udpSocket, senderPacketSet))
                {
                    remoteSenders.Remove(remoteSender);
                    kinectReceivers.Remove(kinectReceiver);
                    sharedSpaceAnchor.RemoveKinectOrigin(kinectReceiver.KinectOrigin);
                    connectionWindow.Visibility = true;
                }
            }
        }
        catch (UdpSocketException e)
        {
            print($"UdpSocketException: {e}");
            var remoteSender = remoteSenders.FirstOrDefault(x => x.SenderEndPoint == e.EndPoint);
            if (remoteSender != null)
            {
                remoteSenders.Remove(remoteSender);
                var kinectReceiver = kinectReceivers.FirstOrDefault(x => x.ReceiverSessionId == remoteSender.ReceiverSessionId);
                if (kinectReceiver != null)
                {
                    kinectReceivers.Remove(kinectReceiver);
                    sharedSpaceAnchor.RemoveKinectOrigin(kinectReceiver.KinectOrigin);
                }
                else
                {
                    print("Failed to find the KinectReceiver to remove...");
                }
                connectionWindow.Visibility = true;
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

        IPAddress controllerIpAddress;
        if (!IPAddress.TryParse(ipAddress, out controllerIpAddress))
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
        catch(TcpSocketException e)
        {
            TextToaster.Toast("Failed not connect to the controller.");
            print($"An TcpSocketException while connecting to the controller: {e.Message}");
        }

        --connectingCount;
    }

    private async void TryConnectToKinectSender(IPEndPoint endPoint)
    {
        ++connectingCount;

        TextToaster.Toast($"Try connecting to a Sender at {endPoint}...");

        var random = new System.Random();
        int receiverSessionId;
        while (true)
        {
            receiverSessionId = random.Next();
            if (kinectReceivers.FirstOrDefault(x => x.ReceiverSessionId == receiverSessionId) == null)
                break;
        }

        var kinectReceiver = new KinectReceiver(receiverSessionId, endPoint);
        kinectReceivers.Add(kinectReceiver);

        // Nudge the sender until a confirm packet is received.
        for (int i = 0; i < 5; ++i)
        {
            if (kinectReceiver.State != PrepareState.Unprepared)
            {
                --connectingCount;
                return;
            }

            udpSocket.Send(PacketHelper.createConnectReceiverPacketBytes(receiverSessionId, true, true, true), endPoint);
            print($"Sent connect packet #{i}");

            await Task.Delay(300);
        }

        // Give up and forget about the connection if a confirm packet has not been received after all the connect packets.
        if (kinectReceiver.State == PrepareState.Unprepared)
            kinectReceivers.Remove(kinectReceiver);

        --connectingCount;
    }
}
