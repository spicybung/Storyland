namespace {
struct NativeBodyFrame {
    Point origin,forward,right,up,minimum,maximum;
    Point local(Point p) const {
        p=sub(p,origin);return {dot(p,forward),dot(p,right),dot(p,up)};
    }
};
NativeBodyFrame nativeBodyFrame(Point position,const StorylandNativeBody& body) {
    const float c=std::cos(body.yaw*radians),s=std::sin(body.yaw*radians);
    const float cp=std::cos(body.pitch*radians),sp=std::sin(body.pitch*radians);
    const float cr=std::cos(body.roll*radians),sr=std::sin(body.roll*radians);
    NativeBodyFrame frame;
    frame.origin=position;frame.forward={c*cp,s*cp,sp};
    frame.right={-s*cr-c*sp*sr,c*cr-s*sp*sr,cp*sr};
    frame.up=cross(frame.forward,frame.right);
    frame.minimum={-body.halfLength,-body.halfWidth,body.lower};
    frame.maximum={body.halfLength,body.halfWidth,body.height};
    return frame;
}
Point nativeInstancePoint(const std::array<float,16>& m,Point p) {
    return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],
        m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
}
bool nativeBodyLocalBounds(const NativeBodyFrame& frame,const std::array<float,16>& matrix,
    Point& minimum,Point& maximum) {
    const Point x{matrix[0],matrix[1],matrix[2]},y{matrix[4],matrix[5],matrix[6]},z{matrix[8],matrix[9],matrix[10]};
    const auto rx=cross(y,z),ry=cross(z,x),rz=cross(x,y);
    const float determinant=dot(x,rx);
    if(!std::isfinite(determinant)||std::fabs(determinant)<1e-8f)return false;
    const auto convert=[&](Point vector) {
        return Point{dot(vector,rx)/determinant,dot(vector,ry)/determinant,dot(vector,rz)/determinant};
    };
    const auto center=mul(add(frame.minimum,frame.maximum),.5f);
    const auto extent=mul(sub(frame.maximum,frame.minimum),.5f);
    const auto worldCenter=add(frame.origin,add(mul(frame.forward,center.x),
        add(mul(frame.right,center.y),mul(frame.up,center.z))));
    const auto localCenter=convert(sub(worldCenter,{matrix[12],matrix[13],matrix[14]}));
    const auto forward=convert(frame.forward),right=convert(frame.right),up=convert(frame.up);
    const Point radius{std::fabs(forward.x)*extent.x+std::fabs(right.x)*extent.y+std::fabs(up.x)*extent.z+.001f,
        std::fabs(forward.y)*extent.x+std::fabs(right.y)*extent.y+std::fabs(up.y)*extent.z+.001f,
        std::fabs(forward.z)*extent.x+std::fabs(right.z)*extent.y+std::fabs(up.z)*extent.z+.001f};
    minimum=sub(localCenter,radius);maximum=add(localCenter,radius);
    return finite(minimum)&&finite(maximum);
}

