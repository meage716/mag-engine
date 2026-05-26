#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "engine.h"

#define SW 1280
#define SH  720
#define TILE 64
#define MW 20
#define MH 12

/* 1 = duvar, 0 = bos */
static int map[MH][MW] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,1,1,0,1,1,0,0,0,0,0,1},
    {1,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,1},
    {1,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1},
    {1,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1},
    {1,0,0,0,0,0,1,0,0,1,0,0,0,1,0,0,0,1,0,1},
    {1,0,1,1,0,0,0,0,0,1,1,0,1,1,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

static int IsWall(float wx, float wy) {
    int tx = (int)(wx / TILE);
    int ty = (int)(wy / TILE);
    if (tx < 0 || tx >= MW || ty < 0 || ty >= MH) return 1;
    return map[ty][tx] != 0;
}

/* 4 köşesi de bos mu? */
static int CanMove(float x, float y, float r) {
    return !IsWall(x-r, y-r) && !IsWall(x+r, y-r) &&
           !IsWall(x-r, y+r) && !IsWall(x+r, y+r);
}

#define MAX_EN 8
typedef struct { float x, y, hp; int alive, light; float dmg_cd; } Enemy;
static Enemy en[MAX_EN];
static int   en_count = 0;
static int   kills    = 0;

typedef enum { GS_INTRO, GS_PLAY, GS_WIN } GameScreen;
static GameScreen screen   = GS_INTRO;
static float      screen_t = 0.0f;
static float      shoot_cd = 0.0f;
static int        pl_light = -1;

static Texture tex_wall, tex_floor, tex_enemy;

/* Izgara doku */
static Texture MakeGridTex(int r1,int g1,int b1, int r2,int g2,int b2, int ts) {
    Texture t = {0};
    t.width = t.height = 64; t.channels = 4;
    t.data = (unsigned char*)malloc(64*64*4);
    for (int y = 0; y < 64; y++)
    for (int x = 0; x < 64; x++) {
        int border = (x%ts == 0 || y%ts == 0);
        int i = (y*64 + x) * 4;
        t.data[i  ] = (uint8_t)(border ? r2 : r1 + ((x/ts ^ y/ts) & 1) * 20);
        t.data[i+1] = (uint8_t)(border ? g2 : g1 + ((x/ts ^ y/ts) & 1) * 20);
        t.data[i+2] = (uint8_t)(border ? b2 : b1 + ((x/ts ^ y/ts) & 1) * 20);
        t.data[i+3] = 255;
    }
    return t;
}

/* Piksel ve dikdörtgen — dusman dokusu icin */
static void SP(unsigned char* d, int x, int y, int r, int g, int b) {
    if (x<0||x>=64||y<0||y>=64) return;
    int i = (y*64+x)*4;
    d[i]=(unsigned char)r; d[i+1]=(unsigned char)g;
    d[i+2]=(unsigned char)b; d[i+3]=255;
}
static void SR(unsigned char* d, int x0,int y0, int x1,int y1, int r,int g,int b) {
    for (int y=y0; y<=y1; y++) for (int x=x0; x<=x1; x++) SP(d,x,y,r,g,b);
}

static Texture MakeEnemyTex(void) {
    Texture t = {0};
    t.width = t.height = 64; t.channels = 4;
    t.data = (unsigned char*)calloc(64*64*4, 1);
    if (!t.data) return t;
    unsigned char* d = t.data;

    /* Govde */
    SR(d, 18,28, 45,52, 60,38,24);
    SR(d, 18,28, 22,52, 38,22,12);
    SR(d, 41,28, 45,52, 38,22,12);
    SR(d, 28,30, 35,50, 80,52,32);

    /* Kollar */
    SR(d, 10,28, 17,46, 55,34,20);
    SR(d, 46,28, 53,46, 55,34,20);
    SR(d,  8,44, 18,50, 35,18,10);
    SR(d, 45,44, 55,50, 35,18,10);

    /* Bacaklar */
    SR(d, 20,52, 28,62, 50,30,18);
    SR(d, 35,52, 43,62, 50,30,18);
    SR(d, 18,60, 30,63, 30,16, 8);
    SR(d, 33,60, 45,63, 30,16, 8);

    /* Kafa */
    SR(d, 18, 4, 45,22, 70,45,28);
    SR(d, 18, 4, 22,22, 45,28,16);
    SR(d, 41, 4, 45,22, 45,28,16);
    SR(d, 25, 5, 38,10, 85,58,38);
    /* Boyun */
    SR(d, 26,22, 37,28, 55,34,20);

    /* Kaslar */
    SR(d, 19, 6, 27, 8, 16, 6, 2);
    SR(d, 36, 6, 44, 8, 16, 6, 2);

    /* Gozler — parlak kirmizi */
    SR(d, 20,10, 28,17, 155, 8, 5);
    SR(d, 22,11, 26,16, 210,20,12);
    SR(d, 23,12, 25,15, 255,55,30);
    SP(d, 23,12, 255,200,180);
    SR(d, 36,10, 44,17, 155, 8, 5);
    SR(d, 38,11, 42,16, 210,20,12);
    SR(d, 39,12, 41,15, 255,55,30);
    SP(d, 39,12, 255,200,180);

    /* Agiz + disler */
    SR(d, 23,19, 41,22, 10, 4, 2);
    SR(d, 24,19, 27,21, 210,200,190);
    SR(d, 30,19, 33,21, 210,200,190);
    SR(d, 36,19, 39,21, 210,200,190);

    return t;
}

/* 3D duvarlari haritadan ekle */
static void BuildLevel(void) {
    Engine_ClearWalls();
    for (int ty = 0; ty < MH; ty++)
    for (int tx = 0; tx < MW; tx++) {
        if (!map[ty][tx]) continue;
        float x0 = (float)(tx*TILE), y0 = (float)(ty*TILE);
        float x1 = x0+TILE,          y1 = y0+TILE;
        if (ty > 0    && !map[ty-1][tx]) Engine_AddWall(x0,y0,x1,y0, 0,100, &tex_wall,1.f);
        if (ty < MH-1 && !map[ty+1][tx]) Engine_AddWall(x1,y1,x0,y1, 0,100, &tex_wall,1.f);
        if (tx > 0    && !map[ty][tx-1]) Engine_AddWall(x0,y1,x0,y0, 0,100, &tex_wall,1.f);
        if (tx < MW-1 && !map[ty][tx+1]) Engine_AddWall(x1,y0,x1,y1, 0,100, &tex_wall,1.f);
    }
}

static void SpawnEnemy(float x, float y) {
    if (en_count >= MAX_EN) return;
    int i = en_count++;
    en[i].x = x; en[i].y = y;
    en[i].hp = 3.0f; en[i].alive = 1; en[i].dmg_cd = 0.f;
    en[i].light = Engine_AddPointLight(x, y, 50.f, 130.f, 200,30,30, 1.2f);
}

static void StartGame(void) {
    srand((unsigned)GetTickCount());

    Engine_ClearLights();   /* eski isiklari temizle */
    Engine_ClearSprites();
    Engine_ClearWalls();

    tex_wall  = Engine_LoadTexture("wall.png");
    if (!tex_wall.data)  tex_wall  = MakeGridTex(58,52,46, 20,16,12, 16);
    tex_floor = Engine_LoadTexture("floor.png");
    if (!tex_floor.data) tex_floor = MakeGridTex(30,27,23, 10, 8, 6, 32);
    tex_enemy = MakeEnemyTex();

    BuildLevel();
    Engine_InitPlayer(96.f, 96.f);
    pl_light = Engine_AddPointLight(96, 96, 50, 180, 255,240,200, 0.7f);
    Engine_SetFog(180.f, 650.f, 6,5,10);
    Engine_InitWeapon();

    en_count = 0; kills = 0;
    SpawnEnemy(640, 192);
    SpawnEnemy(960, 384);
    SpawnEnemy(448, 448);
    SpawnEnemy(832, 576);
    SpawnEnemy(1152, 192);

    p_stats.max_health = p_stats.current_health = 100;
    current_state = STATE_PLAYING;
    shoot_cd = 0.f; screen_t = 0.f; screen = GS_PLAY;
    Engine_FadeIn(0.6f);
}

/* Sol tik basili mi? */
static int LeftClick(void) {
    static int prev = 0;
    int now = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;
    int clicked = now && !prev;
    prev = now;
    return clicked;
}

/* Ates et — raycast ile vurma kontrolu */
static void Shoot(void) {
    if (shoot_cd > 0.f) return;
    shoot_cd = 0.3f;
    Engine_CameraShake(5.f, 0.1f);

    float wx, wy; int widx;
    float wall_d = Engine_RaycastShoot(&wx, &wy, &widx);
    float max_d  = (wall_d > 0.f) ? wall_d : 1200.f;

    int   best = -1;
    float best_d = max_d;

    for (int i = 0; i < en_count; i++) {
        if (!en[i].alive) continue;
        float ex = en[i].x - player_pos.x;
        float ey = en[i].y - player_pos.y;
        float proj = ex*player_dir.x + ey*player_dir.y;
        if (proj < 0.f || proj > best_d) continue;
        float perp = fabsf(ex*player_dir.y - ey*player_dir.x);
        if (perp > 32.f) continue;
        if (!Engine_HasLineOfSight(player_pos.x, player_pos.y,
                                   en[i].x, en[i].y)) continue;
        best = i; best_d = proj;
    }

    if (best >= 0) {
        en[best].hp -= 1.f;
        Engine_SpawnSpark(en[best].x, en[best].y, 50.f, 10);
        if (en[best].hp <= 0.f) {
            en[best].alive = 0; kills++;
            Engine_SpawnBlood(en[best].x, en[best].y, 50.f, 18);
            Engine_RemovePointLight(en[best].light);
            if (kills >= en_count) screen = GS_WIN;
        }
    } else if (wall_d > 0.f) {
        Engine_SpawnSpark(wx, wy, 30.f, 8);
    }
}

/* Dusman AI: gorus hatti varsa koval, yoksa bekle */
static void UpdateEnemies(float dt) {
    Engine_ClearSprites();

    for (int i = 0; i < en_count; i++) {
        if (!en[i].alive) continue;

        float dx = player_pos.x - en[i].x;
        float dy = player_pos.y - en[i].y;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist < 0.001f) dist = 0.001f;

        /* Sadece gorus hattinda olunca koval */
        int can_see = Engine_HasLineOfSight(en[i].x, en[i].y,
                                            player_pos.x, player_pos.y);

        if (can_see && dist < 600.f && dist > 40.f) {
            float spd = 90.f * dt;
            float nx = en[i].x + (dx/dist) * spd;
            float ny = en[i].y + (dy/dist) * spd;
            float R = 20.f;
            if (CanMove(nx, en[i].y, R)) en[i].x = nx;
            if (CanMove(en[i].x, ny,  R)) en[i].y = ny;
        }

        /* Dusmanlar birbirini itsin — itme sonrasi duvar kontrolu */
        for (int j = 0; j < en_count; j++) {
            if (i == j || !en[j].alive) continue;
            float ex = en[i].x - en[j].x;
            float ey = en[i].y - en[j].y;
            float ed = sqrtf(ex*ex + ey*ey);
            if (ed < 42.f && ed > 0.001f) {
                float push = (42.f - ed) / 42.f * 50.f * dt;
                float pnx = en[i].x + (ex/ed) * push;
                float pny = en[i].y + (ey/ed) * push;
                /* Itme yeni konumu duvar disinda mi? */
                float R = 20.f;
                if (CanMove(pnx, en[i].y, R)) en[i].x = pnx;
                if (CanMove(en[i].x, pny,  R)) en[i].y = pny;
            }
        }

        Engine_MovePointLight(en[i].light, en[i].x, en[i].y, 50.f);

        /* Saldiri */
        if (dist < 55.f) {
            en[i].dmg_cd += dt;
            if (en[i].dmg_cd > 0.8f) {
                Engine_TakeDamage(15);
                Engine_SpawnBlood(player_pos.x, player_pos.y, player_z, 6);
                Engine_CameraShake(8.f, 0.18f);
                en[i].dmg_cd = 0.f;
            }
        } else {
            en[i].dmg_cd = 0.f;
        }

        Engine_AddSprite(en[i].x, en[i].y, 0.f, &tex_enemy, 1);
    }
}

