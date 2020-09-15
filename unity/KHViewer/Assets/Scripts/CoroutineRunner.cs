using System.Collections;
using UnityEngine;

public class CoroutineRunner : MonoBehaviour
{
    private static CoroutineRunner instance;

    void Awake()
    {
        instance = this;
    }

    public static void Run(IEnumerator coroutine)
    {
        instance.StartCoroutine(coroutine);
    }
}
