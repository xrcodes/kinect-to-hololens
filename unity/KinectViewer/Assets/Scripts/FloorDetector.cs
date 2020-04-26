using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR;

public class FloorDetector
{
    private XRMeshSubsystem xrMeshSubsystem;
    private Dictionary<MeshId, Mesh> meshes;

    public FloorDetector(MeshCollider testMeshCollider)
    {
        meshes = new Dictionary<MeshId, Mesh>();

        var descriptors = new List<XRMeshSubsystemDescriptor>();
        SubsystemManager.GetSubsystemDescriptors(descriptors);
        
        Debug.Log($"descriptors.Count: {descriptors.Count}");
        // Find one that supports boundary vertices
        foreach (var descriptor in descriptors)
        {
            Debug.Log($"descriptor.id: {descriptor.id}");
            xrMeshSubsystem = descriptor.Create();

        }

        if(xrMeshSubsystem == null)
        {
            Debug.Log("No xrMeshSubsystem. Maybe Unity Editor.");
            return;
        }

        if(!xrMeshSubsystem.running)
        {
            Debug.Log("No running xrMeshSubsystem. Maybe a Windows MR headset not HoloLens.");
        }

        Debug.Log($"xrMeshSubsystem.running: {xrMeshSubsystem.running}");

        xrMeshSubsystem.Start();
        xrMeshSubsystem.SetBoundingVolume(Vector3.zero, new Vector3(3.0f, 3.0f, 3.0f));

        var meshInfos = new List<MeshInfo>();
        bool meshInfosResult = xrMeshSubsystem.TryGetMeshInfos(meshInfos);
        Debug.Log($"meshInfosResult: {meshInfosResult}");
        Debug.Log($"meshInfos.Count: {meshInfos.Count}");

        foreach (var meshInfo in meshInfos)
        {
            Debug.Log($"meshInfo.MeshId: {meshInfo.MeshId}");
            Debug.Log($"meshInfo.ChangeState: {meshInfo.ChangeState}");
            Mesh mesh = new Mesh();
            meshes.Add(meshInfo.MeshId, mesh);
            Debug.Log("with mesh collider");
            xrMeshSubsystem.GenerateMeshAsync(meshInfo.MeshId, mesh, testMeshCollider, MeshVertexAttributes.None, OnMeshGenerationComplete);
        }
    }

    public void OnMeshGenerationComplete(MeshGenerationResult result)
    {
        Debug.Log($"result.MeshId: {result.MeshId}");
        Debug.Log($"result.Mesh.vertexCount: {result.Mesh.vertexCount}");
    }
}
