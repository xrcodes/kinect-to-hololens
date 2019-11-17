Shader "KinectToHololens/AzureKinectShader"
{
    Properties
    {
        _YTex("Y Texture", 2D) = "white" {}
        _UTex("U Texture", 2D) = "white" {}
        _VTex("V Texture", 2D) = "white" {}
        _DepthTex("Depth Texture", 2D) = "white" {}
        //_R0("Depth to Color Rotation 0", Float) = 0.0
        //_R1("Depth to Color Rotation 1", Float) = 0.0
        //_R2("Depth to Color Rotation 2", Float) = 0.0
        //_R3("Depth to Color Rotation 3", Float) = 0.0
        //_R4("Depth to Color Rotation 4", Float) = 0.0
        //_R5("Depth to Color Rotation 5", Float) = 0.0
        //_R6("Depth to Color Rotation 6", Float) = 0.0
        //_R7("Depth to Color Rotation 7", Float) = 0.0
        //_R8("Depth to Color Rotation 8", Float) = 0.0
        //_T0("Depth to Color Translation 0", Float) = 0.0
        //_T1("Depth to Color Translation 1", Float) = 0.0
        //_T2("Depth to Color Translation 2", Float) = 0.0
        //_Width("Color Width", Float) = 0.0
        //_Height("Color Height", Float) = 0.0
        //_Cx("Color Cx", Float) = 0.0
        //_Cy("Color Cy", Float) = 0.0
        //_Fx("Color Fx", Float) = 0.0
        //_Fy("Color Fy", Float) = 0.0
        //_K1("Color K1", Float) = 0.0
        //_K2("Color K2", Float) = 0.0
        //_K3("Color K3", Float) = 0.0
        //_K4("Color K4", Float) = 0.0
        //_K5("Color K5", Float) = 0.0
        //_K6("Color K6", Float) = 0.0
        //_Codx("Color Codx", Float) = 0.0
        //_Cody("Color Cody", Float) = 0.0
        //_P1("Color P1", Float) = 0.0
        //_P2("Color P2", Float) = 0.0
        //_VertexOffsetXVector("Vertex Offset X Vector", Vector) = (0, 0, 0, 0)
        //_VertexOffsetYVector("Vertex Offset Y Vector", Vector) = (0, 0, 0, 0)
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
            #pragma geometry geom
            #pragma fragment frag

            #include "UnityCG.cginc"

            struct appdata
            {
                float3 vertex : POSITION;
                float2 uv : TEXCOORD0;
                float2 uv2 : TEXCOORD1;
                float2 uv3 : TEXCOORD2;
            };

            struct v2g
            {
                fixed4 vertex : POSITION;
                fixed2 uv : TEXCOORD0;
                fixed2 vertex_offset : TEXCOORD1;
                fixed2 uv_offset : TEXCOORD2;
            };

            struct g2f
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

            float _R0;
            float _R1;
            float _R2;
            float _R3;
            float _R4;
            float _R5;
            float _R6;
            float _R7;
            float _R8;
            float _T0;
            float _T1;
            float _T2;

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

            fixed4 _VertexOffsetXVector;
            fixed4 _VertexOffsetYVector;

            v2g vert (appdata v)
            {
                v2g o;

                // 65.535 is equivalent to (2^16 - 1) / 1000, where (2^16 - 1) is to complement
                // the conversion happened in the texture-level from 0 ~ (2^16 - 1) to 0 ~ 1.
                // 1000 is the conversion of mm (the unit of Azure Kinect) to m (the unit of Unity3D).
                fixed depth = tex2Dlod(_DepthTex, fixed4(v.uv, 0, 0)).r * 65.535;
                // vertex in the depth camera coordinate system
                fixed3 depth_vertex = v.vertex * depth;
                
                // Below lines are following the logic of transformation_compute_correspondence in rgbz.c.
                //fixed3 color_vertex = mul(_DepthToColor, depth_vertex);
                // The below line comes from transformation_project() in intrinsic_transformation.c.
                //fixed2 xy = fixed2(color_vertex.x / color_vertex.z, color_vertex.y / color_vertex.z);

                // Can't understand why but the upper matrix version is not giving the same results
                // with the below element-wise version.
                fixed color_x = _R0 * depth_vertex.x + _R1 * depth_vertex.y + _R2 * depth_vertex.z + _T0;
                fixed color_y = _R3 * depth_vertex.x + _R4 * depth_vertex.y + _R5 * depth_vertex.z + _T1;
                fixed color_z = _R6 * depth_vertex.x + _R7 * depth_vertex.y + _R8 * depth_vertex.z + _T2;
                fixed2 xy = fixed2(color_x / color_z, color_y / color_z);

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

                fixed2 color_uv = fixed2((xp_d_cx * _Fx + _Cx) / (_Width - 1), (yp_d_cy * _Fy + _Cy) / (_Height - 1));

                //o.vertex = UnityObjectToClipPos(depth_vertex);
                o.vertex = fixed4(depth_vertex, 1.0);
                o.uv = color_uv;
                o.vertex_offset = v.uv2 * depth;
                o.uv_offset = v.uv3;

                return o;
            }

            [maxvertexcount(4)]
            void geom(point v2g i[1], inout TriangleStream<g2f> triangles)
            {
                // Filtering out invalid depth pixels and depth pixels without a corresponding color pixel.
                // Tried mad here, but it added cost.
                if (i[0].vertex.z > 0.1 && i[0].uv.x > 0.0 && i[0].uv.x < 1.0 && i[0].uv.y > 0.0 && i[0].uv.y < 1.0)
                {
                    g2f o;
                    // Tried using mvp matrix instead of the below one using vp matrix and unity_ObjectToWorld.
                    // It turns calculating vertex, offset_x, and offset_y from 6 matrix-vector multiplications into
                    // a matrix-matrix multiplication and 3 matrix-vector multiplications.
                    // Unfortunately, it didn't improved this shader...
                    fixed4 vertex = mul(UNITY_MATRIX_VP, mul(unity_ObjectToWorld, float4(i[0].vertex.xyz, 1.0)));
                    fixed4 offset_x = mul(UNITY_MATRIX_VP, mul(unity_ObjectToWorld, _VertexOffsetXVector * i[0].vertex_offset.x));
                    fixed4 offset_y = mul(UNITY_MATRIX_VP, mul(unity_ObjectToWorld, _VertexOffsetYVector * i[0].vertex_offset.y));

                    fixed uv_right = i[0].uv_offset.x;
                    fixed uv_up = i[0].uv_offset.y;

                    o.vertex = vertex;
                    o.uv = i[0].uv;
                    triangles.Append(o);

                    o.vertex = o.vertex + offset_x;
                    o.uv = i[0].uv + fixed2(i[0].uv_offset.x, 0.0);
                    triangles.Append(o);

                    o.vertex = vertex + offset_y;
                    o.uv = i[0].uv + fixed2(0.0, i[0].uv_offset.y);
                    triangles.Append(o);

                    o.vertex = vertex + offset_x + offset_y;
                    o.uv = i[0].uv + i[0].uv_offset;
                    triangles.Append(o);
                }
            }

            fixed4 frag(g2f i) : SV_Target
            {
                // Formula came from https://docs.microsoft.com/en-us/windows/desktop/medfound/recommended-8-bit-yuv-formats-for-video-rendering.
                // Commented code before optimization.
                //fixed c = (tex2D(_YTex, i.uv).r - 0.0625) * 1.164383;
                //fixed c = tex2D(_YTex, i.uv).r * 1.164383 - 0.072774;
                //fixed c = mad(tex2D(_YTex, i.uv).r, 1.164383, -0.072774);
                fixed c = mad(_YTex.Sample(sampler_YTex, i.uv).r, 1.164383, -0.072774);
                //fixed d = tex2D(_UTex, i.uv).r - 0.5;
                fixed d = _UTex.Sample(sampler_YTex, i.uv).r - 0.5;
                //fixed e = tex2D(_VTex, i.uv).r - 0.5;
                fixed e = _VTex.Sample(sampler_YTex, i.uv).r - 0.5;

                //return fixed4(c + 1.596027 * e, c - 0.391762 * d - 0.812968 * e, c + 2.017232 * d, 1.0);
                return fixed4(mad(1.596027, e, c), mad(-0.812968, e, mad(-0.391762, d, c)), mad(2.017232, d, c), 1.0);
            }
            ENDCG
        }
    }
}
