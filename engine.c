#include "engine.h"
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ─────────────────────────────────────────────────────────────────────
// SABİTLER
// ─────────────────────────────────────────────────────────────────────
#define MAX_SPRITES     128
#define MAX_SPRINGS     1024
#define MAX_WALLS       1024
#define MAX_ENTITIES    256
#define MAX_PARTICLES   2048
#define MAX_LIGHTS      64
#define TILE_SIZE       64
#define PI              3.14159265f
#define TWO_PI          6.28318530f
#define HALF_PI         1.57079632f

// ─────────────────────────────────────────────────────────────────────
// PENCERE / BUFFER
// ─────────────────────────────────────────────────────────────────────
static int           global_running   = 0;
static HWND          global_window    = 0;
static int           buffer_width     = 0;
static int           buffer_height    = 0;
static uint32_t*     offscreen_buffer = NULL;
static BITMAPINFO    bmp_info         = {0};

// ─────────────────────────────────────────────────────────────────────
// 3D RENDERER
// ─────────────────────────────────────────────────────────────────────
static float*   z_buffer     = NULL;
/* extern -- main.c'den erisebilir */
Vec2    player_pos   = {400.0f, 300.0f};
Vec2    player_dir   = {1.0f, 0.0f};
float   player_z     = 50.0f;
int     player_pitch = 0;

typedef struct {
    Vec2    p1, p2, vec_v;
    float   z_bottom, z_top;
    Texture* tex;
    float   u_repeat, v_repeat;
} Wall;

static Wall  global_walls[MAX_WALLS];
static int   wall_count = 0;

// ─────────────────────────────────────────────────────────────────────
// SPRITE
// ─────────────────────────────────────────────────────────────────────
static Sprite global_sprites[MAX_SPRITES];
static int    sprite_count = 0;

// ─────────────────────────────────────────────────────────────────────
// SPRING FİZİK
// ─────────────────────────────────────────────────────────────────────
static Phys_Spring global_springs[MAX_SPRINGS];
static int         spring_count = 0;

static int weapon_spring_x = -1;
static int weapon_spring_y = -1;

// ─────────────────────────────────────────────────────────────────────
// 2D DÜNYA
// ─────────────────────────────────────────────────────────────────────
float p2d_x     = 0.0f;
float p2d_y     = 0.0f;
float p2d_angle = 0.0f;

static int*      active_map   = NULL;
static int       active_map_w = 0;
static int       active_map_h = 0;
static Texture*  t_floor      = NULL;
static Texture*  t_wall       = NULL;

// ─────────────────────────────────────────────────────────────────────
// ENTITY & OYUNCU
// ─────────────────────────────────────────────────────────────────────
GameState   current_state = STATE_PLAYING;
PlayerStats p_stats       = {100, 100, 0, {0}};

static Entity entities[MAX_ENTITIES];

// ─────────────────────────────────────────────────────────────────────
// DİYALOG
// ─────────────────────────────────────────────────────────────────────
static const char* dialog_speaker    = NULL;
static const char* dialog_text       = NULL;
static Texture*    dialog_portrait   = NULL;
static int         dialog_char_index = 0;
static float       dialog_timer      = 0.0f;

// ─────────────────────────────────────────────────────────────────────
// PARTİKÜL SİSTEMİ  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
typedef struct {
    float    x, y, z;
    float    vx, vy, vz;
    float    lifetime;
    float    max_lifetime;
    uint32_t color;
    float    size;
    float    gravity;
    int      active;
} Particle;

static Particle particles[MAX_PARTICLES];
static int      particle_count = 0;  /* bir sonraki yazılacak slot (ring buffer) */

int Engine_SpawnParticle(float x, float y, float z,
                         float vx, float vy, float vz,
                         uint32_t color, float lifetime,
                         float size, float gravity)
{
    /* Ring buffer — eski parçacıkların üstüne yaz */
    int idx = particle_count % MAX_PARTICLES;
    particle_count++;

    Particle* p  = &particles[idx];
    p->x          = x;
    p->y          = y;
    p->z          = z;
    p->vx         = vx;
    p->vy         = vy;
    p->vz         = vz;
    p->color      = color;
    p->lifetime   = lifetime;
    p->max_lifetime = lifetime;
    p->size       = size;
    p->gravity    = gravity;
    p->active     = 1;
    return idx;
}

/* Hazır efekt: kan sıçraması */
void Engine_SpawnBlood(float x, float y, float z, int count)
{
    for (int i = 0; i < count; i++) {
        float vx = ((float)(rand() % 200) - 100.0f);
        float vy = ((float)(rand() % 200) - 100.0f);
        float vz = ((float)(rand() % 100));
        Engine_SpawnParticle(x, y, z, vx, vy, vz,
                             0xAA1100, 0.6f + (rand()%60)/100.0f,
                             3.0f + (rand()%4), -180.0f);
    }
}

/* Hazır efekt: duman */
void Engine_SpawnSmoke(float x, float y, float z, int count)
{
    for (int i = 0; i < count; i++) {
        float vx = ((float)(rand() % 40) - 20.0f);
        float vy = ((float)(rand() % 40) - 20.0f);
        float vz = 30.0f + (rand() % 40);
        uint32_t grey = 0x404040 + ((rand()%4)*0x101010);
        Engine_SpawnParticle(x, y, z, vx, vy, vz,
                             grey, 1.5f + (rand()%100)/100.0f,
                             6.0f + (rand()%6), -10.0f);
    }
}

/* Hazır efekt: ateş kıvılcımı */
void Engine_SpawnSpark(float x, float y, float z, int count)
{
    for (int i = 0; i < count; i++) {
        float vx = ((float)(rand() % 300) - 150.0f);
        float vy = ((float)(rand() % 300) - 150.0f);
        float vz = 80.0f + (rand() % 120);
        Engine_SpawnParticle(x, y, z, vx, vy, vz,
                             0xFF8800, 0.3f + (rand()%40)/100.0f,
                             2.0f, -200.0f);
    }
}

static void UpdateParticles(float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (!p->active) continue;

        p->lifetime -= dt;
        if (p->lifetime <= 0.0f) { p->active = 0; continue; }

        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->z  += p->vz * dt;
        p->vz += p->gravity * dt;   /* yerçekimi */

        /* Zemin çarpışması */
        if (p->z < 0.0f) { p->z = 0.0f; p->vz *= -0.3f; p->vx *= 0.7f; p->vy *= 0.7f; }
    }
}

/* 3D modda parçacıkları render et */
static void RenderParticles3D(void)
{
    float fov          = PI / 3.0f;
    float player_angle = atan2f(player_dir.y, player_dir.x);
    int   horizon      = (buffer_height / 2) + player_pitch;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (!p->active) continue;

        float dx   = p->x - player_pos.x;
        float dy   = p->y - player_pos.y;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist < 1.0f) continue;

        float sprite_angle = atan2f(dy, dx);
        float angle_diff   = sprite_angle - player_angle;
        while (angle_diff >  PI) angle_diff -= TWO_PI;
        while (angle_diff < -PI) angle_diff += TWO_PI;
        if (fabsf(angle_diff) > (fov / 2.0f) + 0.3f) continue;

        float screen_xf = (buffer_width / 2.0f) +
                          (buffer_height / 2.0f) * (angle_diff / (fov / 2.0f));
        int   screen_x  = (int)screen_xf;

        float z_scale    = buffer_height * 0.8f;
        int   proj_y     = horizon - (int)(((p->z - player_z) * z_scale) / dist);
        int   half_size  = (int)((p->size * (buffer_height * 0.8f)) / dist);
        if (half_size < 1) half_size = 1;

        /* Yaş oranına göre alfa */
        float age_ratio = p->lifetime / p->max_lifetime;
        uint8_t pr = (uint8_t)(((p->color >> 16) & 0xFF) * age_ratio);
        uint8_t pg = (uint8_t)(((p->color >>  8) & 0xFF) * age_ratio);
        uint8_t pb = (uint8_t)(((p->color      ) & 0xFF) * age_ratio);

        for (int py = proj_y - half_size; py <= proj_y + half_size; py++) {
            if (py < 0 || py >= buffer_height) continue;
            for (int px = screen_x - half_size; px <= screen_x + half_size; px++) {
                if (px < 0 || px >= buffer_width) continue;
                if (z_buffer[px] < dist) continue;
                offscreen_buffer[py * buffer_width + px] = (pr<<16)|(pg<<8)|pb;
            }
        }
    }
}

