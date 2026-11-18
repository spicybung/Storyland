#version 120
uniform sampler2D uSource;
uniform sampler2D uPosition;
uniform vec2 uDirection;
uniform int uExtract;
varying vec2 uv;

void main() {
    if (uExtract != 0) {
        vec3 color = texture2D(uSource, uv).rgb;
        float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
        float highlights = smoothstep(0.72, 1.0, brightness);
        float emissive = step(0.12, fract(texture2D(uPosition, uv).w)) * 0.8;
        gl_FragColor = vec4(color * max(highlights, emissive), 1.0);
        return;
    }

    vec3 glow = texture2D(uSource, uv).rgb * 0.227027;
    glow += texture2D(uSource, uv + uDirection * 1.384615).rgb * 0.316216;
    glow += texture2D(uSource, uv - uDirection * 1.384615).rgb * 0.316216;
    glow += texture2D(uSource, uv + uDirection * 3.230769).rgb * 0.070270;
    glow += texture2D(uSource, uv - uDirection * 3.230769).rgb * 0.070270;
    gl_FragColor = vec4(glow, 1.0);
}
