using System.Collections;
using System.Diagnostics;
using UnityEngine;
using UnityEngine.Rendering;

[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
public class KinectScreen : MonoBehaviour
{
    public Shader shader;
    public MeshFilter meshFilter;
    public MeshRenderer meshRenderer;
    public PrepareState State { get; private set; }
    public float Progress { get; private set; }

    public Material Material => meshRenderer.sharedMaterial;

    void Awake()
    {
        State = PrepareState.Unprepared;
        Progress = 0.0f;
        meshRenderer.material = new Material(shader);
        RenderPipelineManager.beginCameraRendering += OnBeginCameraRendering;
    }

    void OnDestroy()
    {
        RenderPipelineManager.beginCameraRendering -= OnBeginCameraRendering;
    }

    public void StartPrepare(VideoInitSenderPacketData initSenderPacketData)
    {
        StartCoroutine(SetupMesh(initSenderPacketData));
    }

    // Since calculation including Unproject() takes too much time,
    // this function is made to run as a coroutine that takes a break
    // every 100 ms.
    private IEnumerator SetupMesh(VideoInitSenderPacketData initSenderPacketData)
    {
        State = PrepareState.Preparing;
        Progress = 0.0f;

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
                    // Flip y since Azure Kinect's y axis is downwards.
                    // https://docs.microsoft.com/en-us/azure/kinect-dk/coordinate-systems
                    vertices[i + j * width] = new Vector3(xy[0], -xy[1], 1.0f);
                }
                else
                {
                    vertices[i + j * width] = new Vector3(0.0f, 0.0f, 0.0f);
                }
                uv[i + j * width] = new Vector2(i / (float)(width - 1), j / (float)(height - 1));
            }

            if(stopWatch.ElapsedMilliseconds > 100)
            {
                Progress = (i * 0.99f) / width;
                yield return null;
                stopWatch = Stopwatch.StartNew();
            }
        }

        Progress = 0.99f;

        //print($"vertices[0]: {vertices[0]}"); // (-1.0, 1.0, 1.0): left-top
        //print($"vertices[last]: {vertices[vertices.Length - 1]}"); // (0.8, -0.6, 1.0): right-bottom

        const float SIZE_AMPLIFIER = 1.2f;
        int quadWidth = width - 2;
        int quadHeight = height - 2;
        var quadVertices = new Vector3[quadWidth * quadHeight];
        var quadUv = new Vector2[quadWidth * quadHeight];
        var quadHalfSizes = new Vector2[quadWidth * quadHeight];

        for (int ii = 0; ii < quadWidth; ++ii)
        {
            for (int jj = 0; jj < quadHeight; ++jj)
            {
                int i = ii + 1;
                int j = jj + 1;
                quadVertices[ii + jj * quadWidth] = vertices[i + j * width];
                quadUv[ii + jj * quadWidth] = uv[i + j * width];
                // Trying to make both x and y to have a positive number. The first 0.5f is to make the size relevant to
                // the vertex in (i, j). The second one is to get the half size of it.
                quadHalfSizes[ii + jj * quadWidth] = (vertices[(i + 1) + (j - 1) * width] - vertices[(i - 1) + (j + 1) * width]) * 0.5f * 0.5f * SIZE_AMPLIFIER;
            }
        }

        //print($"quadSizes[0]: {quadSizes[0].x}, {quadSizes[0].y}"); // 0.002900749, 0.003067017

        var triangles = new int[quadWidth * quadHeight];
        for (int i = 0; i < quadWidth * quadHeight; ++i)
            triangles[i] = i;

        // 65.535 is equivalent to (2^16 - 1) / 1000, where (2^16 - 1) is to complement
        // the conversion happened in the texture-level from 0 ~ (2^16 - 1) to 0 ~ 1.
        // 1000 is the conversion of mm (the unit of Azure Kinect) to m (the unit of Unity3D).
        for (int i = 0; i < quadVertices.Length; ++i)
            quadVertices[i] *= 65.535f;

        for (int i = 0; i < quadHalfSizes.Length; ++i)
            quadHalfSizes[i] *= 65.535f;

        // Without the bounds, Unity decides whether to render this mesh or not based on the vertices calculated here.
        // This causes Unity not rendering the mesh transformed by the depth texture even when the transformed one
        // belongs to the viewport of the camera.
        var bounds = new Bounds(Vector3.zero, Vector3.one * 1000.0f);

        var mesh = new Mesh()
        {
            indexFormat = IndexFormat.UInt32,
            vertices = quadVertices,
            uv = quadUv,
            uv2 = quadHalfSizes,
            bounds = bounds,
        };
        mesh.SetIndices(triangles, MeshTopology.Points, 0);

        meshFilter.mesh = mesh;

        State = PrepareState.Prepared;
    }

    void OnBeginCameraRendering(ScriptableRenderContext context, Camera camera)
    {
        // Ignore when it is called from Unity's "Scene" (not "Game").
        if (camera.cameraType == CameraType.SceneView)
            return;

        var cameraTransform = camera.transform;
        var worldCameraFrontVector = cameraTransform.TransformDirection(new Vector3(0.0f, 0.0f, 1.0f));

        // Using the y direction as the up vector instead the up vector of the camera allows the user to feel more
        // comfortable as it preserves the sense of gravity.
        // Getting the right vector directly from the camera transform through zeroing its y-component does not
        // work when the y-component of the camera's up vector is negative. While it is possible to solve the problem
        // with an if statement, inverting when the y-component is negative, I decided to detour this case with
        // usage of the cross product with the front vector.
        var worldUpVector = new Vector3(0.0f, 1.0f, 0.0f);
        var worldCameraRightVector = Vector3.Cross(worldUpVector, worldCameraFrontVector);
        var worldRightVector = new Vector3(worldCameraRightVector.x, 0.0f, worldCameraRightVector.z);
        worldRightVector.Normalize();

        meshRenderer.sharedMaterial.SetVector("_SizeDirectionX", transform.worldToLocalMatrix * worldRightVector);
        meshRenderer.sharedMaterial.SetVector("_SizeDirectionY", transform.worldToLocalMatrix * worldUpVector);
    }
}