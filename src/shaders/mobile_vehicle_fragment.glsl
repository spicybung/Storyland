#version 120
uniform vec3 uEnvironmentViewRight;
uniform vec3 uEnvironmentViewUp;
uniform vec3 uEnvironmentViewBack;
uniform float uEnvironmentFaceSize;
vec2 vehicleEnvironmentAtlasUv(vec3 eyeDirection) {
    vec3 d = uEnvironmentViewRight * eyeDirection.x +
        uEnvironmentViewUp * eyeDirection.y + uEnvironmentViewBack * eyeDirection.z;
    vec3 a = abs(d);
    vec2 faceUv;
    float face;
    if (a.x >= a.y && a.x >= a.z) {
        face = d.x >= 0.0 ? 0.0 : 1.0;
        faceUv = vec2(d.x >= 0.0 ? -d.z : d.z, -d.y) / max(a.x, 0.00001);
    } else if (a.y >= a.z) {
        face = d.y >= 0.0 ? 2.0 : 3.0;
        faceUv = vec2(d.x, d.y >= 0.0 ? d.z : -d.z) / max(a.y, 0.00001);
    } else {
        face = d.z >= 0.0 ? 4.0 : 5.0;
        faceUv = vec2(d.z >= 0.0 ? d.x : -d.x, -d.y) / max(a.z, 0.00001);
    }
    float inset = 0.5 / max(uEnvironmentFaceSize, 1.0);
    faceUv = clamp(faceUv * 0.5 + vec2(0.5), vec2(inset), vec2(1.0 - inset));
    return (vec2(mod(face, 3.0), floor(face / 3.0)) + faceUv) / vec2(3.0, 2.0);
}
uniform int uUseTexture,uDualTextureMode,uDualTextureUvSet;
uniform float uDualTextureStrength;
uniform vec3 uDualTextureScale;
#define FSIN varying
#define texture texture2D
#define FRAGCOLOR(c) gl_FragColor=(c)
uniform vec3 uFogColor;
#define u_fogColor vec4(uFogColor,1.0)
void DoAlphaTest(float a){if(a<.08)discard;}
uniform sampler2D tex0;
uniform sampler2D tex1;

uniform float u_shininess;
uniform vec3 u_skyTop;
uniform vec3 u_skyBot;

// matfx:
// case 1	normal envmap
// 	custom1 (4.0,  1.0, 1.0, coef)
// 	custom2 (0.25, 3.0, 1.0, 1.0)
// case 2	too strong
// 	custom1 (4.0, 1.0, 2.0, coef)
// 	custom2 (0.5, 3.0, 1.0, 1.0)
// ???:		practically no fresnel
// 	custom1 (4.0, 1.25, 0.01, coef)
// 	custom2 (1.0, 2.0,  1.1,  2.0)

#define power (4.0)

#define preMult (1.0)
#define postMult (1.0)
#define minRefl (0.25)
#define maxRefl (3.0)
#define minOpacity (1.0)
#define maxOpacity (1.0)

//#define preMult (1.0)
//#define postMult (2.0)
//#define minRefl (0.5)
//#define maxRefl (3.0)
//#define minOpacity (1.0)
//#define maxOpacity (1.0)

//#define preMult (1.25)
//#define postMult (0.01)
//#define minRefl (1.0)
//#define maxRefl (2.0)
//#define minOpacity (1.1)
//#define maxOpacity (2.0)

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN vec2 v_tex1;
FSIN vec2 v_detailUv;
FSIN vec3 vNativeEnvironmentDirection;
FSIN float v_fog;
FSIN vec2 v_reflData;

#define v_NdotV (v_reflData.x)
#define v_lightingCont (v_reflData.y)

void
main(void)
{
	vec2 baseUv = vec2(v_tex0.x, 1.0-v_tex0.y);
    vec4 base = uUseTexture != 0 ? texture(tex0, baseUv) : vec4(1.0);
    vec3 environmentReflection = vec3(0.0);
    if (uDualTextureMode != 0) {
        vec2 uv = (uDualTextureUvSet != 0 ? v_detailUv : baseUv) * uDualTextureScale.xy;
        if (uDualTextureMode == 4) uv = vehicleEnvironmentAtlasUv(vNativeEnvironmentDirection);
        else if (uDualTextureMode == 2) uv = v_tex1 * uDualTextureScale.xy;
        vec4 second = texture(tex1, uv);
        float amount = clamp(uDualTextureStrength, 0.0, 1.0);
        if (uDualTextureMode == 1) base.rgb *= mix(vec3(1.0), second.rgb, amount);
        else if (uDualTextureMode == 3) base.rgb = mix(base.rgb, second.rgb, second.a * amount);
        else environmentReflection = second.rgb * second.a * amount;
    }
    vec4 color = v_color * base;
    color.rgb += environmentReflection;

	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);

	DoAlphaTest(color.a);

	FRAGCOLOR(color);
}
