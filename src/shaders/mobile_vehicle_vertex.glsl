#version 120
// Compatibility bindings for the reLCS shader below.
#define VSOUT varying
#define in_pos (gl_Vertex.xyz)
#define in_normal (gl_Normal)
#define in_color (gl_Color)
#define in_tex0 vec2(gl_MultiTexCoord0.x,1.0-gl_MultiTexCoord0.y)
#define u_world mat4(1.0)
#define u_view gl_ModelViewMatrix
#define u_proj gl_ProjectionMatrix
uniform vec4 u_matColor,u_surfProps;
uniform vec3 uDirectionalColor,uSunDirection;
uniform float uFogStart,uFarClip;
#define surfAmbient (u_surfProps.x)
#define surfDiffuse (u_surfProps.z)
float DoFog(float w){return clamp((uFarClip-length((gl_ModelViewMatrix*gl_Vertex).xyz))/max(uFarClip-uFogStart,.001),0.0,1.0);}
vec3 DoDynamicLight(vec3 p,vec3 n){return uDirectionalColor*max(dot(normalize(n),normalize(-uSunDirection)),0.0);}
uniform vec4 u_amb;
uniform vec4 u_emiss;


VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT vec2 v_tex1;
VSOUT vec2 v_detailUv;
VSOUT vec3 vNativeEnvironmentDirection;
VSOUT float v_fog;
VSOUT vec2 v_reflData;

#define v_NdotV (v_reflData.x)
#define v_lightingCont (v_reflData.y)

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_world) * in_normal;

	v_tex0 = in_tex0;
    v_detailUv = gl_MultiTexCoord1.xy;
    vec3 eye = (gl_ModelViewMatrix * gl_Vertex).xyz;
    vNativeEnvironmentDirection = reflect(normalize(eye + vec3(0.00001)),
        normalize(gl_NormalMatrix * gl_Normal + vec3(0.00001)));

	vec3 ViewNormal = mat3(u_view) * Normal;
	v_tex1 = (ViewNormal.xy + vec2(1.0, 1.0))*0.5;

	v_color = in_color;
	vec4 combinedAmbient = mix(u_emiss, u_amb, Normal.z);
	v_color.rgb += combinedAmbient.rgb*surfAmbient;
	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;
	v_lightingCont = max(0.5, (v_color.r + v_color.g + v_color.b) / 3.0);
	v_color *= u_matColor;

	// for fresnel
	vec3 camPos = -u_view[3].xyz * mat3(u_view);
	vec3 viewVec = normalize(Vertex.xyz - camPos);
	v_NdotV = 1.0 - dot(-Normal.xyz, viewVec.xyz);

	v_fog = DoFog(gl_Position.w);
}