/* 2D modda parçacıkları render et (dünya koordinatından ekran koordinatına) */
static void RenderParticles2D(void)
{
    float cam_x = p2d_x - (buffer_width  / 2.0f);
    float cam_y = p2d_y - (buffer_height / 2.0f);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (!p->active) continue;

        float age_ratio = p->lifetime / p->max_lifetime;
        uint8_t pr = (uint8_t)(((p->color >> 16) & 0xFF) * age_ratio);
        uint8_t pg = (uint8_t)(((p->color >>  8) & 0xFF) * age_ratio);
        uint8_t pb = (uint8_t)(((p->color      ) & 0xFF) * age_ratio);

        int sx  = (int)(p->x - cam_x);
        int sy  = (int)(p->y - cam_y);
        int sz  = (int)p->size;
        if (sz < 1) sz = 1;

        for (int dy = -sz; dy <= sz; dy++) {
            int draw_y = sy + dy;
            if (draw_y < 0 || draw_y >= buffer_height) continue;
            for (int dx = -sz; dx <= sz; dx++) {
                int draw_x = sx + dx;
                if (draw_x < 0 || draw_x >= buffer_width) continue;
                offscreen_buffer[draw_y * buffer_width + draw_x] = (pr<<16)|(pg<<8)|pb;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// KAMERA SHAKE  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
static float shake_intensity = 0.0f;
static float shake_duration  = 0.0f;
static float shake_offset_x  = 0.0f;
static float shake_offset_y  = 0.0f;

void Engine_CameraShake(float intensity, float duration)
{
    shake_intensity = intensity;
    shake_duration  = duration;
}

static void UpdateCameraShake(float dt)
{
    if (shake_duration <= 0.0f) {
        shake_offset_x = 0.0f;
        shake_offset_y = 0.0f;
        return;
    }
    shake_duration -= dt;
    float t = shake_duration > 0.0f ? shake_duration : 0.0f;
    float current = shake_intensity * (t / (shake_intensity > 0.0f ? shake_intensity : 1.0f));
    shake_offset_x = ((float)(rand() % 200) / 100.0f - 1.0f) * current;
    shake_offset_y = ((float)(rand() % 200) / 100.0f - 1.0f) * current;
}

// ─────────────────────────────────────────────────────────────────────
// FOG  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
static float   fog_start = 400.0f;
static float   fog_end   = 900.0f;
static uint8_t fog_r     = 10;
static uint8_t fog_g     = 10;
static uint8_t fog_b     = 15;
static int     fog_enabled = 0;

void Engine_SetFog(float start_dist, float end_dist, uint8_t r, uint8_t g, uint8_t b)
{
    fog_start   = start_dist;
    fog_end     = end_dist;
    fog_r       = r;
    fog_g       = g;
    fog_b       = b;
    fog_enabled = 1;
}

void Engine_DisableFog(void) { fog_enabled = 0; }

static void ApplyFogToPixel(int buf_idx, float dist)
{
    if (!fog_enabled || dist <= fog_start) return;
    float t = (dist - fog_start) / (fog_end - fog_start);
    if (t > 1.0f) t = 1.0f;

    uint32_t color = offscreen_buffer[buf_idx];
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >>  8) & 0xFF;
    uint8_t b = (color      ) & 0xFF;

    r = (uint8_t)(r + (fog_r - r) * t);
    g = (uint8_t)(g + (fog_g - g) * t);
    b = (uint8_t)(b + (fog_b - b) * t);

    offscreen_buffer[buf_idx] = (r<<16)|(g<<8)|b;
}

// ─────────────────────────────────────────────────────────────────────
// NOKTA IŞIKLAR  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
typedef struct {
    float    x, y, z;
    float    radius;
    uint8_t  r, g, b;
    float    intensity;
    int      active;
} PointLight;

static PointLight point_lights[MAX_LIGHTS];
static int        light_count = 0;

int Engine_AddPointLight(float x, float y, float z,
                         float radius,
                         uint8_t r, uint8_t g, uint8_t b,
                         float intensity)
{
    /* Once bos slot ara */
    int idx = -1;
    for (int i = 0; i < light_count; i++) {
        if (!point_lights[i].active) { idx = i; break; }
    }
    /* Yoksa yeni slot ekle */
    if (idx < 0) {
        if (light_count >= MAX_LIGHTS) return -1;
        idx = light_count++;
    }
    point_lights[idx].x         = x;
    point_lights[idx].y         = y;
    point_lights[idx].z         = z;
    point_lights[idx].radius    = radius;
    point_lights[idx].r         = r;
    point_lights[idx].g         = g;
    point_lights[idx].b         = b;
    point_lights[idx].intensity = intensity;
    point_lights[idx].active    = 1;
    return idx;
}

void Engine_ClearLights(void)
{
    for (int i = 0; i < MAX_LIGHTS; i++) point_lights[i].active = 0;
    light_count = 0;
}

void Engine_RemovePointLight(int idx)
{
    if (idx >= 0 && idx < light_count) point_lights[idx].active = 0;
}

void Engine_MovePointLight(int idx, float x, float y, float z)
{
    if (idx >= 0 && idx < light_count) {
        point_lights[idx].x = x;
        point_lights[idx].y = y;
        point_lights[idx].z = z;
    }
}

/* Verilen dünya noktasına ışıkların toplam katkısını hesapla */
static void SamplePointLights(float wx, float wy, float wz,
                               float* out_r, float* out_g, float* out_b)
{
    *out_r = 0.0f; *out_g = 0.0f; *out_b = 0.0f;
    for (int i = 0; i < light_count; i++) {
        if (!point_lights[i].active) continue;
        float dx   = wx - point_lights[i].x;
        float dy   = wy - point_lights[i].y;
        float dz   = wz - point_lights[i].z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        if (dist >= point_lights[i].radius) continue;
        float atten = 1.0f - (dist / point_lights[i].radius);
        atten = atten * atten * point_lights[i].intensity;
        *out_r += point_lights[i].r * atten / 255.0f;
        *out_g += point_lights[i].g * atten / 255.0f;
        *out_b += point_lights[i].b * atten / 255.0f;
    }
    if (*out_r > 1.0f) *out_r = 1.0f;
    if (*out_g > 1.0f) *out_g = 1.0f;
    if (*out_b > 1.0f) *out_b = 1.0f;
}

// ─────────────────────────────────────────────────────────────────────
// LEVEL LOADER  ★ YENİ
// Dosya formatı (UTF-8 metin):
//   W <x1> <y1> <x2> <y2> <zbot> <ztop> <tex_file> <scale>
//   S <x> <y> <z> <tex_file> <layers>
//   P <x> <y>           ← player spawn (3D)
//   L <x> <y> <z> <radius> <R> <G> <B> <intensity>
//   # yorum satırı
// ─────────────────────────────────────────────────────────────────────
#define MAX_LEVEL_TEXTURES 32
static Texture level_textures[MAX_LEVEL_TEXTURES];
static char    level_tex_paths[MAX_LEVEL_TEXTURES][256];
static int     level_tex_count = 0;

static Texture* GetOrLoadTexture(const char* path)
{
    for (int i = 0; i < level_tex_count; i++) {
        if (strcmp(level_tex_paths[i], path) == 0)
            return &level_textures[i];
    }
    if (level_tex_count >= MAX_LEVEL_TEXTURES) return NULL;
    level_textures[level_tex_count] = Engine_LoadTexture(path);
    snprintf(level_tex_paths[level_tex_count], 256, "%s", path);
    return &level_textures[level_tex_count++];
}

int Engine_LoadLevel(const char* filepath)
{
    FILE* f = NULL;
    fopen_s(&f, filepath, "r");
    if (!f) return 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        if (line[0] == 'W') {
            float x1, y1, x2, y2, zb, zt, scale;
            char  tex_path[256];
            if (sscanf_s(line+2, "%f %f %f %f %f %f %s %f",
                         &x1,&y1,&x2,&y2,&zb,&zt,
                         tex_path,(unsigned)sizeof(tex_path),
                         &scale) == 8) {
                Texture* t = GetOrLoadTexture(tex_path);
                Engine_AddWall(x1,y1,x2,y2,zb,zt,t,scale);
            }
        }
        else if (line[0] == 'S') {
            float x, y, z;
            int   layers;
            char  tex_path[256];
            if (sscanf_s(line+2, "%f %f %f %s %d",
                         &x,&y,&z,
                         tex_path,(unsigned)sizeof(tex_path),
                         &layers) == 5) {
                Texture* t = GetOrLoadTexture(tex_path);
                Engine_AddSprite(x,y,z,t,layers);
            }
        }
        else if (line[0] == 'P') {
            float x, y;
            if (sscanf_s(line+2, "%f %f", &x, &y) == 2)
                Engine_InitPlayer(x, y);
        }
        else if (line[0] == 'L') {
            float x, y, z, radius, intensity;
            int   r2, g2, b2;
            if (sscanf_s(line+2, "%f %f %f %f %d %d %d %f",
                         &x,&y,&z,&radius,&r2,&g2,&b2,&intensity) == 8)
                Engine_AddPointLight(x,y,z,radius,
                                     (uint8_t)r2,(uint8_t)g2,(uint8_t)b2,
                                     intensity);
        }
    }
    fclose(f);
    return 1;
}

// ─────────────────────────────────────────────────────────────────────
// EKRAN GEÇİŞİ (FADE)  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
static float   fade_alpha    = 0.0f;   /* 0=şeffaf  1=tam siyah */
static float   fade_speed    = 0.0f;   /* >0 kararıyor, <0 aydınlanıyor */
static int     fade_active   = 0;
static void  (*fade_callback)(void) = NULL;

void Engine_FadeOut(float duration, void (*on_complete)(void))
{
    fade_alpha    = 0.0f;
    fade_speed    = 1.0f / (duration > 0.0f ? duration : 1.0f);
    fade_active   = 1;
    fade_callback = on_complete;
}

void Engine_FadeIn(float duration)
{
    fade_alpha  = 1.0f;
    fade_speed  = -1.0f / (duration > 0.0f ? duration : 1.0f);
    fade_active = 1;
}

static void UpdateFade(float dt)
{
    if (!fade_active) return;
    fade_alpha += fade_speed * dt;
    if (fade_speed > 0.0f && fade_alpha >= 1.0f) {
        fade_alpha  = 1.0f;
        fade_active = 0;
        if (fade_callback) { fade_callback(); fade_callback = NULL; }
    }
    else if (fade_speed < 0.0f && fade_alpha <= 0.0f) {
        fade_alpha  = 0.0f;
        fade_active = 0;
    }
}

static void ApplyFade(void)
{
    if (fade_alpha <= 0.0f) return;
    uint8_t alpha = (uint8_t)(fade_alpha * 255.0f);
    int total = buffer_width * buffer_height;
    for (int i = 0; i < total; i++) {
        uint32_t c = offscreen_buffer[i];
        uint8_t r = (uint8_t)(((c >> 16) & 0xFF) * (255 - alpha) / 255);
        uint8_t g = (uint8_t)(((c >>  8) & 0xFF) * (255 - alpha) / 255);
        uint8_t b = (uint8_t)(((c      ) & 0xFF) * (255 - alpha) / 255);
        offscreen_buffer[i] = (r<<16)|(g<<8)|b;
    }
}

// ─────────────────────────────────────────────────────────────────────
// FLOOR / CEILING CASTING  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
void Engine_Render3DFloor(Texture* floor_tex, Texture* ceil_tex)
{
    float player_angle = atan2f(player_dir.y, player_dir.x);
    float fov          = PI / 3.0f;
    int   horizon      = (buffer_height / 2) + player_pitch;

    for (int y = 0; y < buffer_height; y++) {
        /* Ufuk çizgisinin üstü tavan, altı zemin */
        int is_floor = (y > horizon) ? 1 : 0;

        /* Satırın ufuktan mesafesi → dünya koordinatı hesabı */
        float row_dist;
        if (y == horizon) continue;
        if (is_floor)
            row_dist = (player_z * (float)buffer_height * 0.8f) /
                       (float)(y - horizon);
        else
            row_dist = ((100.0f - player_z) * (float)buffer_height * 0.8f) /
                       (float)(horizon - y);

        /* Shake offset uygula */
        float eff_angle = player_angle + shake_offset_x * 0.002f;

        float step_x = cosf(eff_angle - fov / 2.0f) * row_dist * 2.0f / (float)buffer_width;
        float step_y = sinf(eff_angle - fov / 2.0f) * row_dist * 2.0f / (float)buffer_width;

        float floor_x = player_pos.x + cosf(eff_angle - fov / 2.0f) * row_dist;
        float floor_y = player_pos.y + sinf(eff_angle - fov / 2.0f) * row_dist;

        Texture* tex = is_floor ? floor_tex : ceil_tex;

        for (int x = 0; x < buffer_width; x++) {
            if (tex && tex->data) {
                int tx = (int)(floor_x) % tex->width;
                int ty = (int)(floor_y) % tex->height;
                if (tx < 0) tx += tex->width;
                if (ty < 0) ty += tex->height;

                int pidx  = (ty * tex->width + tx) * 4;
                uint8_t r = tex->data[pidx];
                uint8_t g = tex->data[pidx + 1];
                uint8_t b = tex->data[pidx + 2];

                /* Mesafe karartma */
                float shade = 1.0f - (row_dist / fog_end);
                if (shade < 0.05f) shade = 0.05f;
                if (shade > 1.0f)  shade = 1.0f;

                /* Nokta ışık katkısı */
                float wz = is_floor ? 0.0f : 100.0f;
                float lr, lg, lb;
                SamplePointLights(floor_x, floor_y, wz, &lr, &lg, &lb);
                shade += (lr + lg + lb) / 3.0f;
                if (shade > 1.0f) shade = 1.0f;

                r = (uint8_t)(r * shade);
                g = (uint8_t)(g * shade);
                b = (uint8_t)(b * shade);

                if (fog_enabled) {
                    int bidx = y * buffer_width + x;
                    offscreen_buffer[bidx] = (r<<16)|(g<<8)|b;
                    ApplyFogToPixel(bidx, row_dist);
                } else {
                    offscreen_buffer[y * buffer_width + x] = (r<<16)|(g<<8)|b;
                }
            } else {
                /* Tekstür yoksa düz renk */
                uint32_t flat = is_floor ? 0x282828u : 0x101018u;
                offscreen_buffer[y * buffer_width + x] = flat;
            }

            floor_x += step_x;
            floor_y += step_y;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// SPRİTE ANİMASYON  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
#define MAX_ANIMS 64

typedef struct {
    Texture* frames;
    int      frame_count;
    float    fps;
    float    timer;
    int      current_frame;
    int      loop;
    int      active;
} SpriteAnimState;

static SpriteAnimState anim_states[MAX_ANIMS];
static int             anim_count = 0;

int Engine_CreateAnim(Texture* frames, int frame_count, float fps, int loop)
{
    if (anim_count >= MAX_ANIMS) return -1;
    int idx = anim_count++;
    anim_states[idx].frames        = frames;
    anim_states[idx].frame_count   = frame_count;
    anim_states[idx].fps           = fps;
    anim_states[idx].timer         = 0.0f;
    anim_states[idx].current_frame = 0;
    anim_states[idx].loop          = loop;
    anim_states[idx].active        = 1;
    return idx;
}

void Engine_UpdateAnim(int anim_id, float dt)
{
    if (anim_id < 0 || anim_id >= anim_count) return;
    SpriteAnimState* a = &anim_states[anim_id];
    if (!a->active || a->frame_count <= 1) return;

    a->timer += dt;
    if (a->timer >= 1.0f / a->fps) {
        a->timer = 0.0f;
        a->current_frame++;
        if (a->current_frame >= a->frame_count) {
            a->current_frame = a->loop ? 0 : a->frame_count - 1;
        }
    }
}

Texture* Engine_GetAnimFrame(int anim_id)
{
    if (anim_id < 0 || anim_id >= anim_count) return NULL;
    SpriteAnimState* a = &anim_states[anim_id];
    if (!a->active || !a->frames) return NULL;
    return &a->frames[a->current_frame];
}

void Engine_ResetAnim(int anim_id)
{
    if (anim_id >= 0 && anim_id < anim_count) {
        anim_states[anim_id].current_frame = 0;
        anim_states[anim_id].timer = 0.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────
// TIMER / ZAMANLAYICI  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
#define MAX_TIMERS 32
typedef struct {
    float    duration;
    float    elapsed;
    int      repeat;
    int      active;
    void   (*callback)(void);
} TimerState;

static TimerState timers[MAX_TIMERS];
static int        timer_count = 0;

int Engine_SetTimer(float duration, int repeat, void (*callback)(void))
{
    /* Boş slot bul */
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) {
            timers[i].duration  = duration;
            timers[i].elapsed   = 0.0f;
            timers[i].repeat    = repeat;
            timers[i].active    = 1;
            timers[i].callback  = callback;
            return i;
        }
    }
    return -1;
}

void Engine_CancelTimer(int id)
{
    if (id >= 0 && id < MAX_TIMERS) timers[id].active = 0;
}

static void UpdateTimers(float dt)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) continue;
        timers[i].elapsed += dt;
        if (timers[i].elapsed >= timers[i].duration) {
            if (timers[i].callback) timers[i].callback();
            if (timers[i].repeat) timers[i].elapsed -= timers[i].duration;
            else                  timers[i].active = 0;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// MATEMATİK ARAÇLARI  ★ YENİ
// ─────────────────────────────────────────────────────────────────────
float Engine_Lerp(float a, float b, float t)         { return a + (b - a) * t; }
float Engine_Clamp(float v, float lo, float hi)       { return v < lo ? lo : (v > hi ? hi : v); }
float Engine_MapRange(float v, float a0, float a1,
                      float b0, float b1)
{
    return b0 + (v - a0) * (b1 - b0) / (a1 - a0);
}

Vec2 Vec2_Normalize(Vec2 v)
{
    float len = sqrtf(v.x*v.x + v.y*v.y);
    if (len < 0.0001f) { Vec2 z = {0,0}; return z; }
    Vec2 r = {v.x/len, v.y/len};
    return r;
}
float Vec2_Dot(Vec2 a, Vec2 b)    { return a.x*b.x + a.y*b.y; }
float Vec2_Length(Vec2 v)         { return sqrtf(v.x*v.x + v.y*v.y); }
float Vec2_Distance(Vec2 a, Vec2 b)
{
    float dx = a.x-b.x, dy = a.y-b.y;
    return sqrtf(dx*dx+dy*dy);
}

// ─────────────────────────────────────────────────────────────────────
// MEVCUT FONKSİYONLAR (orijinalden, bug fix + fog/ışık entegrasyonu)
// ─────────────────────────────────────────────────────────────────────

static const uint8_t aether_font[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00},{0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00},
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00},{0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00},
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00},{0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00},
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00},{0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},{0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},{0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},{0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00},
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00},{0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00},{0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00},{0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00},{0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00},
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},{0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},{0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00},
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00},{0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00},
    {0x3C,0x66,0x6E,0x6E,0x60,0x66,0x3C,0x00},
    {0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},{0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00},
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},{0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00},
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},{0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00},
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00},{0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},{0x06,0x06,0x06,0x06,0x06,0x66,0x3C,0x00},
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00},{0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},{0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00},
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},{0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},
    {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00},{0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00},
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00},{0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},{0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},{0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},{0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

void Engine_DrawText(const char* text, int x, int y, uint32_t color, int scale, int max_chars)
{
    if (!text) return;
    int start_x = x, char_count = 0;
    for (int i = 0; text[i] != '\0'; i++) {
        if (char_count >= max_chars) break;
        char c = text[i];
        if (c == '\n') { x = start_x; y += 10 * scale; continue; }
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 127) c = 32;
        int font_index = c - 32;
        if (font_index >= 96) font_index = 0;
        for (int py = 0; py < 8; py++) {
            uint8_t row = aether_font[font_index][py];
            for (int px = 0; px < 8; px++) {
                if (row & (1 << (7 - px)))
                    Engine_DrawFillRect(x + px*scale, y + py*scale, scale, scale, color);
            }
        }
        x += 8 * scale;
        char_count++;
    }
}

