// ReLeaf Duo v2.0 - Interactive 3D hand-training concept (OpenGL / GLUT)
// Model only; strength, pressure and therapy feedback are simulations.
// Compatible with Code::Blocks + MinGW + freeglut / GLUT (OpenGL fixed pipeline).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include "geometry.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <GL/glut.h>
#include <GL/glu.h>
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct V3 { float x,y,z; V3(float X=0,float Y=0,float Z=0):x(X),y(Y),z(Z){} };
// Named Color3 to avoid collision with the RGB(r,g,b) macro in windows.h.
struct Color3 { float r,g,b; Color3(float R=1,float G=1,float B=1):r(R),g(G),b(B){} };
static const Color3 WHITE(0.87f,0.89f,0.90f), WARM(0.55f,0.63f,0.66f);
static const Color3 BLUE(0.20f,0.39f,0.51f), FOAM(0.29f,0.46f,0.54f);
static const Color3 METAL(0.65f,0.72f,0.76f), DARK(0.105f,0.14f,0.19f);
static const Color3 LED(0.21f,0.73f,1.0f), RINGPAD(0.57f,0.69f,0.83f);

int W=1600,H=900;
float yaw=28.0f,pitch=30.0f,zoom=11.8f;
float modelYaw=0.0f, animationTime=0.0f;
bool rotating=false, playing=false, showHelp=true, showWire=false, showMarks=false;
bool leftDown=false, dragged=false; int lastMouseX=0,lastMouseY=0, downX=0,downY=0;
int selected=-1; // -1 all fingers, 0 thumb ... 4 little
int strength=3; // illustrative level 1...8
int mode=0; // 0 daily, 1 rehab
float cuffAngles[5]={0,0,0,0,0};
float animationSpeed=1.f, movementRange=28.f, sessionSeconds=0;
int repetitions=0;bool sequential=false, assisted=false, emergencyStopped=false;
float displayedAngle=0.0f;
int lastTick=0;
float fingerSX[5]={0}, fingerSY[5]={0};
float controllerSX[7]={0},controllerSY[7]={0};
bool fingerVisible[5]={false},controllerVisible[7]={false};
std::string capturePath; int captureFrame=0, captureAfter=2; bool selfTest=false;
const char* NAMES[5] = {"THUMB","INDEX","MIDDLE","RING","LITTLE"};
const char* MODE_NAMES[2]={"DAILY TRAINING","REHAB TRAINING"};
GLUquadric* quad = 0;

float rad(float a){return a*(float)M_PI/180.0f;}
float clampf(float n,float lo,float hi){return std::max(lo,std::min(hi,n));}
void col(Color3 c){glColor3f(c.r,c.g,c.b);}
void mat(Color3 c,float shine=28.0f){
    GLfloat dif[]={c.r,c.g,c.b,1}, amb[]={c.r*.45f,c.g*.45f,c.b*.45f,1};
    GLfloat spec[]={0.16f,0.18f,0.20f,1};
    glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,amb);
    glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,dif);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,spec);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,shine);
    col(c);
}
void ell(float x,float y,float z,float sx,float sy,float sz,Color3 c,int detail=24){
    glPushMatrix(); glTranslatef(x,y,z); glScalef(sx,sy,sz);
    mat(c); glutSolidSphere(1,detail,detail/2); glPopMatrix();
}
// axis-aligned segment in local Z (efficient for articulated finger rails)
void segment(float a,float b,float y,float xoff,float thickness,Color3 color){
    if (b<=a) return;
    glPushMatrix(); glTranslatef(xoff,y,a); mat(color,70);
    gluCylinder(quad,thickness,thickness,b-a,14,1);
    glPopMatrix();
}
void tube(V3 a,V3 b,float radius,Color3 c){
    V3 d(b.x-a.x,b.y-a.y,b.z-a.z); float l=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
    if(l <.001f)return;
    glPushMatrix();glTranslatef(a.x,a.y,a.z);
    float angle=std::acos(clampf(d.z/l,-1,1))*180.0f/(float)M_PI;
    if(std::abs(d.x)+std::abs(d.y)>.00001f)glRotatef(angle,-d.y,d.x,0);
    else if(d.z<0)glRotatef(180,1,0,0);
    mat(c,45);
    gluCylinder(quad,radius,radius,l,14,1);glPopMatrix();
}
void torus(float x,float y,float z,float inner,float outer,Color3 color){
    glPushMatrix(); glTranslatef(x,y,z);mat(color,76); glutSolidTorus(inner,outer,12,40);glPopMatrix();
}
void line(V3 a,V3 b,Color3 color,float thick=2.0f){
    glDisable(GL_LIGHTING);col(color);glLineWidth(thick);
    glBegin(GL_LINES);glVertex3f(a.x,a.y,a.z);glVertex3f(b.x,b.y,b.z);glEnd();
    glEnable(GL_LIGHTING);
}
void text3(float x,float y,float z,const char* s,Color3 color){
    glDisable(GL_LIGHTING);col(color);glRasterPos3f(x,y,z);
    for(const char*p=s;*p;p++)glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12,*p);
    glEnable(GL_LIGHTING);
}
void drawCube(float x,float y,float z,float sx,float sy,float sz,Color3 c){
    glPushMatrix();glTranslatef(x,y,z);glScalef(sx,sy,sz);mat(c);glutSolidCube(1);glPopMatrix();
}

