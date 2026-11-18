#version 120
#define FSIN varying
#define texture texture2D
#define FRAGCOLOR(c) gl_FragColor=(c)
uniform vec3 uFogColor;
#define u_fogColor vec4(uFogColor,1.0)
void DoAlphaTest(float a){if(a<.08)discard;}
uniform sampler2D tex0;
uniform vec4 u_colorscale;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;

void
main(void)
{
	vec4 color;
	color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y))*u_colorscale;
	color.rgb = clamp(color.rgb, 0.0, 1.0);
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a);

	FRAGCOLOR(color);
}

