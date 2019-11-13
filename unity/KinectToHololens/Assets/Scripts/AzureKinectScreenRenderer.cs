using UnityEngine;
using UnityEngine.Rendering;

[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
public class AzureKinectScreenRenderer : MonoBehaviour
{
    public MeshFilter meshFilter;
    public MeshRenderer meshRenderer;

    public void SetScreen(AzureKinectScreen screen)
    {
        meshFilter.mesh = CreateMesh(screen);
    }

    private static Mesh CreateMesh(AzureKinectScreen screen)
    {
        const int AZURE_KINECT_DEPTH_WIDTH = 640;
        const int AZURE_KINECT_DEPTH_HEIGHT = 576;

        var vertices = new Vector3[AZURE_KINECT_DEPTH_WIDTH * AZURE_KINECT_DEPTH_HEIGHT];
        var uv = new Vector2[AZURE_KINECT_DEPTH_WIDTH * AZURE_KINECT_DEPTH_HEIGHT];
        for(int i = 0; i < AZURE_KINECT_DEPTH_WIDTH; ++i)
        {
            for(int j = 0; j < AZURE_KINECT_DEPTH_HEIGHT; ++j)
            {
                vertices[i + j * AZURE_KINECT_DEPTH_WIDTH] = new Vector3(i / (float)AZURE_KINECT_DEPTH_WIDTH, j / (float)AZURE_KINECT_DEPTH_HEIGHT, 0.0f);
                uv[i + j * AZURE_KINECT_DEPTH_WIDTH] = new Vector2(i / (float)AZURE_KINECT_DEPTH_WIDTH, j / (float)AZURE_KINECT_DEPTH_HEIGHT);
            }
        }

        var triangles = new int[vertices.Length];
        for (int i = 0; i < triangles.Length; ++i)
            triangles[i] = i;

        // Without the bounds, Unity decides whether to render this mesh or not based on the vertices calculated here.
        // This causes Unity not rendering the mesh transformed by the depth texture even when the transformed one
        // belongs to the viewport of the camera.
        var bounds = new Bounds(Vector3.zero, Vector3.one * 1000.0f);

        var mesh = new Mesh()
        {
            indexFormat = IndexFormat.UInt32,
            vertices = vertices,
            uv = uv,
            triangles = triangles,
            bounds = bounds,
        };
        mesh.SetIndices(triangles, MeshTopology.Points, 0);

        return mesh;
    }
}