Shader "KinectToHololens/AzureKinectGeometryShader"
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
            #pragma geometry geom
            #pragma fragment frag

            #include "UnityCG.cginc"

            struct appdata
            {
                float3 vertex : POSITION;
                float2 uv : TEXCOORD0;
                float2 uv2 : TEXCOORD1;
            };

            struct v2g
            {
                fixed4 vertex : POSITION;
                fixed2 uv : TEXCOORD0;
                fixed2 vertex_offset : TEXCOORD1;
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

                o.vertex = fixed4(depth_vertex, 1.0);
                o.uv = v.uv;
                o.vertex_offset = v.uv2 * depth;

                return o;
            }

            [maxvertexcount(4)]
            void geom(point v2g i[1], inout TriangleStream<g2f> triangles)
            {
                // Filtering out invalid depth pixels.
                // Tried mad here, but it added cost.
                if (i[0].vertex.z > 0.1)
                {
                    g2f o;
                    // Tried using mvp matrix instead of the below one using vp matrix and unity_ObjectToWorld.
                    // It turns calculating vertex, offset_x, and offset_y from 6 matrix-vector multiplications into
                    // a matrix-matrix multiplication and 3 matrix-vector multiplications.
                    // Unfortunately, it didn't improved this shader...
                    fixed4 vertex = mul(UNITY_MATRIX_VP, mul(unity_ObjectToWorld, float4(i[0].vertex.xyz, 1.0)));
                    fixed4 offset_x = mul(UNITY_MATRIX_VP, mul(unity_ObjectToWorld, _VertexOffsetXVector * i[0].vertex_offset.x));
                    fixed4 offset_y = mul(UNITY_MATRIX_VP, mul(unity_ObjectToWorld, _VertexOffsetYVector * i[0].vertex_offset.y));

                    // This does not work:
                    // fixed4 vertex = UnityObjectToClipPos(float4(i[0].vertex.xyz, 1.0));
                    // fixed4 offset_x = UnityObjectToClipPos(_VertexOffsetXVector * i[0].vertex_offset.x);
                    // fixed4 offset_y = UnityObjectToClipPos(_VertexOffsetYVector * i[0].vertex_offset.y);

                    o.vertex = vertex - offset_x * 0.5 - offset_y * 0.5;

                    // This does not optimize code since the code above already was converted to mad by the optimizer.
                    //o.vertex = mad(offset_x + offset_y, -0.5, vertex);

                    o.uv = i[0].uv;
                    triangles.Append(o);

                    o.vertex = o.vertex + offset_x;
                    triangles.Append(o);

                    o.vertex = o.vertex - offset_x + offset_y;
                    triangles.Append(o);

                    o.vertex = o.vertex + offset_x;
                    triangles.Append(o);
                }
            }

            fixed4 frag(g2f i) : SV_Target
            {
                // Formula came from https://docs.microsoft.com/en-us/windows/desktop/medfound/recommended-8-bit-yuv-formats-for-video-rendering.
                // Based on results from the profiler, conversion does not take that much time.
                // Time per frame:
                //   with conversion: 43 ms
                //   without conversion: 40 ms
                fixed4 yuv = fixed4(_YTex.Sample(sampler_YTex, i.uv).r,
                                    _UTex.Sample(sampler_YTex, i.uv).r,
                                    _VTex.Sample(sampler_YTex, i.uv).r,
                                    1.0);

                return fixed4(dot(yuv, fixed4(1.164383,         0,  1.596027, -0.870787)),
                              dot(yuv, fixed4(1.164383, -0.391762, -0.812968,  0.529591)),
                              dot(yuv, fixed4(1.164383,  2.017232,         0, -1.081390)),
                              1.0);
            }
            ENDCG
        }
    }
}
