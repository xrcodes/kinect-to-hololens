using System.Collections.Generic;
using UnityEngine;

public class SharedSpaceScene : MonoBehaviour
{
    public KinectNode kinectNodePrefab;
    public GameObject kinectModel;
    private Dictionary<int, KinectNode> kinectNodes;

    public bool GizmoVisibility
    {
        set
        {
            foreach (var kinectOrigin in kinectNodes.Values)
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
        kinectNodes = new Dictionary<int, KinectNode>();
    }

    public KinectNode AddKinectNode(int receiverId)
    {
        var kinectNodes = Instantiate(kinectNodePrefab, transform);
        kinectNodes.FloorVisibility = GizmoVisibility;
        this.kinectNodes.Add(receiverId, kinectNodes);

        return kinectNodes;
    }

    public KinectNode GetKinectNode(int receiverId)
    {
        if (!kinectNodes.TryGetValue(receiverId, out KinectNode kinectNode))
            return null;

        return kinectNode;
    }

    public void RemoveKinectNode(int receiverId)
    {
        var kinectNode = kinectNodes[receiverId];
        kinectNodes.Remove(receiverId);
        Destroy(kinectNode.gameObject);
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
