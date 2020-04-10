using System.Collections.Concurrent;
using System.Collections.Generic;
using UnityEngine;

public class AzureKinectRoot : MonoBehaviour
{
    public Transform floorTransformInverterTransform;
    public Transform floorTransform;

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
        Vector3 inversePositionWithOnlyY = new Vector3(0.0f, inversePlaneDistance / inversePlaneNormal.y, 0.0f);

        //floorTransformInverterTransform.localPosition = inversePosition;
        floorTransformInverterTransform.localPosition = inversePositionWithOnlyY;
        floorTransformInverterTransform.localRotation = inverseRotation;

        floorTransform.localPosition = position;
        floorTransform.localRotation = rotation;
    }
}
