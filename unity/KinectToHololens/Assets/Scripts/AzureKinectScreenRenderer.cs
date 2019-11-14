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

        Matrix4x4 depthToColorMatrix;
        {
            var extrinsics = screen.Calibration.DepthToColorExtrinsics;
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

        // Taking the opposite of Unproject,
        // Starting from (x / z) = (u * W - cx) / fx - codx, where (W is the pixel width of the color,
        // u = [(fx / W) * x + (cx + fx * codx) / W * z] / z.
        // v = [(fy / H) * y + (cy + fy * cody) / H * z] / z.
        // Thus the project matrix should look like
        // |u|   | a 0 b 0 | |x|
        // |v| - | 0 c d 0 | |y|
        // |0| - | 0 0 0 0 | |z|
        // |z|   | 0 0 1 0 | |w|
        // where a = fx / W, b = (cx + fx * codx) / W,
        //       c = fy / H, d = (cy + fy * cody) / H.
        Matrix4x4 colorProjectionMatrix;
        var colorIntrinsics = screen.Calibration.ColorIntrinsics;
        {
            const int AZURE_KINECT_COLOR_WIDTH = 1280;
            const int AZURE_KINECT_COLOR_HEIGHT = 720;
            float a = colorIntrinsics.Fx / AZURE_KINECT_COLOR_WIDTH;
            float b = (colorIntrinsics.Cx + colorIntrinsics.Fx * colorIntrinsics.Codx) / AZURE_KINECT_COLOR_WIDTH;
            float c = colorIntrinsics.Fy / AZURE_KINECT_COLOR_HEIGHT;
            float d = (colorIntrinsics.Cy + colorIntrinsics.Fy * colorIntrinsics.Cody) / AZURE_KINECT_COLOR_HEIGHT;
            var column0 = new Vector4(a, 0, 0, 0);
            var column1 = new Vector4(0, c, 0, 0);
            var column2 = new Vector4(b, d, 0, 1);
            var column3 = new Vector4(0, 0, 0, 0);
            colorProjectionMatrix = new Matrix4x4(column0, column1, column2, column3);
        }

        meshRenderer.sharedMaterial.SetMatrix("_ColorProjection", colorProjectionMatrix);

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
        meshRenderer.sharedMaterial.SetFloat("_P1", colorIntrinsics.P1);
        meshRenderer.sharedMaterial.SetFloat("_P2", colorIntrinsics.P2);
    }

    private static Mesh CreateMesh(AzureKinectScreen screen)
    {
        const int AZURE_KINECT_DEPTH_WIDTH = 640;
        const int AZURE_KINECT_DEPTH_HEIGHT = 576;

        var depthIntrinsics = screen.Calibration.DepthIntrinsics;

        var vertices = new Vector3[AZURE_KINECT_DEPTH_WIDTH * AZURE_KINECT_DEPTH_HEIGHT];
        var uv = new Vector2[AZURE_KINECT_DEPTH_WIDTH * AZURE_KINECT_DEPTH_HEIGHT];

        for(int i = 0; i < AZURE_KINECT_DEPTH_WIDTH; ++i)
        {
            for(int j = 0; j < AZURE_KINECT_DEPTH_HEIGHT; ++j)
            {
                var xy = Unproject(depthIntrinsics, new Vector2(i, j));
                var vertex = new Vector3(xy.x, xy.y, 1.0f);
                vertices[i + j * AZURE_KINECT_DEPTH_WIDTH] = vertex;
                uv[i + j * AZURE_KINECT_DEPTH_WIDTH] = new Vector2(i / (float)AZURE_KINECT_DEPTH_WIDTH, 1.0f - j / (float)AZURE_KINECT_DEPTH_HEIGHT);
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

    // From transformation_unproject_internal() of src/transformation/intrinsic_transformation.c in Azure Kinect Sensor SDK. 
    private static Vector2 Unproject(AzureKinectCalibration.Intrinsics intrinsics, Vector2 uv)
    {

        float xpD = (uv.x - intrinsics.Cx) / intrinsics.Fx - intrinsics.Codx;
        float ypD = (uv.y - intrinsics.Cy) / intrinsics.Fy - intrinsics.Cody;

        float rs = xpD * xpD + ypD * ypD;
        float rss = rs * rs;
        float rsc = rss * rs;
        float a = 1.0f + intrinsics.K1 * rs + intrinsics.K2 * rss + intrinsics.K3 * rsc;
        float b = 1.0f + intrinsics.K4 * rs + intrinsics.K5 * rss + intrinsics.K6 * rsc;
        float ai;
        if (a != 0.0f)
        {
            ai = 1.0f / a;
        }
        else
        {
            ai = 1.0f;
        }
        float di = ai * b;

        float x = xpD * di;
        float y = ypD * di;

        // approximate correction for tangential params
        float twoXy = 2.0f * x * y;
        float xx = x * x;
        float yy = y * y;

        x -= (yy + 3.0f * xx) * intrinsics.P2 + twoXy * intrinsics.P1;
        // It is (xx + 3.0f * xx) in original code, but it is obviously a typo, so...
        y -= (xx + 3.0f * yy) * intrinsics.P1 + twoXy * intrinsics.P2;

        // add on center of distortion
        x += intrinsics.Codx;
        y += intrinsics.Cody;

        // Original code goes through another path via
        // transformation_iterative_unproject() but I will not here this time...
        return new Vector2(x, y);
    }
}