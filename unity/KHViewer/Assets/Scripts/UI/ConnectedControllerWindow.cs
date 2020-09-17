using UnityEngine;

public class ConnectedControllerWindow : MonoBehaviour
{
    public TextMesh ipAddressText;
    public TextMesh viewerIdText;

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
        set
        {
            ipAddressText.text = $"Controller: {value}";
        }
    }

    public string ViewerId
    {
        set
        {
            viewerIdText.text = $"Viewer ID: {value}";
        }
    }
}