// Cached CPU meshes share the same Bezier evaluator used by geometry tests.
void drawMesh(const releaf::Mesh& mesh,Color3 color,float shine){
    mat(color,shine);glBegin(GL_TRIANGLES);
    for(unsigned id:mesh.indices){const releaf::Vertex& v=mesh.vertices[id];
        glNormal3f(v.n.x,v.n.y,v.n.z);glVertex3f(v.p.x,v.p.y,v.p.z);
    }glEnd();
}
void wing(int sign){
    static const releaf::Mesh left=releaf::shell(releaf::wingPatch(-1),.085f);
    static const releaf::Mesh right=releaf::shell(releaf::wingPatch(1),.085f);
    drawMesh(sign<0?left:right,WHITE,30);
    // Separate closed soft liner is recessed from the shell perimeter.
    static releaf::Mesh liners[2];
    if(liners[0].vertices.empty())for(int k=0;k<2;++k){
        releaf::Patch inner=releaf::linerPatch(releaf::wingPatch(k==0?-1:1));
        liners[k]=releaf::shell(inner,.045f,28,8);
    }
    drawMesh(liners[sign<0?0:1],FOAM,6);
}
float cushionY(float x,float z){return releaf::palmY(x,z);}
void cushion(){
    // Oval foam volume; rounded skirt joins the surface to the tray.
    const int rings=30,slices=80;mat(FOAM,7);
    for(int r=0;r<rings;++r){
        glBegin(GL_TRIANGLE_STRIP);
        for(int j=0;j<=slices;++j)for(int side=1;side>=0;--side){
            float rho=(r+side)/float(rings),a=2*releaf::pi*j/slices;
            float x=2.08f*rho*std::cos(a),z=-.72f+1.46f*rho*std::sin(a);
            releaf::Vec n=releaf::palmNormal(x,z);
            glNormal3f(n.x,n.y,n.z);glVertex3f(x,cushionY(x,z),z);
        }glEnd();
    }
    for(int r=0;r<10;++r){glBegin(GL_TRIANGLE_STRIP);
        for(int j=0;j<=slices;++j)for(int side=1;side>=0;--side){
            float a=2*releaf::pi*j/slices,t=(r+side)*releaf::pi/20;
            float x=(2.08f+.085f*std::sin(t))*std::cos(a);
            float z=-.72f+(1.46f+.085f*std::sin(t))*std::sin(a);
            glNormal3f(std::cos(a)*std::sin(t),std::cos(t),std::sin(a)*std::sin(t));
            glVertex3f(x,cushionY(x,z)-.16f*(1-std::cos(t)),z);
        }glEnd();
    }
    // Finishing seam lies on the oval surface rather than crossing its edge.
    glDisable(GL_LIGHTING);col(BLUE);glBegin(GL_LINE_LOOP);
    for(int j=0;j<100;++j){float a=2*releaf::pi*j/100;
        float x=2.02f*std::cos(a),z=-.72f+1.40f*std::sin(a);
        glVertex3f(x,cushionY(x,z)+.009f,z);
    }glEnd();glEnable(GL_LIGHTING);
}
void chassis(){
    // white monocoque with large cushioned inner tray
    ell(0,.30f,-.72f,2.85f,.35f,2.17f,WHITE,40);
    ell(0,.44f,-.70f,2.61f,.27f,1.88f,WARM,36);
    cushion();
    wing(-1);wing(1);
    // Integrated drive / electronics pod behind wrist, without protruding above use space
    ell(0,.30f,-2.70f,2.30f,.32f,.80f,WHITE,36);
    ell(0,.88f,-2.80f,2.13f,.53f,.50f,WHITE,36);
    ell(0,1.32f,-2.87f,1.86f,.12f,.36f,WARM,32);
    drawCube(0,1.443f,-2.87f,.65f,.018f,.025f,LED);
    // left and right frame foot details
    for(int s=-1;s<=1;s+=2)ell(s*2.20f,.30f,-1.85f,.38f,.22f,.44f,WARM,20);
}
void pin(float x,float y,float z){
    // Physical pivot BELOW the finger cuff, never mounted on its side or above.
    glPushMatrix();glTranslatef(x,y,z);glRotatef(90,0,1,0);
    mat(METAL,110);gluCylinder(quad,.075f,.075f,.18f,24,1);
    torus(0,0,.185f,.014f,.068f,WHITE);
    ell(0,0,.18f,.057f,.057f,.020f,METAL,20);
    glPopMatrix();
}
void arch(float centerZ,float w,float tilt=0.0f){
    glPushMatrix();glTranslatef(0,0,centerZ);
    // Bearing axis stays fixed on the rail; the cuff tilts around this axis.
    tube(V3(-w*.42f,-.11f,0),V3(w*.42f,-.11f,0),.055f,METAL);
    pin(-w*.40f-.09f,-.11f,0);pin(w*.40f-.09f,-.11f,0);
    glTranslatef(0,-.11f,0);glRotatef(tilt,1,0,0);glTranslatef(0,.11f,0);
    for(int side=-1;side<=1;side+=2)
        tube(V3(side*w*.40f,-.11f,0),V3(side*w*.49f,.12f,0),.055f,WHITE);
    for(int i=0;i<24;++i){
        float a=releaf::pi*i/24,b=releaf::pi*(i+1)/24;
        for(int side=-1;side<=1;side+=2){
            float z=side*.105f;
            tube(V3(w*.5f*std::cos(a),.12f+w*.51f*std::sin(a),z),
                 V3(w*.5f*std::cos(b),.12f+w*.51f*std::sin(b),z),.052f,WHITE);
        }
        tube(V3((w*.5f-.060f)*std::cos(a),.12f+(w*.51f-.055f)*std::sin(a),0),
             V3((w*.5f-.060f)*std::cos(b),.12f+(w*.51f-.055f)*std::sin(b),0),.032f,RINGPAD);
        if(i%4==0)tube(V3(w*.5f*std::cos(a),.12f+w*.51f*std::sin(a),-.105f),
                           V3(w*.5f*std::cos(a),.12f+w*.51f*std::sin(a),.105f),.036f,WHITE);
    }
    ell(0,.12f+w*.51f,0,w*.30f,.060f,.14f,BLUE,24);
    glPopMatrix();
}
void fingerPad(float length,float w){
    // integrated soft bed below finger: flexible medical silicone liner
    ell(0,.013f,length*.52f,w*.42f,.12f,length*.60f,RINGPAD,20);
    // perforations are deliberately procedural; no image loading in display callback
    glDisable(GL_LIGHTING);glPointSize(2.0f);glColor3f(.43f,.52f,.64f);
    glBegin(GL_POINTS);
    for(int i=-2;i<=2;i++)for(int k=0;k<18;k++){
        float z=.18f+length*.77f*k/18;
        float x=i*w*.105f;
        float f=1-std::pow(x/(w*.42f),2.f)-std::pow((z-length*.52f)/(length*.60f),2.f);
        if(f>0)glVertex3f(x,.015f+.12f*std::sqrt(f),z);
    }glEnd();glEnable(GL_LIGHTING);
}
// Finger lengths and positions are deliberately ANATOMICAL rather than five identical arms.
typedef releaf::Digit Digit;
const Digit* const digits=releaf::digits();
float angleFor(int i){
    if(selected!=-1 && selected!=i)return 0;
    // Pausing holds the current pose; daily can be synchronized or sequential.
    float phase=sequential && mode==0?i*.9f:0;
    float range=movementRange*(mode==1?.62f:1.f)*(assisted?.85f:1.f);
    return range*(.5f-.5f*std::cos(animationTime+phase));
}
bool screenPoint(float x,float y,float z,float& sx,float& sy){
    GLdouble model[16],proj[16],px,py,pz;GLint view[4];
    glGetDoublev(GL_MODELVIEW_MATRIX,model);glGetDoublev(GL_PROJECTION_MATRIX,proj);
    glGetIntegerv(GL_VIEWPORT,view);
    if(gluProject(x,y,z,model,proj,view,&px,&py,&pz)!=GL_TRUE)return false;
    sx=static_cast<float>(px);sy=H-static_cast<float>(py);
    return pz>=0 && pz<=1 && sx>=0 && sx<W && sy>=0 && sy<H;
}
void finger(int i){
    const Digit d=digits[i];float a=angleFor(i);
    glPushMatrix();glTranslatef(d.x,.99f,d.z);glRotatef(d.spread,0,1,0);
    // Root bracket seats the moving skeleton on the tray's front rim.
    float baseY=.30f+.35f*std::sqrt(std::max(0.f,1-std::pow(d.x/2.85f,2.f)-std::pow((d.z+.72f)/2.17f,2.f)));
    tube(V3(0,baseY-.99f,0),V3(0,-.11f,0),.085f,METAL);
    tube(V3(-d.width*.38f,-.11f,0),V3(d.width*.38f,-.11f,0),.065f,METAL);
    glTranslatef(0,-.11f,0);glRotatef(a*.18f,1,0,0);glTranslatef(0,.11f,0);
    const int sections=i==0?2:3;
    const float fractions[3]={.45f,.32f,.23f};
    for(int section=0;section<sections;++section){
        float length=d.len*(i==0?(section==0?.54f:.46f):fractions[section]);
        float width=d.width*(1.f-.045f*section);
        fingerPad(length,width);
        for(int side=-1;side<=1;side+=2){
            segment(0,length,-.11f,side*width*.38f,.047f,METAL);
            ell(side*width*.38f,-.11f,length,.075f,.075f,.075f,METAL,20);
        }
        if(section==0)arch(length*.43f,width);
        if(section==sections-1){
            arch(length*.62f,width,cuffAngles[i]);
            fingerVisible[i]=screenPoint(0,.28f,length*.62f,fingerSX[i],fingerSY[i]);
            if(selected==i){
                torus(0,-.12f,length*.62f,.016f,width*.58f,LED);
            }
        }else{
            // Child transform rotates at the shared bearing center, without a gap.
            glTranslatef(0,-.11f,length);
            glRotatef(a*(section==0?.48f:.28f),1,0,0);
            glTranslatef(0,.11f,0);
        }
    }
    glPopMatrix();
}
void knob(float x,float y,float z,float radius){
    glPushMatrix();glTranslatef(x,y,z);
    glRotatef(-90,1,0,0);mat(METAL,110);gluCylinder(quad,radius,radius,.24f,48,1);
    glTranslatef(0,0,.24f);mat(WHITE,90);gluDisk(quad,0,radius,48,1);
    torus(0,0,.03f,.030f,radius*.85f,BLUE);
    glPopMatrix();
}
void adjustCuff(float delta){
    for(int i=0;i<5;++i)if(selected<0 || selected==i)cuffAngles[i]=clampf(cuffAngles[i]+delta,-20,20);
}
void startPause(){
    if(repetitions>=20){repetitions=0;sessionSeconds=0;animationTime=0;}
    emergencyStopped=false;playing=!playing;
}
void stopTraining(){playing=false;emergencyStopped=true;}
void controllerAction(int id){
    switch(id){
        case 0:strength=strength%8+1;break;
        case 1:strength=std::max(1,strength-1);break;
        case 2:strength=std::min(8,strength+1);break;
        case 3:mode=1-mode;break;
        case 4:startPause();break;
        case 5:selected=selected==4?-1:selected+1;break;
        case 6:stopTraining();break;
    }
}
void controllerTarget(int id,float x,float y,float z){
    controllerVisible[id]=screenPoint(x,y,z,controllerSX[id],controllerSY[id]);
}
void roundedSlab(float halfX,float halfZ,float radius,float bottom,float top,float zCenter,Color3 color){
    std::vector<V3> edge;
    for(int corner=0;corner<4;++corner)for(int j=0;j<=12;++j){
        float a=(corner*90+j*7.5f)*releaf::pi/180;
        float cx=(corner==0 || corner==3?1.f:-1.f)*(halfX-radius);
        float cz=(corner<2?1.f:-1.f)*(halfZ-radius);
        edge.push_back(V3(cx+radius*std::cos(a),0,zCenter+cz+radius*std::sin(a)));
    }
    mat(color,20);
    glBegin(GL_TRIANGLE_FAN);glNormal3f(0,1,-.05f);glVertex3f(0,top+.05f*zCenter,zCenter);
    for(int j=int(edge.size());j>=0;--j){const V3& p=edge[j%edge.size()];glVertex3f(p.x,top+.05f*p.z,p.z);}glEnd();
    glBegin(GL_TRIANGLE_FAN);glNormal3f(0,-1,0);glVertex3f(0,bottom,zCenter);
    for(unsigned j=0;j<=edge.size();++j){const V3& p=edge[j%edge.size()];glVertex3f(p.x,bottom,p.z);}glEnd();
    glBegin(GL_QUAD_STRIP);
    for(unsigned j=0;j<=edge.size();++j){const V3& p=edge[j%edge.size()];
        const V3& before=edge[(j+edge.size()-1)%edge.size()];const V3& after=edge[(j+1)%edge.size()];
        releaf::Vec n=releaf::unit(releaf::Vec(after.z-before.z,0,before.x-after.x));
        glNormal3f(n.x,0,n.z);glVertex3f(p.x,bottom,p.z);glVertex3f(p.x,top+.05f*p.z,p.z);
    }glEnd();
}
void controller(){
    glPushMatrix();glTranslatef(4.50f,0,1.12f);
    roundedSlab(1.16f,1.30f,.38f,.12f,.69f,0,WHITE);
    roundedSlab(.98f,.66f,.18f,.68f,.715f,-.30f,DARK);
    text3(-.77f,.715f-.60f*.05f+.004f,-.60f,mode==0?"DAILY":"REHAB",WHITE);
    char status[128];
    std::snprintf(status,sizeof(status),"%s L%d %s",selected<0?"ALL":NAMES[selected],strength,playing?"RUN":"PAUSE");
    text3(-.77f,.715f-.34f*.05f+.004f,-.34f,status,WHITE);
    std::snprintf(status,sizeof(status),"%d REP %02d:%02d %.0f DEG",repetitions,int(sessionSeconds)/60,int(sessionSeconds)%60,displayedAngle);
    text3(-.77f,.715f-.08f*.05f+.004f,-.08f,status,WHITE);
    // Progress bar on the inclined display surface.
    line(V3(-.76f,.729f,.20f),V3(.76f,.729f,.20f),BLUE,3);
    line(V3(-.76f,.730f,.20f),V3(-.76f+1.52f*std::min(1.f,repetitions/20.f),.730f,.20f),LED,3);
    knob(0,.735f,.70f,.31f);controllerTarget(0,0,.99f,.70f);
    for(int side=-1;side<=1;side+=2){
        ell(side*.74f,.765f,.82f,.13f,.065f,.15f,BLUE,20);
        text3(side*.74f-.04f,.833f,.82f,side<0?"-":"+",WHITE);
        controllerTarget(side<0?1:2,side*.74f,.833f,.82f);
    }
    const char* labels[4]={"M","II","F","!"};
    for(int j=0;j<4;++j){float x=-.78f+j*.52f;
        ell(x,.664f,-1.02f,.18f,.060f,.14f,j==3?Color3(.64f,.19f,.17f):BLUE,20);
        text3(x-.035f,.727f,-1.02f,labels[j],WHITE);controllerTarget(j+3,x,.727f,-1.02f);
    }
    for(int j=0;j<8;++j)ell(-.68f+j*.195f,.738f,.36f,.035f,.025f,.045f,j<strength?LED:WARM,12);
    glPopMatrix();
    // Cable endpoint terminates in the console's rear side wall.
    V3 last(1.90f,.64f,-2.56f);
    for(int k=1;k<=36;++k){float t=k/36.f;
        V3 cur(1.90f+2.60f*t,.64f-.27f*std::sin(t*releaf::pi),-2.56f+2.38f*t);
        tube(last,cur,.044f,WARM);last=cur;
    }
}
void ground(){
    glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for(int i=0;i<14;i++){
        float a=.036f*(15-i)/15.0f;
        glColor4f(.42f,.55f,.68f,a);
        glBegin(GL_TRIANGLE_FAN);glVertex3f(.5f,-.105f,0.0f);
        for(int j=0;j<=70;j++){
            float t=j*2*(float)M_PI/70;
            glVertex3f(.5f+(.4f+i*.37f)*std::cos(t),-.105f,0+(.2f+i*.25f)*std::sin(t));
        }glEnd();
    }
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_LIGHTING);
}
void model(){
    glPushMatrix();glRotatef(modelYaw,0,1,0);
    ground();chassis();
    for(int i=0;i<5;i++)finger(i);
    controller();
    glPopMatrix();
}

