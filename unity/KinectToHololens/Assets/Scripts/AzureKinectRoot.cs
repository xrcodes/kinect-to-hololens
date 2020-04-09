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
        Vector3 upVector = new Vector3(floorSenderPacketData.a, -floorSenderPacketData.b, floorSenderPacketData.c);
        Vector3 position = upVector * floorSenderPacketData.d;
        Quaternion rotation = Quaternion.FromToRotation(Vector3.up, upVector);
        print($"position: {position}");

        Quaternion inverseRotation = Quaternion.Inverse(rotation);
        Vector3 inversePosition = inverseRotation * (-position);
        // TODO: Make inversePosition to have zero x and y.
        floorTransformInverterTransform.localPosition = inversePosition;
        floorTransformInverterTransform.localRotation = inverseRotation;

        floorTransform.localPosition = position;
        floorTransform.localRotation = rotation;
    }
}
