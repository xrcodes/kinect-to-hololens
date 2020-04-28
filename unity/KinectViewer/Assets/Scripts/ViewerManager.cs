using System.Collections;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class ViewerManager : MonoBehaviour
{
    private const int SENDER_PORT = 3773;
    private const float OFFSET_UNIT = 0.1f;

    // The main camera's Transform.
    public Transform cameraTransform;
    // The TextMesh placed above user's head.
    public TextMesh statusText;
    // TextMeshes for the UI.
    public ConnectionWindow connectionWindow;
    public TextToaster textToaster;
    public TextMesh offsetText;
    // The root of the scene that includes everything else except the main camera.
    // This provides a convenient way to place everything in front of the camera.
    public SharedSpaceAnchor sharedSpaceAnchor;


    private UdpSocket udpSocket;

    private ControllerClient controllerClient;
    private KinectReceiver kinectReceiver;

    private bool ConnectWindowVisibility
    {
        get => connectionWindow.gameObject.activeSelf;
        set => connectionWindow.gameObject.SetActive(value);
    }

    void Start()
    {
        Plugin.texture_group_reset();

        udpSocket = new UdpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 });

        statusText.text = "Waiting for user input.";
        connectionWindow.ConnectionTarget = ConnectionTarget.Controller;
    }

    void Update()
    {
        // Sends virtual keyboards strokes to the TextMeshes for the IP address and the port.
        AbsorbInput();

        if (Input.GetKeyDown(KeyCode.Tab))
        {
            if (connectionWindow.ConnectionTarget == ConnectionTarget.Controller)
                connectionWindow.ConnectionTarget = ConnectionTarget.Kinect;
            else
                connectionWindow.ConnectionTarget = ConnectionTarget.Controller;
        }

        if (Input.GetKeyDown(KeyCode.Return) || Input.GetKeyDown(KeyCode.KeypadEnter))
        {
            if (connectionWindow.ConnectionTarget == ConnectionTarget.Controller)
                TryConnectToController();
            else
                StartCoroutine(TryConnectToKinect());
        }

        // Gives the information of the camera position and floor level.
        if (Input.GetKeyDown(KeyCode.Space))
        {
            sharedSpaceAnchor.UpdateTransform(cameraTransform.position, cameraTransform.rotation);
        }

        if (Input.GetKeyDown(KeyCode.D))
        {
            sharedSpaceAnchor.DebugVisibility = !sharedSpaceAnchor.DebugVisibility;
        }
        
        if (sharedSpaceAnchor.KinectOrigin == null)
        {
            offsetText.gameObject.SetActive(false);
        }
        else
        {
            if (Input.GetKeyDown(KeyCode.LeftArrow))
            {
                sharedSpaceAnchor.KinectOrigin.OffsetDistance -= OFFSET_UNIT;
            }
            if (Input.GetKeyDown(KeyCode.RightArrow))
            {
                sharedSpaceAnchor.KinectOrigin.OffsetDistance += OFFSET_UNIT;
            }
            if (Input.GetKeyDown(KeyCode.DownArrow))
            {
                sharedSpaceAnchor.KinectOrigin.OffsetHeight -= OFFSET_UNIT;
            }
            if (Input.GetKeyDown(KeyCode.UpArrow))
            {
                sharedSpaceAnchor.KinectOrigin.OffsetHeight += OFFSET_UNIT;
            }

            offsetText.gameObject.SetActive(!ConnectWindowVisibility && sharedSpaceAnchor.DebugVisibility);
            if (offsetText.gameObject.activeSelf)
            {
                offsetText.text = $"Offset\n  - Distance: {sharedSpaceAnchor.KinectOrigin.OffsetDistance}\n  - Height: {sharedSpaceAnchor.KinectOrigin.OffsetHeight}";
            }
        }

        if (controllerClient != null)
        {
            var receiverStates = new List<ReceiverState>();
            if (kinectReceiver != null)
            {
                var receiverState = new ReceiverState(kinectReceiver.SenderEndPoint.Address.ToString(),
                                                      kinectReceiver.SenderEndPoint.Port,
                                                      kinectReceiver.ReceiverSessionId);
                receiverStates.Add(receiverState);
            }
            try
            {
                controllerClient.SendViewerState(receiverStates);
            }
            catch (TcpSocketException e)
            {
                print($"TcpSocketException while connecting: {e}");
                controllerClient = null;
            }
        }

        if (kinectReceiver != null)
        {
            var senderPacketSet = SenderPacketReceiver.Receive(udpSocket);
            if (!kinectReceiver.UpdateFrame(udpSocket, senderPacketSet))
            {
                kinectReceiver = null;
                ConnectWindowVisibility = true;
            }
        }
    }

    // Sends keystrokes of the virtual keyboard to TextMeshes.
    // Try connecting the Receiver to a Sender when the user pressed the enter key.
    private void AbsorbInput()
    {
        AbsorbKeyCode(KeyCode.Alpha0, '0');
        AbsorbKeyCode(KeyCode.Keypad0, '0');
        AbsorbKeyCode(KeyCode.Alpha1, '1');
        AbsorbKeyCode(KeyCode.Keypad1, '1');
        AbsorbKeyCode(KeyCode.Alpha2, '2');
        AbsorbKeyCode(KeyCode.Keypad2, '2');
        AbsorbKeyCode(KeyCode.Alpha3, '3');
        AbsorbKeyCode(KeyCode.Keypad3, '3');
        AbsorbKeyCode(KeyCode.Alpha4, '4');
        AbsorbKeyCode(KeyCode.Keypad4, '4');
        AbsorbKeyCode(KeyCode.Alpha5, '5');
        AbsorbKeyCode(KeyCode.Keypad5, '5');
        AbsorbKeyCode(KeyCode.Alpha6, '6');
        AbsorbKeyCode(KeyCode.Keypad6, '6');
        AbsorbKeyCode(KeyCode.Alpha7, '7');
        AbsorbKeyCode(KeyCode.Keypad7, '7');
        AbsorbKeyCode(KeyCode.Alpha8, '8');
        AbsorbKeyCode(KeyCode.Keypad8, '8');
        AbsorbKeyCode(KeyCode.Alpha9, '9');
        AbsorbKeyCode(KeyCode.Keypad9, '9');
        AbsorbKeyCode(KeyCode.Period, '.');
        AbsorbKeyCode(KeyCode.KeypadPeriod, '.');
        if (Input.GetKeyDown(KeyCode.Backspace))
        {
            var text = connectionWindow.IpAddressInputText;
            if (connectionWindow.IpAddressInputText.Length > 0)
            {
                connectionWindow.IpAddressInputText = text.Substring(0, text.Length - 1);
            }
        }
    }

    // A helper method for AbsorbInput().
    private void AbsorbKeyCode(KeyCode keyCode, char c)
    {
        if (Input.GetKeyDown(keyCode))
        {
            connectionWindow.IpAddressInputText += c;
        }
    }

    private async void TryConnectToController()
    {
        if (controllerClient != null)
        {
            textToaster.Toast("A controller is already connected.");
            return;
        }

        if (!ConnectWindowVisibility)
        {
            textToaster.Toast("Cannot try connecting to more than one remote machine.");
            return;
        }

        ConnectWindowVisibility = false;

        var random = new System.Random();
        int userId = random.Next();

        var tcpSocket = new TcpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp));
        if (await tcpSocket.ConnectAsync(IPAddress.Loopback, ControllerMessages.PORT))
        {
            textToaster.Toast("connected");
            controllerClient = new ControllerClient(userId, tcpSocket);
        }
        else
        {
            textToaster.Toast("not connected");
        }


        ConnectWindowVisibility = true;
    }

    // To copy the c++ receiver, for easier development,
    // there should be only one chance to send a ping.
    private IEnumerator TryConnectToKinect()
    {
        if(!ConnectWindowVisibility)
        {
            textToaster.Toast("Cannot try connecting to more than one remote machine.");
            yield break;
        }

        ConnectWindowVisibility = false;

        // The default IP address is 127.0.0.1.
        string ipAddressText = connectionWindow.IpAddressInputText;
        if (ipAddressText.Length == 0)
            ipAddressText = "127.0.0.1";

        string logString = $"Try connecting to {ipAddressText}...";
        textToaster.Toast(logString);
        statusText.text = logString;

        var random = new System.Random();
        int receiverSessionId = random.Next();

        var ipAddress = IPAddress.Parse(ipAddressText);
        var endPoint = new IPEndPoint(ipAddress, SENDER_PORT);

        InitSenderPacketData initPacketData;
        int connectCount = 0;
        while (true)
        {
            udpSocket.Send(PacketHelper.createConnectReceiverPacketBytes(receiverSessionId, true, true, true), endPoint);
            ++connectCount;
            print("Sent connect packet");

            yield return new WaitForSeconds(0.3f);

            try
            {
                var senderPacketSet = SenderPacketReceiver.Receive(udpSocket);
                if (senderPacketSet.InitPacketDataList.Count > 0)
                {
                    initPacketData = senderPacketSet.InitPacketDataList[0];
                    break;
                }
            }
            catch (UdpSocketException e)
            {
                print($"UdpSocketException while connecting: {e}");
            }

            if (connectCount == 10)
            {
                textToaster.Toast("Tried connected 10 times but failed to receive an init packet...\n");
                ConnectWindowVisibility = true;
                yield break;
            }
        }

        textToaster.Toast("Start creating screen");

        if(sharedSpaceAnchor.KinectOrigin == null)
            sharedSpaceAnchor.AddKinectOrigin();

        yield return StartCoroutine(sharedSpaceAnchor.KinectOrigin.Screen.SetupMesh(initPacketData));
        sharedSpaceAnchor.KinectOrigin.Speaker.Setup();
        kinectReceiver = new KinectReceiver(receiverSessionId, endPoint, sharedSpaceAnchor.KinectOrigin, initPacketData);
    }
}
