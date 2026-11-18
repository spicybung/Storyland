#pragma once

inline constexpr const char* StorylandFxaaVertexGl=R"GLSL(
#version 120
varying vec2 frameUv;
void main() {
    gl_Position=vec4(gl_Vertex.xy,0.0,1.0);
    frameUv=gl_MultiTexCoord0.xy;
}
)GLSL";

inline constexpr const char* StorylandFxaaFragmentGl=R"GLSL(
#version 120
uniform sampler2D sceneTexture;
uniform vec2 inverseFrame;
varying vec2 frameUv;

float luminance(vec3 color) {
    return dot(color,vec3(0.299,0.587,0.114));
}

void main() {
    vec4 center=texture2D(sceneTexture,frameUv);
    float middle=luminance(center.rgb);
    float northwest=luminance(texture2D(sceneTexture,frameUv+inverseFrame*vec2(-1.0,-1.0)).rgb);
    float northeast=luminance(texture2D(sceneTexture,frameUv+inverseFrame*vec2(1.0,-1.0)).rgb);
    float southwest=luminance(texture2D(sceneTexture,frameUv+inverseFrame*vec2(-1.0,1.0)).rgb);
    float southeast=luminance(texture2D(sceneTexture,frameUv+inverseFrame*vec2(1.0,1.0)).rgb);
    float low=min(middle,min(min(northwest,northeast),min(southwest,southeast)));
    float high=max(middle,max(max(northwest,northeast),max(southwest,southeast)));
    if(high-low<max(1.0/32.0,high/8.0)) {
        gl_FragColor=center;
        return;
    }
    vec2 direction=vec2(-((northwest+northeast)-(southwest+southeast)),
        (northwest+southwest)-(northeast+southeast));
    float reduction=max((northwest+northeast+southwest+southeast)/32.0,1.0/128.0);
    float scale=1.0/(min(abs(direction.x),abs(direction.y))+reduction);
    direction=clamp(direction*scale,vec2(-8.0),vec2(8.0))*inverseFrame;
    vec3 inner=(texture2D(sceneTexture,frameUv-direction/6.0).rgb+
        texture2D(sceneTexture,frameUv+direction/6.0).rgb)*0.5;
    vec3 outer=inner*0.5+(texture2D(sceneTexture,frameUv-direction*0.5).rgb+
        texture2D(sceneTexture,frameUv+direction*0.5).rgb)*0.25;
    float outerLuma=luminance(outer);
    gl_FragColor=vec4(outerLuma<low||outerLuma>high?inner:outer,center.a);
}
)GLSL";