// ─────────────────────────────────────────────────────────────────────
// PENCERE / MESAJ DÖNGÜSÜ
// ─────────────────────────────────────────────────────────────────────
LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (Message == WM_CLOSE || Message == WM_DESTROY) { global_running = 0; return 0; }
    return DefWindowProcA(Window, Message, WParam, LParam);
}

void Engine_InitWindow(int width, int height, const char* title)
{
    buffer_width  = width;
    buffer_height = height;

    if (offscreen_buffer) free(offscreen_buffer);
    offscreen_buffer = (uint32_t*)malloc(width * height * sizeof(uint32_t));

    if (z_buffer) free(z_buffer);
    z_buffer = (float*)malloc(width * sizeof(float));

    bmp_info.bmiHeader.biSize        = sizeof(bmp_info.bmiHeader);
    bmp_info.bmiHeader.biWidth       = width;
    bmp_info.bmiHeader.biHeight      = -height;
    bmp_info.bmiHeader.biPlanes      = 1;
    bmp_info.bmiHeader.biBitCount    = 32;
    bmp_info.bmiHeader.biCompression = BI_RGB;

    HINSTANCE instance = GetModuleHandleA(NULL);
    WNDCLASSA wc       = {0};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = instance;
    wc.lpszClassName = "KendiMotorSinifim";
    RegisterClassA(&wc);

    global_window = CreateWindowExA(0, wc.lpszClassName, title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        0, 0, instance, 0);

    if (global_window) global_running = 1;
}

