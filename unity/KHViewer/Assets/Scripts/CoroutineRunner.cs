using System.Collections;
using UnityEngine;

public class CoroutineRunner : MonoBehaviour
{
    private static CoroutineRunner instance;

    void Awake()
    {
        if (instance != null)
            Debug.LogError("There should be only one CoroutineRunner.");

        instance = this;
    }

    public static void Run(IEnumerator coroutine)
    {
        instance.StartCoroutine(coroutine);
    }
}
