using UnityEngine;
using UnityEngine.Rendering;

[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
public class AzureKinectScreen : MonoBehaviour
{
    public MeshFilter meshFilter;
    public MeshRenderer meshRenderer;

    public void Setup(AzureKinectCalibration calibration)
    {
        print("calibration.ColorCamera.MetricRadius: " + calibration.ColorCamera.MetricRadius);
        print("calibration.ColorCamera.Intrinsics.Cx: " + calibration.ColorCamera.Intrinsics.Cx);
        print("calibration.ColorCamera.Intrinsics.Cy: " + calibration.ColorCamera.Intrinsics.Cy);
        print("calibration.ColorCamera.Intrinsics.Fx: " + calibration.ColorCamera.Intrinsics.Fx);
        print("calibration.ColorCamera.Intrinsics.Fy: " + calibration.ColorCamera.Intrinsics.Fy);
        print("calibration.ColorCamera.Intrinsics.K1: " + calibration.ColorCamera.Intrinsics.K1);
        print("calibration.ColorCamera.Intrinsics.K2: " + calibration.ColorCamera.Intrinsics.K2);
        print("calibration.ColorCamera.Intrinsics.K3: " + calibration.ColorCamera.Intrinsics.K3);
        print("calibration.ColorCamera.Intrinsics.K4: " + calibration.ColorCamera.Intrinsics.K4);
        print("calibration.ColorCamera.Intrinsics.K5: " + calibration.ColorCamera.Intrinsics.K5);
        print("calibration.ColorCamera.Intrinsics.K6: " + calibration.ColorCamera.Intrinsics.K6);
        print("calibration.ColorCamera.Intrinsics.Codx: " + calibration.ColorCamera.Intrinsics.Codx);
        print("calibration.ColorCamera.Intrinsics.Cody: " + calibration.ColorCamera.Intrinsics.Cody);
        print("calibration.ColorCamera.Intrinsics.P2: " + calibration.ColorCamera.Intrinsics.P2);
        print("calibration.ColorCamera.Intrinsics.P1: " + calibration.ColorCamera.Intrinsics.P1);

        print("calibration.DepthCamera.MetricRadius: " + calibration.DepthCamera.MetricRadius);
        print("calibration.DepthCamera.Intrinsics.Cx: " + calibration.DepthCamera.Intrinsics.Cx);
        print("calibration.DepthCamera.Intrinsics.Cy: " + calibration.DepthCamera.Intrinsics.Cy);
        print("calibration.DepthCamera.Intrinsics.Fx: " + calibration.DepthCamera.Intrinsics.Fx);
        print("calibration.DepthCamera.Intrinsics.Fy: " + calibration.DepthCamera.Intrinsics.Fy);
        print("calibration.DepthCamera.Intrinsics.K1: " + calibration.DepthCamera.Intrinsics.K1);
        print("calibration.DepthCamera.Intrinsics.K2: " + calibration.DepthCamera.Intrinsics.K2);
        print("calibration.DepthCamera.Intrinsics.K3: " + calibration.DepthCamera.Intrinsics.K3);
        print("calibration.DepthCamera.Intrinsics.K4: " + calibration.DepthCamera.Intrinsics.K4);
        print("calibration.DepthCamera.Intrinsics.K5: " + calibration.DepthCamera.Intrinsics.K5);
        print("calibration.DepthCamera.Intrinsics.K6: " + calibration.DepthCamera.Intrinsics.K6);
        print("calibration.DepthCamera.Intrinsics.Codx: " + calibration.DepthCamera.Intrinsics.Codx);
        print("calibration.DepthCamera.Intrinsics.Cody: " + calibration.DepthCamera.Intrinsics.Cody);
        print("calibration.DepthCamera.Intrinsics.P2: " + calibration.DepthCamera.Intrinsics.P2);
        print("calibration.DepthCamera.Intrinsics.P1: " + calibration.DepthCamera.Intrinsics.P1);

        for (int i = 0; i < 9; ++i)
            print($"rotation[{i}]: {calibration.DepthToColorExtrinsics.Rotation[i]}");

        for (int i = 0; i < 3; ++i)
            print($"translation[{i}]: {calibration.DepthToColorExtrinsics.Translation[i]}");

        meshFilter.mesh = CreateMesh(calibration);

        //Matrix4x4 depthToColorMatrix;
        //{
        //    var extrinsics = calibration.DepthToColorExtrinsics;
        //    var r = extrinsics.Rotation;
        //    var t = extrinsics.Translation;
        //    var column0 = new Vector4(r[0], r[3], r[6], 0.0f);
        //    var column1 = new Vector4(r[1], r[4], r[7], 0.0f);
        //    var column2 = new Vector4(r[2], r[5], r[8], 0.0f);
        //    // Scale mm to m.
        //    var column3 = new Vector4(t[0] * 0.001f, t[1] * 0.001f, t[2] * 0.001f, 1.0f);

        //    depthToColorMatrix = new Matrix4x4(column0, column1, column2, column3);
        //}
        //meshRenderer.sharedMaterial.SetMatrix("_DepthToColor", depthToColorMatrix);

        // Entering the matrix as floats to see them in the Editor.
        // Also, matrices get reset from Unity after clicking another app and coming back.
        var extrinsics = calibration.DepthToColorExtrinsics;
        meshRenderer.sharedMaterial.SetFloat("_R0", extrinsics.Rotation[0]);
        meshRenderer.sharedMaterial.SetFloat("_R1", extrinsics.Rotation[1]);
        meshRenderer.sharedMaterial.SetFloat("_R2", extrinsics.Rotation[2]);
        meshRenderer.sharedMaterial.SetFloat("_R3", extrinsics.Rotation[3]);
        meshRenderer.sharedMaterial.SetFloat("_R4", extrinsics.Rotation[4]);
        meshRenderer.sharedMaterial.SetFloat("_R5", extrinsics.Rotation[5]);
        meshRenderer.sharedMaterial.SetFloat("_R6", extrinsics.Rotation[6]);
        meshRenderer.sharedMaterial.SetFloat("_R7", extrinsics.Rotation[7]);
        meshRenderer.sharedMaterial.SetFloat("_R8", extrinsics.Rotation[8]);
        meshRenderer.sharedMaterial.SetFloat("_T0", extrinsics.Translation[0] * 0.001f);
        meshRenderer.sharedMaterial.SetFloat("_T1", extrinsics.Translation[1] * 0.001f);
        meshRenderer.sharedMaterial.SetFloat("_T2", extrinsics.Translation[2] * 0.001f);

        var colorIntrinsics = calibration.ColorCamera.Intrinsics;
        meshRenderer.sharedMaterial.SetFloat("_Width", calibration.ColorCamera.Width);
        meshRenderer.sharedMaterial.SetFloat("_Height", calibration.ColorCamera.Height);
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
        int width = calibration.DepthCamera.Width;
        int height = calibration.DepthCamera.Height;

        var depthCamera = calibration.DepthCamera;

        var vertices = new Vector3[width * height];
        var uv = new Vector2[width * height];

        int failureCount = 0;
        int invalidityCount = 0;

        for (int i = 0; i < width; ++i)
        {
            for(int j = 0; j < height; ++j)
            {
                float[] xy = new float[2];
                int valid = 0;
                if(AzureKinectIntrinsicTransformation.Unproject(depthCamera, new float[2] { i, j }, ref xy, ref valid))
                {
                    vertices[i + j * width] = new Vector3(xy[0], xy[1], 1.0f);
                }
                else
                {
                    vertices[i + j * width] = new Vector3(0.0f, 0.0f, 0.0f);
                    ++failureCount;
                }
                uv[i + j * width] = new Vector2(i / (float)(width - 1), j / (float)(height - 1));

                if (valid == 0)
                    ++invalidityCount;
            }
        }

        print($"failure: {failureCount}");
        print($"invalidity: {invalidityCount}");

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