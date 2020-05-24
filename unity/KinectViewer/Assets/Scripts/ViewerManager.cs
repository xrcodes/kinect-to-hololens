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
    // Key would be receiver session ID.
    private Dictionary<int, KinectReceiver> kinectReceivers;
    private List<RemoteSender> remoteSenders;
    private bool connecting;

    void Start()
    {
        Plugin.texture_group_reset();

        var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 };
        socket.Bind(new IPEndPoint(IPAddress.Any, 0));
        udpSocket = new UdpSocket(socket);

        kinectReceivers = new Dictionary<int, KinectReceiver>();
        remoteSenders = new List<RemoteSender>();
        connecting = false;

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

                    TryConnectToKinect(ipAddressText, SENDER_PORT);
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
            ViewerScene viewerScene = controllerClientSocket.ReceiveViewerScene();
            if(viewerScene != null)
            {
                print($"viewer scene: {viewerScene.kinectSenderElements[0].address}:{viewerScene.kinectSenderElements[0].port}");
                TryConnectToKinect(viewerScene.kinectSenderElements[0].address, viewerScene.kinectSenderElements[0].port);
            }

            var receiverStates = new List<ReceiverState>();
            foreach(var receiver in kinectReceivers.Values)
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

        try
        {
            var senderPacketCollection = SenderPacketReceiver.Receive(udpSocket, remoteSenders);
            foreach (var confirmPacketInfo in senderPacketCollection.ConfirmPacketInfoList)
            {
                if (remoteSenders.Exists(x => x.SenderSessionId == confirmPacketInfo.SenderSessionId))
                    continue;

                // There should be a receiver trying to connect that the confirmation matches.
                KinectReceiver kinectReceiver;
                if (!kinectReceivers.TryGetValue(confirmPacketInfo.ConfirmPacketData.receiverSessionId, out kinectReceiver))
                    continue;

                // Also, the receiver should not have been prepared with a ConfirmSenderPacket yet.
                if (kinectReceiver.State != PrepareState.Unprepared)
                    continue;

                var kinectOrigin = sharedSpaceAnchor.AddKinectOrigin();

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

                KinectReceiver kinectReceiver;
                if (!kinectReceivers.TryGetValue(remoteSender.ReceiverSessionId, out kinectReceiver))
                    continue;

                if (!kinectReceiver.UpdateFrame(this, udpSocket, senderPacketSet))
                {
                    remoteSenders.Remove(remoteSender);
                    kinectReceivers.Remove(remoteSender.ReceiverSessionId);
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
                KinectReceiver kinectReceiver;
                if (kinectReceivers.TryGetValue(remoteSender.ReceiverSessionId, out kinectReceiver))
                {
                    kinectReceivers.Remove(remoteSender.ReceiverSessionId);
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

    private void UpdateUiWindows()
    {
        connectionWindow.Visibility = controllerClientSocket == null && kinectReceivers.Count == 0 && !connecting;

        if (controllerClientSocket != null && kinectReceivers.Count == 0)
        {
            connectedControllerWindow.IpAddress = controllerClientSocket.RemoteEndPoint.Address.ToString();
            connectedControllerWindow.UserId = controllerClientSocket.UserId.ToString();
            connectedControllerWindow.Visibility = true;
        }
        else
        {
            connectedControllerWindow.Visibility = false;
        }
    }

    private async void TryConnectToController(string ipAddress, int port)
    {
        if (controllerClientSocket != null)
        {
            TextToaster.Toast("Cannot connect to multiple controllers at once.");
        }

        if (connecting)
        {
            TextToaster.Toast("Cannot attempt multiple connections at once.");
            return;
        }

        connecting = true;

        var random = new System.Random();
        int userId = random.Next();

        var controllerIpAddress = IPAddress.Parse(ipAddress);
        var controllerEndPoint = new IPEndPoint(controllerIpAddress, port);

        var tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        if (await tcpSocket.ConnectAsync(controllerEndPoint))
        {
            controllerClientSocket = new ControllerClientSocket(userId, tcpSocket);
        }

        connecting = false;
    }

    private async void TryConnectToKinect(string ipAddress, int port)
    {
        if (connecting)
        {
            TextToaster.Toast("Cannot attempt multiple connections at once.");
            return;
        }

        connecting = true;

        TextToaster.Toast($"Try connecting to a Sender at {ipAddress}:{port}...");

        var random = new System.Random();
        int receiverSessionId = random.Next();

        var senderIpAddress = IPAddress.Parse(ipAddress);
        var senderEndPoint = new IPEndPoint(senderIpAddress, port);

        var kinectReceiver = new KinectReceiver(receiverSessionId, senderEndPoint);
        kinectReceivers.Add(receiverSessionId, kinectReceiver);

        // Nudge the sender until a confirm packet is received.
        for (int i = 0; i < 5; ++i)
        {
            if (kinectReceiver.State != PrepareState.Unprepared)
                return;

            udpSocket.Send(PacketHelper.createConnectReceiverPacketBytes(receiverSessionId, true, true, true), senderEndPoint);
            print($"Sent connect packet #{i}");

            await Task.Delay(300);
        }

        // Give up and forget about the connection if a confirm packet has not been received after all the connect packets.
        if (kinectReceiver.State == PrepareState.Unprepared)
            kinectReceivers.Remove(receiverSessionId);

        connecting = false;
    }
}
