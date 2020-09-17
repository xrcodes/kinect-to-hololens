using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using UnityEngine;

public class CoroutineRunner : MonoBehaviour
{
    private static CoroutineRunner instance;
    public float totalTimeOutMsPerFrame;
    private List<IEnumerator> totalTimeOutCoroutines;

    void Awake()
    {
        if (instance != null)
            UnityEngine.Debug.LogError("There should be only one CoroutineRunner.");

        instance = this;
        totalTimeOutCoroutines = new List<IEnumerator>();
    }

    void Update()
    {
        var stopWatch = Stopwatch.StartNew();
        foreach (var coroutine in totalTimeOutCoroutines.ToArray())
        {
            while (coroutine.MoveNext())
            {
                if (stopWatch.ElapsedMilliseconds > instance.totalTimeOutMsPerFrame)
                    return;
            }

            totalTimeOutCoroutines.Remove(coroutine);
        }
    }

    public static void Run(IEnumerator coroutine)
    {
        instance.StartCoroutine(coroutine);
    }

    // Our implementation only includes handing back and forth control since it was built only for KinectRenderer.Prepare().
    // In other words, it does nothing different for instances that would do something with StartCoroutine(), such as WaitForSeconds.
    public static void RunWithTotalTimeOut(IEnumerator coroutine)
    {
        print("RunWithTotalTimeOut");

        // Coroutines should at least start, for example, to set state in KinectRenderer.
        if (coroutine.MoveNext())
        {
            instance.totalTimeOutCoroutines.Add(coroutine);
        }
    }
}