void uiText(int x,int y,const std::string& s,Color3 c,bool large=false){
    float scale=std::min(W/1600.f,H/900.f);scale=std::max(.65f,scale);
    float px=x>W/2?W-(W-x)*scale:x*scale;
    float py=y>H/2?H-(H-y)*scale:y*scale;
    if(scale<=1.1f){col(c);glRasterPos2f(px,H-py);
        for(unsigned char ch:s)glutBitmapCharacter(large?GLUT_BITMAP_HELVETICA_18:GLUT_BITMAP_HELVETICA_12,ch);
        return;
    }
    glEnable(GL_LINE_SMOOTH);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glPushMatrix();glTranslatef(px,H-py,0);
    float fontScale=scale*(large?18.f:12.f)/119.05f;
    glScalef(fontScale,fontScale,fontScale);col(c);glLineWidth(std::max(1.f,scale));
    for(unsigned char ch:s)glutStrokeCharacter(GLUT_STROKE_ROMAN,ch);
    glPopMatrix();glDisable(GL_BLEND);glDisable(GL_LINE_SMOOTH);
}
void hud(){
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();gluOrtho2D(0,W,0,H);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);
    // clean text-based presentation around a centrally framed 3D object
    uiText(36,44,"ReLeaf Duo",DARK,true);
    uiText(36,67,"BIOMIMETIC HAND TRAINING & REHABILITATION",Color3(.35f,.44f,.55f));
    uiText(W-282,42,mode==0?"DAILY MODE":"REHAB MODE",DARK,true);
    char s[128];std::sprintf(s,"SIMULATED STRENGTH   %d / 8",strength);
    uiText(W-282,67,s,Color3(.36f,.44f,.55f));
    uiText(W-282,90,playing?"MOTION: RUNNING":"MOTION: PAUSED",playing?Color3(.16f,.60f,.78f):Color3(.40f,.49f,.59f));
    if(showHelp){
    uiText(37,H-149,"CLICK FINGERTIP OR PRESS 1-5",Color3(.35f,.44f,.55f));
    uiText(37,H-130,"0 ALL  |  SPACE ANIMATE  |  M MODE",Color3(.35f,.44f,.55f));
    uiText(37,H-111,"+/- STRENGTH  |  [ ] CUFF TILT",Color3(.35f,.44f,.55f));
    uiText(37,H-92,"DRAG ORBIT  |  WHEEL ZOOM",Color3(.35f,.44f,.55f));
    uiText(37,H-73,"A AUTOROTATE  |  R RESET  |  H HELP",Color3(.35f,.44f,.55f));
    }
    if(showHelp){
        std::string txt="SELECTED: ";txt+=(selected<0?"ALL FINGERS":NAMES[selected]);
        uiText(W-282,H-74,txt,Color3(.23f,.43f,.62f));
        std::sprintf(s,"CUFF PIVOT: %+0.0f DEG",cuffAngles[selected<0?2:selected]);
        uiText(W-282,H-56,s,Color3(.39f,.49f,.58f));
        uiText(W-282,H-38,"RIGHT CLICK FOR MENU",Color3(.42f,.51f,.60f));
    }
    std::snprintf(s,sizeof(s),"SIMULATION ONLY | %02d:%02d | %d REP | ANGLE %.0f | RANGE %.0f | SPEED %.2f",
        int(sessionSeconds)/60,int(sessionSeconds)%60,repetitions,displayedAngle,movementRange,animationSpeed);
    uiText(36,H-20,s,DARK);
    uiText(W-282,113,emergencyStopped?"EMERGENCY STOP":(assisted?"ACTIVE-ASSISTED":"PASSIVE SIMULATION"),DARK);
    std::snprintf(s,sizeof(s),"SESSION PROGRESS %d%%",std::min(100,repetitions*5));
    uiText(W-282,134,s,DARK);
    glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);
    glMatrixMode(GL_MODELVIEW);glPopMatrix();
    glMatrixMode(GL_PROJECTION);glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
