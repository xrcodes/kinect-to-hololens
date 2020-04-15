Shader "KinectToHololens/AzureKinectShader"
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
            #pragma fragment frag

            #include "UnityCG.cginc"

            struct appdata
            {
                float3 vertex : POSITION;
                float2 uv : TEXCOORD0;

                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct v2f
            {
                float4 vertex : SV_POSITION;
                float2 uv : TEXCOORD0;

                UNITY_VERTEX_OUTPUT_STEREO
            };

            Texture2D _YTex;
            Texture2D _UvTex;
            SamplerState sampler_YTex;
            sampler2D _DepthTex;

            v2f vert (appdata v)
            {
                v2f o;

                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_OUTPUT(v2f, o);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);

                // 65.535 is equivalent to (2^16 - 1) / 1000, where (2^16 - 1) is to complement
                // the conversion happened in the texture-level from 0 ~ (2^16 - 1) to 0 ~ 1.
                // 1000 is the conversion of mm (the unit of Azure Kinect) to m (the unit of Unity3D).
                fixed depth = tex2Dlod(_DepthTex, fixed4(v.uv, 0, 0)).r * 65.535;
                o.vertex = UnityObjectToClipPos(v.vertex * depth);
                o.uv = v.uv;

                return o;
            }

            fixed4 frag(v2f i) : SV_Target
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
