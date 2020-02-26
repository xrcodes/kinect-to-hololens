public class AzureKinectCalibration
{
    //public Camera DepthCamera { get; private set; }
    //public Camera ColorCamera { get; private set; }
    //public Extrinsics DepthToColorExtrinsics { get; private set; }

    //public AzureKinectCalibration(Camera depthCamera, Camera colorCamera,
    //    Extrinsics depthToColorExtrinsics)
    //{
    //    DepthCamera = depthCamera;
    //    ColorCamera = colorCamera;
    //    DepthToColorExtrinsics = depthToColorExtrinsics;
    //}
    //public class Camera
    //{
    //    public Intrinsics Intrinsics { get; private set; }
    //    public int Width { get; private set; }
    //    public int Height { get; private set; }
    //    public float MetricRadius { get; private set; }

    //    public Camera(Intrinsics intrinsics, int width, int height, float metricRadius)
    //    {
    //        Intrinsics = intrinsics;
    //        Width = width;
    //        Height = height;
    //        MetricRadius = metricRadius;
    //    }
    //}

    public class Intrinsics
    {
        public float Cx;
        public float Cy;
        public float Fx;
        public float Fy;
        public float K1;
        public float K2;
        public float K3;
        public float K4;
        public float K5;
        public float K6;
        public float Codx;
        public float Cody;
        public float P2;
        public float P1;
        public float MetricRadius;

        public Intrinsics()
        {
        }

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
        public float[] Rotation;
        // Translation vector (x, y, z), in millimeters.
        public float[] Translation;

        public Extrinsics()
        {
            Rotation = new float[9];
            Translation = new float[3];
        }
        
        public Extrinsics(float[] rotation, float[] translation)
        {
            Rotation = rotation;
            Translation = translation;
        }
    }
}
