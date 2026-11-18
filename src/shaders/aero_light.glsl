// Point lights: same falloff in the world pass and the full-screen pass.
uniform int uPointCount;
uniform vec3 uPointPosition[8],uPointColor[8];
uniform vec3 uPointRange[8];
uniform vec3 uUpEye;
uniform float uAeroWet;
uniform int uLightingModel;

vec3 safeNormal(vec3 v){return v*inversesqrt(max(dot(v,v),1e-16));}

vec3 aeroPointLight(vec3 position,vec3 normal,vec3 view,vec3 albedo,float gloss,float wet){
 vec3 result=vec3(0.0);
 for(int i=0;i<8;i++)if(i<uPointCount){
  vec3 delta=uPointPosition[i]-position;
  float radius=max(uPointRange[i].x,.0001);
  float distanceSquared=dot(delta,delta);
  if(distanceSquared>=radius*radius)continue;
  float distance=sqrt(distanceSquared);
  vec3 light=delta/max(distance,.0001);
  float fade=1.0-smoothstep(0.0,radius,distance);
  float diffuse=max(dot(normal,light),0.0);
  float specAngle=uLightingModel==1 ? max(dot(reflect(-light,normal),view),0.0)
                                : max(dot(normal,safeNormal(light+view)),0.0);
  float specPower=uLightingModel==1?mix(8.0,48.0,gloss):mix(12.0,110.0,gloss);
  float highlight=uLightingModel==0?0.0:pow(specAngle,specPower)*diffuse*gloss;

  // Wet roads stretch the lamp towards the camera (authored reflection flag).
  float reflection=0.0;
  if(wet>0.0&&uPointRange[i].y>0.0){
   vec3 up=safeNormal(uUpEye);
   vec3 h=safeNormal(light+view);
   vec3 across=safeNormal(cross(up,view));
   float side=dot(h,across),along=max(1.0-dot(h,up),0.0);
   reflection=wet*exp(-side*side*75.0-along*12.0)*diffuse*.42;
  }
  result+=uPointColor[i]*fade*fade*(albedo*diffuse+vec3(highlight+reflection));
 }
 return result;
}
