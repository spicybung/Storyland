#ifdef ENVMAP
uniform mat4 u_texMatrix;
#endif
#ifdef SKIN
uniform mat4 u_boneMatrices[64];
#endif

VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
#ifdef ENVMAP
VSOUT vec2 v_tex1;
#endif
VSOUT float v_fog;

VSOUT vec3 v_worldPos;
VSOUT vec3 v_worldNormal;

void
main(void)
{
#ifdef SKIN
	vec3 SkinVertex = vec3(0.0, 0.0, 0.0);
	vec3 SkinNormal = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < 4; i++){
		SkinVertex += (u_boneMatrices[int(in_indices[i])] * vec4(in_pos, 1.0)).xyz * in_weights[i];
		SkinNormal += (mat3(u_boneMatrices[int(in_indices[i])]) * in_normal) * in_weights[i];
	}

	vec4 Vertex = u_world * vec4(SkinVertex, 1.0);
	vec3 Normal = mat3(u_world) * SkinNormal;
#else
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	vec3 Normal = mat3(u_world) * in_normal;
#endif

	gl_Position = u_proj * u_view * Vertex;

	// Pass world-space data for per-pixel lighting
	v_worldPos = Vertex.xyz;
	v_worldNormal = normalize(Normal);

	v_tex0 = in_tex0;
#ifdef ENVMAP
	// Keep envmap coords derived from the world normal (same behavior as before)
	v_tex1 = (u_texMatrix * vec4(v_worldNormal, 1.0)).xy;
#endif

	// IMPORTANT CHANGE:
	// v_color is now "base color" only (prelight * material), NOT pre-baked lighting.
	// The lighting moves to the fragment shader.
	v_color = clamp(in_color, 0.0, 1.0) * u_matColor;

	v_fog = DoFog(gl_Position.w);
}
