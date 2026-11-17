#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("Burnout Masters PSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_WIDTH 512
#define PI 3.14159265358979323846f
#define MAX_SMOKE 48
#define MAX_SKIDS 180
#define SAVE_DIR "ms0:/PSP/SAVEDATA/BMPSP0001"
#define SAVE_FILE SAVE_DIR "/progress.dat"

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, z;
    float life;
    float size;
} Smoke;

typedef struct {
    float x1, z1, x2, z2;
    float life;
} Skid;

typedef struct {
    const char *name;
    const char *make;
    unsigned int body;
    float power;
    float grip;
    int price;
} CarSpec;

static const CarSpec cars[] = {
    {"Falcon XR8", "AUSTRALIA", 0xFF2868D8, 1.00f, 0.82f, 0},
    {"Commodore SS", "AUSTRALIA", 0xFFB82C2C, 1.04f, 0.84f, 1800},
    {"Silvia S15", "JAPAN", 0xFFDDDDDD, 0.91f, 0.93f, 2600},
    {"Skyline R34", "JAPAN", 0xFF287ACB, 1.12f, 0.91f, 4200},
    {"Mustang GT", "USA", 0xFF202020, 1.18f, 0.78f, 5000},
    {"Camaro SS", "USA", 0xFFCC9A20, 1.15f, 0.80f, 4800}
};
#define CAR_COUNT (sizeof(cars) / sizeof(cars[0]))

static unsigned int __attribute__((aligned(16))) list[262144];

static Vertex cube[36];
static Smoke smoke[MAX_SMOKE];
static Skid skids[MAX_SKIDS];
static int smoke_head = 0, skid_head = 0;

static float car_x = 0.0f, car_z = 0.0f;
static float heading = 0.0f;
static float speed = 0.0f;
static float lateral = 0.0f;
static float wheel_angle = 0.0f;
static float wheel_spin = 0.0f;
static float yaw_rate = 0.0f;
static float body_roll = 0.0f;
static float suspension_bob = 0.0f;
static float previous_speed = 0.0f;
static float drift_score = 0.0f;
static float total_score = 0.0f;
static float drift_chain = 0.0f;
static int cash = 1200;
static int selected_car = 0;
static int owned[CAR_COUNT] = {1,0,0,0,0,0};
static int engine_level = 0;
static int grip_level = 0;
static int tire_level = 0;
static int garage_page = 0; /* 0 cars, 1 upgrades */
static int upgrade_selection = 0;
static int event_mode = 0;
static int event_type = 0; /* 0 drift, 1 high-speed, 2 slalom */
static int slalom_next = 0;
static int slalom_hits = 0;
static int event_finished = 0;
static float event_timer = 90.0f;
static int event_reward = 0;
static int game_mode = 0; /* 0 garage, 1 driving, 2 pause */
static int show_help = 1;
static unsigned int frame_count = 0;

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static float wrap_angle(float a) {
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}
static float frand01(void) {
    static unsigned int seed = 0x1234567u;
    seed = seed * 1664525u + 1013904223u;
    return (float)((seed >> 8) & 0x00FFFFFF) / 16777215.0f;
}

static void make_cube(void) {
    static const float p[8][3] = {
        {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},
        {0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},
        {0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}
    };
    static const int faces[6][6] = {
        {0,1,2,0,2,3},{5,4,7,5,7,6},{4,0,3,4,3,7},
        {1,5,6,1,6,2},{3,2,6,3,6,7},{4,5,1,4,1,0}
    };
    int k = 0;
    for (int f=0; f<6; ++f) {
        unsigned int c = (f == 2 || f == 3) ? 0xFFB0B0B0 : 0xFFFFFFFF;
        for (int j=0; j<6; ++j) {
            int idx = faces[f][j];
            cube[k].color = c;
            cube[k].x = p[idx][0]; cube[k].y = p[idx][1]; cube[k].z = p[idx][2];
            ++k;
        }
    }
}

static void draw_cube_rot(float x, float y, float z, float sx, float sy, float sz,
                          float rx, float ry, float rz, unsigned int color) {
    Vertex *temp = (Vertex*)sceGuGetMemory(sizeof(Vertex) * 36);
    for (int i=0; i<36; ++i) {
        temp[i] = cube[i];
        temp[i].color = color;
    }
    sceGumPushMatrix();
    ScePspFVector3 pos = {x,y,z};
    ScePspFVector3 scale = {sx,sy,sz};
    sceGumTranslate(&pos);
    ScePspFVector3 rot = {rx,ry,rz};
    sceGumRotateXYZ(&rot);
    sceGumScale(&scale);
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                   36, 0, temp);
    sceGumPopMatrix();
}

