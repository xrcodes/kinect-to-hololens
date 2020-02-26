using UnityEngine;
using UnityEngine.Rendering;

[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
public class AzureKinectScreen : MonoBehaviour
{
    public Camera mainCamera;
    public MeshFilter meshFilter;
    public MeshRenderer meshRenderer;

    public void Setup(InitSenderPacketData initSenderPacketData)
    {
        meshFilter.mesh = CreateMesh(initSenderPacketData);
        //meshFilter.mesh = CreateGeometryMesh(calibration);
    }

    // Updates _VertexOffsetXVector and _VertexOffsetYVector so the rendered quads can face the headsetCamera.
    // This method gets called right before this ScreenRenderer gets rendered.
    void OnWillRenderObject()
    {
        if (meshRenderer.sharedMaterial == null)
            return;

        // Ignore when this method is called while Unity rendering the Editor's "Scene" (not the "Game" part of the editor).
        if (Camera.current != mainCamera)
            return;

        var cameraTransform = mainCamera.transform;
        var worldCameraFrontVector = cameraTransform.TransformDirection(new Vector3(0.0f, 0.0f, 1.0f));

        // Using the y direction as the up vector instead the up vector of the camera allows the user to feel more
        // comfortable as it preserves the sense of gravity.
        // Getting the right vector directly from the camera transform through zeroing its y-component does not
        // work when the y-component of the camera's up vector is negative. While it is possible to solve the problem
        // with an if statement, inverting when the y-component is negative, I decided to detour this case with
        // usage of the cross product with the front vector.
        var worldUpVector = new Vector3(0.0f, 1.0f, 0.0f);
        var worldRightVector = Vector3.Cross(worldUpVector, worldCameraFrontVector);
        worldRightVector = new Vector3(worldRightVector.x, 0.0f, worldRightVector.z);
        worldRightVector.Normalize();

        var localRightVector = transform.InverseTransformDirection(worldRightVector);
        var localUpVector = transform.InverseTransformDirection(worldUpVector);

        // The coordinate system of Kinect's textures have (1) its origin at its left-up side.
        // Also, the viewpoint of it is sort of (2) the opposite of the viewpoint of the Hololens, considering the typical use case. (this is not the case for Azure Kinect)
        // Due to (1), vertexOffsetYVector = -localCameraUpVector.
        // Due to (2), vertexOffsetXVector = -localCameraRightVector.
        //var vertexOffsetXVector = -localRightVector;
        var vertexOffsetXVector = localRightVector;
        var vertexOffsetYVector = -localUpVector;

        meshRenderer.sharedMaterial.SetVector("_VertexOffsetXVector", new Vector4(vertexOffsetXVector.x, vertexOffsetXVector.y, vertexOffsetXVector.z, 0.0f));
        meshRenderer.sharedMaterial.SetVector("_VertexOffsetYVector", new Vector4(vertexOffsetYVector.x, vertexOffsetYVector.y, vertexOffsetYVector.z, 0.0f));
    }

    //private static Mesh CreateMesh(AzureKinectCalibration calibration)
    private static Mesh CreateMesh(InitSenderPacketData initSenderPacketData)
    {
        int width = initSenderPacketData.depthWidth;
        int height = initSenderPacketData.depthHeight;

        //var depthCamera = calibration.DepthCamera;

        var vertices = new Vector3[width * height];
        var uv = new Vector2[width * height];

        for (int i = 0; i < width; ++i)
        {
            for (int j = 0; j < height; ++j)
            {
                float[] xy = new float[2];
                int valid = 0;
                if (AzureKinectIntrinsicTransformation.Unproject(initSenderPacketData.depthIntrinsics,
                                                                 initSenderPacketData.depthMetricRadius,
                                                                 new float[2] { i, j }, ref xy, ref valid))
                {
                    vertices[i + j * width] = new Vector3(xy[0], xy[1], 1.0f);
                }
                else
                {
                    vertices[i + j * width] = new Vector3(0.0f, 0.0f, 0.0f);
                }
                uv[i + j * width] = new Vector2(i / (float)(width - 1), j / (float)(height - 1));
            }
        }

        int quadWidth = width - 2;
        int quadHeight = height - 2;
        var quadVertices = new Vector3[quadWidth * quadHeight * 4];
        var quadUv = new Vector2[quadWidth * quadHeight * 4];

        for (int ii = 0; ii < quadWidth; ++ii)
        {
            for (int jj = 0; jj < quadHeight; ++jj)
            {
                int quadIndex = ii + jj * quadWidth;
                int i = ii + 1;
                int j = jj + 1;
                quadVertices[quadIndex * 4 + 0] = (vertices[i + j * width] + vertices[(i - 1) + (j - 1) * width]) * 0.5f;
                quadVertices[quadIndex * 4 + 1] = (vertices[i + j * width] + vertices[(i + 1) + (j - 1) * width]) * 0.5f;
                quadVertices[quadIndex * 4 + 2] = (vertices[i + j * width] + vertices[(i - 1) + (j + 1) * width]) * 0.5f;
                quadVertices[quadIndex * 4 + 3] = (vertices[i + j * width] + vertices[(i + 1) + (j + 1) * width]) * 0.5f;

                quadUv[quadIndex * 4 + 0] = uv[i + j * width];
                quadUv[quadIndex * 4 + 1] = uv[i + j * width];
                quadUv[quadIndex * 4 + 2] = uv[i + j * width];
                quadUv[quadIndex * 4 + 3] = uv[i + j * width];
            }
        }

        var triangles = new int[quadWidth * quadHeight * 6];
        for (int i = 0; i < quadWidth * quadHeight; ++i)
        {
            triangles[i * 6 + 0] = i * 4 + 0;
            triangles[i * 6 + 1] = i * 4 + 1;
            triangles[i * 6 + 2] = i * 4 + 2;
            triangles[i * 6 + 3] = i * 4 + 1;
            triangles[i * 6 + 4] = i * 4 + 3;
            triangles[i * 6 + 5] = i * 4 + 2;
        }

        // Without the bounds, Unity decides whether to render this mesh or not based on the vertices calculated here.
        // This causes Unity not rendering the mesh transformed by the depth texture even when the transformed one
        // belongs to the viewport of the camera.
        var bounds = new Bounds(Vector3.zero, Vector3.one * 1000.0f);

        var mesh = new Mesh()
        {
            indexFormat = IndexFormat.UInt32,
            vertices = quadVertices,
            uv = quadUv,
            bounds = bounds,
        };
        mesh.SetIndices(triangles, MeshTopology.Triangles, 0);

        return mesh;
    }

    private static Mesh CreateGeometryMesh(InitSenderPacketData initSenderPacketData)
    {
        int width = initSenderPacketData.depthWidth;
        int height = initSenderPacketData.depthHeight;

        var vertices = new Vector3[width * height];
        var uv = new Vector2[width * height];

        for (int i = 0; i < width; ++i)
        {
            for (int j = 0; j < height; ++j)
            {
                float[] xy = new float[2];
                int valid = 0;
                if (AzureKinectIntrinsicTransformation.Unproject(initSenderPacketData.depthIntrinsics,
                                                                 initSenderPacketData.depthMetricRadius,
                                                                 new float[2] { i, j }, ref xy, ref valid))
                {
                    vertices[i + j * width] = new Vector3(xy[0], xy[1], 1.0f);
                }
                else
                {
                    vertices[i + j * width] = new Vector3(0.0f, 0.0f, 0.0f);
                }
                uv[i + j * width] = new Vector2(i / (float)(width - 1), j / (float)(height - 1));
            }
        }

        // Converting the point cloud version of vertices and uv into a quad version one.
        int quadWidth = width - 2;
        int quadHeight = height - 2;
        var quadPositions = new Vector3[quadWidth * quadHeight];
        var quadUv = new Vector2[quadWidth * quadHeight];
        var quadPositionSizes = new Vector2[quadWidth * quadHeight];

        for (int ii = 0; ii < quadWidth; ++ii)
        {
            for (int jj = 0; jj < quadHeight - 2; ++jj)
            {
                int i = ii + 1;
                int j = jj + 1;
                quadPositions[ii + jj * quadWidth] = vertices[i + j * width];
                quadUv[ii + jj * quadWidth] = uv[i + j * width];
                quadPositionSizes[ii + jj * quadWidth] = (vertices[(i + 1) + (j + 1) * width] - vertices[(i - 1) + (j - 1) * width]) * 0.5f;
            }
        }

        var triangles = new int[quadPositions.Length];
        for (int i = 0; i < triangles.Length; ++i)
            triangles[i] = i;

        // Without the bounds, Unity decides whether to render this mesh or not based on the vertices calculated here.
        // This causes Unity not rendering the mesh transformed by the depth texture even when the transformed one
        // belongs to the viewport of the camera.
        var bounds = new Bounds(Vector3.zero, Vector3.one * 1000.0f);

        var mesh = new Mesh()
        {
            indexFormat = IndexFormat.UInt32,
            vertices = quadPositions,
            uv = quadUv,
            uv2 = quadPositionSizes,
            bounds = bounds,
        };
        mesh.SetIndices(triangles, MeshTopology.Points, 0);

        return mesh;
    }
}