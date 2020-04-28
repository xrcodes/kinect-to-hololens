using System.Collections;
using System.Diagnostics;
using UnityEngine;
using UnityEngine.Rendering;

[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
public class KinectScreen : MonoBehaviour
{
    public MeshFilter meshFilter;
    public MeshRenderer meshRenderer;

    public Material Material => meshRenderer.sharedMaterial;

    void Awake()
    {
        RenderPipelineManager.beginCameraRendering += OnBeginCameraRendering;
    }

    void OnDestroy()
    {
        RenderPipelineManager.beginCameraRendering -= OnBeginCameraRendering;
    }

    void OnBeginCameraRendering(ScriptableRenderContext context, Camera camera)
    {
        // Ignore when it is called from Unity's "Scene" (not "Game").
        if (camera.cameraType == CameraType.SceneView)
            return;

        // Using _ModelMatrix since UNITY_MATRIX_M is not working inside the geometry shader.
        // Seems like it is an identity matrix, especially for the universal rendering pipeline.
        // If Unity fixes this bug, use UNITY_MATRIX_M instead of _ModelMatrix.
        meshRenderer.sharedMaterial.SetMatrix("_ModelMatrix", transform.localToWorldMatrix);

        var cameraTransform = camera.transform;
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

    // Since calculation including Unproject() takes too much time,
    // this function is made to run as a coroutine that takes a break
    // every 100 ms.
    public IEnumerator SetupMesh(InitSenderPacketData initSenderPacketData)
    {
        int width = initSenderPacketData.depthWidth;
        int height = initSenderPacketData.depthHeight;

        var vertices = new Vector3[width * height];
        var uv = new Vector2[width * height];

        var stopWatch = Stopwatch.StartNew();
        for (int i = 0; i < width; ++i)
        {
            for (int j = 0; j < height; ++j)
            {
                float[] xy = new float[2];
                int valid = 0;
                if (KinectIntrinsicTransformation.Unproject(initSenderPacketData.depthIntrinsics,
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

            if(stopWatch.ElapsedMilliseconds > 100)
            {
                yield return null;
                stopWatch = Stopwatch.StartNew();
            }
        }

        int quadWidth = width - 2;
        int quadHeight = height - 2;
        var quadVertices = new Vector3[quadWidth * quadHeight];
        var quadUv = new Vector2[quadWidth * quadHeight];
        var quadSizes = new Vector2[quadWidth * quadHeight];

        for (int ii = 0; ii < quadWidth; ++ii)
        {
            for (int jj = 0; jj < quadHeight; ++jj)
            {
                int i = ii + 1;
                int j = jj + 1;
                quadVertices[ii + jj * quadWidth] = vertices[i + j * width];
                quadUv[ii + jj * quadWidth] = uv[i + j * width];
                quadSizes[ii + jj * quadWidth] = (vertices[(i + 1) + (j + 1) * width] - vertices[(i - 1) + (j - 1) * width]) * 0.5f;
            }
        }

        var triangles = new int[quadWidth * quadHeight];
        for (int i = 0; i < quadWidth * quadHeight; ++i)
            triangles[i] = i;

        // Without the bounds, Unity decides whether to render this mesh or not based on the vertices calculated here.
        // This causes Unity not rendering the mesh transformed by the depth texture even when the transformed one
        // belongs to the viewport of the camera.
        var bounds = new Bounds(Vector3.zero, Vector3.one * 1000.0f);

        var mesh = new Mesh()
        {
            indexFormat = IndexFormat.UInt32,
            vertices = quadVertices,
            uv = quadUv,
            uv2 = quadSizes,
            bounds = bounds,
        };
        mesh.SetIndices(triangles, MeshTopology.Points, 0);

        meshFilter.mesh = mesh;
    }
}