static void draw_cube(float x, float y, float z, float sx, float sy, float sz,
                      float ry, unsigned int color) {
    draw_cube_rot(x,y,z,sx,sy,sz,0.0f,ry,0.0f,color);
}

static void draw_line(float x1, float z1, float x2, float z2, float width,
                      unsigned int color) {
    Vertex *v = (Vertex*)sceGuGetMemory(sizeof(Vertex) * 6);
    float dx = x2-x1, dz = z2-z1;
    float len = sqrtf(dx*dx + dz*dz);
    if (len < 0.001f) return;
    float px = -dz/len * width, pz = dx/len * width;
    float pts[6][3] = {
        {x1+px,0.025f,z1+pz},{x1-px,0.025f,z1-pz},{x2+px,0.025f,z2+pz},
        {x2+px,0.025f,z2+pz},{x1-px,0.025f,z1-pz},{x2-px,0.025f,z2-pz}
    };
    for (int i=0;i<6;i++) {
        v[i].color=color; v[i].x=pts[i][0]; v[i].y=pts[i][1]; v[i].z=pts[i][2];
    }
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                   6, 0, v);
}

static void draw_ground(void) {
    draw_cube(0.0f,-0.18f,0.0f,90.0f,0.25f,90.0f,0.0f,0xFF343638);
    for (int i=-20;i<=20;i+=2) {
        draw_line((float)i,-22.0f,(float)i,22.0f,0.018f,0xFF55585A);
        draw_line(-22.0f,(float)i,22.0f,(float)i,0.018f,0xFF55585A);
    }
    /* Low safety barriers keep the player inside the practice arena. */
    draw_cube(0.0f,0.38f,-42.0f,86.0f,0.75f,0.55f,0.0f,0xFFB9B9B9);
    draw_cube(0.0f,0.38f, 42.0f,86.0f,0.75f,0.55f,0.0f,0xFFB9B9B9);
    draw_cube(-42.0f,0.38f,0.0f,0.55f,0.75f,86.0f,0.0f,0xFFB9B9B9);
    draw_cube( 42.0f,0.38f,0.0f,0.55f,0.75f,86.0f,0.0f,0xFFB9B9B9);
    /* Painted circular burnout pad */
    for (int i=0;i<64;i++) {
        float a=(float)i*2.0f*PI/64.0f;
        float b=(float)(i+1)*2.0f*PI/64.0f;
        draw_line(cosf(a)*8.0f,sinf(a)*8.0f,cosf(b)*8.0f,sinf(b)*8.0f,
                  0.045f,0xFFB9B9B9);
        draw_line(cosf(a)*12.0f,sinf(a)*12.0f,cosf(b)*12.0f,sinf(b)*12.0f,
                  0.025f,0xFF777777);
    }
}

static void draw_track_pylons(void) {
    /* Eight visible markers define a loose slalom around the burnout pad. */
    for(int i=0;i<8;i++) {
        float a=(float)i*(2.0f*PI/8.0f)+PI/8.0f;
        float x=cosf(a)*16.5f;
        float z=sinf(a)*16.5f;
        unsigned int marker=(event_mode && event_type==2 && i==slalom_next)?0xFF35E8A0:0xFFFF7626;
        draw_cube(x,0.22f,z,0.72f,0.42f,0.72f,0.0f,marker);
        draw_cube(x,0.48f,z,0.48f,0.18f,0.48f,0.0f,0xFFF0F0F0);
        draw_cube(x,0.67f,z,0.25f,0.22f,0.25f,0.0f,marker);
    }
}

