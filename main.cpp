// ReLeaf Duo v1.1 - Interactive 3D hand-training concept (OpenGL / GLUT)
// Model only; strength, pressure and therapy feedback are simulations.
// Compatible with Code::Blocks + MinGW + freeglut / GLUT (OpenGL fixed pipeline).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#ifdef _WIN32
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
static const Color3 WHITE(0.93f,0.96f,0.97f), WARM(0.82f,0.87f,0.90f);
static const Color3 BLUE(0.57f,0.68f,0.83f), FOAM(0.66f,0.75f,0.87f);
static const Color3 METAL(0.65f,0.72f,0.76f), DARK(0.105f,0.14f,0.19f);
static const Color3 LED(0.21f,0.73f,1.0f), RINGPAD(0.57f,0.69f,0.83f);

int W=1600,H=900;
float yaw=28.0f,pitch=30.0f,zoom=10.8f;
float modelYaw=0.0f, animationTime=0.0f;
bool rotating=false, playing=false, showHelp=true, showWire=false, showMarks=false;
bool leftDown=false, dragged=false; int lastMouseX=0,lastMouseY=0, downX=0,downY=0;
int selected=-1; // -1 all fingers, 0 thumb ... 4 little
int strength=3; // illustrative level 1...8
int mode=0; // 0 daily, 1 rehab
int scheme=0;
float cuffFineAdjustment=0.0f; // visual mounting angle, not medically validated
float displayedAngle=0.0f;
int lastTick=0;
float fingerSX[5]={0}, fingerSY[5]={0};
const char* NAMES[5] = {"THUMB","INDEX","MIDDLE","RING","LITTLE"};
const char* MODE_NAMES[2]={"DAILY TRAINING","REHAB TRAINING"};
GLUquadric* quad = 0;

