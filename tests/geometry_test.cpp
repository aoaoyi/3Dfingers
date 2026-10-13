#include "../geometry.h"
#include <cassert>
#include <iostream>
#include <map>
#include <utility>
#include <limits>
using namespace releaf;
void validateMesh(const Mesh& m){
    std::map<std::pair<unsigned,unsigned>,int> incidence,direction;
    double volume=0;
    for(const Vertex& v:m.vertices){
        assert(std::isfinite(v.p.x)&&std::isfinite(v.p.y)&&std::isfinite(v.p.z));
        assert(std::abs(dot(v.n,v.n)-1.f)<.002f);
    }
    for(unsigned i=0;i<m.indices.size();i+=3){
        unsigned a=m.indices[i],b=m.indices[i+1],c=m.indices[i+2];
        Vec pa=m.vertices[a].p,pb=m.vertices[b].p,pc=m.vertices[c].p;
        Vec n=cross(pb-pa,pc-pa);
        assert(dot(n,n)>1e-12f);
        assert(dot(n,m.vertices[a].n+m.vertices[b].n+m.vertices[c].n)>0);
        volume+=dot(pa,cross(pb,pc))/6.;
        unsigned edges[3][2]={{a,b},{b,c},{c,a}};
        for(auto& e:edges){auto key=std::make_pair(std::min(e[0],e[1]),std::max(e[0],e[1]));
            ++incidence[key];direction[key]+=e[0]<e[1]?1:-1;
        }
    }
    for(auto& e:incidence){assert(e.second==2);assert(direction[e.first]==0);}
    assert(volume>0);
    std::cout<<"Watertight oriented mesh: "<<m.vertices.size()<<" vertices, "<<m.indices.size()/3<<" triangles, volume "<<volume<<"\n";
}
int main(){
    float maxLeftZ=-100,minGuardX=100;
    for(int sign=-1;sign<=1;sign+=2){
        Patch outer=wingPatch(sign);Mesh m=shell(outer,.085f);validateMesh(m);
        Mesh liner=shell(linerPatch(outer),.045f,28,8);validateMesh(liner);
        for(const Vertex& v:m.vertices){
            minGuardX=std::min(minGuardX,std::abs(v.p.x));
            if(sign<0)maxLeftZ=std::max(maxLeftZ,v.p.z);
        }
        for(const Vertex& v:liner.vertices){
            minGuardX=std::min(minGuardX,std::abs(v.p.x));
            if(sign<0)maxLeftZ=std::max(maxLeftZ,v.p.z);
        }
        for(int i=0;i<=20;++i)for(int j=0;j<=20;++j){
            float u=i/20.f,v=j/20.f;
            Vec inside=linerPatch(outer).at(u,v),surface=outer.at(.07f+.86f*u,.12f+.76f*v);
            assert(dot(inside-surface,outer.normal(.07f+.86f*u,.12f+.76f*v))>.135f);
        }
    }
    const Digit* d=digits();float lo[5]={.65f,.85f,1.f,.90f,.70f},hi[5]={.75f,.92f,1.f,.97f,.80f};
    for(int i=0;i<5;++i){float ratio=d[i].len/d[2].len;assert(ratio>=lo[i]&&ratio<=hi[i]);}
    // Sweep the full flexion envelope and cuff adjustment for all five digits.
    // Conservative Z separation proves guard clearance even with width/tilt margins.
    for(int step=0;step<=320;++step){float a=step*.1f*pi/180;
        for(int i=0;i<5;++i){
            float rootZ=d[i].z-.18f;
            if(i==0)assert(rootZ>maxLeftZ+.15f);
            else {
                float tipX=d[i].x+d[i].len*std::sin(d[i].spread*pi/180);
                float widthMargin=d[i].width*.4f+.18f;
                assert(std::max(std::abs(tipX),std::abs(d[i].x))+widthMargin<minGuardX-.1f);
            }
            float y=.88f,z=d[i].z,totalAngle=a*.18f;
            int sections=i==0?2:3;float fractions[3]={.45f,.32f,.23f};
            for(int section=0;section<sections;++section){
                float len=d[i].len*(i==0?(section==0?.54f:.46f):fractions[section]);
                float width=d[i].width*(1-.045f*section);
                if(section==0 || section==sections-1){
                    float center=len*(section==0?.43f:.62f);
                    for(int tiltStep=-4;tiltStep<=4;++tiltStep)for(int arc=0;arc<=24;++arc)for(int side=-1;side<=1;side+=2){
                        float tilt=(section==0?0.f:tiltStep*5.f)*pi/180;
                        float theta=arc*pi/24;
                        float cx=width*.5f*std::cos(theta),cy=.23f+width*.51f*std::sin(theta),cz=side*.105f;
                        float py=cy*std::cos(tilt)-cz*std::sin(tilt);
                        float pz=center+cy*std::sin(tilt)+cz*std::cos(tilt);
                        float globalY=y+py*std::cos(totalAngle)-pz*std::sin(totalAngle);
                        float longitudinal=z-d[i].z+py*std::sin(totalAngle)+pz*std::cos(totalAngle);
                        float spread=d[i].spread*pi/180;
                        float globalX=d[i].x+longitudinal*std::sin(spread)+cx*std::cos(spread);
                        float globalZ=d[i].z+longitudinal*std::cos(spread)-cx*std::sin(spread);
                        if(i==0)assert(globalZ-.06f>maxLeftZ+.05f);
                        else assert(std::abs(globalX)+.06f<minGuardX-.05f);
                        assert(globalY-.06f>0);
                        if(std::pow(globalX/2.08f,2.f)+std::pow((globalZ+.72f)/1.46f,2.f)<1)
                            assert(globalY-.06f>palmY(globalX,globalZ));
                    }
                }
                y-=len*std::sin(totalAngle);z+=len*std::cos(totalAngle);
                assert(y>.08f);assert(z>d[i].z);
                totalAngle+=a*(section==0?.48f:.28f);
            }
        }
    }
    // Palm surface clears both structural ellipsoids across its entire oval.
    for(int r=0;r<=30;++r)for(int j=0;j<80;++j){
        float rho=r/30.f,t=2*pi*j/80;
        float x=2.08f*rho*std::cos(t),z=-.72f+1.46f*rho*std::sin(t);
        float base=.30f+.35f*std::sqrt(std::max(0.f,1-std::pow(x/2.85f,2.f)-std::pow((z+.72f)/2.17f,2.f)));
        float tray=.44f+.27f*std::sqrt(std::max(0.f,1-std::pow(x/2.61f,2.f)-std::pow((z+.70f)/1.88f,2.f)));
        assert(palmY(x,z)>std::max(base,tray)+.02f);
    }
    std::cout<<"PASS: palm/base separation, four-finger guard clearance, anatomy ratios, 321 flexion poses with 9 cuff tilt settings, thumb/left-guard separation, positive mesh normals and liner separation\n";
}
