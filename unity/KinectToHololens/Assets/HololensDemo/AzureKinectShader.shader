Shader "KinectToHololens/AzureKinectShader"
{
    Properties
    {
        _YTex("Y Texture", 2D) = "white" {}
        _UTex("U Texture", 2D) = "white" {}
        _VTex("V Texture", 2D) = "white" {}
        _DepthTex("Depth Texture", 2D) = "white" {}
    }
    SubShader
    {
        Tags { "RenderType" = "Opaque" }
        Cull Off
        LOD 100

        Pass
        {
            CGPROGRAM
            #pragma target 4.0
            #pragma vertex vert
            //#pragma geometry geom
            #pragma fragment frag

            #include "UnityCG.cginc"

            struct appdata
            {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct v2f
            {
                float4 vertex : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            Texture2D _YTex;
            Texture2D _UTex;
            Texture2D _VTex;
            SamplerState sampler_YTex;
            sampler2D _DepthTex;

            float4x4 _DepthToColor;

            // Color Intrinsics
            float _Width;
            float _Height;
            float _Cx;
            float _Cy;
            float _Fx;
            float _Fy;
            float _K1;
            float _K2;
            float _K3;
            float _K4;
            float _K5;
            float _K6;
            float _Codx;
            float _Cody;
            float _P1;
            float _P2;

            v2f vert (appdata v)
            {
                v2f o;

                // 65.535 is equivalent to (2^16 - 1) / 1000, where (2^16 - 1) is to complement
                // the conversion happened in the texture-level from 0 ~ (2^16 - 1) to 0 ~ 1.
                // 1000 is the conversion of mm (the unit of Azure Kinect) to m (the unit of Unity3D).
                fixed depth = tex2Dlod(_DepthTex, fixed4(v.uv, 0, 0)).r * 65.535;
                // vertex in the depth camera coordinate system
                fixed4 depth_vertex = v.vertex * depth;

                // Below lines are following the logic of transformation_compute_correspondence in rgbz.c.
                fixed4 color_vertex = mul(_DepthToColor, depth_vertex);

                // The below line comes from transformation_project() in intrinsic_transformation.c.
                fixed2 xy = fixed2(color_vertex.x / color_vertex.z, color_vertex.y / color_vertex.z);

                // Using the procedure from transformation_project_internal()
                // in src/transformation/intrinsic_transformation.c of Azure Kinect Sensor SDK.
                fixed xp = xy.x - _Codx;
                fixed yp = xy.y - _Cody;

                fixed xp2 = xp * xp;
                fixed yp2 = yp * yp;
                fixed xyp = xp * yp;
                fixed rs = xp2 + yp2;
                fixed rss = rs * rs;
                fixed rsc = rss * rs;
                fixed a = 1.0 + _K1 * rs + _K2 * rss + _K3 * rsc;
                fixed b = 1.0 + _K4 * rs + _K5 * rss + _K6 * rsc;
                fixed d = a / b;
                
                fixed xp_d = xp * d;
                fixed yp_d = yp * d;

                fixed rs_2xp2 = rs + 2.0 * xp2;
                fixed rs_2yp2 = rs + 2.0 * yp2;

                xp_d += rs_2xp2 * _P2 + 2.0 * xyp * _P1;
                yp_d += rs_2yp2 * _P1 + 2.0 * xyp * _P2;

                fixed xp_d_cx = xp_d + _Codx;
                fixed yp_d_cy = yp_d + _Cody;

                fixed2 color_uv = fixed2((xp_d_cx * _Fx + _Cx) / (_Width - 1), (yp_d_cy * _Fx + _Cy) / (_Height - 1));

                o.vertex = UnityObjectToClipPos(depth_vertex);
                o.uv = color_uv;
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                // Formula came from https://docs.microsoft.com/en-us/windows/desktop/medfound/recommended-8-bit-yuv-formats-for-video-rendering.
                // Commented code before optimization.
                //fixed c = (tex2D(_YTex, i.uv).r - 0.0625) * 1.164383;
                //fixed c = tex2D(_YTex, i.uv).r * 1.164383 - 0.072774;
                //fixed c = mad(tex2D(_YTex, i.uv).r, 1.164383, -0.072774);
                //fixed d = tex2D(_UTex, i.uv).r - 0.5;
                //fixed e = tex2D(_VTex, i.uv).r - 0.5;
                fixed c = mad(_YTex.Sample(sampler_YTex, i.uv).r, 1.164383, -0.072774);
                fixed d = _UTex.Sample(sampler_YTex, i.uv).r - 0.5;
                fixed e = _VTex.Sample(sampler_YTex, i.uv).r - 0.5;

                //return fixed4(c + 1.596027 * e, c - 0.391762 * d - 0.812968 * e, c + 2.017232 * d, 1.0);
                return fixed4(mad(1.596027, e, c), mad(-0.812968, e, mad(-0.391762, d, c)), mad(2.017232, d, c), 1.0);
            }
            ENDCG
        }
    }
}
