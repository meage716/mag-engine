#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────────────
// PENCERE & ANA DÖNGÜ
// ─────────────────────────────────────────────────────────────────────
void Engine_InitWindow(int width, int height, const char* title);
int  Engine_IsRunning(void);
void Engine_UpdateEvents(void);

typedef void (*OnUserUpdateFunc)(float fElapsedTime);
void Engine_Start(OnUserUpdateFunc update_callback);

bool Engine_GetKeyDown(int key_code);

// ─────────────────────────────────────────────────────────────────────
// ÇİZİM
// ─────────────────────────────────────────────────────────────────────
void Engine_ClearScreen(uint8_t r, uint8_t g, uint8_t b);
void Engine_DrawPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
void Engine_DrawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void Engine_DrawFillRect(int x, int y, int w, int h, uint32_t color);
void Engine_DrawBlendRect(int x, int y, int w, int h, uint32_t color, float alpha); /* YENİ */
void Engine_DrawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);
void Engine_DrawText(const char* text, int x, int y, uint32_t color, int scale, int max_chars);

// ─────────────────────────────────────────────────────────────────────
// TEXTURE
// ─────────────────────────────────────────────────────────────────────
typedef struct {
    int            width, height, channels;
    unsigned char* data;
    uint8_t*       heightmap;
} Texture;

Texture Engine_LoadTexture(const char* filepath);
void    Engine_FreeTexture(Texture* t);   /* YENİ */

// ─────────────────────────────────────────────────────────────────────
// 2D VEKTÖR
// ─────────────────────────────────────────────────────────────────────
typedef struct { float x, y; } Vec2;

Vec2  Vec2_Add(Vec2 a, Vec2 b);
Vec2  Vec2_Sub(Vec2 a, Vec2 b);
Vec2  Vec2_Mul(Vec2 v, float scalar);
Vec2  Vec2_Rotate(Vec2 v, float angle);
Vec2  Vec2_Normalize(Vec2 v);          /* YENİ */
float Vec2_Dot(Vec2 a, Vec2 b);        /* YENİ */
float Vec2_Length(Vec2 v);             /* YENİ */
float Vec2_Distance(Vec2 a, Vec2 b);   /* YENİ */

// ─────────────────────────────────────────────────────────────────────
// MATEMATİK ARAÇLARI  YENİ
// ─────────────────────────────────────────────────────────────────────
float Engine_Lerp(float a, float b, float t);
float Engine_Clamp(float v, float lo, float hi);
float Engine_MapRange(float v, float in_min, float in_max, float out_min, float out_max);

// ─────────────────────────────────────────────────────────────────────
// OYUNCU (3D)
// ─────────────────────────────────────────────────────────────────────
void Engine_InitPlayer(float start_x, float start_y);
void Engine_UpdatePlayer(float move_speed, float rot_speed, float fElapsedTime);
void Engine_DrawPlayerDebug(int size);
void Engine_DrawCameraDebug(int size);

// ─────────────────────────────────────────────────────────────────────
// DUVAR & LEVEL
// ─────────────────────────────────────────────────────────────────────
void Engine_AddWall(float x1, float y1, float x2, float y2,
                    float z_bottom, float z_top, Texture* tex, float tex_scale);
void Engine_ClearWalls(void);   /* YENİ */
int  Engine_LoadLevel(const char* filepath);  /* YENİ — .lvl dosyasından yükle */

// ─────────────────────────────────────────────────────────────────────
// 3D RENDERER
// ─────────────────────────────────────────────────────────────────────
void Engine_Render3D(void);
void Engine_Render3DFloor(Texture* floor_tex, Texture* ceil_tex);  /* YENİ */
void Engine_RenderSprites(void);
void Engine_DrawVectorMapDebug(void);
void Engine_CastVectorRaysDebug(void);

// ─────────────────────────────────────────────────────────────────────
// SPRITE & ANİMASYON
// ─────────────────────────────────────────────────────────────────────
typedef struct {
    float    x, y, z;
    Texture* tex;
    int      layers;
} Sprite;

void    Engine_AddSprite(float x, float y, float z, Texture* tex, int layers);
void    Engine_ClearSprites(void);  /* YENİ */

/* Sprite animasyon — YENİ */
int     Engine_CreateAnim(Texture* frames, int frame_count, float fps, int loop);
void    Engine_UpdateAnim(int anim_id, float dt);
Texture* Engine_GetAnimFrame(int anim_id);
void    Engine_ResetAnim(int anim_id);

// ─────────────────────────────────────────────────────────────────────
// PARTİKÜL SİSTEMİ  YENİ
// ─────────────────────────────────────────────────────────────────────
int  Engine_SpawnParticle(float x, float y, float z,
                          float vx, float vy, float vz,
                          uint32_t color, float lifetime,
                          float size, float gravity);
void Engine_SpawnBlood(float x, float y, float z, int count);
void Engine_SpawnSmoke(float x, float y, float z, int count);
void Engine_SpawnSpark(float x, float y, float z, int count);