static void draw_car(void) {
    const CarSpec *c=&cars[selected_car];
    /* Give the three car families distinct proportions while keeping one
       low-poly mesh budget for PSP-1000 compatibility. */
    float body_w = selected_car>=4 ? 2.32f : (selected_car>=2 ? 2.00f : 2.16f);
    float body_l = selected_car>=4 ? 4.55f : (selected_car>=2 ? 4.05f : 4.42f);
    float roof_l = selected_car>=4 ? 1.82f : (selected_car>=2 ? 1.72f : 2.05f);
    float roof_w = body_w*0.76f;
    float front_z = body_l*0.50f;
    float rear_z = -body_l*0.50f;

    draw_cube_rot(car_x,0.62f+suspension_bob,car_z,body_w,0.48f,body_l,0.0f,heading,body_roll,c->body);
    draw_cube_rot(car_x,1.02f+suspension_bob,car_z+0.12f,roof_w,0.60f,roof_l,0.0f,heading,body_roll*0.72f,0xFF252A30);
    draw_cube(car_x,0.76f,car_z+body_l*0.36f,body_w*0.95f,0.22f,0.62f,
              heading,0xFF17191C);
    draw_cube(car_x,0.72f,car_z+front_z-0.10f,body_w*1.02f,0.20f,0.34f,
              heading,0xFF151515);
    draw_cube(car_x,0.48f,car_z+rear_z+0.10f,body_w*0.98f,0.18f,0.30f,
              heading,0xFF151515);
    draw_cube(car_x-body_w*0.48f,0.43f,car_z,0.12f,0.20f,body_l*0.60f,
              heading,0xFF17191C);
    draw_cube(car_x+body_w*0.48f,0.43f,car_z,0.12f,0.20f,body_l*0.60f,
              heading,0xFF17191C);

    /* Wheels: front axle is at +Z. */
    float sn=sinf(heading), cs=cosf(heading);
    for (int side=-1;side<=1;side+=2) {
        for (int axle=-1;axle<=1;axle+=2) {
            float lx=(float)side*(body_w*0.50f+0.02f);
            float lz=(float)axle*(body_l*0.31f);
            float wx=car_x+lx*cs+lz*sn;
            float wz=car_z-lx*sn+lz*cs;
            draw_cube_rot(wx,0.38f+suspension_bob*0.45f,wz,0.34f,0.66f,0.82f,
                          wheel_spin,heading+(axle==1?wheel_angle:0.0f),0.0f,0xFF101113);
        }
    }

    /* Family-specific details: Japanese coupe wing, American bonnet scoop. */
    if(selected_car>=2 && selected_car<=3) {
        draw_cube(car_x,1.02f,car_z+rear_z+0.24f,body_w*0.92f,0.10f,0.22f,
                  heading,0xFF17191C);
        draw_cube(car_x-body_w*0.34f,0.90f,car_z+rear_z+0.22f,0.08f,0.22f,0.10f,
                  heading,0xFF17191C);
        draw_cube(car_x+body_w*0.34f,0.90f,car_z+rear_z+0.22f,0.08f,0.22f,0.10f,
                  heading,0xFF17191C);
    } else if(selected_car>=4) {
        draw_cube(car_x,0.94f,car_z+body_l*0.25f,0.62f,0.22f,0.72f,
                  heading,0xFF17191C);
    }

    draw_cube(car_x-body_w*0.31f,0.70f,car_z+front_z+0.02f,0.42f,0.15f,0.08f,
              heading,0xFFB9DFFF);
    draw_cube(car_x+body_w*0.31f,0.70f,car_z+front_z+0.02f,0.42f,0.15f,0.08f,
              heading,0xFFB9DFFF);
    /* Windscreen, rear glass, mirrors, grille, tail lamps and side graphics. */
    draw_cube(car_x,1.13f+suspension_bob,car_z+0.48f,roof_w*0.88f,0.035f,0.48f,
              heading,0xFF526C7C);
    draw_cube(car_x,1.13f+suspension_bob,car_z-0.42f,roof_w*0.88f,0.035f,0.38f,
              heading,0xFF526C7C);
    for(int side=-1;side<=1;side+=2) {
        float sx=car_x+(float)side*(body_w*0.52f);
        draw_cube(sx,0.91f,car_z+0.58f,0.20f,0.15f,0.25f,heading,0xFF17191C);
        draw_cube(car_x+(float)side*(body_w*0.49f),0.59f,car_z,0.055f,0.12f,body_l*0.48f,
                  heading,0xFF8C969E);
        draw_cube(car_x+(float)side*(body_w*0.43f),0.67f,car_z+rear_z+0.08f,
                  0.30f,0.13f,0.10f,heading,0xFFFF3434);
    }
    draw_cube(car_x,0.55f,car_z+front_z+0.04f,0.72f,0.13f,0.10f,heading,0xFF090B0D);
    draw_cube(car_x,0.52f,car_z+rear_z-0.04f,0.72f,0.10f,0.10f,heading,0xFF151515);
}
static void add_smoke(float x,float z) {
    Smoke *s=&smoke[smoke_head++ % MAX_SMOKE];
    s->x=x+(frand01()-0.5f)*0.55f;
    s->z=z+(frand01()-0.5f)*0.55f;
    s->y=0.35f; s->life=0.75f; s->size=0.35f+frand01()*0.35f;
}
static void update_effects(float dt) {
    for (int i=0;i<MAX_SMOKE;i++) if (smoke[i].life>0.0f) {
        smoke[i].life-=dt;
        smoke[i].y+=dt*1.0f;
        smoke[i].size+=dt*0.8f;
    }
    for (int i=0;i<MAX_SKIDS;i++) if (skids[i].life>0.0f) skids[i].life-=dt*0.012f;
}
static void draw_effects(void) {
    for (int i=0;i<MAX_SKIDS;i++) if (skids[i].life>0.0f) {
        unsigned int alpha=(unsigned int)(clampf(skids[i].life,0.0f,1.0f)*120.0f);
        draw_line(skids[i].x1,skids[i].z1,skids[i].x2,skids[i].z2,0.075f,
                  (alpha<<24)|0x00202020);
    }
    for (int i=0;i<MAX_SMOKE;i++) if (smoke[i].life>0.0f) {
        unsigned int a=(unsigned int)(clampf(smoke[i].life/0.75f,0.0f,1.0f)*105.0f);
        draw_cube(smoke[i].x,smoke[i].y,smoke[i].z,smoke[i].size,
                  smoke[i].size,smoke[i].size,0.0f,(a<<24)|0x00C7C9CB);
    }
}

