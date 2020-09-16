using System.Collections.Generic;
using UnityEngine;

public class SharedSpaceAnchor : MonoBehaviour
{
    public KinectOrigin kinectOriginPrefab;
    public GameObject kinectModel;
    private Dictionary<int, KinectOrigin> kinectOrigins;

    public bool GizmoVisibility
    {
        set
        {
            foreach (var kinectOrigin in kinectOrigins.Values)
                kinectOrigin.FloorVisibility = value;

            kinectModel.SetActive(value);
        }
        get
        {
            return kinectModel.activeSelf;
        }
    }

    void Awake()
    {
        kinectOrigins = new Dictionary<int, KinectOrigin>();
    }

    public KinectOrigin AddKinectOrigin(int receiverId)
    {
        var kinectOrigin = Instantiate(kinectOriginPrefab, transform);
        kinectOrigin.FloorVisibility = GizmoVisibility;
        kinectOrigins.Add(receiverId, kinectOrigin);

        return kinectOrigin;
    }

    public KinectOrigin GetKinectOrigin(int receiverId)
    {
        if (!kinectOrigins.TryGetValue(receiverId, out KinectOrigin kinectOrigin))
            return null;

        return kinectOrigin;
    }

    public void RemoveKinectOrigin(int receiverId)
    {
        var kinectOrigin = kinectOrigins[receiverId];
        kinectOrigins.Remove(receiverId);
        Destroy(kinectOrigin.gameObject);
    }

    // Rotation of the anchor does not directly gets set to the input camera rotation.
    // It rotates in a way that the virtual floor in Unity can match the real floor.
    // Adds 180 degrees to yaw since headset will face the opposite direction of the direction of the anchor.
    public void SetPositionAndRotation(Vector3 headsetPosition, Quaternion headsetRotation)
    {
        transform.localPosition = headsetPosition;

        Vector3 forward = headsetRotation * new Vector3(0.0f, 0.0f, 1.0f);
        float yaw = Mathf.Atan2(forward.x, forward.z) * Mathf.Rad2Deg;
        yaw += 180.0f;
        transform.localRotation = Quaternion.Euler(0.0f, yaw, 0.0f);
    }
}
