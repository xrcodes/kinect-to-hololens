using System;
using System.Collections.Generic;
using System.Net;
using UnityEngine;

public class KinectOrigin : MonoBehaviour
{
    public TextMesh progressText;
    public Transform gimbalTransform;
    public KinectRenderer screen;
    public KinectSpeaker speaker;
    public Transform floorTransform;
    private Queue<Vector3> inversePlaneNormalQueue = new Queue<Vector3>();
    private Queue<float> inversePlaneHeightQueue = new Queue<float>();

    public KinectRenderer Screen => screen;
    public KinectSpeaker Speaker => speaker;

    public bool ProgressTextVisibility
    {
        set
        {
            progressText.gameObject.SetActive(value);
        }
        get
        {
            return progressText.gameObject.activeSelf;
        }
    }

    public bool FloorVisibility
    {
        set
        {
            floorTransform.gameObject.SetActive(value);
        }
        get
        {
            return floorTransform.gameObject.activeSelf;
        }
    }

    public void SetProgressText(IPEndPoint senderEndPoint, float progress)
    {
        progressText.text = $"Preparation for {senderEndPoint}\n{progress * 100.0f:F0}% done.";
    }

    public void UpdateFrame(List<Tuple<int, VideoSenderMessageData>> videoMessageList)
    {
        VideoSenderMessageData videoMessageData = null;
        foreach(var videoMessagePair in videoMessageList)
        {
            if (videoMessagePair.Item2.floor != null)
                videoMessageData = videoMessagePair.Item2;
        }

        if (videoMessageData == null)
            return;

        Vector3 position;
        Quaternion rotation;

        FloorUtils.ConvertFloorFromVideoSenderMessageDataToPositionAndRotation(videoMessageData, out position, out rotation);

        floorTransform.localPosition = position;
        floorTransform.localRotation = rotation;

        Vector3 inversePosition;
        Quaternion inverseRotation;
        FloorUtils.InvertPositionAndRotation(position, rotation, out inversePosition, out inverseRotation);

        Vector3 inversePlaneNormal;
        float inversePlaneHeight;
        FloorUtils.ConvertPositionAndRotationToNormalAndY(inversePosition, inverseRotation, out inversePlaneNormal, out inversePlaneHeight);

        //floorTransformInverterTransform.localPosition = new Vector3(0.0f, inversePositionY, 0.0f);
        //floorTransformInverterTransform.localRotation = inverseRotation;

        inversePlaneNormalQueue.Enqueue(inversePlaneNormal);
        inversePlaneHeightQueue.Enqueue(inversePlaneHeight);

        if (inversePlaneNormalQueue.Count > 30)
            inversePlaneNormalQueue.Dequeue();
        if (inversePlaneHeightQueue.Count > 30)
            inversePlaneHeightQueue.Dequeue();
        
        Vector3 inversePlaneNormalSum = Vector3.zero;
        foreach (var normal in inversePlaneNormalQueue)
            inversePlaneNormalSum += normal;
        float inversePlaneHeightSum = 0.0f;
        foreach (var height in inversePlaneHeightQueue)
            inversePlaneHeightSum += height;

        Vector3 inversePlaneNormalAverage = inversePlaneNormalSum / inversePlaneHeightQueue.Count;
        float inversePlaneHeightAverage = inversePlaneHeightSum / inversePlaneHeightQueue.Count;
        Quaternion inversePlaneNormalAverageRotation = Quaternion.FromToRotation(Vector3.up, inversePlaneNormalAverage);

        //floorTransformInverterTransform.localPosition = new Vector3(0.0f, inversePlaneHeightAverage, 0.0f);
        gimbalTransform.localRotation = inversePlaneNormalAverageRotation;
    }
}