void runInteractionChecks();
void display(){
    glClearColor(.90f,.92f,.935f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    float ry=rad(yaw),rp=rad(pitch);
    float tx=1.05f,ty=.67f,tz=.20f;
    gluLookAt(tx+zoom*std::sin(ry)*std::cos(rp),ty+zoom*std::sin(rp),
              tz+zoom*std::cos(ry)*std::cos(rp), tx,ty,tz, 0,1,0);
    GLfloat key[]={-3.0f,8.0f,9.0f,0.0f};glLightfv(GL_LIGHT0,GL_POSITION,key);
    GLfloat fill[]={7.0f,4.0f,-5.0f,1.0f};glLightfv(GL_LIGHT1,GL_POSITION,fill);
    model();hud();
    if(selfTest && captureFrame==0){runInteractionChecks();selfTest=false;if(capturePath.empty())std::exit(0);}
    if(!capturePath.empty() && ++captureFrame>=captureAfter){
        std::vector<unsigned char> pixels(static_cast<size_t>(W)*H*3);
        glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
        glReadPixels(0,0,W,H,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        FILE* file=std::fopen(capturePath.c_str(),"wb");
        if(!file){std::perror("capture");std::exit(2);}
        std::fprintf(file,"P6\n%d %d\n255\n",W,H);
        for(int y=H-1;y>=0;--y)std::fwrite(pixels.data()+static_cast<size_t>(y)*W*3,1,W*3,file);
        if(std::fclose(file)!=0 || glGetError()!=GL_NO_ERROR)std::exit(3);
        std::printf("Captured %s (%d x %d), angle %.2f, session %.2fs\n",capturePath.c_str(),W,H,displayedAngle,sessionSeconds);std::exit(0);
    }
    glutSwapBuffers();
}
void reshape(int w,int h){
    W=std::max(w,1);H=std::max(h,1);
    glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();
    gluPerspective(43.0,(double)W/H,.1,100.0);
    glMatrixMode(GL_MODELVIEW);
}
void reset(){yaw=28;pitch=30;zoom=11.8f;modelYaw=0;selected=-1;rotating=false;}
void key(unsigned char k,int,int){
    switch(k){
        case 27:std::exit(0);
        case ' ':startPause();break;
        case '0':selected=-1;break;
        case '1':case '2':case '3':case '4':case '5':selected=k-'1';break;
        case 'm':case 'M':mode=1-mode;break;
        case 'a':case 'A':rotating=!rotating;break;
        case 'h':case 'H':showHelp=!showHelp;break;
        case 'r':case 'R':reset();break;
        case '+':case '=':strength=std::min(8,strength+1);break;
        case '-':case '_':strength=std::max(1,strength-1);break;
        case '[':adjustCuff(-5);break;
        case ']':adjustCuff(5);break;
        case 'w':case 'W':pitch=clampf(pitch+4,-5,78);break;
        case 's':case 'S':yaw=90;pitch=12;break;
        case 'f':case 'F':yaw=0;pitch=12;break;
        case 'e':case 'E':stopTraining();break;
        case 't':case 'T':sequential=!sequential;break;
        case 'b':case 'B':assisted=!assisted;break;
        case ',':animationSpeed=std::max(.25f,animationSpeed-.25f);break;
        case '.':animationSpeed=std::min(2.f,animationSpeed+.25f);break;
        case 'j':case 'J':movementRange=std::max(8.f,movementRange-2);break;
        case 'k':case 'K':movementRange=std::min(32.f,movementRange+2);break;
        case 'd':case 'D':yaw+=5;break;
        case 'q':case 'Q':yaw-=5;break;
        case 'v':case 'V':yaw=0;pitch=78;break;
        default:break;
    }glutPostRedisplay();
}
void special(int k,int,int){
    if(k==GLUT_KEY_LEFT)yaw-=5;
    if(k==GLUT_KEY_RIGHT)yaw+=5;
    if(k==GLUT_KEY_UP)pitch=clampf(pitch+5,-5,78);
    if(k==GLUT_KEY_DOWN)pitch=clampf(pitch-5,-5,78);
    glutPostRedisplay();
}
void pick(int x,int y){
    float targetRadius=std::max(12.f,H*.035f);
    int control=-1;float controlDistance=targetRadius*targetRadius;
    for(int id=0;id<7;++id){float dx=x-controllerSX[id],dy=y-controllerSY[id],dist=dx*dx+dy*dy;
        if(controllerVisible[id] && dist<controlDistance){controlDistance=dist;control=id;}
    }
    if(control>=0){controllerAction(control);return;}
    float closest=targetRadius*targetRadius;int idx=-1;
    for(int i=0;i<5;i++){
        float dx=x-fingerSX[i],dy=y-fingerSY[i],d=dx*dx+dy*dy;
        if(fingerVisible[i] && d<closest){closest=d;idx=i;}
    }
    if(idx>=0){selected=idx;return;}
    if(x>W-310&&y<130){mode=1-mode;return;}
}
void mouse(int button,int state,int x,int y){
    if(button==GLUT_LEFT_BUTTON){
        if(state==GLUT_DOWN){leftDown=true;dragged=false;downX=x;downY=y;lastMouseX=x;lastMouseY=y;rotating=false;}
        else {leftDown=false;if(!dragged)pick(x,y);}
    }
    if(state==GLUT_DOWN&&button==3)zoom=clampf(zoom-.7f,8.0f,23);
    if(state==GLUT_DOWN&&button==4)zoom=clampf(zoom+.7f,8.0f,23);
    glutPostRedisplay();
}
void motion(int x,int y){
    if(!leftDown)return;
    if(std::abs(x-downX)+std::abs(y-downY)>4)dragged=true;
    yaw+=(x-lastMouseX)*.38f;pitch=clampf(pitch+(y-lastMouseY)*.25f,-5,78);
    lastMouseX=x;lastMouseY=y;glutPostRedisplay();
}
void advanceTraining(float dt){
    if(playing){
        sessionSeconds+=dt;float before=animationTime;
        float loadRate=mode==0?1.4f*(1-.035f*(strength-1)):.70f*(assisted?1+.04f*strength:1.f);
        animationTime+=dt*animationSpeed*loadRate;
        repetitions+=int(animationTime/(2*releaf::pi))-int(before/(2*releaf::pi));
        if(repetitions>=20)playing=false;
    }
    displayedAngle=angleFor(selected<0?2:selected);
}
void timer(int){
    int now=glutGet(GLUT_ELAPSED_TIME);float dt=clampf((now-lastTick)*.001f,0,.04f);lastTick=now;
    advanceTraining(dt);
    if(rotating)modelYaw+=dt*11;
    glutPostRedisplay();glutTimerFunc(16,timer,0);
}
void menu(int v){
    switch(v){
        case 1:startPause();break;
        case 2:mode=1-mode;break;
        case 3:rotating=!rotating;break;
        case 4:strength=std::min(8,strength+1);break;
        case 5:strength=std::max(1,strength-1);break;
        case 6:selected=-1;break;
        case 7:reset();break;
    }glutPostRedisplay();
}
void init(){
    quad=gluNewQuadric();gluQuadricNormals(quad,GLU_SMOOTH);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
    glEnable(GL_NORMALIZE);glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);glEnable(GL_LIGHT1);glDisable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_MULTISAMPLE);glShadeModel(GL_SMOOTH);
    GLfloat global[]={.26f,.26f,.27f,1};glLightModelfv(GL_LIGHT_MODEL_AMBIENT,global);
    GLfloat a[]={0,0,0,1},d[]={.84f,.83f,.80f,1};
    glLightfv(GL_LIGHT0,GL_AMBIENT,a);glLightfv(GL_LIGHT0,GL_DIFFUSE,d);
    GLfloat fill[]={.20f,.25f,.30f,1};glLightfv(GL_LIGHT1,GL_DIFFUSE,fill);
    GLfloat spec[]={.24f,.24f,.24f,1};glLightfv(GL_LIGHT0,GL_SPECULAR,spec);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    lastTick=glutGet(GLUT_ELAPSED_TIME);
}
void runInteractionChecks(){
    int originalW=W,originalH=H;
    const auto check=[](bool ok,const char* label){if(!ok){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(4);}};
    key('3',0,0);check(selected==2,"individual selection");
    key(']',0,0);check(cuffAngles[2]==5 && cuffAngles[1]==0,"per-finger cuff adjustment");
    key('[',0,0);key('0',0,0);key(' ',0,0);check(playing,"start animation");
    animationTime=1.f;float held=angleFor(2);key(' ',0,0);check(!playing && angleFor(2)==held,"pause holds pose");
    key('m',0,0);check(mode==1,"rehab mode");key('m',0,0);
    key('+',0,0);check(strength==4,"intensity");key('-',0,0);
    key('t',0,0);check(sequential,"sequential mode");key('t',0,0);
    key('b',0,0);check(assisted,"active assistance");key('b',0,0);
    key('j',0,0);check(movementRange==26,"range");key('k',0,0);
    key('.',0,0);check(animationSpeed==1.25f,"speed");key(',',0,0);
    key('f',0,0);check(yaw==0 && pitch==12,"front view");
    key('s',0,0);check(yaw==90,"side view");key('v',0,0);check(pitch==78,"top view");key('r',0,0);
    float oldZoom=zoom;mouse(3,GLUT_DOWN,0,0);check(zoom<oldZoom,"wheel zoom");mouse(4,GLUT_DOWN,0,0);
    mouse(GLUT_LEFT_BUTTON,GLUT_DOWN,100,100);motion(120,110);mouse(GLUT_LEFT_BUTTON,GLUT_UP,120,110);
    check(dragged && yaw!=28,"mouse orbit");key('r',0,0);
    // Projected button/finger coordinates were obtained from the actual rendered scene.
    check(fingerVisible[2],"finger screen projection");pick(int(fingerSX[2]),int(fingerSY[2]));check(selected==2,"finger mouse selection");
    check(controllerVisible[2],"controller screen projection");
    pick(int(controllerSX[2]),int(controllerSY[2]));check(strength==4,"controller plus click");
    controllerAction(4);check(playing,"controller run");float stoppedAngle=angleFor(2);controllerAction(6);
    check(emergencyStopped && !playing && angleFor(2)==stoppedAngle,"emergency stop holds pose");
    key(' ',0,0);check(playing && !emergencyStopped,"restart after stop");
    animationTime=2*releaf::pi-.001f;sessionSeconds=0;repetitions=0;
    advanceTraining(.04f);check(repetitions==1 && sessionSeconds>.03f,"session timer and repetition counter");
    animationTime=40*releaf::pi-.001f;repetitions=19;advanceTraining(.04f);
    check(repetitions==20 && !playing,"controlled session completion");
    startPause();check(repetitions==0 && playing && sessionSeconds==0,"new training session");
    playing=false;animationTime=0;strength=3;sessionSeconds=0;key('r',0,0);
    reshape(1200,750);check(W==1200 && H==750,"resize projection");reshape(originalW,originalH);
    check(glGetError()==GL_NO_ERROR,"OpenGL state");
    std::puts("PASS: interaction/state/render checks");
}
int main(int argc,char**argv){
#ifdef _WIN32
    SetProcessDPIAware();
#endif
    for(int i=1;i<argc;++i){
        if(std::strcmp(argv[i],"--capture")==0 && i+1<argc)capturePath=argv[++i];
        else if(std::strcmp(argv[i],"--play")==0)playing=true;
        else if(std::strcmp(argv[i],"--frames")==0 && i+1<argc){
            captureAfter=std::atoi(argv[++i]);if(captureAfter<2 || captureAfter>10000)return 2;
        }
        else if(std::strcmp(argv[i],"--size")==0 && i+2<argc){
            W=std::atoi(argv[++i]);H=std::atoi(argv[++i]);
            if(W<320 || H<240 || W>8192 || H>8192){std::fprintf(stderr,"Invalid window size\n");return 2;}
        }
        else if(std::strcmp(argv[i],"--view")==0 && i+1<argc){
            std::string view=argv[++i];
            if(view=="top"){yaw=0;pitch=78;}
            else if(view=="side"){yaw=90;pitch=12;}
            else if(view=="front"){yaw=0;pitch=12;}
            else if(view!="perspective"){std::fprintf(stderr,"Unknown view\n");return 2;}
        }else if(std::strcmp(argv[i],"--flex")==0)animationTime=releaf::pi;
        else if(std::strcmp(argv[i],"--self-test")==0)selfTest=true;
        else {std::fprintf(stderr,"Usage: ReLeaf_Duo [--capture file.ppm] [--view perspective|top|front|side] [--flex] [--self-test] [--size width height] [--play] [--frames count]\n");return 2;}
    }
    int glutArgc=1;glutInit(&glutArgc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH|GLUT_MULTISAMPLE);
    if(!glutGet(GLUT_DISPLAY_MODE_POSSIBLE))glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowSize(W,H);
    glutInitWindowPosition(60,40);
    glutCreateWindow("ReLeaf Duo - Interactive 3D Hand Trainer");
    init();
    glutDisplayFunc(display);glutReshapeFunc(reshape);
    glutKeyboardFunc(key);glutSpecialFunc(special);
    glutMouseFunc(mouse);glutMotionFunc(motion);
    glutCreateMenu(menu);
    glutAddMenuEntry("Start / pause finger motion",1);
    glutAddMenuEntry("Switch Daily / Rehab",2);
    glutAddMenuEntry("Auto rotation on / off",3);
    glutAddMenuEntry("Increase strength",4);
    glutAddMenuEntry("Decrease strength",5);
    glutAddMenuEntry("Select all fingers",6);
    glutAddMenuEntry("Reset view",7);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
    glutTimerFunc(16,timer,0);
    glutMainLoop();return 0;
}