bool nativeBodySeparated(const NativeBodyFrame& body,const Point* points,size_t count,Point axis) {
    const float magnitude=length(axis);
    if(magnitude<1e-7f)return false;
    axis=mul(axis,1/magnitude);
    float minimum=dot(points[0],axis),maximum=minimum;
    for(size_t i=1;i<count;++i) {
        const float value=dot(points[i],axis);minimum=std::min(minimum,value);maximum=std::max(maximum,value);
    }
    const Point center=mul(add(body.minimum,body.maximum),.5f),extent=mul(sub(body.maximum,body.minimum),.5f);
    const float projection=dot(center,axis);
    const float radius=std::fabs(axis.x)*extent.x+std::fabs(axis.y)*extent.y+std::fabs(axis.z)*extent.z;
    return maximum<=projection-radius+.00001f||minimum>=projection+radius-.00001f;
}
bool nativeBodyPolyOverlap(const NativeBodyFrame& body,const Point* points,size_t count,
    const Point* edges,size_t edgeCount,const Point* normals,size_t normalCount) {
    constexpr Point axes[]={{1,0,0},{0,1,0},{0,0,1}};
    for(const auto axis:axes)if(nativeBodySeparated(body,points,count,axis))return false;
    for(size_t i=0;i<normalCount;++i)if(nativeBodySeparated(body,points,count,normals[i]))return false;
    for(size_t i=0;i<edgeCount;++i)for(const auto axis:axes)
        if(nativeBodySeparated(body,points,count,cross(edges[i],axis)))return false;
    return true;
}
void nativeBodyBoxPoints(const NativeBodyFrame& frame,const std::array<float,16>& matrix,
    Point minimum,Point maximum,Point* points) {
    for(unsigned i=0;i<8;++i)points[i]=frame.local(nativeInstancePoint(matrix,
        {i&1?maximum.x:minimum.x,i&2?maximum.y:minimum.y,i&4?maximum.z:minimum.z}));
}
bool nativeBodyBoxOverlap(const NativeBodyFrame& frame,const Point* points) {
    const Point edges[]={sub(points[1],points[0]),sub(points[2],points[0]),sub(points[4],points[0])};
    const Point normals[]={cross(edges[0],edges[1]),cross(edges[1],edges[2]),cross(edges[2],edges[0])};
    return nativeBodyPolyOverlap(frame,points,8,edges,3,normals,3);
}
NativeCollisionCandidates nativeBodyCandidates(const StorylandNativeWorld& world,Point position,float radius) {
    NativeCollisionCandidates candidates;
    const bool indexed=world.collisionIndexOwner==world.instances.data()&&
        world.collisionIndexCount==world.instances.size()&&world.collisionIndexModels==world.collisionModels.size();
    if(!indexed) {
        candidates.owned.resize(world.instances.size());
        for(size_t i=0;i<candidates.owned.size();++i)candidates.owned[i]=uint32_t(i);
        return candidates;
    }
    const int32_t x0=int32_t(std::floor((position.x-radius)/StorylandCollisionCellSize));
    const int32_t x1=int32_t(std::floor((position.x+radius)/StorylandCollisionCellSize));
    const int32_t y0=int32_t(std::floor((position.y-radius)/StorylandCollisionCellSize));
    const int32_t y1=int32_t(std::floor((position.y+radius)/StorylandCollisionCellSize));
    if(x0==x1&&y0==y1&&world.collisionLargeInstances.empty()) {
        const auto found=world.collisionCells.find({x0,y0});
        if(found!=world.collisionCells.end())candidates.borrowed=&found->second;
        return candidates;
    }
    candidates.owned=world.collisionLargeInstances;
    for(int32_t y=y0;y<=y1;++y)for(int32_t x=x0;x<=x1;++x) {
        const auto found=world.collisionCells.find({x,y});
        if(found!=world.collisionCells.end())candidates.owned.insert(candidates.owned.end(),found->second.begin(),found->second.end());
    }
    std::sort(candidates.owned.begin(),candidates.owned.end());
    candidates.owned.erase(std::unique(candidates.owned.begin(),candidates.owned.end()),candidates.owned.end());
    return candidates;
}
}

static bool nativeBodyParametersValid(const Point& position,const StorylandNativeBody& body) {
    if(!finite(position)||std::max({std::fabs(position.x),std::fabs(position.y),std::fabs(position.z)})>1e7f||
        !std::isfinite(body.yaw)||!std::isfinite(body.pitch)||!std::isfinite(body.roll)||
        !(body.halfLength>0&&body.halfLength<100&&body.halfWidth>0&&body.halfWidth<100&&
            body.height>body.lower&&body.height<100&&body.lower>-100))return false;
    return true;
}

