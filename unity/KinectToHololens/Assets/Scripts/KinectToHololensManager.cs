using System.Collections;
using System.Net;
using System.Net.Sockets;
using UnityEngine;
using UnityEngine.XR.WSA.Input;

public enum InputState
{
    IpAddress, Port
}

public class KinectToHololensManager : MonoBehaviour
{
    // The main camera's Transform.
    public Transform cameraTransform;
    // The TextMesh placed above user's head.
    public TextMesh statusText;
    // The root of the scene that includes everything else except the main camera.
    // This provides a convenient way to place everything in front of the camera.
    public Transform scenceRootTransform;
    // TextMeshes for the UI.
    public TextMesh ipAddressText;
    public TextMesh ipAddressInputField;
    public TextMesh portText;
    public TextMesh portInputField;
    public TextMesh instructionText;
    // For rendering the Kinect pixels in 3D.
    public Material azureKinectScreenMaterial;
    public AzureKinectScreen azureKinectScreen;
    public Transform floorPlaneTransform;

    // To recognize when the user taps.
    private GestureRecognizer gestureRecognizer;
    // Varaibles that represent states of the scene.
    private InputState inputState;

    private KinectReceiver kinectReceiver;


    private InputState InputState
    {
        set
        {
            if (value == InputState.IpAddress)
            {
                ipAddressText.color = Color.yellow;
                portText.color = Color.white;
            }
            else
            {
                ipAddressText.color = Color.white;
                portText.color = Color.yellow;
            }

            inputState = value;
        }
    }

    private TextMesh ActiveInputField
    {
        get
        {
            return inputState == InputState.IpAddress ? ipAddressInputField : portInputField;
        }
    }

    private bool UiVisibility
    {
        set
        {
            ipAddressText.gameObject.SetActive(value);
            ipAddressInputField.gameObject.SetActive(value);
            portText.gameObject.SetActive(value);
            portInputField.gameObject.SetActive(value);
            instructionText.gameObject.SetActive(value);
        }
        get
        {
            return ipAddressText.gameObject.activeSelf;
        }
    }

    void Awake()
    {
        InputState = InputState.IpAddress;
        UiVisibility = true;

        gestureRecognizer = new GestureRecognizer();

        kinectReceiver = new KinectReceiver(azureKinectScreenMaterial, azureKinectScreen, floorPlaneTransform);

        // Prepare a GestureRecognizer to recognize taps.
        gestureRecognizer.Tapped += OnTapped;
        gestureRecognizer.StartCapturingGestures();

        statusText.text = "Waiting for user input.";
    }

    void Update()
    {
        // Space key resets the scene to be placed in front of the camera.
        if (Input.GetKeyDown(KeyCode.Space))
        {
            ResetView();
        }

        // Sends virtual keyboards strokes to the TextMeshes for the IP address and the port.
        AbsorbInput();

        kinectReceiver.UpdateFrame();
    }

    void OnDestroy()
    {
        kinectReceiver.Stop();
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        const float AMPLIFIER = 8.0f;

        kinectReceiver.RingBuffer.Read(data);
        for (int i = 0; i < data.Length; ++i)
            data[i] = data[i] * AMPLIFIER;
    }

    private void OnTapped(TappedEventArgs args)
    {
        // Place the scene in front of the camera when the user taps.
        ResetView();
    }

    // Places everything in front of the camera by positing and turning a root transform for
    // everything else except the camera.
    private void ResetView()
    {
        scenceRootTransform.localPosition = cameraTransform.localPosition;
        scenceRootTransform.localRotation = cameraTransform.localRotation;
    }

    // Sends keystrokes of the virtual keyboard to TextMeshes.
    // Try connecting the Receiver to a Sender when the user pressed the enter key.
    private void AbsorbInput()
    {
        if (Input.GetKeyDown(KeyCode.UpArrow) || Input.GetKeyDown(KeyCode.DownArrow) || Input.GetKeyDown(KeyCode.Tab))
        {
            InputState = inputState != InputState.IpAddress ? InputState.IpAddress : InputState.Port;
        }
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
        if (inputState == InputState.IpAddress)
        {
            AbsorbKeyCode(KeyCode.Period, '.');
            AbsorbKeyCode(KeyCode.KeypadPeriod, '.');
        }
        if (Input.GetKeyDown(KeyCode.Backspace))
        {
            var text = ActiveInputField.text;
            if (text.Length > 0)
            {
                ActiveInputField.text = text.Substring(0, text.Length - 1);
            }
        }
        if (Input.GetKeyDown(KeyCode.Return) || Input.GetKeyDown(KeyCode.KeypadEnter) || Input.GetKeyDown("enter"))
        {
            StartCoroutine(Ping());
        }
    }

    // A helper method for AbsorbInput().
    private void AbsorbKeyCode(KeyCode keyCode, char c)
    {
        if (Input.GetKeyDown(keyCode))
        {
            ActiveInputField.text += c;
        }
    }

    // To copy the c++ receiver, for easier development,
    // there should be only one chance to send a ping.
    private IEnumerator Ping()
    {
        if(!UiVisibility)
        {
            print("No more than one ping at a time.");
            yield break;
        }

        UiVisibility = false;

        // The default IP address is 127.0.0.1.
        string ipAddressText = ipAddressInputField.text;
        if (ipAddressText.Length == 0)
            ipAddressText = "127.0.0.1";

        // The default port is 7777.
        string portString = portInputField.text;
        int port = portString.Length != 0 ? int.Parse(portString) : 7777;

        string logString = $"Try connecting to {ipAddressText}:{port}...";
        print(logString);
        statusText.text = logString;

        var ipAddress = IPAddress.Parse(ipAddressText);
        var udpSocket = new UdpSocket(new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp) { ReceiveBufferSize = 1024 * 1024 },
                                      new IPEndPoint(ipAddress, port));

        kinectReceiver.Ping(udpSocket);
    }
}
