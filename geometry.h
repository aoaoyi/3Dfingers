#ifndef RELEAF_GEOMETRY_H
#define RELEAF_GEOMETRY_H
#include <algorithm>
#include <cmath>
#include <vector>
namespace releaf {
const float pi = 3.14159265358979323846f;
struct Vec {
    float x,y,z;
    Vec(float X=0,float Y=0,float Z=0):x(X),y(Y),z(Z){}
    Vec operator+(Vec b)const{return Vec(x+b.x,y+b.y,z+b.z);}
    Vec operator-(Vec b)const{return Vec(x-b.x,y-b.y,z-b.z);}
    Vec operator*(float s)const{return Vec(x*s,y*s,z*s);}
};
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec cross(Vec a,Vec b){return Vec(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
inline Vec unit(Vec a){float l=std::sqrt(dot(a,a));return l>1e-7f?a*(1/l):Vec(0,1,0);}
struct Vertex {Vec p,n; Vertex(Vec P,Vec N):p(P),n(N){}};
struct Mesh {std::vector<Vertex> vertices;std::vector<unsigned> indices;};
inline void triangle(Mesh& m,unsigned a,unsigned b,unsigned c){
    m.indices.push_back(a);m.indices.push_back(b);m.indices.push_back(c);
}
struct Patch {
    Vec cp[4][4];
    Vec at(float u,float v)const{
        const float bu[4]={(1-u)*(1-u)*(1-u),3*u*(1-u)*(1-u),3*u*u*(1-u),u*u*u};
        const float bv[4]={(1-v)*(1-v)*(1-v),3*v*(1-v)*(1-v),3*v*v*(1-v),v*v*v};
        Vec p;for(int i=0;i<4;++i)for(int j=0;j<4;++j)p=p+cp[i][j]*(bu[i]*bv[j]);return p;
    }
    Vec normal(float u,float v)const{
        float ua=std::max(0.f,u-.001f),ub=std::min(1.f,u+.001f);
        float va=std::max(0.f,v-.001f),vb=std::min(1.f,v+.001f);
        Vec n=unit(cross(at(u,vb)-at(u,va),at(ub,v)-at(ua,v)));
        return n.y<0?n*(-1):n;
    }
};
inline void splitCurve(const Vec c[4],float t,Vec left[4],Vec right[4]){
    Vec a=c[0]*(1-t)+c[1]*t,b=c[1]*(1-t)+c[2]*t,d=c[2]*(1-t)+c[3]*t;
    Vec e=a*(1-t)+b*t,f=b*(1-t)+d*t,g=e*(1-t)+f*t;
    left[0]=c[0];left[1]=a;left[2]=e;left[3]=g;
    right[0]=g;right[1]=f;right[2]=d;right[3]=c[3];
}
inline void cropCurve(const Vec c[4],float a,float b,Vec out[4]){
    Vec unused[4],tail[4];splitCurve(c,a,unused,tail);splitCurve(tail,(b-a)/(1-a),out,unused);
}
inline Patch linerPatch(const Patch& outer){
    Patch tmp,inner;
    for(int j=0;j<4;++j){Vec c[4],out[4];for(int i=0;i<4;++i)c[i]=outer.cp[i][j];
        cropCurve(c,.07f,.93f,out);for(int i=0;i<4;++i)tmp.cp[i][j]=out[i];
    }
    for(int i=0;i<4;++i){Vec out[4];cropCurve(tmp.cp[i],.12f,.88f,out);
        for(int j=0;j<4;++j)inner.cp[i][j]=out[j];
    }
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)
        inner.cp[i][j]=inner.cp[i][j]+outer.normal(.07f+.86f*i/3.f,.12f+.76f*j/3.f)*.165f;
    return inner;
}
inline Patch wingPatch(int sign){
    Patch p;
    // Units: 1 model unit = 30 mm. Left leaf stops behind the thumb MCP.
    const float longitudinal[4]={0,.27f,.73f,1};
    const float width[4]={.30f,1.02f,1.08f,.26f};
    const float rise[4]={.55f,1.95f,1.40f,.22f};
    const float front=sign<0?.05f:.65f;
    for(int i=0;i<4;++i)for(int j=0;j<4;++j){
        float v=j/3.f;
        p.cp[i][j]=Vec(sign*(2.32f+width[i]*v),.64f+rise[i]*v,
                          -2.25f+(front+2.25f)*longitudinal[i]);
    }
    return p;
}
// Closed normal-offset Bezier shell: two skins connected by a semicircular rim.
// Skin boundary indices are shared by the fillet, so the mesh is watertight.
inline Mesh shell(const Patch& p,float radius,int res=32,int rimSteps=8){
    Mesh m;const unsigned count=(res+1)*(res+1);
    for(int layer=0;layer<2;++layer)for(int i=0;i<=res;++i)for(int j=0;j<=res;++j){
        float u=i/float(res),v=j/float(res);Vec n=p.normal(u,v);
        m.vertices.push_back(Vertex(p.at(u,v)+n*(layer?-radius:radius),n*(layer?-1.f:1.f)));
    }
    bool flip=p.cp[0][3].x>p.cp[0][0].x;
    for(int i=0;i<res;++i)for(int j=0;j<res;++j){
        unsigned a=i*(res+1)+j,b=a+1,c=a+res+1,d=c+1;
        if(flip){std::swap(b,c);}
        triangle(m,a,b,c);triangle(m,b,d,c);
        triangle(m,a+count,c+count,b+count);triangle(m,b+count,c+count,d+count);
    }
    std::vector<unsigned> edge;
    for(int j=0;j<res;++j)edge.push_back(j);
    for(int i=0;i<res;++i)edge.push_back(i*(res+1)+res);
    for(int j=res;j>0;--j)edge.push_back(res*(res+1)+j);
    for(int i=res;i>0;--i)edge.push_back(i*(res+1));
    if(flip)std::reverse(edge.begin(),edge.end());
    std::vector<unsigned> prev=edge;
    const unsigned size=static_cast<unsigned>(edge.size());
    for(int k=1;k<=rimSteps;++k){
        std::vector<unsigned> next;
        float a=pi*k/rimSteps;
        for(unsigned e=0;e<size;++e){
            unsigned id=edge[e];
            if(k==rimSteps){next.push_back(id+count);continue;}
            Vec n=m.vertices[id].n;
            Vec before=m.vertices[edge[(e+size-1)%size]].p;
            Vec after=m.vertices[edge[(e+1)%size]].p;
            Vec outward=unit(cross(after-before,n));
            Vec normal=n*std::cos(a)+outward*std::sin(a);
            Vec center=m.vertices[id].p-n*radius;
            next.push_back(static_cast<unsigned>(m.vertices.size()));
            m.vertices.push_back(Vertex(center+normal*radius,normal));
        }
        for(unsigned e=0;e<size;++e){unsigned f=(e+1)%size;
            triangle(m,prev[e],next[e],prev[f]);triangle(m,prev[f],next[e],next[f]);
        }
        prev=next;
    }
    return m;
}
struct Digit {float x,z,len,spread,width;};
inline const Digit* digits(){
    // Adult neutral posture: 55 / 75 / 84 / 78 / 60 mm, measured MCP-to-tip.
    static const Digit d[5]={
        {-1.52f,.72f,1.83f,-43.f,.67f},
        {-.99f,.78f,2.50f,-5.f,.60f},
        {-.30f,.88f,2.80f,0.f,.63f},
        {.42f,.81f,2.60f,4.f,.59f},
        {1.08f,.64f,2.00f,10.f,.51f}
    };return d;
}
inline float palmY(float x,float z){
    float rear=std::exp(-std::pow((z+1.65f)/.68f,2.f));
    float bowl=.09f*std::exp(-std::pow(x/1.35f,2.f)-std::pow((z+.45f)/1.1f,2.f));
    return .83f+.09f*rear-bowl+.055f*std::pow(x/2.1f,2.f);
}
inline Vec palmNormal(float x,float z){
    const float h=.001f;
    return unit(Vec(-(palmY(x+h,z)-palmY(x-h,z))/(2*h),1,
                    -(palmY(x,z+h)-palmY(x,z-h))/(2*h)));
}
}
#endif