Point nativeClosestTriangle(Point p,Point a,Point b,Point c) {
    const Point ab=sub(b,a),ac=sub(c,a),ap=sub(p,a);
    const float d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0&&d2<=0)return a;
    const Point bp=sub(p,b);
    const float d3=dot(ab,bp),d4=dot(ac,bp);
    if(d3>=0&&d4<=d3)return b;
    const float vc=d1*d4-d3*d2;
    if(vc<=0&&d1>=0&&d3<=0)return add(a,mul(ab,d1/(d1-d3)));
    const Point cp=sub(p,c);
    const float d5=dot(ab,cp),d6=dot(ac,cp);
    if(d6>=0&&d5<=d6)return c;
    const float vb=d5*d2-d1*d6;
    if(vb<=0&&d2>=0&&d6<=0)return add(a,mul(ac,d2/(d2-d6)));
    const float va=d3*d6-d5*d4;
    if(va<=0&&d4-d3>=0&&d5-d6>=0)return add(b,mul(sub(c,b),(d4-d3)/((d4-d3)+(d5-d6))));
    const float sum=va+vb+vc;
    if(std::fabs(sum)<1e-12f)return a;
    return add(a,add(mul(ab,vb/sum),mul(ac,vc/sum)));
}
bool nativeVehicleSpheresClear(Point position,const StorylandNativeBody& body,
    const StorylandNativeCollisionModel& model,const std::array<float,16>& matrix,
    bool groundContacts=false,float* groundLift=nullptr,StorylandNativeContact* output=nullptr,
    float sphereInset=.002f) {
    const auto contact=[&](Point center,Point closest,float radius) {
        const Point delta=sub(center,closest);const float distance=length(delta);
        if(distance>=radius)return false;
        const float depth=radius-distance;
        if(groundContacts&&distance>1e-6f&&delta.z/distance>.65f&&depth<.5f) {
            const float lift=depth/(delta.z/distance);
            if(output&&(!groundLift||lift>*groundLift))*output={closest,mul(delta,1.0f/distance),true,depth};
            if(groundLift)*groundLift=std::max(*groundLift,lift);
            return false;
        }
        if(output&&distance>1e-6f)*output={closest,mul(delta,1.0f/distance),true,depth};
        return true;
    };
    const Point axes[]={{matrix[0],matrix[1],matrix[2]},
        {matrix[4],matrix[5],matrix[6]},{matrix[8],matrix[9],matrix[10]}};
    const float squaredLengths[]={dot(axes[0],axes[0]),dot(axes[1],axes[1]),dot(axes[2],axes[2])};
    const float scale=model.spheres.empty()?0.0f:
        std::sqrt(std::max({squaredLengths[0],squaredLengths[1],squaredLengths[2]}));
    const float low[]={model.minimum.x,model.minimum.y,model.minimum.z};
    const float high[]={model.maximum.x,model.maximum.y,model.maximum.z};
    Point worldMinimum{matrix[12],matrix[13],matrix[14]},worldMaximum=worldMinimum;
    // Enclose the projected box in world space using the same axis order and
    // degenerate-axis rule as closestBox. Exact contact tests remain below.
    for(size_t axis=0;axis<3;++axis)if(squaredLengths[axis]>1e-12f) {
        const Point first=mul(axes[axis],low[axis]),last=mul(axes[axis],high[axis]);
        worldMinimum=add(worldMinimum,{std::min(first.x,last.x),std::min(first.y,last.y),std::min(first.z,last.z)});
        worldMaximum=add(worldMaximum,{std::max(first.x,last.x),std::max(first.y,last.y),std::max(first.z,last.z)});
    }
    Point inverseX{},inverseY{},inverseZ{},inverseExtent{};
    float determinant=0;
    bool triangleBounds=false,triangleBoundsPrepared=false;
    const auto closestBox=[&](Point center,Point minimum,Point maximum) {
        const Point origin{matrix[12],matrix[13],matrix[14]};
        const Point delta=sub(center,origin);
        const float low[]={minimum.x,minimum.y,minimum.z},high[]={maximum.x,maximum.y,maximum.z};
        Point closest=origin;
        for(size_t axis=0;axis<3;++axis) {
            const float squared=squaredLengths[axis];
            if(squared>1e-12f)closest=add(closest,mul(axes[axis],std::clamp(dot(delta,axes[axis])/squared,low[axis],high[axis])));
        }
        return closest;
    };
    for(const auto& sphere:body.collision->spheres) {
        if(sphere.radius<=0)continue;
        const Point center=add(position,nativeInstancePoint(body.collisionTransform,sphere.center));
        const float radius=std::max(0.0f,sphere.radius-sphereInset);
        const float reach=radius+.001f;
        if(center.x-worldMaximum.x>reach||worldMinimum.x-center.x>reach||
            center.y-worldMaximum.y>reach||worldMinimum.y-center.y>reach||
            center.z-worldMaximum.z>reach||worldMinimum.z-center.z>reach)continue;
        if(length(sub(center,closestBox(center,model.minimum,model.maximum)))>=radius)continue;
        for(const auto& box:model.boxes)
            if(contact(center,closestBox(center,box.minimum,box.maximum),radius))return false;
        for(const auto& other:model.spheres) {
            const Point otherCenter=nativeInstancePoint(matrix,other.center);
            const Point delta=sub(center,otherCenter);
            const float separation=length(delta),otherRadius=other.radius*scale;
            if(separation>=radius+otherRadius)continue;
            if(output&&separation>1e-6f) {
                const Point normal=mul(delta,1.0f/separation);
                *output={add(otherCenter,mul(normal,otherRadius)),normal,true,
                    radius+otherRadius-separation};
            }
            return false;
        }
        if(!triangleBoundsPrepared&&!model.triangles.empty()) {
            triangleBoundsPrepared=true;
            inverseX=cross(axes[1],axes[2]);inverseY=cross(axes[2],axes[0]);inverseZ=cross(axes[0],axes[1]);
            determinant=dot(axes[0],inverseX);
            triangleBounds=std::isfinite(determinant)&&std::fabs(determinant)>=1e-8f;
            if(triangleBounds)inverseExtent={length(inverseX)/std::fabs(determinant),
                length(inverseY)/std::fabs(determinant),length(inverseZ)/std::fabs(determinant)};
        }
        Point localMinimum{},localMaximum{};
        if(triangleBounds) {
            // Reject triangles outside the sphere's conservative bounds in the
            // model frame before transforming their vertices. This also covers
            // scaled and sheared instances without changing the contact test.
            const auto offset=sub(center,{matrix[12],matrix[13],matrix[14]});
            const Point localCenter{dot(offset,inverseX)/determinant,dot(offset,inverseY)/determinant,
                dot(offset,inverseZ)/determinant};
            const auto extent=add(mul(inverseExtent,radius),{.001f,.001f,.001f});
            localMinimum=sub(localCenter,extent);localMaximum=add(localCenter,extent);
        }
        for(const auto& triangle:model.triangles) {
            if(triangle.vertices[0]>=model.vertices.size()||triangle.vertices[1]>=model.vertices.size()||
                triangle.vertices[2]>=model.vertices.size())continue;
            const auto& localA=model.vertices[triangle.vertices[0]];
            const auto& localB=model.vertices[triangle.vertices[1]];
            const auto& localC=model.vertices[triangle.vertices[2]];
            if(triangleBounds&&triangleOutsideBounds(localA,localB,localC,localMinimum,localMaximum))continue;
            const Point a=nativeInstancePoint(matrix,localA);
            const Point b=nativeInstancePoint(matrix,localB);
            const Point c=nativeInstancePoint(matrix,localC);
            if(length(cross(sub(b,a),sub(c,a)))<1e-7f)continue;
            if(contact(center,nativeClosestTriangle(center,a,b,c),radius))return false;
        }
    }
    return true;
}