void Engine_UpdateEvents(void)
{
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int  Engine_IsRunning(void)  { return global_running; }

bool Engine_GetKeyDown(int key_code)
{
    return ((GetAsyncKeyState(key_code) & 0x8000) != 0);
}

void Engine_Start(OnUserUpdateFunc update_callback)
{
    /* HDC bir kere al, pencerenin omru boyunca tut */
    HDC hdc = GetDC(global_window);

    LARGE_INTEGER freq, t0;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    /* 360 FPS = 1/360 saniye minimum frame suresi */
    float target_frame = 1.0f / 360.0f;

    while (global_running) {
        /* Frame suresini olc */
        LARGE_INTEGER t1;
        QueryPerformanceCounter(&t1);
        float dt = (float)(t1.QuadPart - t0.QuadPart) / (float)freq.QuadPart;

        /* Hedef frame suresine ulasmadiysa bekle (busy-wait — en dusuk latency) */
        if (dt < target_frame) continue;
        t0 = t1;

        /* dt'yi makul bir ust sinira kilitle */
        if (dt > 0.05f) dt = 0.05f;

        Engine_UpdateEvents();
        Phys_UpdateSprings(dt);
        UpdateParticles(dt);
        UpdateCameraShake(dt);
        UpdateFade(dt);
        UpdateTimers(dt);

        if (update_callback) update_callback(dt);

        ApplyFade();

        /* Buffer'i ekrana kopyala */
        StretchDIBits(hdc, 0,0,buffer_width,buffer_height,
                          0,0,buffer_width,buffer_height,
                          offscreen_buffer, &bmp_info, DIB_RGB_COLORS, SRCCOPY);
    }

    ReleaseDC(global_window, hdc);
}

// ─────────────────────────────────────────────────────────────────────
// VEKTÖRLEr
// ─────────────────────────────────────────────────────────────────────
Vec2 Vec2_Add(Vec2 a, Vec2 b)        { Vec2 r = {a.x+b.x, a.y+b.y}; return r; }
Vec2 Vec2_Sub(Vec2 a, Vec2 b)        { Vec2 r = {a.x-b.x, a.y-b.y}; return r; }
Vec2 Vec2_Mul(Vec2 v, float s)       { Vec2 r = {v.x*s,   v.y*s  }; return r; }
Vec2 Vec2_Rotate(Vec2 v, float a)
{
    float c = cosf(a), s = sinf(a);
    Vec2 r = { v.x*c - v.y*s, v.x*s + v.y*c };
    return r;
}

// ─────────────────────────────────────────────────────────────────────
// ÇIZIM PRİMİTİFLERİ
// ─────────────────────────────────────────────────────────────────────
void Engine_ClearScreen(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    int total = buffer_width * buffer_height;
    for (int i = 0; i < total; i++) offscreen_buffer[i] = color;
}

void Engine_DrawPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= buffer_width || y < 0 || y >= buffer_height) return;
    offscreen_buffer[y * buffer_width + x] = ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
}

void Engine_DrawRect(int sx, int sy, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    for (int y = sy; y < sy+h; y++)
        for (int x = sx; x < sx+w; x++)
            Engine_DrawPixel(x, y, r, g, b);
}

void Engine_DrawFillRect(int sx, int sy, int w, int h, uint32_t color)
{
    for (int y = sy; y < sy+h; y++) {
        if (y < 0 || y >= buffer_height) continue;
        for (int x = sx; x < sx+w; x++) {
            if (x < 0 || x >= buffer_width) continue;
            offscreen_buffer[y * buffer_width + x] = color;
        }
    }
}

