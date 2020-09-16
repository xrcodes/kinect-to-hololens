using UnityEngine;

public class TextToaster : MonoBehaviour
{
    private static TextToaster instance;
    public GameObject toastText;

    void Awake()
    {
        if(instance != null)
            Debug.LogError("There should be only one TextToaster.");

        instance = this;
    }

    public static void Toast(string text)
    {
        print("From TextToaster: " + text);
        var instantiatedToastTest = Instantiate(instance.toastText, instance.transform.position, instance.transform.rotation);
        instantiatedToastTest.GetComponent<TextMesh>().text = text;
    }
}