bool storylandNativeVehicleBodiesOverlap(const Point& first,const StorylandNativeBody& firstBody,
    const Point& second,const StorylandNativeBody& secondBody,StorylandNativeContact* contact) {
    if(contact)*contact={};
    if(!nativeBodyParametersValid(first,firstBody)||!nativeBodyParametersValid(second,secondBody))return false;
    const auto spheresOverlapBody=[](Point vehiclePosition,const StorylandNativeBody& vehicle,
        Point otherPosition,const StorylandNativeBody& other) {
        const auto frame=nativeBodyFrame(otherPosition,other);
        const auto& transform=vehicle.collisionTransform;
        const float scale=std::max({length({transform[0],transform[1],transform[2]}),
            length({transform[4],transform[5],transform[6]}),length({transform[8],transform[9],transform[10]})});
        for(const auto& sphere:vehicle.collision->spheres) {
            if(sphere.radius<=0)continue;
            const auto center=frame.local(add(vehiclePosition,nativeInstancePoint(transform,sphere.center)));
            const Point closest{std::clamp(center.x,frame.minimum.x,frame.maximum.x),
                std::clamp(center.y,frame.minimum.y,frame.maximum.y),
                std::clamp(center.z,frame.minimum.z,frame.maximum.z)};
            if(length(sub(center,closest))<sphere.radius*scale-.002f)return true;
        }
        return false;
    };
    if(!firstBody.collision&&!secondBody.collision)return false;
    if(!firstBody.collision)return spheresOverlapBody(second,secondBody,first,firstBody);
    if(!secondBody.collision)return spheresOverlapBody(first,firstBody,second,secondBody);
    auto matrix=secondBody.collisionTransform;
    matrix[12]+=second.x;matrix[13]+=second.y;matrix[14]+=second.z;
    if(!nativeVehicleSpheresClear(first,firstBody,*secondBody.collision,matrix,false,nullptr,contact))return true;
    matrix=firstBody.collisionTransform;
    matrix[12]+=first.x;matrix[13]+=first.y;matrix[14]+=first.z;
    if(nativeVehicleSpheresClear(second,secondBody,*firstBody.collision,matrix,false,nullptr,contact))return false;
    if(contact&&contact->valid)contact->normal=mul(contact->normal,-1.0f);
    return true;
}

