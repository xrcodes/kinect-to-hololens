using System.Collections;
using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class ViewerManager : MonoBehaviour
{
    private const int PORT = 3773;
    private const float OFFSET_OFFSET_UNIT = 0.1f;

    // The main camera's Transform.
    public Transform cameraTransform;
    // The TextMesh placed above user's head.
    public TextMesh statusText;
    // TextMeshes for the UI.
    public ConnectionWindow connectionWindow;
    public TextMesh offsetText;

    // The root of the scene that includes everything else except the main camera.
    // This provides a convenient way to place everything in front of the camera.
    public AzureKinectRoot azureKinectRoot;

    private KinectReceiver kinectReceiver;

    private bool ConnectWindowVisibility
    {
        get => connectionWindow.gameObject.activeSelf;
        set => connectionWindow.gameObject.SetActive(value);
    }

    void Awake()
    {
        Plugin.texture_group_reset();

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
            StartCoroutine(Connect());
        }

        // Gives the information of the camera position and floor level.
        if (Input.GetKeyDown(KeyCode.Space))
        {
            azureKinectRoot.SetRootTransform(cameraTransform.position, cameraTransform.rotation);
        }

        if (Input.GetKeyDown(KeyCode.D))
        {
            azureKinectRoot.DebugVisibility = !azureKinectRoot.DebugVisibility;
        }

        if (Input.GetKeyDown(KeyCode.LeftArrow))
        {
            azureKinectRoot.OffsetDistance -= OFFSET_OFFSET_UNIT;
        }
        if (Input.GetKeyDown(KeyCode.RightArrow))
        {
            azureKinectRoot.OffsetDistance += OFFSET_OFFSET_UNIT;
        }
        if (Input.GetKeyDown(KeyCode.DownArrow))
        {
            azureKinectRoot.OffsetHeight -= OFFSET_OFFSET_UNIT;
        }
        if (Input.GetKeyDown(KeyCode.UpArrow))
        {
            azureKinectRoot.OffsetHeight += OFFSET_OFFSET_UNIT;
        }

        offsetText.gameObject.SetActive(!ConnectWindowVisibility && azureKinectRoot.DebugVisibility);
        if(offsetText.gameObject.activeSelf)
        {
            offsetText.text = $"Offset\n  - Distance: {azureKinectRoot.OffsetDistance}\n  - Height: {azureKinectRoot.OffsetHeight}";
        }

        if (kinectReceiver == null)
            return;

        if (!kinectReceiver.UpdateFrame())
        {
            kinectReceiver = null;
            ConnectWindowVisibility = true;
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

    // To copy the c++ receiver, for easier development,
    // there should be only one chance to send a ping.
    private IEnumerator Connect()
    {
        if(!ConnectWindowVisibility)
        {
            print("No more than one ping at a time.");
            yield break;
        }

        ConnectWindowVisibility = false;

        // The default IP address is 127.0.0.1.
        string ipAddressText = connectionWindow.IpAddressInputText;
        if (ipAddressText.Length == 0)
            ipAddressText = "127.0.0.1";

        string logString = $"Try connecting to {ipAddressText}...";
        print(logString);
        statusText.text = logString;

        var random = new System.Random();
        int sessionId = random.Next();

        var ipAddress = IPAddress.Parse(ipAddressText);
        var udpSocket = new UdpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 });
        var endPoint = new IPEndPoint(ipAddress, PORT);

        InitSenderPacketData initPacketData;
        int connectCount = 0;
        while (true)
        {
            udpSocket.Send(PacketHelper.createConnectReceiverPacketBytes(sessionId), endPoint);
            ++connectCount;
            UnityEngine.Debug.Log("Sent connect packet");

            yield return new WaitForSeconds(0.3f);

            try
            {
                var senderPacketSet = SenderPacketReceiver.Receive(udpSocket, null);
                if (senderPacketSet.InitPacketDataList.Count > 0)
                {
                    initPacketData = senderPacketSet.InitPacketDataList[0];
                    break;
                }
            }
            catch (UdpSocketException e)
            {
                Debug.Log($"UdpSocketRuntimeError while connecting: {e}");
            }

            if (connectCount == 10)
            {
                UnityEngine.Debug.Log("Tried pinging 10 times and failed to received an init packet...\n");
                ConnectWindowVisibility = true;
                yield break;
            }
        }

        azureKinectRoot.Screen.Setup(initPacketData);
        azureKinectRoot.Speaker.Setup();
        kinectReceiver = new KinectReceiver(azureKinectRoot, udpSocket, sessionId, endPoint, initPacketData);
    }
}
