using System;

public static class ManagerHelper
{
    public static AzureKinectCalibration ReadAzureKinectCalibrationFromMessage(byte[] message)
    {
        int cursor = 1;

        int colorWidth = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        int colorHeight = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        int depthWidth = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        int depthHeight = BitConverter.ToInt32(message, cursor);
        cursor += 4;

        AzureKinectCalibration.Intrinsics colorIntrinsics;
        {
            float cx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float cy = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float fx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float fy = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k1 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k2 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k3 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k4 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k5 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k6 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float codx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float cody = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float p2 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float p1 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float metricRadius = BitConverter.ToSingle(message, cursor);
            cursor += 4;

            colorIntrinsics = new AzureKinectCalibration.Intrinsics(cx: cx,
                                                                    cy: cy,
                                                                    fx: fx,
                                                                    fy: fy,
                                                                    k1: k1,
                                                                    k2: k2,
                                                                    k3: k3,
                                                                    k4: k4,
                                                                    k5: k5,
                                                                    k6: k6,
                                                                    codx: codx,
                                                                    cody: cody,
                                                                    p2: p2,
                                                                    p1: p1,
                                                                    metricRadius: metricRadius);
        }

        float colorMetricRadius = BitConverter.ToSingle(message, cursor);
        cursor += 4;

        AzureKinectCalibration.Intrinsics depthIntrinsics;
        {
            float cx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float cy = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float fx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float fy = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k1 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k2 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k3 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k4 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k5 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float k6 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float codx = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float cody = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float p2 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float p1 = BitConverter.ToSingle(message, cursor);
            cursor += 4;
            float metricRadius = BitConverter.ToSingle(message, cursor);
            cursor += 4;

            depthIntrinsics = new AzureKinectCalibration.Intrinsics(cx: cx,
                                                                    cy: cy,
                                                                    fx: fx,
                                                                    fy: fy,
                                                                    k1: k1,
                                                                    k2: k2,
                                                                    k3: k3,
                                                                    k4: k4,
                                                                    k5: k5,
                                                                    k6: k6,
                                                                    codx: codx,
                                                                    cody: cody,
                                                                    p2: p2,
                                                                    p1: p1,
                                                                    metricRadius: metricRadius);
        }

        float depthMetricRadius = BitConverter.ToSingle(message, cursor);
        cursor += 4;

        AzureKinectCalibration.Extrinsics depthToColorExtrinsics;
        {
            float[] rotation = new float[9];
            for (int i = 0; i < 9; ++i)
            {
                rotation[i] = BitConverter.ToSingle(message, cursor);
                cursor += 4;
            }

            float[] translation = new float[3];
            for (int i = 0; i < 3; ++i)
            {
                translation[i] = BitConverter.ToSingle(message, cursor);
                cursor += 4;
            }

            depthToColorExtrinsics = new AzureKinectCalibration.Extrinsics(rotation, translation);
        }

        var depthCamera = new AzureKinectCalibration.Camera(depthIntrinsics, depthWidth, depthHeight, depthMetricRadius);
        var colorCamera = new AzureKinectCalibration.Camera(colorIntrinsics, colorWidth, colorHeight, colorMetricRadius);

        return new AzureKinectCalibration(depthCamera: depthCamera,
                                          colorCamera: colorCamera,
                                          depthToColorExtrinsics: depthToColorExtrinsics);
    }
}
