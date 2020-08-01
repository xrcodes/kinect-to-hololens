using UnityEngine;

public enum ConnectionTarget
{
    Kinect, Controller
}

public class ConnectionWindow : MonoBehaviour
{
    private static readonly Color BRIGHT_COLOR = new Color(0.0f, 1.0f, 0.254902f);
    private static readonly Color DARK_COLOR = new Color(0.0f, 0.5f, 0.254902f * 0.5f);
    public TextMesh ipAddressInputField;
    public TextMesh kinectText;
    public TextMesh controllerText;
    private ConnectionTarget connectionTarget;

    public bool Visibility
    {
        get
        {
            return gameObject.activeSelf;
        }
        set
        {
            gameObject.SetActive(value);
        }
    }

    public string IpAddress
    {
        get
        {
            return ipAddressInputField.text;
        }
        set
        {
            ipAddressInputField.text = value;
        }
    }

    public ConnectionTarget ConnectionTarget
    {
        get
        {
            return connectionTarget;
        }
        private set
        {
            if (value == ConnectionTarget.Kinect)
            {
                controllerText.color = DARK_COLOR;
                kinectText.color = BRIGHT_COLOR;
            }
            else
            {
                controllerText.color = BRIGHT_COLOR;
                kinectText.color = DARK_COLOR;
            }
            connectionTarget = value;
        }
    }

    void Awake()
    {
        ConnectionTarget = ConnectionTarget.Kinect;
    }

    void Update()
    {
        // Sends virtual keyboards strokes to the TextMeshes for the IP address and the port.
        AbsorbInput();

        if (Input.GetKeyDown(KeyCode.Tab))
        {
            if (ConnectionTarget == ConnectionTarget.Kinect)
                ConnectionTarget = ConnectionTarget.Controller;
            else
                ConnectionTarget = ConnectionTarget.Kinect;
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
            var text = IpAddress;
            if (IpAddress.Length > 0)
            {
                IpAddress = text.Substring(0, text.Length - 1);
            }
        }
    }

    // A helper method for AbsorbInput().
    private void AbsorbKeyCode(KeyCode keyCode, char c)
    {
        if (Input.GetKeyDown(keyCode))
        {
            IpAddress += c;
        }
    }
}