// ─────────────────────────────────────────────────────────────────────
// KAMERA EFEKTLER  YENİ
// ─────────────────────────────────────────────────────────────────────
void Engine_CameraShake(float intensity, float duration);

// ─────────────────────────────────────────────────────────────────────
// FOG  YENİ
// ─────────────────────────────────────────────────────────────────────
void Engine_SetFog(float start_dist, float end_dist, uint8_t r, uint8_t g, uint8_t b);
void Engine_DisableFog(void);

// ─────────────────────────────────────────────────────────────────────
// NOKTA IŞIK  YENİ
// ─────────────────────────────────────────────────────────────────────
int  Engine_AddPointLight(float x, float y, float z, float radius,
                          uint8_t r, uint8_t g, uint8_t b, float intensity);
void Engine_RemovePointLight(int idx);
void Engine_MovePointLight(int idx, float x, float y, float z);
void Engine_ClearLights(void);

// ─────────────────────────────────────────────────────────────────────
// EKRAN GEÇİŞİ  YENİ
// ─────────────────────────────────────────────────────────────────────
void Engine_FadeOut(float duration, void (*on_complete)(void));
void Engine_FadeIn(float duration);

// ─────────────────────────────────────────────────────────────────────
// TIMER  YENİ
// ─────────────────────────────────────────────────────────────────────
int  Engine_SetTimer(float duration, int repeat, void (*callback)(void));
void Engine_CancelTimer(int id);

// ─────────────────────────────────────────────────────────────────────
// SİLAH
// ─────────────────────────────────────────────────────────────────────
void Engine_InitWeapon(void);
void Engine_UpdateAndRenderWeapon(float fElapsedTime);

// ─────────────────────────────────────────────────────────────────────
// SPRING FİZİK
// ─────────────────────────────────────────────────────────────────────
typedef struct {
    float pos, vel, target;
    float k, c, mass;
} Phys_Spring;

int   Phys_AddSpring(float stiffness, float damping, float mass);
void  Phys_SetSpringTarget(int index, float target_pos);
float Phys_GetSpringPos(int index);
void  Phys_UpdateSprings(float dt);

// ─────────────────────────────────────────────────────────────────────
// OYUN DURUMU & ENTİTY
// ─────────────────────────────────────────────────────────────────────
typedef enum {
    STATE_PLAYING, STATE_DIALOG, STATE_HIDDEN, STATE_INVENTORY, STATE_DEAD
} GameState;

typedef enum {
    ENT_NPC, ENT_ITEM, ENT_HIDING_SPOT, ENT_MONSTER, ENT_TRIGGER
} EntityType;

#define MAX_INVENTORY 16
typedef struct {
    int      active;
    char     name[32];
    Texture* icon;
} InventorySlot;

struct Entity;
typedef void (*InteractCallback)(struct Entity* self);

typedef struct Entity {
    int      active, id;
    EntityType type;
    float    x, y, width, height;
    int      is_solid;
    int      health, damage;
    float    speed;
    Texture* sprite;
    InteractCallback on_interact;
    char     name[32];
} Entity;

typedef struct {
    int           max_health, current_health, is_hidden;
    InventorySlot inventory[MAX_INVENTORY];
} PlayerStats;

extern PlayerStats p_stats;
extern GameState   current_state;
extern float       p2d_x, p2d_y, p2d_angle;

/* 3D oyuncu verileri -- dogrudan erisim */
extern Vec2  player_pos;
extern Vec2  player_dir;
extern float player_z;
extern int   player_pitch;

/* Shoot raycast: oyuncunun baktigi yonde maksimum max_dist mesafede
   bir hit noktasi arar. hit_x/hit_y hit noktasini, wall_idx hit duvari (-1 = yok) dondurur.
   Donus degeri: hit mesafesi, yok ise -1 */
float Engine_RaycastShoot(float* hit_x, float* hit_y, int* wall_idx);

/* Oyuncu ile verilen nokta arasinda cizgi gorus var mi? (duvar yok = 1) */
int   Engine_HasLineOfSight(float x1, float y1, float x2, float y2);

// ─────────────────────────────────────────────────────────────────────
// 2D DÜNYA
// ─────────────────────────────────────────────────────────────────────
void    Engine_InitLevel(int* map_data, int map_w, int map_h,
                         Texture* tex_floor, Texture* tex_wall);
Entity* Engine_AddEntity(EntityType type, float x, float y, float w, float h,
                         int is_solid, Texture* sprite, const char* name, InteractCallback cb);
void    Engine_RemoveEntity(Entity* ent);
void    Engine_StartDialog(const char* speaker, const char* text, Texture* portrait);
void    Engine_GiveItem(const char* item_name, Texture* icon);
void    Engine_TakeDamage(int amount);
void    Engine_Heal(int amount);
void    Engine_ToggleHide(void);
void    Engine_UpdateAndRender2D(float fElapsedTime, float mouse_x, float mouse_y);
void    Engine_Render2DWorld(int* map_data, int map_w, int map_h,
                             Texture* tex_floor, Texture* tex_wall);
void    Engine_Apply2DLighting(float mouse_angle);

#endif /* ENGINE_H */