#define SAVE_MAGIC 0x424D5053 /* "BMPS" */
#define SAVE_VERSION 3

static void save_game(void) {
    sceIoMkdir("ms0:/PSP",0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA",0777);
    sceIoMkdir(SAVE_DIR,0777);
    SceUID fd=sceIoOpen(SAVE_FILE,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);
    if(fd>=0) {
        int data[7+CAR_COUNT];
        data[0]=SAVE_MAGIC; data[1]=SAVE_VERSION;
        data[2]=cash; data[3]=selected_car;
        data[4]=engine_level; data[5]=grip_level; data[6]=tire_level;
        for(int i=0;i<(int)CAR_COUNT;i++) data[7+i]=owned[i]?1:0;
        sceIoWrite(fd,data,sizeof(data));
        sceIoClose(fd);
    }
}
static void load_game(void) {
    SceUID fd=sceIoOpen(SAVE_FILE,PSP_O_RDONLY,0777);
    if(fd>=0) {
        int data[7+CAR_COUNT];
        int n=sceIoRead(fd,data,sizeof(data));
        sceIoClose(fd);
        if(n==(int)sizeof(data) && data[0]==SAVE_MAGIC && data[1]==SAVE_VERSION) {
            cash=data[2];
            if(cash<0 || cash>100000000) cash=1200;
            selected_car=data[3];
            engine_level=(int)clampf((float)data[4],0,5);
            grip_level=(int)clampf((float)data[5],0,5);
            tire_level=(int)clampf((float)data[6],0,5);
            if(selected_car<0 || selected_car>=(int)CAR_COUNT) selected_car=0;
            for(int i=0;i<(int)CAR_COUNT;i++) owned[i]=(data[7+i]==1)?1:0;
            owned[0]=1;
            if(!owned[selected_car]) selected_car=0;
        }
    }
}

static void init_gu(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT,list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
    sceGuDispBuffer(SCREEN_W,SCREEN_H,(void*)0x88000,BUF_WIDTH);
    sceGuDepthBuffer((void*)0x110000,BUF_WIDTH);
    sceGuOffset(2048-(SCREEN_W/2),2048-(SCREEN_H/2));
    sceGuViewport(2048,2048,SCREEN_W,SCREEN_H);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SCREEN_W,SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void begin_scene(void) {
    sceGuStart(GU_DIRECT,list);
    sceGuClearColor(0xFF20252B);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_CULL_FACE);
    sceGuColor(0xFFFFFFFF);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(48.0f,16.0f/9.0f,0.5f,180.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    float sn=sinf(heading), cs=cosf(heading);
    ScePspFVector3 eye={car_x-sn*10.0f,7.0f,car_z-cs*10.0f};
    ScePspFVector3 at={car_x,0.4f,car_z};
    ScePspFVector3 up={0.0f,1.0f,0.0f};
    sceGumLookAt(&eye,&at,&up);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

static void end_scene(void) {
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void update_car(SceCtrlData pad,float dt) {
    float steer=0.0f;
    if (pad.Lx < 112) steer = -clampf((112.0f - (float)pad.Lx) / 112.0f, 0.0f, 1.0f);
    else if (pad.Lx > 144) steer = clampf(((float)pad.Lx - 144.0f) / 111.0f, 0.0f, 1.0f);
    if(pad.Buttons & PSP_CTRL_LEFT) steer=-1.0f;
    if(pad.Buttons & PSP_CTRL_RIGHT) steer=1.0f;
    if(pad.Buttons & PSP_CTRL_CROSS)
        speed+=dt*8.0f*cars[selected_car].power*(1.0f+engine_level*0.08f);
    if(pad.Buttons & PSP_CTRL_SQUARE) speed-=dt*13.0f;
    if(pad.Buttons & PSP_CTRL_CIRCLE) speed*=powf(0.90f,dt*60.0f);
    if(!(pad.Buttons & (PSP_CTRL_CROSS|PSP_CTRL_SQUARE))) speed*=powf(0.985f,dt*60.0f);
    speed=clampf(speed,-5.0f,23.0f);
    wheel_angle=steer*0.42f;
    float handbrake=(pad.Buttons & PSP_CTRL_CIRCLE)?1.0f:0.0f;
    float drift_factor=clampf(fabsf(speed)/13.0f,0.0f,1.0f);
    float grip_bonus=cars[selected_car].grip+grip_level*0.09f+tire_level*0.05f;
    grip_bonus=clampf(grip_bonus,0.60f,1.50f);

    /* Rear-drive handling: yaw response, lateral slip and grip-dependent
       recovery. The handbrake releases rear traction for a controllable slide. */
    float target_yaw=steer*(0.22f+drift_factor*0.95f)*(0.72f+grip_bonus*0.28f);
    yaw_rate+=(target_yaw-yaw_rate)*clampf(dt*(handbrake?2.0f:5.0f),0.0f,1.0f);
    heading=wrap_angle(heading+yaw_rate*dt*(speed>=0.0f?1.0f:-1.0f));
    float lateral_push=steer*drift_factor*fabsf(speed)*(handbrake?0.72f:0.24f)/grip_bonus;
    lateral+=(lateral_push-lateral)*clampf(dt*(handbrake?1.25f:2.8f*grip_bonus),0.0f,1.0f);
    if(!handbrake && fabsf(steer)<0.08f)
        lateral*=powf(0.90f+0.025f*grip_bonus,dt*60.0f);

    float accel=(speed-previous_speed)/dt;
    previous_speed=speed;
    body_roll+=(-steer*drift_factor*0.13f-body_roll)*clampf(dt*4.0f,0.0f,1.0f);
    suspension_bob+=(clampf(-accel*0.0022f,-0.10f,0.10f)-suspension_bob)*clampf(dt*5.0f,0.0f,1.0f);
    wheel_spin+=speed*dt*1.65f;
    float sn=sinf(heading), cs=cosf(heading);
    car_x+=(sn*speed+cs*lateral)*dt;
    car_z+=(cs*speed-sn*lateral)*dt;
    if(fabsf(car_x)>38.0f) {car_x=clampf(car_x,-38.0f,38.0f); speed*=0.65f; lateral*=0.5f;}
    if(fabsf(car_z)>38.0f) {car_z=clampf(car_z,-38.0f,38.0f); speed*=0.65f; lateral*=0.5f;}

    /* Simple low-cost pylon collision: push the car away and bleed speed. */
    for(int i=0;i<8;i++) {
        float a=(float)i*(2.0f*PI/8.0f)+PI/8.0f;
        float ox=cosf(a)*16.5f, oz=sinf(a)*16.5f;
        float dx=car_x-ox, dz=car_z-oz;
        float d2=dx*dx+dz*dz;
        if(d2<1.55f*1.55f) {
            float d=sqrtf(d2);
            if(d<0.001f) { dx=1.0f; dz=0.0f; d=1.0f; }
            float nx=dx/d, nz=dz/d;
            car_x=ox+nx*1.55f; car_z=oz+nz*1.55f;
            speed*=0.72f;
            lateral*=0.45f;
            yaw_rate*=0.65f;
        }
    }

    if(event_mode && !event_finished) {
        if(event_type==1 && fabsf(speed)>13.0f) {
            /* The speed challenge rewards sustained speed, not just drifting. */
            float speed_points=(fabsf(speed)-13.0f)*18.0f*dt;
            total_score+=speed_points;
        } else if(event_type==2) {
            /* Drive through the eight marker zones in sequence. */
            float a=(float)slalom_next*(2.0f*PI/8.0f)+PI/8.0f;
            float gx=cosf(a)*16.5f, gz=sinf(a)*16.5f;
            float dx=car_x-gx, dz=car_z-gz;
            if(dx*dx+dz*dz<2.7f*2.7f && fabsf(speed)>3.0f) {
                slalom_hits++;
                total_score+=250.0f+fabsf(speed)*8.0f;
                slalom_next=(slalom_next+1)%8;
                if(slalom_next==0) total_score+=500.0f;
            }
        }
        event_timer-=dt;
        if(event_timer<=0.0f) {
            event_timer=0.0f;
            total_score+=drift_score;
            int final_points=(int)(total_score);
            if(event_type==0) {
                if(final_points>=1500) event_reward=1800;
                else if(final_points>=900) event_reward=1000;
                else if(final_points>=400) event_reward=450;
                else event_reward=100;
            } else if(event_type==1) {
                if(final_points>=2200) event_reward=2400;
                else if(final_points>=1400) event_reward=1450;
                else if(final_points>=700) event_reward=700;
                else event_reward=150;
            } else {
                if(slalom_hits>=16) event_reward=2600;
                else if(slalom_hits>=10) event_reward=1500;
                else if(slalom_hits>=5) event_reward=750;
                else event_reward=150;
            }
            cash+=event_reward;
            drift_score=0.0f;
            event_finished=1;
            event_mode=0;
            save_game();
            game_mode=2;
        }
    }
    float slip=fabsf(atan2f(lateral,fabsf(speed)+0.1f));
    int drifting=(fabsf(speed)>5.0f && (handbrake>0.0f || fabsf(steer)>0.1f) && slip>0.08f);
    if(drifting) {
        float combo=clampf(1.0f+drift_chain*0.22f,1.0f,3.5f);
        float event_bonus=(event_mode && event_type==1 && fabsf(speed)>13.0f)?1.5f:1.0f;
        float points=(fabsf(speed)*slip*26.0f+4.0f)*dt*combo*event_bonus;
        drift_score+=points;
        drift_chain+=dt;
        if((frame_count%3)==0) {
            float rx=car_x-sinf(heading)*1.45f;
            float rz=car_z-cosf(heading)*1.45f;
            add_smoke(rx+0.9f*cosf(heading),rz-0.9f*sinf(heading));
            add_smoke(rx-0.9f*cosf(heading),rz+0.9f*sinf(heading));
            Skid *k=&skids[skid_head++%MAX_SKIDS];
            k->x1=rx+0.75f*cosf(heading); k->z1=rz-0.75f*sinf(heading);
            k->x2=rx-0.75f*cosf(heading); k->z2=rz+0.75f*sinf(heading);
            k->life=1.0f;
        }
    } else if(drift_chain>0.5f) {
        total_score+=drift_score;
        if(!event_mode) cash+=(int)(drift_score/25.0f);
        drift_score=0.0f; drift_chain=0.0f;
        if(!event_mode) save_game();
    } else {
        drift_score*=0.985f;
    }
}

static void draw_hud(void) {
    sceGuFinish();
    sceGuSync(0,0);
    pspDebugScreenSetXY(1,0);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("BURNOUT MASTERS PSP  |  %s\n",cars[selected_car].name);
    pspDebugScreenPrintf("SPEED %3d km/h   CASH $%d\n",(int)(fabsf(speed)*8.0f),cash);
    pspDebugScreenPrintf("DRIFT %07d       TOTAL %07d\n",(int)drift_score,(int)total_score);
    float combo=clampf(1.0f+drift_chain*0.22f,1.0f,3.5f);
    pspDebugScreenPrintf("CHAIN %4.1f SEC    MULTIPLIER x%3.1f\n",drift_chain,combo);
    if(event_mode)
        if(event_type==2)
            pspDebugScreenPrintf("SLALOM: %02d SEC  GATES %d/8  HITS %d\n",
                (int)event_timer,slalom_next,slalom_hits);
        else
            pspDebugScreenPrintf("%s: %02d SEC LEFT  TARGET %d\n",
                event_type==0?"DRIFT SESSION":"HIGH-SPEED",
                (int)event_timer,event_type==0?1500:2200);
    else
        pspDebugScreenPrintf("FREE DRIVE\n");
    pspDebugScreenPrintf("X throttle  [] brake  O handbrake  ANALOG/D-PAD steer\n");
    pspDebugScreenPrintf("START: pause   SELECT: finish drift / bank score\n");
}

static void draw_garage(void) {
    pspDebugScreenClear();
    pspDebugScreenSetXY(1,1);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("BURNOUT MASTERS: PSP EDITION\n\n");
    pspDebugScreenPrintf("GARAGE  |  CASH: $%d\n\n",cash);

    if(garage_page==0) {
        pspDebugScreenPrintf("CAR SHOWROOM\n");
        for(int i=0;i<(int)CAR_COUNT;i++) {
            pspDebugScreenPrintf("%c %s  [%s]  %s",
                i==selected_car?'>':' ',cars[i].name,cars[i].make,
                owned[i]?"OWNED":"LOCKED");
            if(!owned[i]) pspDebugScreenPrintf("  $%d",cars[i].price);
            pspDebugScreenPrintf("\n");
        }
        pspDebugScreenPrintf("\nLEFT/RIGHT: choose car   X: buy/select\n");
    } else {
        pspDebugScreenPrintf("UPGRADES  (five levels each)\n");
        pspDebugScreenPrintf("%c ENGINE  Lv.%d  Cost $%d  (+8%% power/level)\n",
            upgrade_selection==0?'>':' ',engine_level,900+engine_level*650);
        pspDebugScreenPrintf("%c GRIP    Lv.%d  Cost $%d  (+9%% drift control/level)\n",
            upgrade_selection==1?'>':' ',grip_level,700+grip_level*500);
        pspDebugScreenPrintf("%c TYRES   Lv.%d  Cost $%d  (+5%% grip/level)\n",
            upgrade_selection==2?'>':' ',tire_level,500+tire_level*400);
        pspDebugScreenPrintf("\nUP/DOWN: choose upgrade   X: purchase\n");
        pspDebugScreenPrintf("Upgrades improve all owned cars.\n");
    }
    pspDebugScreenPrintf("\nL: showroom/upgrades   START: free drive (owned car)\n");
    pspDebugScreenPrintf("SQUARE: event type (%s)\n",event_type==0?"DRIFT SESSION":(event_type==1?"HIGH-SPEED":"SLALOM"));
    pspDebugScreenPrintf("R: start event   TRIANGLE: save & quit\n");
    pspDebugScreenPrintf("Slalom: hit the green marker sequence for bonus cash.\n");
    pspDebugScreenPrintf("\nNo paid currency or real-money purchases.\n");
}
static void draw_pause(void) {
    pspDebugScreenClear();
    pspDebugScreenSetXY(2,3);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf(event_finished ? "DRIFT EVENT COMPLETE\n\n" : "PAUSED\n\n");
    if(event_finished) {
        pspDebugScreenPrintf("Final score: %d\n", (int)total_score);
        pspDebugScreenPrintf("Cash reward: $%d\n", event_reward);
        if(event_type==2) pspDebugScreenPrintf("Slalom markers hit: %d\n\n",slalom_hits);
        else pspDebugScreenPrintf("\n");
    }
    pspDebugScreenPrintf("X: resume driving\n");
    pspDebugScreenPrintf("O: return to garage\n");
    pspDebugScreenPrintf("TRIANGLE: save progress\n");
}


static void reset_session(void) {
    car_x=0.0f; car_z=0.0f; speed=0.0f; lateral=0.0f;
    heading=0.0f; yaw_rate=0.0f; previous_speed=0.0f;
    body_roll=0.0f; suspension_bob=0.0f; wheel_spin=0.0f;
    drift_score=0.0f; total_score=0.0f; drift_chain=0.0f;
    smoke_head=0; skid_head=0;
    memset(smoke,0,sizeof(smoke));
    memset(skids,0,sizeof(skids));
}

int main(void) {
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    make_cube();
    load_game();
    init_gu();

    unsigned int last= sceKernelGetSystemTimeLow();
    while(1) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad,1);
        unsigned int now=sceKernelGetSystemTimeLow();
        float dt=(float)(now-last)/1000000.0f;
        last=now;
        dt=clampf(dt,0.001f,0.05f);
        frame_count++;

        if(game_mode==0) {
            draw_garage();
            if(pad.Buttons & PSP_CTRL_LTRIGGER) {
                garage_page=1-garage_page;
                sceKernelDelayThread(180000);
            }
            if(garage_page==0) {
                if(pad.Buttons & PSP_CTRL_LEFT) {
                    selected_car=(selected_car+(int)CAR_COUNT-1)%(int)CAR_COUNT;
                    sceKernelDelayThread(150000);
                }
                if(pad.Buttons & PSP_CTRL_RIGHT) {
                    selected_car=(selected_car+1)%(int)CAR_COUNT;
                    sceKernelDelayThread(150000);
                }
                if(pad.Buttons & PSP_CTRL_CROSS) {
                    if(owned[selected_car]) game_mode=1;
                    else if(cash>=cars[selected_car].price) {
                        cash-=cars[selected_car].price;
                        owned[selected_car]=1;
                        save_game();
                    }
                    sceKernelDelayThread(200000);
                }
            } else {
                if(pad.Buttons & PSP_CTRL_UP) {
                    upgrade_selection=(upgrade_selection+2)%3;
                    sceKernelDelayThread(140000);
                }
                if(pad.Buttons & PSP_CTRL_DOWN) {
                    upgrade_selection=(upgrade_selection+1)%3;
                    sceKernelDelayThread(140000);
                }
                if(pad.Buttons & PSP_CTRL_CROSS) {
                    int cost = upgrade_selection==0 ? 900+engine_level*650 :
                               upgrade_selection==1 ? 700+grip_level*500 :
                               500+tire_level*400;
                    int level = upgrade_selection==0 ? engine_level :
                                upgrade_selection==1 ? grip_level : tire_level;
                    if(level<5 && cash>=cost) {
                        cash-=cost;
                        if(upgrade_selection==0) engine_level++;
                        else if(upgrade_selection==1) grip_level++;
                        else tire_level++;
                        save_game();
                    }
                    sceKernelDelayThread(200000);
                }
            }
            if(pad.Buttons & PSP_CTRL_SQUARE) {
                event_type=(event_type+1)%3;
                sceKernelDelayThread(180000);
            }
            if((pad.Buttons & PSP_CTRL_START) && owned[selected_car]) {
                event_mode=0; event_finished=0;
                reset_session();
                game_mode=1;
                sceKernelDelayThread(180000);
            }
            if((pad.Buttons & PSP_CTRL_RTRIGGER) && owned[selected_car]) {
                event_mode=1; event_finished=0;
                event_timer=event_type==0?90.0f:(event_type==1?60.0f:75.0f);
                slalom_next=0; slalom_hits=0;
                reset_session();
                game_mode=1;
                sceKernelDelayThread(180000);
            }
            if(pad.Buttons & PSP_CTRL_TRIANGLE) {save_game(); sceKernelExitGame();}
        } else if(game_mode==1) {
            if(pad.Buttons & PSP_CTRL_START) {
                game_mode=2; sceKernelDelayThread(180000);
            }
            if((pad.Buttons & PSP_CTRL_SELECT) && !event_mode) {
                total_score+=drift_score;
                cash+=(int)(drift_score/25.0f);
                drift_score=0.0f; drift_chain=0.0f;
                save_game(); sceKernelDelayThread(180000);
            }
            update_car(pad,dt);
            update_effects(dt);
            begin_scene();
            draw_ground();
            draw_track_pylons();
            draw_effects();
            draw_car();
            end_scene();
            draw_hud();
        } else {
            draw_pause();
            if(pad.Buttons & PSP_CTRL_CROSS) game_mode=1;
            if(pad.Buttons & PSP_CTRL_CIRCLE) {
                game_mode=0;
                event_finished=0;
                event_mode=0;
                save_game();
            }
            if(pad.Buttons & PSP_CTRL_TRIANGLE) save_game();
            sceKernelDelayThread(100000);
        }
        sceDisplayWaitVblankStart();
    }
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
