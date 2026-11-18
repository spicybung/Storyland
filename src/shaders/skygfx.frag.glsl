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
uniform vec3 uDualTextureScale;
#define STORYLAND_STYLE 0
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec3 uAmbientColor;
uniform vec3 uDirectionalColor;
uniform vec3 uLightEye;
uniform vec3 uFogColor;
uniform float uDualTextureStrength;
uniform float uNativeMaterialEffect;
uniform float uReflection;
uniform int uUseTexture;
uniform int uDualTextureMode;
uniform int uDualTextureUvSet;
uniform int uNeoVehicle;
uniform int uPrelightMode;
uniform int uSkyGfxGame;
uniform int uRenderMode;
varying vec4 vMaterialColor;
varying vec2 vBaseUv;
varying vec2 vDetailUv;
varying vec2 vEnvironmentUv;
varying vec3 vNormalEye;
varying vec3 vViewEye;
varying float vFogFactor;
varying float vReflectionFresnel;
float lightContribution(vec3 normal, vec3 sun) {
    float lengthOfSun = length(sun);
    if (lengthOfSun <= 0.00001) return 0.0;
    return max(dot(normal, sun / lengthOfSun), 0.0);
}
void main() {
    vec4 base = uUseTexture != 0 ? texture2D(tex0, vBaseUv) : vec4(1.0);
    if (base.a * vMaterialColor.a < 0.045) discard;
    vec3 normalEye = normalize(vNormalEye);
    vec3 eyeDirection = normalize(vViewEye);
    float diffuse = lightContribution(normalEye, uLightEye);
    vec3 ambient = max(uAmbientColor, vec3(0.0));
    vec3 direct = max(uDirectionalColor, vec3(0.0));
    vec3 colour;
#if STORYLAND_STYLE == 0
    // SkyGFX Leeds world: prelight interacts with the timecycle ambient and
    // material emissive, while vehicles take native directional illumination.
    if (uNeoVehicle != 0) {
        colour = base.rgb * vMaterialColor.rgb * clamp(ambient + direct * diffuse, 0.0, 1.35);
    } else {
        colour = base.rgb * vMaterialColor.rgb * clamp(ambient * 2.0 + direct * diffuse * 0.28, 0.0, 1.4);
    }
    if (uPrelightMode == 1) colour = base.rgb * vMaterialColor.rgb;
    if (uPrelightMode == 2) colour = base.rgb * clamp(ambient + direct * diffuse, 0.0, 1.35);
#elif STORYLAND_STYLE == 1
    // Cel: four discrete light bands, pronounced ink at grazing angles.
    float band = diffuse < 0.18 ? 0.32 : diffuse < 0.46 ? 0.55 : diffuse < 0.77 ? 0.78 : 1.0;
    float contour = smoothstep(0.035, 0.23, abs(dot(normalEye, eyeDirection)));
    vec3 light = clamp(ambient * 1.30 + direct * band, 0.0, 1.4);
    colour = base.rgb * vMaterialColor.rgb * light * mix(0.10, 1.0, contour);
#elif STORYLAND_STYLE == 2
    // Original Budokai 1 look: soft hand-painted tonal bands, restrained
    // specular and a darker ink contour rather than a copied game shader.
    float band = smoothstep(0.14, 0.38, diffuse) * 0.37 + smoothstep(0.53, 0.85, diffuse) * 0.43 + 0.20;
    float contour = smoothstep(0.065, 0.22, abs(dot(normalEye, eyeDirection)));
    vec3 light = clamp(ambient * 0.9 + direct * band * 1.35, 0.0, 1.45);
    colour = base.rgb * vMaterialColor.rgb * light * mix(0.21, 1.0, contour);
    vec3 sun = normalize(uLightEye + vec3(0.00001));
    vec3 halfVector = normalize(eyeDirection + sun);
    float specular = pow(max(dot(normalEye, halfVector), 0.0), 36.0);
    colour += vec3(1.0, 0.88, 0.68) * specular * 0.13;
#endif
    if (uDualTextureMode == 1) {
        vec2 uv = uDualTextureUvSet != 0 ? vDetailUv : vBaseUv;
        vec3 secondary = texture2D(tex1, uv * uDualTextureScale.xy).rgb;
        float strength = clamp(uDualTextureStrength, 0.0, 1.0);
        if (uSkyGfxGame == 0) {
            colour *= max(vec3(0.0), vec3(1.0) + strength * (2.0 * secondary - vec3(1.0)));
        } else {
            colour *= mix(vec3(1.0), secondary, strength);
        }
    } else if (uDualTextureMode == 2 || uDualTextureMode == 4) {
        vec2 uv = uDualTextureMode == 4 ? vehicleEnvironmentAtlasUv(reflect(-eyeDirection, normalEye)) :
            vEnvironmentUv * uDualTextureScale.xy;
        vec4 environment = texture2D(tex1, uv);
        float reflection = clamp(uDualTextureStrength, 0.0, 1.0);
        colour += environment.rgb * environment.a * reflection;
    } else if (uDualTextureMode == 3) {
        vec2 uv = (uDualTextureUvSet != 0 ? vDetailUv : vBaseUv) * uDualTextureScale.xy;
        vec4 secondary = texture2D(tex1, uv);
        colour = mix(colour, secondary.rgb, secondary.a * clamp(uDualTextureStrength, 0.0, 1.0));
    }
    gl_FragColor = vec4(mix(uFogColor, clamp(colour, 0.0, 1.0), vFogFactor),
        base.a * vMaterialColor.a);
}
