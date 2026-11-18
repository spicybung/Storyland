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

#define surfEmissive (u_surfProps.w)


VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT float v_fog;

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_world) * in_normal;

	v_tex0 = in_tex0;

	v_color = in_color;
	v_color.rgb *= u_amb.rgb;
	v_color.rgb += u_emiss.rgb*surfEmissive;
	v_color = clamp(v_color, 0.0, 1.0);
	v_color.a *= u_matColor.a;

	v_fog = DoFog(gl_Position.w);
}