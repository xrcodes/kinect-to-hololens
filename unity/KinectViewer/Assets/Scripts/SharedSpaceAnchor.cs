using System.Collections.Generic;
using UnityEngine;

public class SharedSpaceAnchor : MonoBehaviour
{
    public KinectOrigin kinectOriginPrefab;
    public GameObject kinectModel;
    
    public List<KinectOrigin> KinectOrigins { get; private set; }

    public bool DebugVisibility
    {
        set
        {
            foreach (var kinectOrigin in KinectOrigins)
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
        KinectOrigins = new List<KinectOrigin>();
    }

    public KinectOrigin AddKinectOrigin()
    {
        var kinectOrigin = Instantiate(kinectOriginPrefab, transform);
        kinectOrigin.transform.localRotation = Quaternion.Euler(0.0f, 180.0f, 0.0f);
        kinectOrigin.FloorVisibility = DebugVisibility;
        KinectOrigins.Add(kinectOrigin);

        return kinectOrigin;
    }

    public void RemoteKinectOrigin(KinectOrigin kinectOrigin)
    {
        KinectOrigins.Remove(kinectOrigin);
        Destroy(kinectOrigin);
    }

    public void UpdateTransform(Vector3 position, Quaternion rotation)
    {
        transform.localPosition = position;

        Vector3 forward = rotation * new Vector3(0.0f, 0.0f, 1.0f);
        float yaw = Mathf.Atan2(forward.x, forward.z) * Mathf.Rad2Deg;
        transform.localRotation = Quaternion.Euler(0.0f, yaw, 0.0f);
    }
}
