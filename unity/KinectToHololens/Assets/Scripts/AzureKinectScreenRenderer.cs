using UnityEngine;
using UnityEngine.Rendering;

[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
public class AzureKinectScreenRenderer : MonoBehaviour
{
    public MeshFilter meshFilter;
    public MeshRenderer meshRenderer;

    public void InitializeScreen(AzureKinectCalibration calibration)
    {
        meshFilter.mesh = CreateMesh(calibration);

        Matrix4x4 depthToColorMatrix;
        {
            var extrinsics = calibration.DepthToColorExtrinsics;
            var r = extrinsics.Rotation;
            var t = extrinsics.Translation;
            // Scale mm to m.
            t[0] *= 0.001f;
            t[1] *= 0.001f;
            t[2] *= 0.001f;
            var column0 = new Vector4(r[0], r[3], r[6], 0.0f);
            var column1 = new Vector4(r[1], r[4], r[7], 0.0f);
            var column2 = new Vector4(r[2], r[5], r[8], 0.0f);
            var column3 = new Vector4(t[0], t[1], t[2], 1.0f);
            depthToColorMatrix = new Matrix4x4(column0, column1, column2, column3);
            print("depthToColorMatrix: " + depthToColorMatrix);
        }
        meshRenderer.sharedMaterial.SetMatrix("_DepthToColor", depthToColorMatrix);

        var colorIntrinsics = calibration.ColorCamera.Intrinsics;
        meshRenderer.sharedMaterial.SetFloat("_Cx", colorIntrinsics.Cx);
        meshRenderer.sharedMaterial.SetFloat("_Cy", colorIntrinsics.Cy);
        meshRenderer.sharedMaterial.SetFloat("_Fx", colorIntrinsics.Fx);
        meshRenderer.sharedMaterial.SetFloat("_Fy", colorIntrinsics.Fy);
        meshRenderer.sharedMaterial.SetFloat("_K1", colorIntrinsics.K1);
        meshRenderer.sharedMaterial.SetFloat("_K2", colorIntrinsics.K2);
        meshRenderer.sharedMaterial.SetFloat("_K3", colorIntrinsics.K3);
        meshRenderer.sharedMaterial.SetFloat("_K4", colorIntrinsics.K4);
        meshRenderer.sharedMaterial.SetFloat("_K5", colorIntrinsics.K5);
        meshRenderer.sharedMaterial.SetFloat("_K6", colorIntrinsics.K6);
        meshRenderer.sharedMaterial.SetFloat("_Codx", colorIntrinsics.Codx);
        meshRenderer.sharedMaterial.SetFloat("_Cody", colorIntrinsics.Cody);
        meshRenderer.sharedMaterial.SetFloat("_P2", colorIntrinsics.P2);
        meshRenderer.sharedMaterial.SetFloat("_P1", colorIntrinsics.P1);
    }

    private static Mesh CreateMesh(AzureKinectCalibration calibration)
    {
        const int AZURE_KINECT_DEPTH_WIDTH = 640;
        const int AZURE_KINECT_DEPTH_HEIGHT = 576;

        var depthCamera = calibration.DepthCamera;

        var vertices = new Vector3[AZURE_KINECT_DEPTH_WIDTH * AZURE_KINECT_DEPTH_HEIGHT];
        var uv = new Vector2[AZURE_KINECT_DEPTH_WIDTH * AZURE_KINECT_DEPTH_HEIGHT];

        int validCount = 0;
        int successCount = 0;
        for(int i = 0; i < AZURE_KINECT_DEPTH_WIDTH; ++i)
        {
            for(int j = 0; j < AZURE_KINECT_DEPTH_HEIGHT; ++j)
            {
                float[] xy = new float[2];
                int valid = 0;
                bool success = AzureKinectIntrinsicTransformation.Unproject(depthCamera, new float[2] { i, j }, ref xy, ref valid);
                vertices[i + j * AZURE_KINECT_DEPTH_WIDTH] = new Vector3(xy[0], xy[1], 1.0f);
                uv[i + j * AZURE_KINECT_DEPTH_WIDTH] = new Vector2(i / (float)(AZURE_KINECT_DEPTH_WIDTH - 1),
                                                                   j / (float)(AZURE_KINECT_DEPTH_HEIGHT - 1));

                validCount += valid;
                if (success)
                    ++successCount;
            }
        }

        print("depthCamera.MetricRadius: " + depthCamera.MetricRadius);
        print("validCount: " + validCount);
        print("successCount: " + successCount);

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