using System.Collections.Generic;
using UnityEngine;

public class FpsCounter : MonoBehaviour
{
    public float timeInterval;
    private List<float> timeStamps = new List<float>();

    void Update()
    {
        float time = Time.time;
        var timeStamps = new List<float>();

        foreach (var t in this.timeStamps)
        {
            if ((time - t) < timeInterval)
                timeStamps.Add(t);
        }

        timeStamps.Add(time);

        this.timeStamps = timeStamps;

        if (Input.GetKeyDown(KeyCode.F))
        {
            TextToaster.Toast($"FPS: {this.timeStamps.Count / timeInterval}");
        }
    }
}