static void DrawHUD(void) {
    /* Saglik bari */
    Engine_DrawBlendRect(18, SH-38, 204, 20, 0x000000, 0.55f);
    Engine_DrawFillRect(20, SH-36, 200, 16, 0x2a0a0a);
    int fill = (int)((float)p_stats.current_health / 100.f * 200);
    uint32_t hc = p_stats.current_health > 60 ? 0x44bb44
                : p_stats.current_health > 30 ? 0xcc9900 : 0xcc2200;
    Engine_DrawFillRect(20, SH-36, fill, 16, hc);
    Engine_DrawText("HP", 226, SH-36, 0x889988, 2, 99);

    /* Kil sayaci */
    char buf[24];
    sprintf_s(buf, sizeof(buf), "KIL %d/%d", kills, en_count);
    Engine_DrawText(buf, SW-148, 18, 0xbbbbbb, 2, 99);

    /* Nisangah */
    int cx = SW/2, cy = SH/2;
    Engine_DrawLine(cx-12, cy,    cx-5,  cy,    180,180,180);
    Engine_DrawLine(cx+5,  cy,    cx+12, cy,    180,180,180);
    Engine_DrawLine(cx,    cy-12, cx,    cy-5,  180,180,180);
    Engine_DrawLine(cx,    cy+5,  cx,    cy+12, 180,180,180);
    Engine_DrawFillRect(cx-1, cy-1, 3, 3, shoot_cd <= 0.f ? 0xff3333 : 0x444444);
}

