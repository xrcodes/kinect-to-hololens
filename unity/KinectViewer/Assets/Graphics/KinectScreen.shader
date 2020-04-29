// Made this shader work with single-pass instanced rendering using code of built-in shader SpatialMappingWireframe.
Shader "KinectViewer/KinectScreen"
{
    Properties
    {
        _YTex("Y Texture", 2D) = "white" {}
        _UvTex("UV Texture", 2D) = "white" {}
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
                fixed3 vertex : POSITION;
                fixed2 uv : TEXCOORD0;
                fixed2 size : TEXCOORD1;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct v2g
            {
                fixed4 vertex : POSITION;
                fixed2 uv : TEXCOORD0;
                fixed2 size : TEXCOORD1;
                UNITY_VERTEX_OUTPUT_STEREO_EYE_INDEX
            };

            struct g2f
            {
                fixed4 vertex : SV_POSITION;
                fixed2 uv : TEXCOORD0;
                UNITY_VERTEX_OUTPUT_STEREO
            };

            Texture2D _YTex;
            Texture2D _UvTex;
            SamplerState sampler_YTex;
            sampler2D _DepthTex;
            fixed4x4 _ModelMatrix;
            fixed4 _SizeDirectionX;
            fixed4 _SizeDirectionY;

            v2g vert (appdata v)
            {
                v2g o;
                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_OUTPUT_STEREO_EYE_INDEX(o);

                // 65.535 is equivalent to (2^16 - 1) / 1000, where (2^16 - 1) is to complement
                // the conversion happened in the texture-level from 0 ~ (2^16 - 1) to 0 ~ 1.
                // 1000 is the conversion of mm (the unit of Azure Kinect) to m (the unit of Unity3D).
                // TODO: Move multiplication of 65.535 to mesh.
                fixed depth = tex2Dlod(_DepthTex, fixed4(v.uv, 0, 0)).r * 65.535;
                
                o.vertex = fixed4(v.vertex * depth, 1.0);
                o.uv = v.uv;
                o.size = v.size * depth;

                return o;
            }

            [maxvertexcount(4)]
            void geom(point v2g i[1], inout TriangleStream<g2f> triangles)
            {
                // Filtering out invalid depth pixels.
                if (i[0].vertex.z > 0.1)
                {
                    g2f o;
                    UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(i[0]);
                    UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);

                    // Tried using mvp matrix instead of the below one using vp matrix and unity_ObjectToWorld.
                    // It turns calculating vertex, offset_x, and offset_y from 6 matrix-vector multiplications into
                    // a matrix-matrix multiplication and 3 matrix-vector multiplications.
                    // Unfortunately, it didn't improved this shader...
                    
                    // Using _ModelMatrix since UNITY_MATRIX_M is not working inside the geometry shader.
                    // Seems like it is an identity matrix, especially for the universal rendering pipeline.
                    // If Unity fixes this bug, use UNITY_MATRIX_M instead of _ModelMatrix.

                    // It is not possible to prepare the whole MVP_MATRIX for vertex from the script
                    // since left/right camera has different positions.
                    // However, for the size vectors, it is possible since they are directions that ignores
                    // translations. Since camera positions only relate to the view matrix's translation,
                    // mul(UNITY_MATRIX_VP, _SizeDirectionX) can be prepared from the script-side.
                    fixed4 vertex = mul(UNITY_MATRIX_VP, mul(_ModelMatrix, i[0].vertex));
                    //fixed4 size_x = mul(UNITY_MATRIX_VP, _SizeDirectionX) * i[0].size.x;
                    //fixed4 size_y = mul(UNITY_MATRIX_VP, _SizeDirectionY) * i[0].size.y;
                    fixed4 size_x = _SizeDirectionX * i[0].size.x;
                    fixed4 size_y = _SizeDirectionY * i[0].size.y;

                    // TODO: make mesh in a way that this step can be skipped.
                    o.vertex = vertex - size_x * 0.5 - size_y * 0.5;

                    // This does not optimize code since the code above already was converted to mad by the optimizer.
                    //o.vertex = mad(offset_x + offset_y, -0.5, vertex);

                    o.uv = i[0].uv;
                    triangles.Append(o);

                    o.vertex = o.vertex + size_x;
                    triangles.Append(o);

                    o.vertex = o.vertex - size_x + size_y;
                    triangles.Append(o);

                    o.vertex = o.vertex + size_x;
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
                                    _UvTex.Sample(sampler_YTex, i.uv).rg,
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
