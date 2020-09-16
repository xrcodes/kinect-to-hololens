using UnityEngine;

public static class FloorUtils
{
    public static void ConvertFloorFromVideoSenderMessageDataToPositionAndRotation(VideoSenderMessage videoSenderMessageData, out Vector3 position, out Quaternion rotation)
    {
        //Vector3 upVector = new Vector3(floorSenderPacketData.a, floorSenderPacketData.b, floorSenderPacketData.c);
        // y component is fliped since the coordinate system of unity and azure kinect is different.
        Plane floorPacketPlane = new Plane(new Vector3(videoSenderMessageData.floor[0], -videoSenderMessageData.floor[1], videoSenderMessageData.floor[2]), videoSenderMessageData.floor[3]);
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
}
