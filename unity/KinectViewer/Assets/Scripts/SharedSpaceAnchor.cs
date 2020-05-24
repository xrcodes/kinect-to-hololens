using System.Collections.Generic;
using UnityEngine;

public class SharedSpaceAnchor : MonoBehaviour
{
    public KinectOrigin kinectOriginPrefab;
    public GameObject kinectModel;
    private List<KinectOrigin> kinectOrigins;

    public bool GizmoVisibility
    {
        set
        {
            foreach (var kinectOrigin in kinectOrigins)
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
        kinectOrigins = new List<KinectOrigin>();
    }

    public KinectOrigin AddKinectOrigin()
    {
        var kinectOrigin = Instantiate(kinectOriginPrefab, transform);
        kinectOrigin.transform.localRotation = Quaternion.Euler(0.0f, 180.0f, 0.0f);
        kinectOrigin.FloorVisibility = GizmoVisibility;
        kinectOrigins.Add(kinectOrigin);

        return kinectOrigin;
    }

    public void RemoveKinectOrigin(KinectOrigin kinectOrigin)
    {
        kinectOrigins.Remove(kinectOrigin);
        Destroy(kinectOrigin.gameObject);
    }

    // Rotation of the anchor does not directly gets set to the input camera rotation.
    // It rotates in a way that the virtual floor in Unity can match the real floor.
    public void SetPositionAndRotation(Vector3 position, Quaternion rotation)
    {
        transform.localPosition = position;

        Vector3 forward = rotation * new Vector3(0.0f, 0.0f, 1.0f);
        float yaw = Mathf.Atan2(forward.x, forward.z) * Mathf.Rad2Deg;
        transform.localRotation = Quaternion.Euler(0.0f, yaw, 0.0f);
    }
}
