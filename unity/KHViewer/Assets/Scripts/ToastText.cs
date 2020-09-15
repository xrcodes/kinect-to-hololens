using System.Diagnostics;
using UnityEngine;

public class ToastText : MonoBehaviour
{
    private const int DURATION_MS = 5000;
    public TextMesh textMesh;
    private Stopwatch stopwatch;
    private Color textColor;

    void Start()
    {
        stopwatch = Stopwatch.StartNew();
        textColor = textMesh.color;
    }

    // Update is called once per frame
    void Update()
    {
        if (stopwatch.ElapsedMilliseconds > DURATION_MS / 2)
        {
            float ratio = (float) stopwatch.ElapsedMilliseconds / DURATION_MS;
            ratio = ratio * 2.0f - 1.0f;
            textMesh.color = Color.Lerp(textColor, Color.clear, ratio);
        }

        if (stopwatch.ElapsedMilliseconds > DURATION_MS)
            Destroy(gameObject);
    }
}
