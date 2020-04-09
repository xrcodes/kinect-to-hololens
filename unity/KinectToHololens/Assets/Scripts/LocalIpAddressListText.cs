using System.Net;
using System.Net.Sockets;
using UnityEngine;

public class LocalIpAddressListText : MonoBehaviour
{
    public TextMesh textMesh;

    void Start()
    {
        var text = "Local IP Addresses:\n";
        var host = Dns.GetHostEntry(Dns.GetHostName());
        foreach (var ipAddress in host.AddressList)
        {
            print($"ipAddress: {ipAddress}");
            if (ipAddress.AddressFamily == AddressFamily.InterNetwork)
            {
                text += $"  - {ipAddress}\n";
            }
        }
        textMesh.text = text;
    }
}