float rad(float a){return a*(float)M_PI/180.0f;}
float clampf(float n,float lo,float hi){return std::max(lo,std::min(hi,n));}
void col(Color3 c){glColor3f(c.r,c.g,c.b);}
void mat(Color3 c,float shine=42.0f){
    GLfloat dif[]={c.r,c.g,c.b,1}, amb[]={c.r*.42f,c.g*.42f,c.b*.42f,1};
    GLfloat spec[]={0.34f,0.36f,0.39f,1};
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
    glRotatef(angle,-d.y,d.x,0);mat(c,70);
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

// Custom cubic Bezier patches - modeled using the same approach as the sample report.
void patch4(GLfloat cp[4][4][3],Color3 c,int res=28){
    mat(c,65);
    glMap2f(GL_MAP2_VERTEX_3,0,1,3,4,0,1,12,4,&cp[0][0][0]);
    glEnable(GL_MAP2_VERTEX_3);glEnable(GL_AUTO_NORMAL);
    glMapGrid2f(res,0,1,res,0,1);glEvalMesh2(GL_FILL,0,res,0,res);
    glDisable(GL_MAP2_VERTEX_3);
}
void wing(int sign){
    GLfloat cps[4][4][3];
    for(int layer=0;layer<2;layer++){
        for(int u=0;u<4;u++)for(int v=0;v<4;v++){
            float t=u/3.0f,s=v/3.0f;
            float swell=std::sin(t*(float)M_PI);
            cps[u][v][0]=sign*(2.16f+s*(.70f+.35f*swell)-(layer?.08f:0));
            cps[u][v][1]=.70f+s*(.70f+1.05f*swell)-(layer?.12f:0);
            cps[u][v][2]=-2.35f+3.20f*t+(.22f*s*swell);
        }
        patch4(cps,layer==0?WHITE:BLUE,30);
    }
    // Thin metallic trim follows the wave of the guard's outer rim
    glDisable(GL_LIGHTING); glLineWidth(2.0f);col(WARM);
    glBegin(GL_LINE_STRIP);
    for(int k=0;k<=44;k++){
        float t=k/44.0f,s=std::sin(t*(float)M_PI);
        glVertex3f(sign*(2.16f+.70f+.35f*s),.70f+.70f+1.05f*s,-2.35f+3.20f*t+.22f*s);
    }glEnd();glEnable(GL_LIGHTING);
}
float cushionY(float x,float z){
    float u=x/2.55f;
    float groove=.03f*std::sin((x+1.1f)*5.8f)*std::exp(-std::pow((z-.12f)*.8f,2));
    return .79f-.13f*u*u+.02f*std::cos((z+.6f)*1.8f)+groove;
}
void cushion(){
    mat(FOAM,18);
    // Upholstered contoured palm cradle, parametrically sampled for smooth shading
    int nx=42,nz=42;
    for(int j=0;j<nz;j++){
        float z1=-2.25f+3.1f*j/nz, z2=-2.25f+3.1f*(j+1)/nz;
        glBegin(GL_TRIANGLE_STRIP);
        for(int i=0;i<=nx;i++){
            float x=-2.35f+4.70f*i/nx;
            float s1=1.0f-std::pow((z1+.7f)/2.30f,4.0f);
            float s2=1.0f-std::pow((z2+.7f)/2.30f,4.0f);
            float a=std::sqrt(std::max(0.0f,s1)),b=std::sqrt(std::max(0.0f,s2));
            float xx1=x*a,xx2=x*b;
            glNormal3f(.09f*xx1,1,0);glVertex3f(xx1,cushionY(xx1,z1),z1);
            glNormal3f(.09f*xx2,1,0);glVertex3f(xx2,cushionY(xx2,z2),z2);
        }glEnd();
    }
    // Finishing seam, not a separate external texture asset
    glDisable(GL_LIGHTING);glLineWidth(1.8f);col(RINGPAD);glBegin(GL_LINE_LOOP);
    for(int k=0;k<100;k++){
        float a=2*(float)M_PI*k/100.0f;
        float x=2.31f*std::cos(a),z=-.69f+1.40f*std::sin(a);
        glVertex3f(x,cushionY(x,z)+.025f,z);
    }glEnd();glEnable(GL_LIGHTING);
    // Faint divisions along the soft upholstery following anatomical fingers
    for(int i=0;i<4;i++){
        float x=-1.20f+i*.79f;
        glDisable(GL_LIGHTING);col(RINGPAD);glLineWidth(1.0f);
        glBegin(GL_LINE_STRIP);
        for(int j=0;j<25;j++){float z=-.2f+j*.041f;glVertex3f(x+.02f*std::sin(z*4),cushionY(x,z)+.035f,z);}glEnd();
        glEnable(GL_LIGHTING);
    }
}
void chassis(){
    // white monocoque with large cushioned inner tray
    ell(0,.35f,-.72f,2.85f,.43f,2.17f,WHITE,40);
    ell(0,.44f,-.70f,2.61f,.27f,1.88f,WARM,36);
    cushion();
    wing(-1);wing(1);
    // Integrated drive / electronics pod behind wrist, without protruding above use space
    ell(0,.83f,-2.23f,2.13f,.66f,.75f,WHITE,36);
    ell(0,1.25f,-2.38f,1.92f,.20f,.52f,WARM,32);
    drawCube(0,1.415f,-2.39f,.65f,.018f,.025f,LED);
    // left and right frame foot details
    for(int s=-1;s<=1;s+=2)ell(s*2.20f,.30f,-1.85f,.38f,.22f,.44f,WARM,20);
}
void pin(float x,float y,float z){
    // Physical pivot BELOW the finger cuff, never mounted on its side or above.
    glPushMatrix();glTranslatef(x,y,z);glRotatef(90,0,1,0);
    mat(METAL,110);gluCylinder(quad,.115f,.115f,.37f,24,1);
    torus(0,0,.38f,.018f,.095f,WHITE);
    ell(0,0,.375f,.074f,.074f,.024f,METAL,18);
    glPopMatrix();
}
void arch(float centerZ,float w,float tilt=0.0f){
    glPushMatrix();glTranslatef(0,0,centerZ);glRotatef(tilt,1,0,0);
    // cuff opens downwards onto fingertip, arch is ABOVE finger channel
    glDisable(GL_LIGHTING);col(WHITE); glLineWidth(4.0f);
    // material geometry uses fused thick arcs (3D surfaces)
    glEnable(GL_LIGHTING);
    for(int i=0;i<19;i++){
        float a=(float)M_PI-((float)M_PI*i/18.0f);
        float b=(float)M_PI-((float)M_PI*(i+1)/18.0f);
        V3 v1(w*.5f*std::cos(a),.12f+w*.51f*std::sin(a),-.115f);
        V3 v2(w*.5f*std::cos(b),.12f+w*.51f*std::sin(b),-.115f);
        tube(v1,v2,.065f,WHITE);
        v1.z=.115f;v2.z=.115f;tube(v1,v2,.060f,WHITE);
        if(i%3==0)tube(v1,V3(v1.x,v1.y,-.115f),.045f,WHITE);
    }
    // blue soft elastomer inner insert and upper accent
    for(int i=0;i<15;i++){
        float a=(float)M_PI-(float)M_PI*i/14;
        float b=(float)M_PI-(float)M_PI*(i+1)/14;
        tube(V3((w*.5f-.068f)*std::cos(a),.12f+(w*.51f-.065f)*std::sin(a),0),
             V3((w*.5f-.068f)*std::cos(b),.12f+(w*.51f-.065f)*std::sin(b),0),.028f,RINGPAD);
    }
    ell(0,.12f+w*.52f,0,w*.32f,.072f,.145f,BLUE,20);
    drawCube(0,.12f+w*.58f,-.005f,w*.22f,.012f,.04f,LED);
    // 2 pivots visible under cuff; important mechanical correction
    pin(-w*.40f,-.14f,-.055f);pin(w*.40f,-.14f,-.055f);
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
        glVertex3f(i*w*.105f,.135f,z);
    }glEnd();glEnable(GL_LIGHTING);
}
// Finger lengths and positions are deliberately ANATOMICAL rather than five identical arms.
struct Digit{float x,z,len,spread,width;};
Digit digits[5] = {
    {-2.05f,-.25f,1.75f,-42.0f,.59f}, // thumb: oblique, two phalanges
    {-1.25f,.47f,2.37f, -7.0f,.55f}, // index
    {- .40f,.52f,2.68f, -1.0f,.58f}, // middle: longest
    {  .49f,.46f,2.47f,  6.0f,.55f}, // ring
    { 1.37f,.32f,1.89f, 14.0f,.47f}  // little: shortest among four aligned digits
};
float angleFor(int i){
    if(!playing) return 0;
    if(selected!=-1 && selected!=i) return 0;
    float phase=mode==0?(i*.31f):0.0f;
    float amplitude=(8.0f+strength*2.7f)*(mode==1?.73f:1.0f);
    return amplitude*(.5f+.5f*std::sin(animationTime*1.4f+phase));
}
void finger(int i){
    const Digit d=digits[i];
    glPushMatrix();glTranslatef(d.x,.77f,d.z);glRotatef(d.spread,0,1,0);
    float a=angleFor(i);
    // digit root frames move relative to their underlying support (kinematic demonstration)
    glRotatef(a*.23f,1,0,0);
    float knuckle=d.len*.49f, dist=d.len-knuckle;
    fingerPad(knuckle,d.width);
    for(int s=-1;s<=1;s+=2){
        segment(0,knuckle,.19f,s*d.width*.38f,.048f,WHITE);
        ell(s*d.width*.38f,.19f,knuckle,.095f,.085f,.095f,METAL,18);
    }
    arch(knuckle*.45f,d.width,0);
    glTranslatef(0,.19f,knuckle);
    glRotatef(a*.70f,1,0,0); // independent distal articulation
    for(int s=-1;s<=1;s+=2)segment(0,dist,.0f,s*d.width*.38f,.050f,WHITE);
    ell(0,-.05f,dist*.50f,d.width*.40f,.12f,dist*.65f,RINGPAD,18);
    arch(dist*.87f,d.width*.96f,cuffFineAdjustment);
    // Convert final cuff's 3D position to screen coords to allow selecting fingers via clicking.
    GLdouble model[16],proj[16],sx,sy,sz; GLint view[4];
    glGetDoublev(GL_MODELVIEW_MATRIX,model);glGetDoublev(GL_PROJECTION_MATRIX,proj);
    glGetIntegerv(GL_VIEWPORT,view);
    if(gluProject(0,.23f,dist*.87f,model,proj,view,&sx,&sy,&sz)==GL_TRUE){
        fingerSX[i]=(float)sx;fingerSY[i]=H-(float)sy;
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
void controller(){
    glPushMatrix();glTranslatef(4.50f,.0f,1.12f);
    ell(0,.36f,0,1.22f,.41f,1.42f,WHITE,38);
    ell(0,.69f,-.27f,1.04f,.080f,.78f,DARK,32);
    // miniature blue line chart and readout set into 3D display
    glDisable(GL_LIGHTING);col(LED);glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<28;i++){
        float x=-.78f+i*.056f;
        glVertex3f(x,.792f,-.49f+.064f*std::sin(i*.60f));
    }glEnd();glEnable(GL_LIGHTING);
    text3(-.68f,.795f,-.12f,mode==0?"DAILY":"REHAB",WHITE);
    knob(0,.80f,.66f,.37f);
    for(int s=-1;s<=1;s+=2){
        ell(s*.74f,.75f,.82f,.135f,.060f,.15f,s<0?WARM:BLUE,18);
        text3(s*.74f-.042f,.816f,.82f,s<0?"-":"+",DARK);
    }
    // side system-status LEDs
    for(int j=0;j<8;j++)ell(-.75f+j*.18f,.773f,.11f,.04f,.026f,.06f,j<strength?LED:WARM,10);
    glPopMatrix();
    // flexible cable to electronics pod (explicit visual connection)
    V3 last(1.90f,.64f,-2.56f);
    for(int k=1;k<=26;k++){
        float t=k/26.0f;
        V3 cur(1.90f+2.28f*t,.64f-.25f*std::sin(t*(float)M_PI),-2.56f+2.75f*t);
        tube(last,cur,.044f,WARM);last=cur;
    }
}
void ground(){
    glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for(int i=0;i<15;i++){
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
    col(c);glRasterPos2i(x,H-y);
    const unsigned char* p=(const unsigned char*)s.c_str();
    while(*p){glutBitmapCharacter(large?GLUT_BITMAP_HELVETICA_18:GLUT_BITMAP_HELVETICA_12,*p);p++;}
}
void hud(){
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();gluOrtho2D(0,W,0,H);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);
    // clean text-based presentation around a centrally framed 3D object
    uiText(36,44,"ReLeaf Duo",DARK,true);
    uiText(36,67,"BIOMIMETIC HAND TRAINER  /  3D INTERACTIVE CONCEPT",Color3(.35f,.44f,.55f));
    uiText(W-282,42,mode==0?"DAILY MODE":"REHAB MODE",DARK,true);
    char s[128];std::sprintf(s,"SIMULATED STRENGTH   %d / 8",strength);
    uiText(W-282,67,s,Color3(.36f,.44f,.55f));
    uiText(W-282,90,playing?"MOTION: RUNNING":"MOTION: PAUSED",playing?Color3(.16f,.60f,.78f):Color3(.40f,.49f,.59f));
    uiText(37,H-149,"CLICK FINGERTIP OR PRESS 1-5",Color3(.35f,.44f,.55f));
    uiText(37,H-130,"0 ALL  |  SPACE ANIMATE  |  M MODE",Color3(.35f,.44f,.55f));
    uiText(37,H-111,"+/- STRENGTH  |  [ ] CUFF TILT",Color3(.35f,.44f,.55f));
    uiText(37,H-92,"DRAG ORBIT  |  WHEEL ZOOM",Color3(.35f,.44f,.55f));
    uiText(37,H-73,"A AUTOROTATE  |  R RESET  |  H HELP",Color3(.35f,.44f,.55f));
    if(showHelp){
        std::string txt="SELECTED: ";txt+=(selected<0?"ALL FINGERS":NAMES[selected]);
        uiText(W-282,H-74,txt,Color3(.23f,.43f,.62f));
        std::sprintf(s,"CUFF PIVOT: %+0.0f DEG",cuffFineAdjustment);
        uiText(W-282,H-56,s,Color3(.39f,.49f,.58f));
        uiText(W-282,H-38,"RIGHT CLICK FOR MENU",Color3(.42f,.51f,.60f));
    }
    glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);
    glMatrixMode(GL_MODELVIEW);glPopMatrix();
    glMatrixMode(GL_PROJECTION);glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
void display(){
    glClearColor(.965f,.974f,.985f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    float ry=rad(yaw),rp=rad(pitch);
    float tx=.47f,ty=.67f,tz=.05f;
    gluLookAt(tx+zoom*std::sin(ry)*std::cos(rp),ty+zoom*std::sin(rp),
              tz+zoom*std::cos(ry)*std::cos(rp), tx,ty,tz, 0,1,0);
    GLfloat key[]={-3.0f,8.0f,9.0f,1.0f};glLightfv(GL_LIGHT0,GL_POSITION,key);
    GLfloat fill[]={7.0f,4.0f,-5.0f,1.0f};glLightfv(GL_LIGHT1,GL_POSITION,fill);
    model();hud();glutSwapBuffers();
}
void reshape(int w,int h){
    W=std::max(w,500);H=std::max(h,320);
    glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();
    gluPerspective(43.0,(double)W/H,.1,100.0);
    glMatrixMode(GL_MODELVIEW);
}
void reset(){yaw=28;pitch=30;zoom=10.8f;modelYaw=0;selected=-1;cuffFineAdjustment=0;}
void key(unsigned char k,int,int){
    switch(k){
        case 27:std::exit(0);
        case ' ':playing=!playing;break;
        case '0':selected=-1;break;
        case '1':case '2':case '3':case '4':case '5':selected=k-'1';break;
        case 'm':case 'M':mode=1-mode;break;
        case 'a':case 'A':rotating=!rotating;break;
        case 'h':case 'H':showHelp=!showHelp;break;
        case 'r':case 'R':reset();break;
        case '+':case '=':strength=std::min(8,strength+1);break;
        case '-':case '_':strength=std::max(1,strength-1);break;
        case '[':cuffFineAdjustment=std::max(-20.0f,cuffFineAdjustment-5);break;
        case ']':cuffFineAdjustment=std::min(20.0f,cuffFineAdjustment+5);break;
        case 'w':case 'W':pitch=clampf(pitch+4,-5,78);break;
        case 's':case 'S':pitch=clampf(pitch-4,-5,78);break;
        case 'd':case 'D':yaw+=5;break;
        case 'q':case 'Q':yaw-=5;break;
        case 'v':case 'V':yaw=0;pitch=75;break;
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
    float closest=60*60;int idx=-1;
    for(int i=0;i<5;i++){
        float dx=x-fingerSX[i],dy=y-fingerSY[i],d=dx*dx+dy*dy;
        if(d<closest){closest=d;idx=i;}
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
void timer(int){
    int now=glutGet(GLUT_ELAPSED_TIME);float dt=clampf((now-lastTick)*.001f,0,.04f);lastTick=now;
    if(playing)animationTime+=dt;
    if(rotating)modelYaw+=dt*11;
    glutPostRedisplay();glutTimerFunc(16,timer,0);
}
void menu(int v){
    switch(v){
        case 1:playing=!playing;break;
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
    glEnable(GL_LIGHT0);glEnable(GL_LIGHT1);glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_MULTISAMPLE);glShadeModel(GL_SMOOTH);
    GLfloat a[]={.58f,.62f,.67f,1},d[]={.88f,.90f,.94f,1};
    glLightfv(GL_LIGHT0,GL_AMBIENT,a);glLightfv(GL_LIGHT0,GL_DIFFUSE,d);
    GLfloat fill[]={.33f,.42f,.54f,1};glLightfv(GL_LIGHT1,GL_DIFFUSE,fill);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    lastTick=glutGet(GLUT_ELAPSED_TIME);
}
int main(int argc,char**argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH|GLUT_MULTISAMPLE);
    glutInitWindowSize(1600,900);
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
