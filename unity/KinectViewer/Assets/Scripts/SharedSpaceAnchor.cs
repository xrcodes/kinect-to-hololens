using UnityEngine;

public class SharedSpaceAnchor : MonoBehaviour
{
    public KinectOrigin kinectOriginPrefab;
    public GameObject kinectModel;
    private KinectOrigin kinectOrigin;

    public KinectOrigin KinectOrigin => kinectOrigin;

    public bool DebugVisibility
    {
        set
        {
            if(kinectOrigin)
                kinectOrigin.FloorVisibility = value;
            kinectModel.SetActive(value);
        }
        get
        {
            return kinectModel.activeSelf;
        }
    }

    public KinectOrigin AddKinectOrigin()
    {
        kinectOrigin = Instantiate(kinectOriginPrefab, transform);
        kinectOrigin.transform.localRotation = Quaternion.Euler(0.0f, 180.0f, 0.0f);
        kinectOrigin.FloorVisibility = DebugVisibility;

        return kinectOrigin;
    }

    public void UpdateTransform(Vector3 position, Quaternion rotation)
    {
        transform.localPosition = position;

        Vector3 forward = rotation * new Vector3(0.0f, 0.0f, 1.0f);
        float yaw = Mathf.Atan2(forward.x, forward.z) * Mathf.Rad2Deg;
        transform.localRotation = Quaternion.Euler(0.0f, yaw, 0.0f);
    }
}
