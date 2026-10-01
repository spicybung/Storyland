#include "storyland_shaders.h"

const char* storylandStoriesVertexShaderSource() {
    return
        "#version 120\n"
        "varying vec4 vColor;\n"
        "varying vec2 vTex0;\n"
        "varying vec3 vLighting;\n"
        "varying float vFog;\n"
        "uniform vec3 uAmbientColor;\n"
        "uniform vec3 uDirectionalColor;\n"
        "uniform vec3 uSunDirection;\n"
        "uniform float uFogStart;\n"
        "uniform float uFarClip;\n"
        "void main(){\n"
        "  vec4 eyePos = gl_ModelViewMatrix * gl_Vertex;\n"
        "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "  vColor = gl_Color;\n"
        "  vTex0 = gl_MultiTexCoord0.xy;\n"
        "  vec3 n = normalize(gl_NormalMatrix * gl_Normal);\n"
        "  vec3 lightDir = normalize(gl_NormalMatrix * uSunDirection);\n"
        "  float ndl = max(dot(n, lightDir), 0.0);\n"
        "  vLighting = clamp(uAmbientColor + uDirectionalColor * ndl, 0.0, 1.0);\n"
        "  float distanceFromCamera = length(eyePos.xyz);\n"
        "  float fogRange = max(uFarClip - uFogStart, 0.001);\n"
        "  vFog = clamp((uFarClip - distanceFromCamera) / fogRange, 0.0, 1.0);\n"
        "}\n";
}

const char* storylandStoriesFragmentShaderSource() {
    return
        "#version 120\n"
        "uniform sampler2D tex0;\n"
        "uniform int uUseTexture;\n"
        "uniform int uRenderMode;\n"
        "uniform vec3 uFogColor;\n"
        "varying vec4 vColor;\n"
        "varying vec2 vTex0;\n"
        "varying vec3 vLighting;\n"
        "varying float vFog;\n"
        "void main(){\n"
        "  vec4 base = vColor;\n"
        "  if(uUseTexture != 0){\n"
        "    base *= texture2D(tex0, vTex0);\n"
        "  }\n"
        "  vec3 lit = base.rgb;\n"
        "  if(uRenderMode == 0 || uRenderMode == 2){\n"
        "    lit = base.rgb * vLighting;\n"
        "  }\n"
        "  lit = mix(uFogColor, clamp(lit, 0.0, 1.0), vFog);\n"
        "  gl_FragColor = vec4(lit, base.a);\n"
        "}\n";
}
