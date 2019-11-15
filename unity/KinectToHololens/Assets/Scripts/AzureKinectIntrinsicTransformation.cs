// A class that is a ported version of src/transformation/intrinsic_transformation.c in Azure Kinect Sensor SDK.
public static class AzureKinectIntrinsicTransformation
{
    // Equivalent to transformation_project_internal().
    // Size of uv and xy should be 2.
    // Size of J should be 4.
    public static bool Project(in AzureKinectCalibration.Camera camera, in float[] xy, ref float[] uv, ref int valid, ref float[] J_xy)
    {
        var intrinsics = camera.Intrinsics;

        if(!(intrinsics.Fx > 0.0f && intrinsics.Fy > 0.0f))
        {
            return false;
        }

        valid = 1;

        float xp = xy[0] - intrinsics.Codx;
        float yp = xy[1] - intrinsics.Cody;

        float xp2 = xp * xp;
        float yp2 = yp * yp;
        float xyp = xp * yp;
        float rs = xp2 + yp2;
        if (rs > camera.MetricRadius * camera.MetricRadius)
        {
            valid = 0;
            return true;
        }
        float rss = rs * rs;
        float rsc = rss * rs;
        float a = 1.0f + intrinsics.K1 * rs + intrinsics.K2 * rss + intrinsics.K3 * rsc;
        float b = 1.0f + intrinsics.K4 * rs + intrinsics.K5 * rss + intrinsics.K6 * rsc;
        float bi;
        if (b != 0.0f)
        {
            bi = 1.0f / b;
        }
        else
        {
            bi = 1.0f;
        }
        float d = a * bi;

        float xp_d = xp * d;
        float yp_d = yp * d;

        float rs_2xp2 = rs + 2.0f * xp2;
        float rs_2yp2 = rs + 2.0f * yp2;

        xp_d += rs_2xp2 * intrinsics.P2 + 2.0f * xyp * intrinsics.P1;
        yp_d += rs_2yp2 * intrinsics.P1 + 2.0f * xyp * intrinsics.P2;

        float xp_d_cx = xp_d + intrinsics.Codx;
        float yp_d_cy = yp_d + intrinsics.Cody;

        uv[0] = xp_d_cx * intrinsics.Fx + intrinsics.Cx;
        uv[1] = yp_d_cy * intrinsics.Fy + intrinsics.Cy;

        if (J_xy == null)
        {
            return true;
        }

        float dudrs = intrinsics.K1 + 2.0f * intrinsics.K2 * rs + 3.0f * intrinsics.K3 * rss;
        float dvdrs = intrinsics.K4 + 2.0f * intrinsics.K5 * rs + 3.0f * intrinsics.K6 * rss;
        float bis = bi * bi;
        float dddrs = (dudrs * b - a * dvdrs) * bis;

        float dddrs_2 = dddrs * 2.0f;
        float xp_dddrs_2 = xp * dddrs_2;
        float yp_xp_dddrs_2 = yp * xp_dddrs_2;

        J_xy[0] = intrinsics.Fx * (d + xp * xp_dddrs_2 + 6.0f * xp * intrinsics.P2 + 2.0f * yp * intrinsics.P1);
        J_xy[1] = intrinsics.Fx * (yp_xp_dddrs_2 + 2.0f * yp * intrinsics.P2 + 2.0f * xp * intrinsics.P1);
        J_xy[2] = intrinsics.Fy * (yp_xp_dddrs_2 + 2.0f * xp * intrinsics.P1 + 2.0f * yp * intrinsics.P2);
        J_xy[3] = intrinsics.Fy * (d + yp * yp * dddrs_2 + 6.0f * yp * intrinsics.P1 + 2.0f * xp * intrinsics.P2);

        return true;
    }

    // Equivalent to invert_2x2(). 
    // Size of J and Jinv should be 4.
    public static void Invert2x2(in float[] J, ref float[] Jinv)
    {
        float detJ = J[0] * J[3] - J[1] * J[2];
        float invDetJ = 1.0f / detJ;

        Jinv[0] = invDetJ * J[3];
        Jinv[3] = invDetJ * J[0];
        Jinv[1] = -invDetJ * J[1];
        Jinv[2] = -invDetJ * J[2];
    }

    // Equivalent to transformation_iterative_unproject().
    // Size of uv and xy should be 2.
    public static bool InterativeUnproject(in AzureKinectCalibration.Camera camera, in float[] uv, ref float[] xy, ref int valid, int maxPasses)
    {
        var intrinsics = camera.Intrinsics;

        valid = 1;
        float[] Jinv = new float[4];
        float[] best_xy = new float[2] { 0.0f, 0.0f };
        float best_err = float.MaxValue;

        for (int pass = 0; pass < maxPasses; ++pass)
        {
            float[] p = new float[2];
            float[] J = new float[4];

            if (!Project(camera, xy, ref p, ref valid, ref J))
            {
                return false;
            }
            if (valid == 0)
            {
                return true;
            }

            float err_x = uv[0] - p[0];
            float err_y = uv[1] - p[1];
            float err = err_x * err_x + err_y * err_y;
            if(err >= best_err)
            {
                xy[0] = best_xy[0];
                xy[1] = best_xy[1];
                break;
            }

            best_err = err;
            best_xy[0] = xy[0];
            best_xy[1] = xy[1];
            Invert2x2(J, ref Jinv);
            if(pass + 1 == maxPasses || best_err < 1e-22f)
            {
                break;
            }

            float dx = Jinv[0] * err_x + Jinv[1] * err_y;
            float dy = Jinv[2] * err_x + Jinv[3] * err_y;

            xy[0] += dx;
            xy[1] += dy;
        }

        if (best_err > 1e-6f)
        {
            valid = 0;
        }

        return true;
    }

    // Equivalent to transformation_unproject_internal(). 
    // Size of uv and xy should be 2.
    public static bool Unproject(in AzureKinectCalibration.Camera camera, in float[] uv, ref float[] xy, ref int valid)
    {
        var intrinsics = camera.Intrinsics;
        if (!(intrinsics.Fx > 0.0f && intrinsics.Fy > 0.0f))
        {
            return false;
        }

        float xp_d = (uv[0] - intrinsics.Cx) / intrinsics.Fx - intrinsics.Codx;
        float yp_d = (uv[1] - intrinsics.Cy) / intrinsics.Fy - intrinsics.Cody;

        float rs = xp_d * xp_d + yp_d * yp_d;
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

        xy[0] = xp_d * di;
        xy[1] = yp_d * di;

        // approximate correction for tangential params
        float two_xy = 2.0f * xy[0] * xy[1];
        float xx = xy[0] * xy[0];
        float yy = xy[1] * xy[1];

        xy[0] -= (yy + 3.0f * xx) * intrinsics.P2 + two_xy * intrinsics.P1;
        // It is (xx + 3.0f * xx) in original code, but it is obviously a typo, so...
        //y -= (xx + 3.0f * yy) * intrinsics.P1 + twoXy * intrinsics.P2;
        xy[1] -= (xx + 3.0f * yy) * intrinsics.P1 + two_xy * intrinsics.P2;

        // add on center of distortion
        xy[0] += intrinsics.Codx;
        xy[1] += intrinsics.Cody;

        return InterativeUnproject(camera, uv, ref xy, ref valid, 20);
    }
}
