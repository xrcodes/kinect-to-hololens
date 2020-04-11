using System.Collections.Concurrent;
using System.Collections.Generic;
using UnityEngine;

public class AzureKinectRoot : MonoBehaviour
{
    public Transform floorTransformInverterTransform;
    public Transform floorTransform;
    private List<float> inversePositionYList = new List<float>();
    private List<Vector3> inversePlaneNormalList = new List<Vector3>();

    public void SetRootTransform(Vector3 position, Quaternion rotation)
    {
        transform.localPosition = position;

        Vector3 azureKinectForward = rotation * new Vector3(0.0f, 0.0f, 1.0f);
        float yaw = Mathf.Atan2(azureKinectForward.x, azureKinectForward.z) * Mathf.Rad2Deg;
        transform.localRotation = Quaternion.Euler(0.0f, yaw, 0.0f);
    }

    public void UpdateFrame(ConcurrentQueue<FloorSenderPacketData> floorPacketDataQueue)
    {
        var floorSenderPacketDataList = new List<FloorSenderPacketData>();
        FloorSenderPacketData floorSenderPacketData;
        while (floorPacketDataQueue.TryDequeue(out floorSenderPacketData))
        {
            floorSenderPacketDataList.Add(floorSenderPacketData);
        }

        if (floorSenderPacketDataList.Count == 0)
            return;

        floorSenderPacketData = floorSenderPacketDataList[floorSenderPacketDataList.Count - 1];

        //Vector3 upVector = new Vector3(floorSenderPacketData.a, floorSenderPacketData.b, floorSenderPacketData.c);
        // y component is fliped since the coordinate system of unity and azure kinect is different.
        Plane floorPacketPlane = new Plane(new Vector3(floorSenderPacketData.a, -floorSenderPacketData.b, floorSenderPacketData.c), floorSenderPacketData.d);
        //Vector3 upVector = new Vector3(floorSenderPacketData.a, -floorSenderPacketData.b, floorSenderPacketData.c);
        Vector3 position = floorPacketPlane.normal * floorPacketPlane.distance;
        Quaternion rotation = Quaternion.FromToRotation(Vector3.up, floorPacketPlane.normal);

        Quaternion inverseRotation = Quaternion.Inverse(rotation);
        Vector3 inversePosition = inverseRotation * (-position);
        Vector3 inversePlaneNormal = inverseRotation * Vector3.up;
        float inversePlaneDistance = Vector3.Dot(inversePosition, inversePlaneNormal);
        // In ax + by + cz = d, if x = z = 0, y = d/b.
        float inversePositionY = inversePlaneDistance / inversePlaneNormal.y;

        floorTransform.localPosition = position;
        floorTransform.localRotation = rotation;

        //floorTransformInverterTransform.localPosition = new Vector3(0.0f, inversePositionY, 0.0f);
        //floorTransformInverterTransform.localRotation = inverseRotation;

        inversePositionYList.Add(inversePositionY);
        inversePlaneNormalList.Add(inversePlaneNormal);

        if (inversePositionYList.Count > 30)
            inversePositionYList.RemoveAt(0);
        if (inversePlaneNormalList.Count > 30)
            inversePlaneNormalList.RemoveAt(0);

        float inversePositionYSum = 0.0f;
        foreach (var y in inversePositionYList)
            inversePositionYSum += y;
        Vector3 inversePlaneNormalSum = Vector3.zero;
        foreach (var normal in inversePlaneNormalList)
            inversePlaneNormalSum += normal;

        float inversePositionYAverage = inversePositionYSum / inversePositionYList.Count;
        Vector3 inversePlaneNormalAverage = inversePlaneNormalSum / inversePositionYList.Count;
        Quaternion inversePlaneNormalAverageRotation = Quaternion.FromToRotation(Vector3.up, inversePlaneNormalAverage);

        floorTransformInverterTransform.localPosition = new Vector3(0.0f, inversePositionYAverage, 0.0f);
        floorTransformInverterTransform.localRotation = inversePlaneNormalAverageRotation;
    }
}