static bool nativeBoxBodyModelClear(const NativeBodyFrame& frame,const StorylandNativeBody& body,
    const StorylandNativeCollisionModel& model,const std::array<float,16>& matrix,
    float minimumSupportNormal=.70710678f) {
    Point points[8];nativeBodyBoxPoints(frame,matrix,model.minimum,model.maximum,points);
    constexpr Point axes[]={{1,0,0},{0,1,0},{0,0,1}};
    bool distant=false;
    for(const auto axis:axes)if(nativeBodySeparated(frame,points,8,axis)) {distant=true;break;}
    if(distant)return true;
    for(const auto& box:model.boxes) {
        nativeBodyBoxPoints(frame,matrix,box.minimum,box.maximum,points);
        if(nativeBodyBoxOverlap(frame,points))return false;
    }
    for(const auto& sphere:model.spheres) {
        const auto center=frame.local(nativeInstancePoint(matrix,sphere.center));
        const auto& m=matrix;
        const float radius=sphere.radius*std::max({length({m[0],m[1],m[2]}),length({m[4],m[5],m[6]}),length({m[8],m[9],m[10]})});
        const Point closest{std::clamp(center.x,frame.minimum.x,frame.maximum.x),
            std::clamp(center.y,frame.minimum.y,frame.maximum.y),std::clamp(center.z,frame.minimum.z,frame.maximum.z)};
        if(length(sub(center,closest))<radius-.00001f)return false;
    }
    Point localMinimum{},localMaximum{};
    const bool localBounds=!model.triangles.empty()&&nativeBodyLocalBounds(frame,matrix,localMinimum,localMaximum);
    for(const auto& triangle:model.triangles) {
        if(triangle.vertices[0]>=model.vertices.size()||triangle.vertices[1]>=model.vertices.size()||triangle.vertices[2]>=model.vertices.size())continue;
        if(localBounds) {
            const auto& a=model.vertices[triangle.vertices[0]];
            const auto& b=model.vertices[triangle.vertices[1]];
            const auto& c=model.vertices[triangle.vertices[2]];
            if(std::max({a.x,b.x,c.x})<localMinimum.x||std::min({a.x,b.x,c.x})>localMaximum.x||
                std::max({a.y,b.y,c.y})<localMinimum.y||std::min({a.y,b.y,c.y})>localMaximum.y||
                std::max({a.z,b.z,c.z})<localMinimum.z||std::min({a.z,b.z,c.z})>localMaximum.z)continue;
        }
        Point vertices[3];
        for(size_t i=0;i<3;++i)vertices[i]=frame.local(nativeInstancePoint(matrix,model.vertices[triangle.vertices[i]]));
        const Point edges[]={sub(vertices[1],vertices[0]),sub(vertices[2],vertices[1]),sub(vertices[0],vertices[2])};
        const Point normal=cross(edges[0],edges[1]);
        if(body.character&&std::fabs(normal.z)>=length(normal)*minimumSupportNormal&&std::fabs(normal.z)>1e-7f) {
            const float floor=dot(normal,vertices[0])/normal.z;
            // The supporting plane may enter the foot box on its uphill side.
            if(floor>=-.36f&&floor<=.051f)continue;
        }
        if(length(normal)>1e-7f&&nativeBodyPolyOverlap(frame,vertices,3,edges,3,&normal,1))return false;
    }
    return true;
}

bool storylandNativeCharacterModelClear(const Point& position,
    const StorylandNativeCollisionModel& model,const std::array<float,16>& matrix) {
    const auto body=storylandNativeCharacterBody();
    if(!nativeBodyParametersValid(position,body)||
        !std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}))return false;
    return nativeBoxBodyModelClear(nativeBodyFrame(position,body),body,model,matrix);
}

static bool nativeBodyClearCandidates(const StorylandNativeWorld& world,const Point& position,
    const StorylandNativeBody& body,const NativeCollisionCandidates& candidates,float* groundLift=nullptr,
    StorylandNativeContact* contact=nullptr) {
    if(!nativeBodyParametersValid(position,body))return false;
    const auto frame=nativeBodyFrame(position,body);
    const auto& selected=candidates.values();
    const auto* scriptWorld=world.scriptEnvironment;
    const size_t scriptCount=scriptWorld?scriptWorld->objects.size():0;
    for(size_t query=0;query<selected.size()+scriptCount;++query) {
        StorylandNativeCollisionInstance live;
        if(query>=selected.size()&&!storylandScriptKinematicCollision(*scriptWorld,
            scriptWorld->objects[query-selected.size()],live))continue;
        if(query<selected.size()&&selected[query]>=world.instances.size())continue;
        const auto& instance=query>=selected.size()?live:world.instances[selected[query]];
        if(query<selected.size()&&!(instance.nativeSectorLists&body.sectorLists))continue;
        if(world.paths.vcs&&!instance.nativeUsesCollision)continue;
        const auto found=world.collisionModels.find(instance.modelId);
        if(found==world.collisionModels.end())continue;
        const auto& model=found->second;
        if(!world.collisionModelEnabled(model))continue;
        if(body.collision) {
            if(!nativeVehicleSpheresClear(position,body,model,instance.matrix,true,groundLift,contact))return false;
            continue;
        }
        const float minimumSupportNormal=world.paths.vcs?.5f:.70710678f;
        if(!nativeBoxBodyModelClear(frame,body,model,instance.matrix,minimumSupportNormal))return false;
    }
    return true;
}

bool storylandNativeSphereModelContact(const Point& position,float radius,
    const StorylandNativeCollisionModel& model,const std::array<float,16>& matrix,bool& hit,
    StorylandNativeContact* contact) {
    hit=false;
    if(contact)*contact={};
    if(!finite(position)||!std::isfinite(radius)||radius<=0||radius>=100||
        !std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);})||
        std::max({std::fabs(position.x),std::fabs(position.y),std::fabs(position.z)})>1e7f)return false;
    StorylandNativeCollisionModel sphere;
    sphere.minimum={-radius,-radius,-radius};sphere.maximum={radius,radius,radius};
    sphere.spheres.push_back({{},radius,0,0});
    StorylandNativeBody body;body.collision=&sphere;
    hit=!nativeVehicleSpheresClear(position,body,model,matrix,false,nullptr,contact,0);
    return true;
}

