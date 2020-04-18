using UnityEngine;

public class TextToaster : MonoBehaviour
{
    public GameObject toastText;

    public void Toast(string text)
    {
        print("From TextToaster: " + text);
        var instantiatedToastTest = Instantiate(toastText, transform.position, transform.rotation);
        instantiatedToastTest.GetComponent<TextMesh>().text = text;
    }
}
