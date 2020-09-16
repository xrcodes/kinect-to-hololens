using System.Collections.Generic;
using UnityEngine;

public class FpsCounter : MonoBehaviour
{
    private static FpsCounter instance;
    public float timeInterval;
    private List<float> timeStamps = new List<float>();

    void Awake()
    {
        if (instance != null)
            Debug.LogError("There should be only one FpsCounter.");

        instance = this;
    }

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
    }

    public static void Toast()
    {
        TextToaster.Toast($"FPS: {instance.timeStamps.Count / instance.timeInterval}");
    }
}