bool storylandNativeWorldSphereContact(const StorylandNativeWorld& world,
    const Point& position,float radius,uint16_t sectorLists,bool scriptObjects,bool& hit,int32_t ignoredVehicle) {
    hit=false;
    if(!finite(position)||!std::isfinite(radius)||radius<=0||radius>=100||
        std::max({std::fabs(position.x),std::fabs(position.y),std::fabs(position.z)})>1e7f)return false;
    const auto candidates=nativeBodyCandidates(world,position,radius);
    for(const auto index:candidates.values()) {
        if(index>=world.instances.size())continue;
        const auto& instance=world.instances[index];
        if(!(instance.nativeSectorLists&sectorLists)||!instance.nativeUsesCollision)continue;
        if(world.scriptEnvironment&&world.scriptEnvironment->lcsExtendedWorld.deletedNativeObjects.count(
            (uint64_t(instance.poolHeader)<<32)|instance.poolIndex))continue;
        const auto found=world.collisionModels.find(instance.modelId);
        if(found==world.collisionModels.end()||!world.collisionModelEnabled(found->second))continue;
        if(!storylandNativeSphereModelContact(position,radius,found->second,instance.matrix,hit))return false;
        if(hit)return true;
    }
    if(scriptObjects&&world.scriptEnvironment) {
        const auto& environment=*world.scriptEnvironment;
        for(const auto& object:environment.objects) {
            if(!object.collisionEnabled||object.area!=environment.area||object.model<0)continue;
            if(ignoredVehicle>=0&&object.attachedVehicle==ignoredVehicle)continue;
            const auto found=world.collisionModels.find(uint32_t(object.model));
            if(found==world.collisionModels.end()||!world.collisionModelEnabled(found->second))continue;
            const auto matrix=storylandScriptObjectMatrix(object.x,object.y,object.z,
                object.rotationX,object.rotationY,object.heading);
            if(!storylandNativeSphereModelContact(position,radius,found->second,matrix,hit))return false;
            if(hit)return true;
        }
    }
    return true;
}

bool storylandNativeBuildingSphereContact(const StorylandNativeWorld& world,
    const Point& position,float radius,bool& hit) {
    return storylandNativeWorldSphereContact(world,position,radius,0x63u,false,hit);
}

bool storylandNativeBodyClear(const StorylandNativeWorld& world,const Point& position,const StorylandNativeBody& body,
    StorylandNativeContact* contact) {
    if(contact)*contact={};
    if(!nativeBodyParametersValid(position,body))return false;
    const auto candidates=nativeBodyCandidates(world,position,
        body.halfLength+body.halfWidth+body.height+std::fabs(body.lower));
    return nativeBodyClearCandidates(world,position,body,candidates,nullptr,contact);
}

// A vehicle's swept positions use one conservative collision candidate list.
// Every sample still runs the identical native narrow-phase collision query,
// in the same order and with the same contact and impact decisions.
// The candidate list is scoped to this call and never retained across frames.
bool storylandNativeBodySweepClear(const StorylandNativeWorld& world,
    const std::vector<StorylandNativeBodySample>& samples,
    StorylandNativeContact* contact,size_t* blockedSample) {
    if(contact)*contact={};
    if(blockedSample)*blockedSample=0;
    if(samples.empty())return true;
    Point lower=samples.front().position,upper=lower;
    for(const auto& sample:samples) {
        if(!nativeBodyParametersValid(sample.position,sample.body)) {
            for(size_t index=0;index<samples.size();++index) {
                const auto& candidate=samples[index];
                if(contact)*contact={};
                if(storylandNativeBodyClear(world,candidate.position,candidate.body,contact))continue;
                if(blockedSample)*blockedSample=index;
                return false;
            }
            return true;
        }
        lower.x=std::min(lower.x,sample.position.x);
        lower.y=std::min(lower.y,sample.position.y);
        lower.z=std::min(lower.z,sample.position.z);
        upper.x=std::max(upper.x,sample.position.x);
        upper.y=std::max(upper.y,sample.position.y);
        upper.z=std::max(upper.z,sample.position.z);
    }
    const Point center{(lower.x+upper.x)*.5f,(lower.y+upper.y)*.5f,(lower.z+upper.z)*.5f};
    float radius=0;
    for(const auto& sample:samples) {
        const auto& body=sample.body;
        const float reach=body.halfLength+body.halfWidth+body.height+std::fabs(body.lower);
        radius=std::max(radius,length(sub(sample.position,center))+reach);
    }
    const auto candidates=nativeBodyCandidates(world,center,radius+.001f);
    for(size_t index=0;index<samples.size();++index) {
        const auto& sample=samples[index];
        if(contact)*contact={};
        if(!nativeBodyClearCandidates(world,sample.position,sample.body,candidates,nullptr,contact)) {
            if(blockedSample)*blockedSample=index;
            return false;
        }
    }
    return true;
}

