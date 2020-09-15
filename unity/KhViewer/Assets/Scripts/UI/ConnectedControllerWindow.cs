using UnityEngine;

public class ConnectedControllerWindow : MonoBehaviour
{
    public TextMesh ipAddressText;
    public TextMesh userIdText;

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

    public string UserId
    {
        set
        {
            userIdText.text = $"User ID: {value}";
        }
    }
}
