#version 120
uniform sampler2D diffuseTexture;
uniform bool uUseTexture;
uniform float alphaCutoff;
uniform vec3 ambientColour;
uniform vec3 sunColour;
uniform vec3 specularColour;
uniform bool nativeFog;
uniform bool orthographicView;
uniform vec3 sunEye;
uniform vec3 upEye;
uniform vec3 fogColour;
uniform float fogStart;
uniform float fogEnd;
uniform float fogAmount;
uniform float lightning;
uniform float wetness;
uniform float specularStrength;
uniform float roughness;
varying vec3 eyePosition;
varying vec2 textureCoordinate;
varying vec4 materialColour;

void main() {
    vec4 surface = materialColour;
    if (uUseTexture) surface *= texture2D(diffuseTexture, textureCoordinate);
    if (surface.a < alphaCutoff) discard;

    vec3 normal = cross(dFdx(eyePosition), dFdy(eyePosition));
    normal /= max(length(normal), 0.00001);
    if (!gl_FrontFacing) normal = -normal;
    vec3 light = sunEye / max(length(sunEye), 0.00001);
    vec3 view = orthographicView ? vec3(0.0, 0.0, 1.0) :
        -eyePosition / max(length(eyePosition), 0.00001);
    float diffuse = max(dot(normal, light), 0.0);
    float wet = clamp(wetness, 0.0, 1.0) * smoothstep(0.25, 0.85, dot(normal, upEye));
    vec3 colour = surface.rgb * (ambientColour + sunColour * diffuse + vec3(lightning));
    colour *= 1.0 - wet * 0.16;

    vec3 halfVector = light + view;
    halfVector /= max(length(halfVector), 0.00001);
    float normalView = max(dot(normal, view), 0.0);
    float normalHalf = max(dot(normal, halfVector), 0.0);
    float viewHalf = max(dot(view, halfVector), 0.0);
    float surfaceRoughness = mix(clamp(roughness, 0.08, 1.0), 0.12, wet);
    float alpha = surfaceRoughness * surfaceRoughness;
    float alphaSquared = alpha * alpha;
    float denominator = normalHalf * normalHalf * (alphaSquared - 1.0) + 1.0;
    float distribution = alphaSquared / max(3.14159265 * denominator * denominator, 0.00001);
    float geometryFactor = (surfaceRoughness + 1.0) * (surfaceRoughness + 1.0) * 0.125;
    float geometryView = normalView / max(normalView * (1.0 - geometryFactor) + geometryFactor, 0.00001);
    float geometryLight = diffuse / max(diffuse * (1.0 - geometryFactor) + geometryFactor, 0.00001);
    float fresnel = 0.04 + 0.96 * pow(1.0 - viewHalf, 5.0);
    float specular = distribution * geometryView * geometryLight * fresnel /
        max(4.0 * normalView * diffuse, 0.00001);
    float strength = max(clamp(specularStrength, 0.0, 1.0), wet * 0.65);
    colour += specularColour * min(specular, 8.0) * diffuse * strength;

    // CTW timecycle fog uses view depth.
    float fog = clamp((-eyePosition.z - fogStart) / max(fogEnd - fogStart, 0.00001), 0.0, 1.0);
    if (!nativeFog) fog = fog * fog * (3.0 - 2.0 * fog);
    gl_FragColor = vec4(mix(colour, fogColour, fog * clamp(fogAmount, 0.0, 1.0)), surface.a);
}
