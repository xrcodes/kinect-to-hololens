using UnityEngine;

public class ConnectionWindow : MonoBehaviour
{
    public TextMesh ipAddressText;
    public TextMesh ipAddressInputField;
    public TextMesh instructionText;
    public TextMesh localIpAddressListText;

    public string IpAddressInputText
    {
        get => ipAddressInputField.text;
        set => ipAddressInputField.text = value;
    }

    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