StorylandNativeMoveResult storylandMoveNativeBody(const StorylandNativeWorld& world,
    const Point& from,const Point& desired,const StorylandNativeBody& body) {
    StorylandNativeMoveResult result;result.position=from;
    if(!nativeBodyParametersValid(from,body)||!finite(desired)||length(sub(desired,from))>50)return result;
    if(!storylandNativeBodyClear(world,result.position,body,&result.contact)) {
        bool recovered=false;
        const auto recoveryCandidates=nativeBodyCandidates(world,from,
            8.0f+body.halfLength+body.halfWidth+body.height+std::fabs(body.lower));
        if(body.collision&&result.contact.valid) {
            const float normalLength=length(result.contact.normal);
            if(std::isfinite(normalLength)&&normalLength>1e-6f) {
                const Point normal=mul(result.contact.normal,1.0f/normalLength);
                for(unsigned step=1;step<=10&&!recovered;++step) {
                    float high=float(step)*.05f;
                    const auto candidate=add(from,mul(normal,high));
                    if(!nativeBodyClearCandidates(world,candidate,body,recoveryCandidates))continue;
                    float low=float(step-1)*.05f;
                    for(unsigned attempt=0;attempt<12;++attempt) {
                        const float middle=(low+high)*.5f;
                        if(nativeBodyClearCandidates(world,add(from,mul(normal,middle)),body,recoveryCandidates))
                            high=middle;
                        else low=middle;
                    }
                    result.position=add(from,mul(normal,high));
                    result.recovered=true;recovered=true;
                }
            }
        }
        if(body.character&&!body.collision) {
            // Search every direction in the first clear shell and choose the
            // shortest correction. A fixed east-first displacement can push
            // a pedestrian off an edge when a much smaller upward correction
            // would clear the same shallow overlap.
            for(unsigned step=1;step<=80&&!recovered;++step) {
                const float radius=step*.1f;
                float best=radius+.1f;
                Point bestPosition=from;
                for(unsigned direction=0;direction<18;++direction) {
                    const float angle=direction*6.28318530718f/16;
                    const Point unit=direction<16?Point{std::cos(angle),std::sin(angle),0}:
                        Point{0,0,direction==16?1.0f:-1.0f};
                    if(!nativeBodyClearCandidates(world,add(from,mul(unit,radius)),body,recoveryCandidates))continue;
                    float low=(step-1)*.1f,high=radius;
                    for(unsigned attempt=0;attempt<12;++attempt) {
                        const float middle=(low+high)*.5f;
                        if(nativeBodyClearCandidates(world,add(from,mul(unit,middle)),body,recoveryCandidates))high=middle;
                        else low=middle;
                    }
                    if(high<best) {best=high;bestPosition=add(from,mul(unit,high));}
                }
                if(best<=radius) {
                    result.position=bestPosition;result.recovered=true;recovered=true;
                }
            }
        }
        for(unsigned step=1;step<=(body.collision?10u:80u)&&!recovered;++step) {
            const float radius=step*(body.collision?.05f:.1f);
            for(unsigned direction=body.collision?16u:0u;direction<(body.collision?17u:18u);++direction) {
                const float angle=direction*6.28318530718f/16;
                const Point offset=direction<16?Point{std::cos(angle)*radius,std::sin(angle)*radius,0}:
                    Point{0,0,direction==16?radius:-radius};
                const auto candidate=add(from,offset);
                if(nativeBodyClearCandidates(world,candidate,body,recoveryCandidates)) {
                    result.position=candidate;result.recovered=true;recovered=true;break;
                }
            }
        }
        if(!recovered) {result.blocked=true;return result;}
    }
    if(!result.recovered||!body.collision)result.contact={};
    bool supportContact=result.contact.valid&&result.contact.normal.z>.65f;
    const auto delta=sub(desired,from);
    if(!body.collision&&delta.x==0&&delta.y==0&&delta.z==0) {
        // The initial/recovery query already cleared this position. Only
        // native vehicle spheres can request an additional support lift.
        result.position=add(result.position,delta);result.clear=true;return result;
    }
    const unsigned steps=std::max(1u,unsigned(std::ceil(length(delta)/.08f)));
    const Point velocity=mul(delta,1.0f/steps);
    const auto destination=add(result.position,delta);
    const auto center=mul(add(result.position,destination),.5f);
    const auto candidates=nativeBodyCandidates(world,center,
        body.halfLength+body.halfWidth+body.height+std::fabs(body.lower)+length(delta)*.5f);
    const auto clear=[&](Point position) {return nativeBodyClearCandidates(world,position,body,candidates);};
    for(unsigned step=0;step<steps;++step) {
        auto candidate=add(result.position,velocity);
        float groundLift=0;
        StorylandNativeContact contact;
        if(nativeBodyClearCandidates(world,candidate,body,candidates,&groundLift,&contact)) {
            candidate.z+=groundLift;
            // The first query already checked this exact position unless a
            // vehicle contact lifted it onto the supporting surface.
            if(groundLift==0||clear(candidate)) {
                if(groundLift>0&&contact.valid&&(!result.contact.valid||supportContact)) {
                    result.contact=contact;supportContact=true;
                }
                result.position=candidate;continue;
            }
        }
        if(contact.valid&&(!result.contact.valid||supportContact)) {
            result.contact=contact;supportContact=false;
        }
        result.blocked=true;
        for(const auto axis:{Point{velocity.x,0,0},Point{0,velocity.y,0},Point{0,0,velocity.z}}) {
            if(length(axis)<1e-8f)continue;
            const auto next=add(result.position,axis);
            if(clear(next))result.position=next;
            else {
                float low=0,high=1;
                for(unsigned attempt=0;attempt<10;++attempt) {
                    const float t=(low+high)*.5f;
                    if(clear(add(result.position,mul(axis,t))))low=t;
                    else high=t;
                }
                result.position=add(result.position,mul(axis,low));
            }
        }
    }
    result.clear=clear(result.position);
    return result;
}

