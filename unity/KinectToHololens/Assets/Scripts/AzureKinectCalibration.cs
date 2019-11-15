public class AzureKinectCalibration
{
    public Camera ColorCamera { get; private set; }
    public Camera DepthCamera { get; private set; }
    public Extrinsics DepthToColorExtrinsics { get; private set; }

    public AzureKinectCalibration(Camera colorCamera, Camera depthCamera,
        Extrinsics depthToColorExtrinsics)
    {
        ColorCamera = colorCamera;
        DepthCamera = depthCamera;
        DepthToColorExtrinsics = depthToColorExtrinsics;
    }
    public class Camera
    {
        public Intrinsics Intrinsics { get; private set; }
        public float MetricRadius { get; private set; }

        public Camera(Intrinsics intrinsics, float metricRadius)
        {
            Intrinsics = intrinsics;
            MetricRadius = metricRadius;
        }
    }

    public class Intrinsics
    {
        public float Cx { get; private set; }
        public float Cy { get; private set; }
        public float Fx { get; private set; }
        public float Fy { get; private set; }
        public float K1 { get; private set; }
        public float K2 { get; private set; }
        public float K3 { get; private set; }
        public float K4 { get; private set; }
        public float K5 { get; private set; }
        public float K6 { get; private set; }
        public float Codx { get; private set; }
        public float Cody { get; private set; }
        public float P2 { get; private set; }
        public float P1 { get; private set; }
        public float MetricRadius { get; private set; }

        public Intrinsics(float cx, float cy, float fx, float fy,
            float k1, float k2, float k3, float k4, float k5, float k6,
            float codx, float cody, float p2, float p1, float metricRadius)
        {
            Cx = cx;
            Cy = cy;
            Fx = fx;
            Fy = fy;
            K1 = k1;
            K2 = k2;
            K3 = k3;
            K4 = k4;
            K5 = k5;
            K6 = k6;
            Codx = codx;
            Cody = cody;
            P2 = p2;
            P1 = p1;
            MetricRadius = metricRadius;
        }
    }

    public class Extrinsics
    {
        // 3x3 rotation matrix stored in row major order.
        public float[] Rotation { get; private set; }
        // Translation vector (x, y, z), in millimeters.
        public float[] Translation { get; private set; }

        public Extrinsics(float[] rotation, float[] translation)
        {
            Rotation = rotation;
            Translation = translation;
        }
    }
}
