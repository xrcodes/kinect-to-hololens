// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

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
                fixed2 half_size : TEXCOORD1;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct v2g
            {
                fixed4 left_bottom : POSITION0;
                fixed4 right_bottom : POSITION1;
                fixed4 left_top : POSITION2;
                fixed4 right_top : POSITION3;
                fixed2 uv : TEXCOORD0;
                UNITY_VERTEX_OUTPUT_STEREO_EYE_INDEX
            };

            struct g2f
            {
                fixed4 vertex : SV_POSITION;
                fixed2 uv : TEXCOORD0;
                UNITY_VERTEX_OUTPUT_STEREO
            };

            sampler2D _YTex;
            sampler2D _UvTex;
            sampler2D _DepthTex;
            fixed3 _SizeDirectionX;
            fixed3 _SizeDirectionY;

            v2g vert (appdata v)
            {
                v2g o;
                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_OUTPUT_STEREO_EYE_INDEX(o);

                fixed depth = tex2Dlod(_DepthTex, fixed4(v.uv, 0, 0)).r;
                fixed3 half_size_x = v.half_size.x * _SizeDirectionX;
                fixed3 half_size_y = v.half_size.y * _SizeDirectionY;

                o.left_bottom = UnityObjectToClipPos((v.vertex - half_size_x - half_size_y) * depth);
                o.right_bottom = UnityObjectToClipPos((v.vertex + half_size_x - half_size_y) * depth);
                o.left_top = UnityObjectToClipPos((v.vertex - half_size_x + half_size_y) * depth);
                o.right_top = UnityObjectToClipPos((v.vertex + half_size_x + half_size_y) * depth);
                o.uv = v.uv;

                return o;
            }

            [maxvertexcount(4)]
            void geom(point v2g i[1], inout TriangleStream<g2f> triangles)
            {
                g2f o;
                UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(i[0]);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);

                o.vertex = i[0].left_bottom;
                o.uv = i[0].uv;
                triangles.Append(o);

                o.vertex = i[0].right_bottom;
                triangles.Append(o);

                o.vertex = i[0].left_top;
                triangles.Append(o);

                o.vertex = i[0].right_top;
                triangles.Append(o);
            }

            fixed4 frag(g2f i) : SV_Target
            {
                // Formula came from https://docs.microsoft.com/en-us/windows/desktop/medfound/recommended-8-bit-yuv-formats-for-video-rendering.
                // Based on results from the profiler, conversion does not take that much time.
                // Time per frame:
                //   with conversion: 43 ms
                //   without conversion: 40 ms
                fixed4 yuv = fixed4(tex2D(_YTex, i.uv).r,
                                    tex2D(_UvTex, i.uv).rg,
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
