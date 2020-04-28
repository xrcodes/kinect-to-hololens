using System.Collections.Generic;
using UnityEngine;

public class KinectOrigin : MonoBehaviour
{
    public Transform gimbalTransform;
    public Transform offsetTransform;
    public KinectScreen screen;
    public KinectSpeaker speaker;
    public Transform floorTransform;
    public GameObject arrow;
    private float offsetDistance = 0.0f;
    private float offsetHeight = 0.0f;
    private Queue<Vector3> inversePlaneNormalQueue = new Queue<Vector3>();
    private Queue<float> inversePlaneHeightQueue = new Queue<float>();

    public KinectScreen Screen => screen;
    public KinectSpeaker Speaker => speaker;

    public bool DebugVisibility
    {
        set
        {
            floorTransform.gameObject.SetActive(value);
            arrow.SetActive(value);
        }
        get
        {
            return floorTransform.gameObject.activeSelf;
        }
    }

    public float OffsetDistance
    {
        set
        {
            var position = offsetTransform.localPosition;
            offsetTransform.localPosition = new Vector3(0.0f, position.y, value);
            offsetDistance = value;
        }
        get
        {
            return offsetDistance;
        }
    }

    public float OffsetHeight
    {
        set
        {
            var position = offsetTransform.localPosition;
            offsetTransform.localPosition = new Vector3(0.0f, value, position.z);
            offsetHeight = value;
        }
        get
        {
            return offsetHeight;
        }
    }

    public void SetRootTransform(Vector3 position, Quaternion rotation)
    {
        transform.localPosition = position;

        // Using (0,0,-1) instead of (0,0,1) to let the hololens to avoid facing walls.
        Vector3 azureKinectForward = rotation * new Vector3(0.0f, 0.0f, -1.0f);
        float yaw = Mathf.Atan2(azureKinectForward.x, azureKinectForward.z) * Mathf.Rad2Deg;
        transform.localRotation = Quaternion.Euler(0.0f, yaw, 0.0f);
    }

    public void UpdateFrame(List<FloorSenderPacketData> floorPacketDataList)
    {
        if (floorPacketDataList.Count == 0)
            return;

        FloorSenderPacketData floorSenderPacketData = floorPacketDataList[floorPacketDataList.Count - 1];

        Vector3 position;
        Quaternion rotation;

        FloorUtils.ConvertFloorSenderPacketDataToPositionAndRotation(floorSenderPacketData, out position, out rotation);

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