void Engine_DrawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b)
{
    int dx = abs(x1-x0), sx = x0<x1?1:-1;
    int dy = -abs(y1-y0), sy = y0<y1?1:-1;
    int err = dx+dy, e2;
    for (;;) {
        Engine_DrawPixel(x0, y0, r, g, b);
        if (x0==x1 && y0==y1) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Alfa karıştırmalı dikdörtgen — UI için kullanışlı */
void Engine_DrawBlendRect(int sx, int sy, int w, int h, uint32_t color, float alpha)
{
    uint8_t cr = (color>>16)&0xFF;
    uint8_t cg = (color>>8) &0xFF;
    uint8_t cb = (color    )&0xFF;
    for (int y = sy; y < sy+h; y++) {
        if (y < 0 || y >= buffer_height) continue;
        for (int x = sx; x < sx+w; x++) {
            if (x < 0 || x >= buffer_width) continue;
            int idx = y*buffer_width+x;
            uint32_t old = offscreen_buffer[idx];
            uint8_t or2 = (old>>16)&0xFF;
            uint8_t og  = (old>>8) &0xFF;
            uint8_t ob  = (old    )&0xFF;
            uint8_t nr  = (uint8_t)(or2 + (cr - or2) * alpha);
            uint8_t ng  = (uint8_t)(og  + (cg - og ) * alpha);
            uint8_t nb  = (uint8_t)(ob  + (cb - ob ) * alpha);
            offscreen_buffer[idx] = ((uint32_t)nr<<16)|((uint32_t)ng<<8)|nb;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// TEXTURE
// ─────────────────────────────────────────────────────────────────────
static void Engine_GenerateHeightmap(Texture* tex)
{
    if (!tex->data || tex->width == 0 || tex->height == 0) return;
    int w = tex->width, h = tex->height;
    float* dist = (float*)malloc(w * h * sizeof(float));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            dist[y*w+x] = (tex->data[(y*w+x)*4+3] < 128) ? 0.0f : 9999.0f;

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (dist[y*w+x] > 0.0f) {
                float m = dist[y*w+x];
                if (x>0) m = fminf(m, dist[y*w+(x-1)]+1.0f);
                if (y>0) m = fminf(m, dist[(y-1)*w+x]+1.0f);
                dist[y*w+x] = m;
            }
    for (int y = h-1; y >= 0; y--)
        for (int x = w-1; x >= 0; x--)
            if (dist[y*w+x] > 0.0f) {
                float m = dist[y*w+x];
                if (x<w-1) m = fminf(m, dist[y*w+(x+1)]+1.0f);
                if (y<h-1) m = fminf(m, dist[(y+1)*w+x]+1.0f);
                dist[y*w+x] = m;
            }

    float max_d = 1.0f;
    for (int i = 0; i < w*h; i++)
        if (dist[i] > max_d && dist[i] < 9999.0f) max_d = dist[i];

    tex->heightmap = (uint8_t*)malloc(w*h);
    for (int i = 0; i < w*h; i++)
        tex->heightmap[i] = (uint8_t)((dist[i]/max_d)*255.0f);
    free(dist);
}

Texture Engine_LoadTexture(const char* filepath)
{
    Texture t = {0};
    t.data = stbi_load(filepath, &t.width, &t.height, &t.channels, 4);
    if (t.data) Engine_GenerateHeightmap(&t);
    return t;
}

void Engine_FreeTexture(Texture* t)
{
    if (!t) return;
    if (t->data)      { stbi_image_free(t->data); t->data = NULL; }
    if (t->heightmap) { free(t->heightmap); t->heightmap = NULL; }
}

// ─────────────────────────────────────────────────────────────────────
// DUVAR / COLLISION
// ─────────────────────────────────────────────────────────────────────
void Engine_AddWall(float x1, float y1, float x2, float y2,
                    float z_bottom, float z_top,
                    Texture* tex, float tex_scale)
{
    if (wall_count >= MAX_WALLS) return;
    Wall* w   = &global_walls[wall_count++];
    w->p1.x   = x1; w->p1.y = y1;
    w->p2.x   = x2; w->p2.y = y2;
    w->vec_v.x = x2-x1; w->vec_v.y = y2-y1;
    w->z_bottom = z_bottom; w->z_top = z_top;
    w->tex = tex;
    if (tex && tex->width > 0) {
        float len = sqrtf((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
        w->u_repeat = (len   / (float)tex->width ) * tex_scale;
        w->v_repeat = ((z_top-z_bottom)/(float)tex->height) * tex_scale;
    } else { w->u_repeat = 1.0f; w->v_repeat = 1.0f; }
}

void Engine_ClearWalls(void) { wall_count = 0; }

static void ResolveCollisions(void)
{
    float R = 32.0f;  /* oyuncu yaricapi */
    /* 3 iterasyon: koseler icin */
    for (int iter = 0; iter < 3; iter++) {
        for (int i = 0; i < wall_count; i++) {
            Wall* w = &global_walls[i];
            float vx = w->vec_v.x, vy = w->vec_v.y;
            float len_sq = vx*vx + vy*vy;
            if (len_sq < 0.0001f) continue;
            /* En yakin nokta: duvar segmentine projeksiyon */
            float wx = player_pos.x - w->p1.x;
            float wy = player_pos.y - w->p1.y;
            float t = (wx*vx + wy*vy) / len_sq;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float cx = w->p1.x + t*vx;
            float cy = w->p1.y + t*vy;
            float dx = player_pos.x - cx;
            float dy = player_pos.y - cy;
            float d_sq = dx*dx + dy*dy;
            if (d_sq >= R*R || d_sq < 0.0001f) continue;
            /* Z kontrolu: oyuncu duvarin yukseklik araligindaysa carpisir */
            if (player_z - 30.0f >= w->z_top)    continue;
            if (player_z + 10.0f <= w->z_bottom)  continue;
            float d = sqrtf(d_sq);
            float push = R - d;
            player_pos.x += (dx/d) * push;
            player_pos.y += (dy/d) * push;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// OYUNCU (3D)
// ─────────────────────────────────────────────────────────────────────
void Engine_InitPlayer(float start_x, float start_y)
{
    player_pos.x = start_x; player_pos.y = start_y;
}

void Engine_UpdatePlayer(float move_speed, float rot_speed, float dt)
{
    if (Engine_GetKeyDown(VK_RIGHT)||Engine_GetKeyDown('D'))
        player_dir = Vec2_Rotate(player_dir,  rot_speed * dt);
    if (Engine_GetKeyDown(VK_LEFT) ||Engine_GetKeyDown('A'))
        player_dir = Vec2_Rotate(player_dir, -rot_speed * dt);

    Vec2 vel = Vec2_Mul(player_dir, move_speed * dt);
    if (Engine_GetKeyDown(VK_UP)  ||Engine_GetKeyDown('W'))
        player_pos = Vec2_Add(player_pos, vel);
    if (Engine_GetKeyDown(VK_DOWN)||Engine_GetKeyDown('S'))
        player_pos = Vec2_Sub(player_pos, vel);

    /* Pitch: R yukari bakar, birakilinca yumusak sifira doner */
    if (Engine_GetKeyDown('R')) player_pitch += (int)(300.0f*dt);
    else player_pitch += (int)((0 - player_pitch) * 8.0f * dt);
    if (player_pitch >  200) player_pitch =  200;
    if (player_pitch < -200) player_pitch = -200;

    /* SPACE = ziplama, C = egilme — spring ile normal yukseklige doner */
    float z_target = 50.0f;
    if (Engine_GetKeyDown(' '))  z_target = 75.0f;
    if (Engine_GetKeyDown('C'))  z_target = 28.0f;
    player_z += (z_target - player_z) * 12.0f * dt;
    if (player_z > 78.0f) player_z = 78.0f;
    if (player_z < 25.0f) player_z = 25.0f;

    ResolveCollisions();
}

void Engine_DrawPlayerDebug(int size)
{
    Engine_DrawRect((int)player_pos.x - size/2, (int)player_pos.y - size/2, size, size, 255,0,0);
    Vec2 t = Vec2_Add(player_pos, Vec2_Mul(player_dir, size*1.5f));
    Engine_DrawRect((int)t.x-2, (int)t.y-2, 4,4, 255,255,0);
}

// ─────────────────────────────────────────────────────────────────────
// SHOOT RAYCAST  -- oyuncu bakis yonunde duvar kesisimi
// ─────────────────────────────────────────────────────────────────────
float Engine_RaycastShoot(float* hit_x, float* hit_y, int* wall_idx)
{
    Vec2  ray_pos = player_pos;
    Vec2  ray_dir = player_dir;
    float best_t  = 99999.0f;
    int   best_w  = -1;

    for (int w = 0; w < wall_count; w++) {
        Wall* wall  = &global_walls[w];
        Vec2  vec_w = { ray_pos.x - wall->p1.x, ray_pos.y - wall->p1.y };
        float denom = wall->vec_v.x * ray_dir.y - wall->vec_v.y * ray_dir.x;
        if (fabsf(denom) < 0.0001f) continue;
        float u = (vec_w.x * ray_dir.y  - vec_w.y * ray_dir.x)  / denom;
        float t = (vec_w.x * wall->vec_v.y - vec_w.y * wall->vec_v.x) / denom;
        if (u >= 0.0f && u <= 1.0f && t > 1.0f && t < best_t) {
            best_t = t;
            best_w = w;
        }
    }

    if (best_w >= 0 && hit_x && hit_y) {
        *hit_x = ray_pos.x + ray_dir.x * best_t;
        *hit_y = ray_pos.y + ray_dir.y * best_t;
    }
    if (wall_idx) *wall_idx = best_w;
    return (best_w >= 0) ? best_t : -1.0f;
}

// Iki nokta arasinda duvar var mi? (0=var, 1=yok)
int Engine_HasLineOfSight(float x1, float y1, float x2, float y2)
{
    float dx  = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.001f) return 1;
    Vec2 ray_dir  = { dx/len, dy/len };
    Vec2 ray_pos  = { x1, y1 };

    for (int w = 0; w < wall_count; w++) {
        Wall* wall  = &global_walls[w];
        Vec2  vec_w = { ray_pos.x - wall->p1.x, ray_pos.y - wall->p1.y };
        float denom = wall->vec_v.x * ray_dir.y - wall->vec_v.y * ray_dir.x;
        if (fabsf(denom) < 0.0001f) continue;
        float u = (vec_w.x * ray_dir.y  - vec_w.y * ray_dir.x)  / denom;
        float t = (vec_w.x * wall->vec_v.y - vec_w.y * wall->vec_v.x) / denom;
        /* u: duvar uzerinde 0-1, kose sinirinda hata olmasin diye 0.01 tolerans
           t: baslangic noktasinin hemen yanindaki noise atla (1px)
              hedef noktasina ulasmadan once kesismeli */
        if (u > 0.01f && u < 0.99f && t > 1.0f && t < len - 1.0f) return 0;
    }
    return 1;
}

// ─────────────────────────────────────────────────────────────────────
// 3D RENDERER
// ─────────────────────────────────────────────────────────────────────
typedef struct { int wall_index; float distance; float u; } RayHit;

void Engine_Render3D(void)
{
    /* z_buffer'i her frame sifirla — sprite renderer buna bakar */
    for (int i = 0; i < buffer_width; i++) z_buffer[i] = 99999.0f;

    float fov          = PI / 3.0f;
    float player_angle = atan2f(player_dir.y, player_dir.x);
    float start_angle  = player_angle - fov/2.0f;

    /* Shake ufuk kayması */
    int horizon = (buffer_height/2) + player_pitch + (int)shake_offset_y;

    for (int x = 0; x < buffer_width; x++) {
        float ray_angle = start_angle + ((float)x/(float)buffer_width)*fov;
        Vec2  ray_dir   = {cosf(ray_angle), sinf(ray_angle)};

        RayHit hits[32];
        int    hit_count = 0;

        for (int w = 0; w < wall_count; w++) {
            Wall* wall = &global_walls[w];
            Vec2  vec_w = { player_pos.x - wall->p1.x, player_pos.y - wall->p1.y };
            float denom = wall->vec_v.x * ray_dir.y - wall->vec_v.y * ray_dir.x;
            if (fabsf(denom) > 0.0001f) {
                float u = (vec_w.x*ray_dir.y - vec_w.y*ray_dir.x) / denom;
                float t = (vec_w.x*wall->vec_v.y - vec_w.y*wall->vec_v.x) / denom;
                if (u >= 0.0f && u <= 1.0f && t > 0.0f && hit_count < 32) {
                    hits[hit_count].wall_index = w;
                    hits[hit_count].distance   = t;
                    hits[hit_count].u          = u;
                    hit_count++;
                }
            }
        }

        /* Uzaktan yakina sirala: hits[0]=uzak, hits[hit_count-1]=yakin
           Bu sekilde yakin duvar uzak duvarin ustune cizilebilir (painter) */
        for (int i = 0; i < hit_count-1; i++)
            for (int j = 0; j < hit_count-i-1; j++)
                if (hits[j].distance < hits[j+1].distance) {
                    RayHit tmp = hits[j]; hits[j] = hits[j+1]; hits[j+1] = tmp;
                }
        /* hits[hit_count-1] = en yakin duvar -- z_buffer bunu kullanir */

        int sky_bottom = horizon < 0 ? 0 : (horizon >= buffer_height ? buffer_height-1 : horizon);
        Engine_DrawRect(x, 0, 1, sky_bottom, 20,20,40);
        Engine_DrawRect(x, sky_bottom, 1, buffer_height-sky_bottom, 40,40,40);

        for (int i = 0; i < hit_count; i++) {
            Wall*  hit_wall  = &global_walls[hits[i].wall_index];
            float  raw_dist  = hits[i].distance;
            float  perp_dist = raw_dist * cosf(ray_angle - player_angle);
            if (perp_dist < 5.0f) perp_dist = 5.0f;

            float z_scale    = buffer_height * 0.8f;
            int   proj_top   = horizon - (int)(((hit_wall->z_top    - player_z)*z_scale)/perp_dist);
            int   proj_bot   = horizon - (int)(((hit_wall->z_bottom - player_z)*z_scale)/perp_dist);

            int render_start = proj_top  < 0              ? 0              : proj_top;
            int render_end   = proj_bot  >= buffer_height ? buffer_height-1 : proj_bot;

            /* Temel mesafe karartma */
            int color_int = 255 - (int)(raw_dist * 0.3f);
            if (color_int < 20) color_int = 20;

            /* Nokta ışık katkısı */
            float hit_x = player_pos.x + ray_dir.x * raw_dist;
            float hit_y = player_pos.y + ray_dir.y * raw_dist;
            float lr, lg, lb;
            SamplePointLights(hit_x, hit_y, (hit_wall->z_top+hit_wall->z_bottom)*0.5f,
                              &lr, &lg, &lb);
            float light_bonus = (lr + lg + lb) / 3.0f;

            if (hit_wall->tex && hit_wall->tex->data) {
                float u_tiled = fmodf(hits[i].u * hit_wall->u_repeat, 1.0f);
                if (u_tiled < 0.0f) u_tiled += 1.0f;
                int tex_x = (int)(u_tiled * hit_wall->tex->width);
                if (tex_x < 0) tex_x = 0;
                if (tex_x >= hit_wall->tex->width) tex_x = hit_wall->tex->width-1;

                float wall_h  = (float)(proj_bot - proj_top);
                if (wall_h < 0.0001f) wall_h = 1.0f;
                float v_step  = hit_wall->v_repeat / wall_h;
                float v       = (float)(render_start - proj_top) * v_step;

                for (int y = render_start; y <= render_end; y++) {
                    float v_tiled = v - (int)v;
                    if (v_tiled < 0.0f) v_tiled += 1.0f;
                    int tex_y = (int)(v_tiled * hit_wall->tex->height);
                    if (tex_y < 0) tex_y = 0;
                    if (tex_y >= hit_wall->tex->height) tex_y = hit_wall->tex->height-1;

                    int pidx = (tex_y * hit_wall->tex->width + tex_x) * 4;
                    if (hit_wall->tex->data[pidx+3] > 128) {
                        float shade = (float)color_int/255.0f + light_bonus;
                        if (shade > 1.0f) shade = 1.0f;
                        uint8_t r = (uint8_t)(hit_wall->tex->data[pidx  ] * shade);
                        uint8_t g = (uint8_t)(hit_wall->tex->data[pidx+1] * shade);
                        uint8_t b = (uint8_t)(hit_wall->tex->data[pidx+2] * shade);
                        int buf_idx = y*buffer_width+x;
                        offscreen_buffer[buf_idx] = ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                        ApplyFogToPixel(buf_idx, perp_dist);
                    }
                    v += v_step;
                }
            } else {
                if (render_end > render_start) {
                    float shade = (float)color_int/255.0f + light_bonus;
                    if (shade > 1.0f) shade = 1.0f;
                    uint8_t cv = (uint8_t)(200 * shade);
                    Engine_DrawRect(x, render_start, 1, render_end-render_start, cv, cv/4, cv/4);
                }
            }
        }

        /* hits[hit_count-1] = en yakin duvar (descending sort) */
        z_buffer[x] = hit_count > 0
            ? hits[hit_count-1].distance * cosf(ray_angle - player_angle)
            : 99999.0f;
        if (z_buffer[x] < 0.5f) z_buffer[x] = 0.5f;
    }
}

// ─────────────────────────────────────────────────────────────────────
// SPRİTE RENDERER
// ─────────────────────────────────────────────────────────────────────
void Engine_AddSprite(float x, float y, float z, Texture* tex, int layers)
{
    if (sprite_count >= MAX_SPRITES) return;
    global_sprites[sprite_count++] = (Sprite){x, y, z, tex, layers};
}

void Engine_ClearSprites(void) { sprite_count = 0; }

void Engine_RenderSprites(void)
{
    if (sprite_count == 0) { RenderParticles3D(); return; }

    /* ── ADIM 1: Uzaktan yakina sirala ── */
    for (int i = 0; i < sprite_count-1; i++)
        for (int j = 0; j < sprite_count-i-1; j++) {
            float d1 = (global_sprites[j  ].x-player_pos.x)*(global_sprites[j  ].x-player_pos.x)
                      +(global_sprites[j  ].y-player_pos.y)*(global_sprites[j  ].y-player_pos.y);
            float d2 = (global_sprites[j+1].x-player_pos.x)*(global_sprites[j+1].x-player_pos.x)
                      +(global_sprites[j+1].y-player_pos.y)*(global_sprites[j+1].y-player_pos.y);
            if (d1 < d2) {
                Sprite tmp = global_sprites[j];
                global_sprites[j] = global_sprites[j+1];
                global_sprites[j+1] = tmp;
            }
        }

    /* ── Render3D ile TAMAMEN ayni sabitler ── */
    float fov         = PI / 3.0f;          /* 60 derece */
    float player_angle = atan2f(player_dir.y, player_dir.x);
    int   horizon     = (buffer_height/2) + player_pitch + (int)shake_offset_y;
    float z_scale     = buffer_height * 0.8f;  /* Render3D ile ayni */

    /* Kamera sag vektoru: player_dir'e 90 derece saat yonu
       dir=(cx,cy) => right=(cy,-cx)  */
    float rx = player_dir.y;
    float ry = -player_dir.x;

    /* Render3D formulu: kolon x icin
         ray_angle = player_angle - fov/2 + (x/W)*fov
       Sprite icin: sprite kamera uzayindaki yatay sapma
         cam_x = dot(sp-player, right)
         cam_z = dot(sp-player, dir)    [bu Render3D'nin perp_dist'i]
       Ekran X:
         ray_angle_sp = atan2(sp-player) degil, dogrudan:
         x/W = (ray_angle_sp - start_angle) / fov
         => screen_x = ( (cam_x/cam_z) / tan(fov/2) + 1 ) * W/2
       Bu Render3D ray formuluyle matematiksel olarak identik. */
    float half_fov_tan = tanf(fov * 0.5f);   /* tan(30) = 0.5774 */

    for (int si = 0; si < sprite_count; si++) {
        Sprite* sp = &global_sprites[si];
        if (!sp->tex || !sp->tex->data) continue;

        float dx = sp->x - player_pos.x;
        float dy = sp->y - player_pos.y;

        /* Kamera uzayi */
        float cam_z = dx * player_dir.x + dy * player_dir.y;
        float cam_x = dx * rx           + dy * ry;

        /* Near clip: sprite cok yakinsa cizme (yoksa proj_top -inf'e gider) */
        if (cam_z < 30.0f) continue;

        /* Tam LOS kontrolu: oyuncu-sprite arasinda duvar varsa atla.
           Sprite merkezine cizilen ray duvar geciyorsa sprite gozukmemeli. */
        if (!Engine_HasLineOfSight(player_pos.x, player_pos.y, sp->x, sp->y)) continue;

        /* Ekran X merkezi */
        int screen_x = (int)( (float)buffer_width * 0.5f *
                              (1.0f + (cam_x / cam_z) / half_fov_tan) );

        /* ── Boyut ──
           Render3D: proj_top = horizon - (z_top - player_z)*z_scale/perp_dist
                     proj_bot = horizon - (z_bot - player_z)*z_scale/perp_dist
           Sprite world yuksekligi = duvar yuksekligi = 100 birim.
           z_bot = sp->z (zemin), z_top = sp->z + 100
           Genislik: aspect ratio korunur */
        float world_h = 100.0f;
        int proj_top = horizon - (int)(((sp->z + world_h) - player_z) * z_scale / cam_z);
        int proj_bot = horizon - (int)(( sp->z            - player_z) * z_scale / cam_z);

        /* Bozuk deger korumasi */
        if (proj_bot <= proj_top) continue;

        /* Boyut sinirlari: cok yakin sprite ekrani 10x dolduramaz */
        int sp_h_raw = proj_bot - proj_top;
        if (sp_h_raw > buffer_height * 4) continue;  /* sprite icindesin */

        /* Genislik: texture aspect orani koru */
        float aspect = (float)sp->tex->width / (float)sp->tex->height;
        int   sp_h   = sp_h_raw;
        int   sp_w   = (int)(sp_h * aspect);
        if (sp_h < 1 || sp_w < 1) continue;

        int draw_x0 = screen_x - sp_w / 2;
        int draw_x1 = screen_x + sp_w / 2;
        if (draw_x1 <= 0 || draw_x0 >= buffer_width) continue;

        /* ── Isik hesabi ── */
        float dist = sqrtf(dx*dx + dy*dy);
        float shade = 1.0f - (dist / (fog_end > 1.0f ? fog_end : 900.0f));
        if (shade < 0.1f) shade = 0.1f;
        if (shade > 1.0f) shade = 1.0f;

        float lr, lg2, lb;
        SamplePointLights(sp->x, sp->y, sp->z + world_h*0.5f, &lr, &lg2, &lb);
        float la = (lr+lg2+lb)/3.0f;
        shade = shade + la;
        if (shade > 1.0f) shade = 1.0f;

        /* ── Sutun sutun ciz ── */
        int total_w = draw_x1 - draw_x0;
        int total_h = proj_bot - proj_top;

        for (int stripe = draw_x0; stripe < draw_x1; stripe++) {
            if (stripe < 0 || stripe >= buffer_width) continue;

            /* Z-buffer: duvar bu sutunda sprite'tan yakinsa atla.
               1px tolerans: ayni mesafedeki sprite duvarin onunde sayilir. */
            if (z_buffer[stripe] + 1.0f < cam_z) continue;

            /* U: 0..1 soldan saga */
            float u = (float)(stripe - draw_x0) / (float)total_w;
            int tx = (int)(u * (float)sp->tex->width);
            if (tx < 0) tx = 0;
            if (tx >= sp->tex->width) tx = sp->tex->width - 1;

            for (int row = proj_top; row < proj_bot; row++) {
                if (row < 0 || row >= buffer_height) continue;

                float v = (float)(row - proj_top) / (float)total_h;
                int ty = (int)(v * (float)sp->tex->height);
                if (ty < 0) ty = 0;
                if (ty >= sp->tex->height) ty = sp->tex->height - 1;

                int cidx = (ty * sp->tex->width + tx) * 4;
                if (sp->tex->data[cidx + 3] < 128) continue;

                /* Sade golgegolge: mesafe + nokta isik */
                uint8_t sr = (uint8_t)(sp->tex->data[cidx  ] * shade);
                uint8_t sg = (uint8_t)(sp->tex->data[cidx+1] * shade);
                uint8_t sb = (uint8_t)(sp->tex->data[cidx+2] * shade);
                offscreen_buffer[row * buffer_width + stripe] =
                    ((uint32_t)sr << 16) | ((uint32_t)sg << 8) | sb;
            }
        }
    }

    RenderParticles3D();
}



// ─────────────────────────────────────────────────────────────────────
// SİLAH
// ─────────────────────────────────────────────────────────────────────
void Engine_InitWeapon(void)
{
    weapon_spring_x = Phys_AddSpring(150.0f, 12.0f, 2.0f);
    weapon_spring_y = Phys_AddSpring(150.0f, 12.0f, 2.0f);
}

void Engine_UpdateAndRenderWeapon(float dt)
{
    if (weapon_spring_x == -1 || weapon_spring_y == -1) return;
    float tx = buffer_width - 200.0f;
    float ty = buffer_height - 100.0f;

    if (Engine_GetKeyDown('D')) tx -= 60.0f;
    if (Engine_GetKeyDown('A')) tx += 60.0f;
    if (Engine_GetKeyDown('W') || Engine_GetKeyDown('S')) {
        static float acc = 0.0f;
        acc += dt * 12.0f;
        ty += sinf(acc) * 25.0f;
        tx += cosf(acc * 0.5f) * 10.0f;
    }
    if (Engine_GetKeyDown(' ')) ty += 40.0f;
    if (Engine_GetKeyDown('C')) ty -= 40.0f;

    Phys_SetSpringTarget(weapon_spring_x, tx);
    Phys_SetSpringTarget(weapon_spring_y, ty);

    float wx = Phys_GetSpringPos(weapon_spring_x);
    float wy = Phys_GetSpringPos(weapon_spring_y);
    Engine_DrawRect((int)wx, (int)wy, 60, 150, 200,200,200);
    Engine_DrawRect((int)wx+20, (int)wy-10, 20, 20, 50,50,50);
}

// ─────────────────────────────────────────────────────────────────────
// SPRING FİZİK
// ─────────────────────────────────────────────────────────────────────
int Phys_AddSpring(float stiffness, float damping, float mass)
{
    if (spring_count >= MAX_SPRINGS) return -1;
    global_springs[spring_count] = (Phys_Spring){0,0,0,stiffness,damping,mass};
    return spring_count++;
}

void Phys_SetSpringTarget(int idx, float target)
{
    if (idx >= 0 && idx < spring_count) global_springs[idx].target = target;
}

float Phys_GetSpringPos(int idx)
{
    return (idx >= 0 && idx < spring_count) ? global_springs[idx].pos : 0.0f;
}

void Phys_UpdateSprings(float dt)
{
    for (int i = 0; i < spring_count; i++) {
        Phys_Spring* s = &global_springs[i];
        float force = -(s->k * (s->pos - s->target)) - (s->c * s->vel);
        s->vel += (force / s->mass) * dt;
        s->pos += s->vel * dt;
    }
}

// ─────────────────────────────────────────────────────────────────────
// DEBUG
// ─────────────────────────────────────────────────────────────────────
void Engine_DrawVectorMapDebug(void)
{
    for (int i = 0; i < wall_count; i++) {
        Engine_DrawLine((int)global_walls[i].p1.x,(int)global_walls[i].p1.y,
                        (int)global_walls[i].p2.x,(int)global_walls[i].p2.y,255,255,255);
        Engine_DrawRect((int)global_walls[i].p1.x-2,(int)global_walls[i].p1.y-2,4,4,0,100,255);
        Engine_DrawRect((int)global_walls[i].p2.x-2,(int)global_walls[i].p2.y-2,4,4,0,100,255);
    }
}

void Engine_DrawCameraDebug(int size)
{
    Engine_DrawPlayerDebug(size);
}

void Engine_CastVectorRaysDebug(void)
{
    float fov = PI/3.0f, player_angle = atan2f(player_dir.y,player_dir.x);
    float start_angle = player_angle - fov/2.0f;
    for (int i = 0; i < 60; i++) {
        float ray_angle = start_angle + ((float)i/60.0f)*fov;
        Vec2  ray_dir   = {cosf(ray_angle), sinf(ray_angle)};
        float closest   = 800.0f;
        int   has_hit   = 0;
        float hx = 0, hy = 0;
        for (int w = 0; w < wall_count; w++) {
            Wall* wall  = &global_walls[w];
            Vec2  vec_w = {player_pos.x-wall->p1.x, player_pos.y-wall->p1.y};
            float denom = wall->vec_v.x*ray_dir.y - wall->vec_v.y*ray_dir.x;
            if (fabsf(denom) > 0.0001f) {
                float u = (vec_w.x*ray_dir.y - vec_w.y*ray_dir.x)/denom;
                float t = (vec_w.x*wall->vec_v.y - vec_w.y*wall->vec_v.x)/denom;
                if (u>=0&&u<=1&&t>0&&t<closest) { closest=t; has_hit=1;
                    hx=player_pos.x+ray_dir.x*t; hy=player_pos.y+ray_dir.y*t; }
            }
        }
        if (has_hit) Engine_DrawRect((int)hx-2,(int)hy-2,4,4,0,255,0);
    }
}

// ─────────────────────────────────────────────────────────────────────
// 2D DÜNYA
// ─────────────────────────────────────────────────────────────────────
static int IsSolid(float test_x, float test_y)
{
    int gx = (int)(test_x/TILE_SIZE), gy = (int)(test_y/TILE_SIZE);
    if (gx<0||gx>=active_map_w||gy<0||gy>=active_map_h) return 1;
    if (active_map[gy*active_map_w+gx]==1) return 1;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = &entities[i];
        if (e->active && e->is_solid &&
            test_x > e->x && test_x < e->x+e->width &&
            test_y > e->y && test_y < e->y+e->height) return 1;
    }
    return 0;
}

void Engine_Render2DWorld(int* map_data, int map_w, int map_h, Texture* tex_floor, Texture* tex_wall)
{
    (void)tex_floor;
    for (int i = 0; i < buffer_width*buffer_height; i++) offscreen_buffer[i] = 0;

    float cam_x = p2d_x-(buffer_width/2.0f);
    float cam_y = p2d_y-(buffer_height/2.0f);

    int smx = (int)(cam_x/TILE_SIZE)-1, smy = (int)(cam_y/TILE_SIZE)-1;
    int emx = smx+(buffer_width/TILE_SIZE)+2;
    int emy = smy+(buffer_height/TILE_SIZE)+2;

    for (int ty = smy; ty < emy; ty++) {
        for (int tx = smx; tx < emx; tx++) {
            if (tx<0||tx>=map_w||ty<0||ty>=map_h) continue;
            int tile      = map_data[ty*map_w+tx];
            int screen_x  = tx*TILE_SIZE-(int)cam_x;
            int screen_y  = ty*TILE_SIZE-(int)cam_y;

            for (int py = 0; py < TILE_SIZE; py++) {
                int dy = screen_y+py;
                if (dy<0||dy>=buffer_height) continue;
                for (int px2 = 0; px2 < TILE_SIZE; px2++) {
                    int dx = screen_x+px2;
                    if (dx<0||dx>=buffer_width) continue;
                    if (tile == 0) {
                        offscreen_buffer[dy*buffer_width+dx] = 0x1A1A1A;
                    } else if (tex_wall && tex_wall->data) {
                        int ttx = (int)((float)px2/TILE_SIZE * tex_wall->width);
                        int tty = (int)((float)py /TILE_SIZE * tex_wall->height);
                        int pidx = (tty*tex_wall->width+ttx)*4;
                        if (tex_wall->data[pidx+3]>128) {
                            uint8_t r = tex_wall->data[pidx];
                            uint8_t g = tex_wall->data[pidx+1];
                            uint8_t b = tex_wall->data[pidx+2];
                            offscreen_buffer[dy*buffer_width+dx]=((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                        }
                    } else {
                        offscreen_buffer[dy*buffer_width+dx] = 0x444444;
                    }
                }
            }
        }
    }
}

void Engine_Apply2DLighting(float mouse_angle)
{
    int cx = buffer_width/2, cy = buffer_height/2;
    float flashlight_range = 350.0f;
    float flashlight_width = PI/4.0f;

    for (int y = 0; y < buffer_height; y++) {
        for (int x = 0; x < buffer_width; x++) {
            float dx   = (float)(x-cx), dy2 = (float)(y-cy);
            float dist = sqrtf(dx*dx+dy2*dy2);
            float li   = (dist < 50.0f) ? 0.5f : 0.0f;

            if (dist < flashlight_range) {
                float pa   = atan2f(dy2, dx);
                float diff = pa - mouse_angle;
                while (diff >  PI) diff -= TWO_PI;
                while (diff < -PI) diff += TWO_PI;
                if (fabsf(diff) < flashlight_width) {
                    float cur = (1.0f-(dist/flashlight_range)) * (1.0f-(fabsf(diff)/flashlight_width));
                    if (cur > li) li = cur;
                }
            }

            /* 2D nokta ışıklar */
            for (int i = 0; i < light_count; i++) {
                if (!point_lights[i].active) continue;
                float wx  = p2d_x - (buffer_width/2.0f) + x;
                float wy  = p2d_y - (buffer_height/2.0f) + y;
                float ldx = wx - point_lights[i].x;
                float ldy = wy - point_lights[i].y;
                float ld  = sqrtf(ldx*ldx+ldy*ldy);
                if (ld < point_lights[i].radius) {
                    float atten = (1.0f - ld/point_lights[i].radius) * point_lights[i].intensity;
                    if (atten > li) li = atten;
                }
            }

            if (li < 1.0f) {
                int idx = y*buffer_width+x;
                uint32_t c = offscreen_buffer[idx];
                uint8_t r = (uint8_t)(((c>>16)&0xFF)*li);
                uint8_t g = (uint8_t)(((c>> 8)&0xFF)*li);
                uint8_t b = (uint8_t)(((c    )&0xFF)*li);
                offscreen_buffer[idx] = ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// ENTITY SİSTEMİ
// ─────────────────────────────────────────────────────────────────────
void Engine_InitLevel(int* map_data, int map_w, int map_h, Texture* tex_floor2, Texture* tex_wall2)
{
    active_map = map_data; active_map_w = map_w; active_map_h = map_h;
    t_floor = tex_floor2; t_wall = tex_wall2;
    for (int i = 0; i < MAX_ENTITIES; i++) entities[i].active = 0;
    current_state = STATE_PLAYING;
    p_stats.current_health = p_stats.max_health;
    p_stats.is_hidden = 0;
}

Entity* Engine_AddEntity(EntityType type, float x, float y, float w, float h,
                         int is_solid, Texture* sprite, const char* name, InteractCallback cb)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entities[i].active) {
            Entity* e   = &entities[i];
            *e = (Entity){0};
            e->active   = 1; e->id = i; e->type = type;
            e->x = x; e->y = y; e->width = w; e->height = h;
            e->is_solid = is_solid; e->sprite = sprite; e->on_interact = cb;
            e->health = 100; e->damage = 10; e->speed = 100.0f;
            if (name) snprintf(e->name, 32, "%s", name);
            return e;
        }
    }
    return NULL;
}

void Engine_RemoveEntity(Entity* ent) { if (ent) ent->active = 0; }

// ─────────────────────────────────────────────────────────────────────
// DİYALOG
// ─────────────────────────────────────────────────────────────────────
void Engine_StartDialog(const char* speaker, const char* text, Texture* portrait)
{
    dialog_speaker    = speaker;
    dialog_text       = text;
    dialog_portrait   = portrait;
    current_state     = STATE_DIALOG;
    dialog_char_index = 0;
    dialog_timer      = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────
// INVENTORY / SAĞLIK
// ─────────────────────────────────────────────────────────────────────
void Engine_GiveItem(const char* item_name, Texture* icon)
{
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (!p_stats.inventory[i].active) {
            p_stats.inventory[i].active = 1;
            snprintf(p_stats.inventory[i].name, 32, "%s", item_name);
            p_stats.inventory[i].icon = icon;
            break;
        }
    }
}

void Engine_TakeDamage(int amount)
{
    if (p_stats.is_hidden) return;
    p_stats.current_health -= amount;
    if (p_stats.current_health <= 0) {
        p_stats.current_health = 0;
        current_state = STATE_DEAD;
    }
    /* Hasar aldığında otomatik shake */
    Engine_CameraShake((float)amount * 0.3f, 0.25f);
}

void Engine_Heal(int amount)
{
    p_stats.current_health += amount;
    if (p_stats.current_health > p_stats.max_health)
        p_stats.current_health = p_stats.max_health;
}

void Engine_ToggleHide(void)
{
    p_stats.is_hidden = !p_stats.is_hidden;
    current_state = p_stats.is_hidden ? STATE_HIDDEN : STATE_PLAYING;
}

// ─────────────────────────────────────────────────────────────────────
// KAN OVERLAY
// ─────────────────────────────────────────────────────────────────────
static void RenderBloodOverlay(void)
{
    if (p_stats.current_health >= p_stats.max_health) return;
    float ratio  = 1.0f - ((float)p_stats.current_health / (float)p_stats.max_health);
    int   cx     = buffer_width/2, cy = buffer_height/2;
    float max_sq = (buffer_width/2.0f)*(buffer_width/2.0f);
    for (int y = 0; y < buffer_height; y++) {
        for (int x = 0; x < buffer_width; x++) {
            float dx = (float)(x-cx), dy = (float)(y-cy);
            float f  = (dx*dx+dy*dy)/max_sq * ratio * 1.5f;
            if (f > 1.0f) f = 1.0f;
            if (f > 0.05f) {
                int     idx = y*buffer_width+x;
                uint32_t c  = offscreen_buffer[idx];
                uint8_t r = (uint8_t)(((c>>16)&0xFF) + (255-((c>>16)&0xFF))*f);
                uint8_t g = (uint8_t)(((c>> 8)&0xFF) * (1.0f-f));
                uint8_t b = (uint8_t)(((c    )&0xFF) * (1.0f-f));
                offscreen_buffer[idx] = ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// 2D GÜNCELLEMEsİ + RENDER
// ─────────────────────────────────────────────────────────────────────
void Engine_UpdateAndRender2D(float dt, float mouse_x, float mouse_y)
{
    if (!active_map) return;

    static int e_was_down = 0, sp_was_down = 0;
    int e_down  = (GetAsyncKeyState('E')    & 0x8000) != 0;
    int sp_down = (GetAsyncKeyState(VK_SPACE)& 0x8000) != 0;

    if (current_state == STATE_PLAYING || current_state == STATE_HIDDEN) {
        float cx = buffer_width/2.0f, cy2 = buffer_height/2.0f;
        p2d_angle = atan2f(mouse_y-cy2, mouse_x-cx);

        if (current_state == STATE_PLAYING) {
            float spd = 200.0f*dt;
            float dx  = 0, dy2 = 0;
            if (GetAsyncKeyState('W')&0x8000) dy2 -= spd;
            if (GetAsyncKeyState('S')&0x8000) dy2 += spd;
            if (GetAsyncKeyState('A')&0x8000) dx  -= spd;
            if (GetAsyncKeyState('D')&0x8000) dx  += spd;
            if (dx!=0&&dy2!=0) { float l=sqrtf(dx*dx+dy2*dy2); dx=dx/l*spd; dy2=dy2/l*spd; }
            if (!IsSolid(p2d_x+dx, p2d_y)) p2d_x+=dx;
            if (!IsSolid(p2d_x, p2d_y+dy2)) p2d_y+=dy2;
        }

        if (e_down && !e_was_down) {
            for (int i = 0; i < MAX_ENTITIES; i++) {
                Entity* ent = &entities[i];
                if (!ent->active || !ent->on_interact) continue;
                float ecx = ent->x+ent->width/2.0f, ecy = ent->y+ent->height/2.0f;
                float dist = sqrtf((ecx-p2d_x)*(ecx-p2d_x)+(ecy-p2d_y)*(ecy-p2d_y));
                if (dist < 80.0f) { ent->on_interact(ent); break; }
            }
        }

        for (int i = 0; i < MAX_ENTITIES; i++) {
            Entity* ent = &entities[i];
            if (!ent->active || ent->type != ENT_MONSTER) continue;
            if (current_state == STATE_HIDDEN) continue;
            float mx = ent->x+ent->width/2.0f, my = ent->y+ent->height/2.0f;
            float dist = sqrtf((p2d_x-mx)*(p2d_x-mx)+(p2d_y-my)*(p2d_y-my));
            if (dist < 300.0f) {
                float dir_x = (p2d_x-mx)/dist, dir_y = (p2d_y-my)/dist;
                float nx = ent->x+dir_x*ent->speed*dt;
                float ny = ent->y+dir_y*ent->speed*dt;
                if (!IsSolid(nx, ent->y)) ent->x=nx;
                if (!IsSolid(ent->x, ny)) ent->y=ny;
                if (dist < 40.0f) {
                    static float dmg_timer = 0.0f;
                    dmg_timer += dt;
                    if (dmg_timer > 1.0f) { Engine_TakeDamage(ent->damage); dmg_timer=0; }
                }
            }
        }
    }

    if (current_state == STATE_DIALOG && sp_down && !sp_was_down)
        current_state = p_stats.is_hidden ? STATE_HIDDEN : STATE_PLAYING;

    e_was_down  = e_down;
    sp_was_down = sp_down;

    Engine_Render2DWorld(active_map, active_map_w, active_map_h, t_floor, t_wall);

    float cam_x = p2d_x-(buffer_width/2.0f);
    float cam_y = p2d_y-(buffer_height/2.0f);

    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* ent = &entities[i];
        if (!ent->active) continue;
        int sx = (int)(ent->x-cam_x), sy = (int)(ent->y-cam_y);
        if (sx+ent->width<0||sx>buffer_width||sy+ent->height<0||sy>buffer_height) continue;

        if (ent->sprite) {
            for (int py = 0; py < (int)ent->height; py++) {
                int dy2 = sy+py; if(dy2<0||dy2>=buffer_height) continue;
                float v = (float)py/ent->height;
                int ty  = (int)(v*ent->sprite->height);
                for (int px2 = 0; px2 < (int)ent->width; px2++) {
                    int dx2 = sx+px2; if(dx2<0||dx2>=buffer_width) continue;
                    float u = (float)px2/ent->width;
                    int tx  = (int)(u*ent->sprite->width);
                    int pidx = (ty*ent->sprite->width+tx)*4;
                    if (ent->sprite->data[pidx+3]>128) {
                        uint8_t r=ent->sprite->data[pidx];
                        uint8_t g=ent->sprite->data[pidx+1];
                        uint8_t b=ent->sprite->data[pidx+2];
                        offscreen_buffer[dy2*buffer_width+dx2]=((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                    }
                }
            }
        } else {
            uint32_t c = (ent->type==ENT_MONSTER)?0xFF0000:(ent->type==ENT_HIDING_SPOT)?0x8B4513:
                         (ent->type==ENT_ITEM)?0xFFFF00:0x0000FF;
            Engine_DrawFillRect(sx, sy, (int)ent->width, (int)ent->height, c);
        }
    }

    RenderParticles2D();

    if (current_state != STATE_HIDDEN && current_state != STATE_DEAD) {
        int pcx = buffer_width/2, pcy = buffer_height/2;
        Engine_DrawFillRect(pcx-16, pcy-16, 32, 32, 0x00FF00);
    }

    Engine_Apply2DLighting(p2d_angle);
    RenderBloodOverlay();

    /* DİYALOG */
    if (current_state == STATE_DIALOG) {
        dialog_timer += dt;
        if (dialog_timer > 0.03f) { dialog_char_index++; dialog_timer = 0.0f; }

        /* Portrait varsa sol tarafa çiz */
        if (dialog_portrait && dialog_portrait->data) {
            int pw = 100, ph = 100;
            int px2 = 25, py = buffer_height-125;
            for (int ty2 = 0; ty2 < ph; ty2++) {
                int dy3 = py+ty2; if(dy3<0||dy3>=buffer_height) continue;
                for (int tx2 = 0; tx2 < pw; tx2++) {
                    int dx3 = px2+tx2; if(dx3<0||dx3>=buffer_width) continue;
                    float u = (float)tx2/pw, v = (float)ty2/ph;
                    int ttx = (int)(u*dialog_portrait->width);
                    int tty = (int)(v*dialog_portrait->height);
                    int pidx = (tty*dialog_portrait->width+ttx)*4;
                    if (dialog_portrait->data[pidx+3]>128) {
                        offscreen_buffer[dy3*buffer_width+dx3] =
                            ((uint32_t)dialog_portrait->data[pidx]<<16)|
                            ((uint32_t)dialog_portrait->data[pidx+1]<<8)|
                            dialog_portrait->data[pidx+2];
                    }
                }
            }
        }

        Engine_DrawFillRect(20, buffer_height-130, buffer_width-40, 110, 0x111111);
        Engine_DrawFillRect(20, buffer_height-130, buffer_width-40, 2,   0xAAAAAA);
        if (dialog_speaker)
            Engine_DrawText(dialog_speaker, 140, buffer_height-110, 0xFFFF00, 2, 999);
        if (dialog_text)
            Engine_DrawText(dialog_text, 140, buffer_height-70, 0xFFFFFF, 2, dialog_char_index);
    } else if (current_state == STATE_DEAD) {
        Engine_DrawFillRect(0,0,buffer_width,buffer_height, 0x550000);
        Engine_DrawText("OLDUN", buffer_width/2-40, buffer_height/2, 0xFF0000, 4, 999);
    }
}