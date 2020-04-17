using UnityEngine;

public enum ConnectionTarget
{
    Controller, Kinect
}

public class ConnectionWindow : MonoBehaviour
{
    private static readonly Color BRIGHT_COLOR = new Color(0.0f, 1.0f, 0.254902f);
    private static readonly Color DARK_COLOR = new Color(0.0f, 0.5f, 0.254902f * 0.5f);
    public TextMesh ipAddressInputField;
    public TextMesh controllerText;
    public TextMesh kinectText;
    private ConnectionTarget connectionTarget;

    public string IpAddressInputText
    {
        get => ipAddressInputField.text;
        set => ipAddressInputField.text = value;
    }

    public ConnectionTarget ConnectionTarget
    {
        get => connectionTarget;
        set
        {
            if (value == ConnectionTarget.Controller)
            {
                controllerText.color = BRIGHT_COLOR;
                kinectText.color = DARK_COLOR;
            }
            else
            {
                controllerText.color = DARK_COLOR;
                kinectText.color = BRIGHT_COLOR;
            }
            connectionTarget = value;
        }
    }
}