StorylandNativeBody storylandNativeCharacterBody() {
    StorylandNativeBody body;
    body.character=true;
    body.lower=.36f;
    return body;
}

StorylandNativeMoveResult storylandMoveNativeCharacter(const StorylandNativeWorld& world,
    const Point& from,const Point& desired) {
    const auto body=storylandNativeCharacterBody();
    const auto support=[&](Point position,float above,float below,Point& floor) {
        const Point top{position.x,position.y,position.z+above};
        const Point bottom{position.x,position.y,position.z-below};
        Point normal;uint8_t surface=0;
        const float hit=storylandTraceNativeGround(world,top,bottom,surface,&normal);
        // VCS has walkable shoreline collision steeper than 45 degrees,
        // including the bank reached during Soldier. Keep near-vertical walls
        // excluded and leave LCS's existing support threshold unchanged.
        const float minimumUp=world.paths.vcs?.5f:.70710678f;
        if(hit>=1||std::fabs(normal.z)<minimumUp)return false;
        floor={position.x,position.y,top.z+(bottom.z-top.z)*hit};
        return true;
    };
    Point oldFloor;
    const bool grounded=support(from,.08f,.12f,oldFloor)&&std::fabs(desired.z-from.z)<=.36f;
    auto target=desired;
    Point floor;
    if(grounded&&support({target.x,target.y,from.z},.36f,.45f,floor))target.z=floor.z;
    auto result=storylandMoveNativeBody(world,from,target,body);
    Point resultFloor;
    if(grounded&&support(result.position,.36f,.45f,resultFloor)&&resultFloor.z>result.position.z+.051f) {
        result.position=from;
        result.blocked=true;
        result.clear=storylandNativeBodyClear(world,from,body);
    }
    if(!grounded||!result.blocked)return result;
    // Sweep up, across, then down. A curb cannot be climbed through a ceiling.
    Point raised=from;raised.z+=.36f;
    const auto lift=storylandMoveNativeBody(world,from,raised,body);
    if(!lift.clear||lift.blocked||lift.recovered)return result;
    const Point across{target.x,target.y,raised.z};
    const auto step=storylandMoveNativeBody(world,raised,across,body);
    if(!step.clear||step.blocked||step.recovered)return result;
    if(!support(across,.001f,.81f,floor)||floor.z>from.z+.3601f)return result;
    const auto settle=storylandMoveNativeBody(world,across,floor,body);
    if(!settle.clear||settle.recovered)return result;
    // Adjacent shoreline triangles can stop the downward sweep a few
    // centimetres above the traced floor. Keep the clear supported position
    // instead of discarding the entire horizontal step at the seam.
    if(settle.blocked&&(!world.paths.vcs||settle.position.z<floor.z||
        settle.position.z>floor.z+.05f))return result;
    const float direct=std::hypot(result.position.x-from.x,result.position.y-from.y);
    const float stepped=std::hypot(settle.position.x-from.x,settle.position.y-from.y);
    if(stepped>direct+.0001f)return settle;
    return result;
}
