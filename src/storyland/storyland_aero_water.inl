// Aero water: reLCS-style world-space waves, Fresnel/timecycle reflection,
// multilayer normal detail and Ultra High player reaction-diffusion.
static bool beginAeroWater(float time,bool textured,const StorylandWaterReaction* reaction,GLuint reactionTexture) {
    if(!gAeroEnabled||storylandDx::active()||!loadStoriesShaderFunctions())return false;
    static GLuint program=0;
    static HGLRC owner=nullptr;
    if(owner!=wglGetCurrentContext()) {program=0;owner=wglGetCurrentContext();}
    if(!program) {
        const char* vertex=R"GLSL(#version 120
uniform float time,wind;
varying vec3 worldPosition,eyePosition;
float waterHeight(vec2 p) {
 float strength=clamp(wind,0.0,1.0);
 return (.035+.080*strength)*sin(dot(p,vec2(.095,.034))+time*.72)
      + (.026+.048*strength)*sin(dot(p,vec2(-.041,.108))+time*.91+1.7)
      + (.070+.070*strength)*sin(dot(p,vec2(.018,.011))+time*.31+.9)
      + (.055+.050*strength)*sin(dot(p,vec2(-.013,.021))+time*.24+2.6);
}
void main() {
 vec4 p=gl_Vertex;
 p.z+=waterHeight(p.xy);
 worldPosition=p.xyz;
 eyePosition=(gl_ModelViewMatrix*p).xyz;
 gl_Position=gl_ModelViewProjectionMatrix*p;
})GLSL";
        const char* fragment=R"GLSL(#version 120
uniform sampler2D waterTexture,reactionTexture;
uniform int textured,reactionEnabled;
uniform float time,wind,fogStart,fogEnd,rain,sunVisibility;
uniform vec3 cameraPosition,skyHorizon,skyZenith,skyLower,sunDirection,sunColor,fogColor;
uniform vec3 reactionOrigin,reactionInverseSize,reactionTexel;
varying vec3 worldPosition,eyePosition;
vec3 reactionValues(vec2 p) {
 if(reactionEnabled==0)return vec3(0.0);
 vec2 uv=(p-reactionOrigin.xy)*reactionInverseSize.xy;
 if(uv.x<=0.015||uv.y<=0.015||uv.x>=.985||uv.y>=.985)return vec3(0.0);
 float center=texture2D(reactionTexture,uv).r;
 float dx=texture2D(reactionTexture,uv+vec2(reactionTexel.x,0.0)).r
         -texture2D(reactionTexture,uv-vec2(reactionTexel.x,0.0)).r;
 float dy=texture2D(reactionTexture,uv+vec2(0.0,reactionTexel.y)).r
         -texture2D(reactionTexture,uv-vec2(0.0,reactionTexel.y)).r;
 return vec3(center,dx,dy);
}
vec2 waveSlope(vec2 p) {
 float strength=clamp(wind,0.0,1.0);
 vec2 slope=(.035+.080*strength)*vec2(.095,.034)*cos(dot(p,vec2(.095,.034))+time*.72);
 slope+=(.026+.048*strength)*vec2(-.041,.108)*cos(dot(p,vec2(-.041,.108))+time*.91+1.7);
 slope+=(.070+.070*strength)*vec2(.018,.011)*cos(dot(p,vec2(.018,.011))+time*.31+.9);
 slope+=(.055+.050*strength)*vec2(-.013,.021)*cos(dot(p,vec2(-.013,.021))+time*.24+2.6);
 return slope;
}
void main() {
 vec2 p=worldPosition.xy;
 vec2 slope=waveSlope(p);
 // Multiscale world-space micro-ripples, not a repeating screen-space pattern.
 slope+=vec2(.034*cos(dot(p,vec2(.37,.18))+time*1.7),
             .028*cos(dot(p,vec2(-.17,.44))-time*1.31))*(.28+.72*wind);
 slope+=vec2(.015*cos(dot(p,vec2(.93,-.71))+time*2.2),
             .019*cos(dot(p,vec2(-.68,-.95))+time*1.8))*(.25+.75*wind);
 slope+=rain*.012*vec2(cos(dot(p,vec2(1.73,-1.29))+time*5.2),
                           cos(dot(p,vec2(-2.01,-1.54))-time*4.6));
 vec3 reaction=reactionValues(p);
 slope+=reaction.yz*.65;
 vec3 n=normalize(vec3(-slope,1.0));
 vec3 v=normalize(cameraPosition-worldPosition);
 if(v.z<0.0)n=-n;
 float fresnel=.020+.980*pow(1.0-clamp(dot(n,v),0.0,1.0),5.0);
 vec3 reflection=reflect(-v,n);
 vec3 sky=mix(skyHorizon,skyZenith,smoothstep(0.0,1.0,reflection.z));
 vec3 lower=mix(skyHorizon,skyLower,smoothstep(0.0,.65,-reflection.z));
 vec3 environment=mix(lower,sky,smoothstep(-.12,.14,reflection.z));
 vec3 detail=vec3(.5);
 if(textured!=0) {
  detail=texture2D(waterTexture,p*.028+time*vec2(.0042,.0029)).rgb*.48;
  detail+=texture2D(waterTexture,vec2(dot(p,vec2(.0211,.0174)),dot(p,vec2(-.0174,.0211)))
                        +time*vec2(-.0025,.0036)).rgb*.32;
  detail+=texture2D(waterTexture,vec2(dot(p,vec2(.0487,-.0196)),dot(p,vec2(.0196,.0487)))
                        +time*vec2(.0017,-.0021)).rgb*.20;
 }
 // The body colour follows the current timecycle, including sunset and night.
 vec3 ambient=max(.055*skyZenith+.085*skyHorizon,vec3(.01));
 vec3 body=ambient*vec3(.50,1.02,.88)*(.7+dot(detail,vec3(.16,.28,.10)));
 body=mix(body,skyLower*.23,clamp(rain*.36,0.0,.36));
 vec3 color=mix(body,environment,clamp(.12+fresnel*.85,0.0,1.0));
 vec3 light=normalize(sunDirection);
 vec3 halfDirection=normalize(light+v);
 float ndh=max(dot(n,halfDirection),0.0);
 float sharp=pow(ndh,mix(240.0,65.0,max(wind,rain)));
 float broad=pow(ndh,mix(46.0,18.0,max(wind,rain)));
 color+=sunColor*(sharp*1.22+broad*.14)*max(light.z,0.0)*sunVisibility;
 color+=reaction.x*.075*mix(vec3(.22,.42,.45),skyHorizon,.45);
 float fog=smoothstep(fogStart,max(fogStart+.001,fogEnd),length(eyePosition));
 color=mix(color,fogColor,fog);
 gl_FragColor=vec4(clamp(color,0.0,1.5),clamp(.84+fresnel*.13,0.0,.98));
})GLSL";
        std::string error;
        GLuint vs=compileStoriesShader(GL_VERTEX_SHADER,vertex,error);
        if(!vs){gStoriesShaderStatus="Aero water vertex shader: "+error;return false;}
        GLuint fs=compileStoriesShader(GL_FRAGMENT_SHADER,fragment,error);
        if(!fs){pglDeleteShader(vs);gStoriesShaderStatus="Aero water fragment shader: "+error;return false;}
        program=pglCreateProgram();pglAttachShader(program,vs);pglAttachShader(program,fs);pglLinkProgram(program);
        pglDeleteShader(vs);pglDeleteShader(fs);
        GLint linked=0;pglGetProgramiv(program,GL_LINK_STATUS,&linked);
        if(!linked){pglDeleteProgram(program);program=0;gStoriesShaderStatus="Aero water shader link failed.";return false;}
    }
    float view[16],inverse[16];glGetFloatv(GL_MODELVIEW_MATRIX,view);
    if(!aeroInverse(view,inverse))return false;
    pglUseProgram(program);
    auto scalar=[&](const char* name,float value){pglUniform1f(pglGetUniformLocation(program,name),value);};
    auto color=[&](const char* name,const StorylandSkyColor& value){pglUniform3f(pglGetUniformLocation(program,name),value.r,value.g,value.b);};
    const auto& sky=gStoriesSky.state();
    scalar("time",time);scalar("wind",gPlayTestMissionVm.vcs?
        gPlayTestMissionEnvironment.vcsSwimmingBoundary.waterWind(sky.windStrength):std::clamp(sky.windStrength,0.0f,1.0f));
    scalar("rain",std::clamp(sky.rainIntensity,0.0f,1.0f));scalar("sunVisibility",gAeroSunVisibility+gAeroMoonVisibility*.32f);
    const float scale=std::sqrt(view[0]*view[0]+view[1]*view[1]+view[2]*view[2]);
    scalar("fogStart",sky.worldFogStart*scale);scalar("fogEnd",sky.worldFarClip*scale);
    color("skyHorizon",sky.horizon);color("skyZenith",sky.zenith);
    color("skyLower",sky.lower);color("sunColor",sky.sunCore);color("fogColor",sky.fog);
    pglUniform3f(pglGetUniformLocation(program,"sunDirection"),sky.sunDirectionX,sky.sunDirectionY,sky.sunDirectionZ);
    pglUniform3f(pglGetUniformLocation(program,"cameraPosition"),inverse[12],inverse[13],inverse[14]);
    pglUniform1i(pglGetUniformLocation(program,"waterTexture"),0);
    pglUniform1i(pglGetUniformLocation(program,"textured"),textured?1:0);
    const bool reactions=gAeroQuality==3&&reaction&&reaction->ready()&&reactionTexture&&pglActiveTexture;
    pglUniform1i(pglGetUniformLocation(program,"reactionEnabled"),reactions?1:0);
    pglUniform1i(pglGetUniformLocation(program,"reactionTexture"),1);
    if(reactions) {
        pglUniform3f(pglGetUniformLocation(program,"reactionOrigin"),reaction->west(),reaction->south(),0.0f);
        const float inverseSize=1.0f/reaction->extent();
        pglUniform3f(pglGetUniformLocation(program,"reactionInverseSize"),inverseSize,inverseSize,0.0f);
        pglUniform3f(pglGetUniformLocation(program,"reactionTexel"),
            1.0f/StorylandWaterReaction::Width,1.0f/StorylandWaterReaction::Height,0.0f);
        pglActiveTexture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,reactionTexture);
        pglActiveTexture(GL_TEXTURE0);
    }
    return true;
}
