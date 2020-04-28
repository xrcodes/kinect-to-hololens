using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public static class FloorUtils
{
    public static void ConvertFloorSenderPacketDataToPositionAndRotation(FloorSenderPacketData floorSenderPacketData, out Vector3 position, out Quaternion rotation)
    {
        //Vector3 upVector = new Vector3(floorSenderPacketData.a, floorSenderPacketData.b, floorSenderPacketData.c);
        // y component is fliped since the coordinate system of unity and azure kinect is different.
        Plane floorPacketPlane = new Plane(new Vector3(floorSenderPacketData.a, -floorSenderPacketData.b, floorSenderPacketData.c), floorSenderPacketData.d);
        //Vector3 upVector = new Vector3(floorSenderPacketData.a, -floorSenderPacketData.b, floorSenderPacketData.c);
        position = floorPacketPlane.normal * floorPacketPlane.distance;
        rotation = Quaternion.FromToRotation(Vector3.up, floorPacketPlane.normal);
    }

    public static void InvertPositionAndRotation(Vector3 position, Quaternion rotation, out Vector3 inversePosition, out Quaternion inverseRotation)
    {
        inverseRotation = Quaternion.Inverse(rotation);
        inversePosition = inverseRotation * (-position);
    }

    public static void ConvertPositionAndRotationToNormalAndY(Vector3 position, Quaternion rotation, out Vector3 normal, out float height)
    {
        normal = rotation * Vector3.up;
        float distance = Vector3.Dot(position, normal);
        // In ax + by + cz = d, if x = z = 0, y = d/b.
        height = distance / normal.y;
    }

    // TODO: Implant this code into Controller in the right way.
    //public void UpdateFrame(List<FloorSenderPacketData> floorPacketDataList)
    //{
    //    if (floorPacketDataList.Count == 0)
    //        return;

    //    FloorSenderPacketData floorSenderPacketData = floorPacketDataList[floorPacketDataList.Count - 1];

    //    Vector3 position;
    //    Quaternion rotation;

    //    FloorUtils.ConvertFloorSenderPacketDataToPositionAndRotation(floorSenderPacketData, out position, out rotation);

    //    floorTransform.localPosition = position;
    //    floorTransform.localRotation = rotation;

    //    Vector3 inversePosition;
    //    Quaternion inverseRotation;
    //    FloorUtils.InvertPositionAndRotation(position, rotation, out inversePosition, out inverseRotation);

    //    Vector3 inversePlaneNormal;
    //    float inversePlaneHeight;
    //    FloorUtils.ConvertPositionAndRotationToNormalAndY(inversePosition, inverseRotation, out inversePlaneNormal, out inversePlaneHeight);

    //    //floorTransformInverterTransform.localPosition = new Vector3(0.0f, inversePositionY, 0.0f);
    //    //floorTransformInverterTransform.localRotation = inverseRotation;

    //    inversePlaneNormalQueue.Enqueue(inversePlaneNormal);
    //    inversePlaneHeightQueue.Enqueue(inversePlaneHeight);

    //    if (inversePlaneNormalQueue.Count > 30)
    //        inversePlaneNormalQueue.Dequeue();
    //    if (inversePlaneHeightQueue.Count > 30)
    //        inversePlaneHeightQueue.Dequeue();

    //    Vector3 inversePlaneNormalSum = Vector3.zero;
    //    foreach (var normal in inversePlaneNormalQueue)
    //        inversePlaneNormalSum += normal;
    //    float inversePlaneHeightSum = 0.0f;
    //    foreach (var height in inversePlaneHeightQueue)
    //        inversePlaneHeightSum += height;

    //    Vector3 inversePlaneNormalAverage = inversePlaneNormalSum / inversePlaneHeightQueue.Count;
    //    float inversePlaneHeightAverage = inversePlaneHeightSum / inversePlaneHeightQueue.Count;
    //    Quaternion inversePlaneNormalAverageRotation = Quaternion.FromToRotation(Vector3.up, inversePlaneNormalAverage);

    //    floorTransformInverterTransform.localPosition = new Vector3(0.0f, inversePlaneHeightAverage, 0.0f);
    //    floorTransformInverterTransform.localRotation = inversePlaneNormalAverageRotation;
    //}
}