static int space_prev = 0;

static void Update(float dt) {
    int sp_now = Engine_GetKeyDown(VK_SPACE) ? 1 : 0;
    int sp_pressed = sp_now && !space_prev;
    space_prev = sp_now;

    if (Engine_GetKeyDown(VK_ESCAPE)) ExitProcess(0);
    screen_t += dt;

    /* Giris ekrani */
    if (screen == GS_INTRO) {
        Engine_ClearScreen(4, 3, 7);
        Engine_DrawText("KARANLIK KORIDOR", SW/2-128, SH/2-60, 0xccbbaa, 3, 99);
        Engine_DrawText("WASD HAREKET",     SW/2-96,  SH/2+10, 0x887766, 2, 99);
        Engine_DrawText("SOL TIK ATES ET",  SW/2-112, SH/2+34, 0x887766, 2, 99);
        Engine_DrawText("SPACE ZIPLA",      SW/2-88,  SH/2+58, 0x887766, 2, 99);
        if (screen_t > 0.6f) {
            Engine_DrawText("SPACE BASLAT", SW/2-96, SH/2+96, 0x554433, 2, 99);
            if (sp_pressed) StartGame();
        }
        return;
    }

    /* Kazanma / Olum */
    if (screen == GS_WIN || current_state == STATE_DEAD) {
        int win = (screen == GS_WIN);
        Engine_ClearScreen(win?4:20, win?8:0, win?4:0);
        Engine_DrawText(win ? "TAMAMLANDI" : "OLDUN",
                        SW/2-64, SH/2-20,
                        win ? 0x88cc88 : 0xff2222, 3, 99);
        Engine_DrawText("SPACE TEKRAR OYNA", SW/2-128, SH/2+30,
                        win ? 0x446644 : 0x882222, 2, 99);
        if (sp_pressed) { screen = GS_INTRO; screen_t = 0.f; }
        return;
    }

    /* Oyun */
    Engine_UpdatePlayer(260.f, 2.2f, dt);
    Engine_MovePointLight(pl_light, player_pos.x, player_pos.y, 50.f);

    shoot_cd -= dt;
    if (shoot_cd < 0.f) shoot_cd = 0.f;
    if (LeftClick()) Shoot();

    UpdateEnemies(dt);

    /* Render — sira onemli */
    Engine_Render3DFloor(&tex_floor, NULL);
    Engine_Render3D();
    Engine_RenderSprites();
    Engine_UpdateAndRenderWeapon(dt);
    DrawHUD();
}

int main(void) {
    Engine_InitWindow(SW, SH, "Karanlik Koridor");
    Engine_InitPlayer(96.f, 96.f);
    screen = GS_INTRO;
    screen_t = 0.f;
    Engine_Start(Update);
    return 0;
}