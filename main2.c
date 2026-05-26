#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "engine.h"

#define HUD_H 72
#define HUD_H_F 72.0f
#define PLAY_TOP_F 82.0f

#define MAX_ENEMIES 100
#define MAX_PARTICLES 160

#define PLAYER_RADIUS 18.0f
#define PICKUP_RADIUS 15.0f
#define PLAYER_KEY_SPEED 1400.0f
#define PLAYER_MOUSE_FOLLOW 14.0f
#define PLAYER_KEY_ACCEL 18.0f
#define ENEMY_START_SPEED 440.0f
#define ENEMY_MAX_SPEED 1550.0f
#define HIGHSCORE_FILE "highscore.txt"

// Dinamik Ekran Cozunurlugu Icin Global Degiskenler
static int screen_w;
static int screen_h;
static float screen_w_f;
static float screen_h_f;

typedef enum
{
    MODE_TITLE,
    MODE_PLAYING,
    MODE_GAME_OVER
} GameMode;

typedef struct
{
    float x;
    float y;
    float vx;
    float vy;
    float radius;
} Player;

typedef struct
{
    int active;
    float x;
    float y;
    float vx;
    float vy;
    float radius;
    float speed;
} Enemy;

typedef struct
{
    float x;
    float y;
    float radius;
    float pulse;
} Pickup;

typedef struct
{
    int active;
    float x;
    float y;
    float vx;
    float vy;
    float life;
    float max_life;
    int size;
    uint32_t color;
} Particle;

typedef struct
{
    GameMode mode;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    Particle particles[MAX_PARTICLES];
    Pickup pickup;
    int score;
    int high_score;
    int new_high_score;
    int control_mode;
    int mouse_ready;
    float last_mouse_x;
    float last_mouse_y;
    float time_alive;
    float title_time;
    float spawn_timer;
    float game_over_timer;
} Game;

static Game global_game;

static uint32_t ColorRGB(unsigned int r, unsigned int g, unsigned int b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static int RoundFloatToInt(float value)
{
    int result;
    if (value >= 0.0f) { result = (int)(value + 0.5f); }
    else { result = (int)(value - 0.5f); }
    return result;
}

static float ClampFloat(float value, float min_value, float max_value)
{
    float result = value;
    if (result < min_value) { result = min_value; }
    if (result > max_value) { result = max_value; }
    return result;
}

static float RandomFloat(float min_value, float max_value)
{
    float t = (float)rand() / (float)RAND_MAX;
    return min_value + ((max_value - min_value) * t);
}

static int IsDown(int virtual_key)
{
    return ((GetAsyncKeyState(virtual_key) & 0x8000) != 0) ? 1 : 0;
}

static int PressedNow(int virtual_key, int* was_down)
{
    int is_down = IsDown(virtual_key);
    int result = ((is_down != 0) && (*was_down == 0)) ? 1 : 0;
    *was_down = is_down;
    return result;
}

static int GetMouseClientPosition(float* out_x, float* out_y)
{
    POINT point;
    HWND window = FindWindowA("KendiMotorSinifim", NULL);
    int result = 0;

    if ((window != NULL) && (GetCursorPos(&point) != 0))
    {
        if (ScreenToClient(window, &point) != 0)
        {
            *out_x = (float)point.x;
            *out_y = (float)point.y;
            result = 1;
        }
    }
    return result;
}

static int TextLength(const char* text)
{
    int result = 0;
    while (text[result] != '\0') { result++; }
    return result;
}

static void DrawTextCentered(const char* text, int center_x, int y, uint32_t color, int scale)
{
    int width = TextLength(text) * 8 * scale;
    Engine_DrawText(text, center_x - (width / 2), y, color, scale, 999);
}

static void DrawFilledCircle(int center_x, int center_y, int radius, uint32_t color)
{
    int radius_sq = radius * radius;
    for (int y = -radius; y <= radius; y++)
    {
        int y_sq = y * y;
        int span = (int)(sqrtf((float)(radius_sq - y_sq)) + 0.5f);
        Engine_DrawFillRect(center_x - span, center_y + y, (span * 2) + 1, 1, color);
    }
}

static void DrawGlowCircle(float x, float y, float radius, uint32_t core_color, uint32_t glow_a, uint32_t glow_b)
{
    int cx = RoundFloatToInt(x);
    int cy = RoundFloatToInt(y);
    int r0 = RoundFloatToInt(radius);
    int r1 = RoundFloatToInt(radius + 8.0f);
    int r2 = RoundFloatToInt(radius + 17.0f);

    DrawFilledCircle(cx, cy, r2, glow_b);
    DrawFilledCircle(cx, cy, r1, glow_a);
    DrawFilledCircle(cx, cy, r0, core_color);
    DrawFilledCircle(cx - (r0 / 4), cy - (r0 / 4), r0 / 3, ColorRGB(255u, 255u, 255u));
}

static int LoadHighScore(void)
{
    FILE* file = NULL;
    int result = 0;
    if ((fopen_s(&file, HIGHSCORE_FILE, "r") == 0) && (file != NULL))
    {
        int value = 0;
        if ((fscanf_s(file, "%d", &value) == 1) && (value > 0)) { result = value; }
        fclose(file);
    }
    return result;
}

static void SaveHighScore(int score)
{
    FILE* file = NULL;
    if ((fopen_s(&file, HIGHSCORE_FILE, "w") == 0) && (file != NULL))
    {
        fprintf_s(file, "%d\n", score);
        fclose(file);
    }
}

static void ClearEnemies(Game* game)
{
    for (int i = 0; i < MAX_ENEMIES; i++) { game->enemies[i].active = 0; }
}

static void ClearParticles(Game* game)
{
    for (int i = 0; i < MAX_PARTICLES; i++) { game->particles[i].active = 0; }
}

static void SpawnParticle(Game* game, float x, float y, float vx, float vy, float life, int size, uint32_t color)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active == 0)
        {
            game->particles[i].active = 1;
            game->particles[i].x = x;     game->particles[i].y = y;
            game->particles[i].vx = vx;   game->particles[i].vy = vy;
            game->particles[i].life = life; game->particles[i].max_life = life;
            game->particles[i].size = size; game->particles[i].color = color;
            return;
        }
    }
}

static void SpawnBurst(Game* game, float x, float y, uint32_t color, int count, float speed_min, float speed_max)
{
    for (int i = 0; i < count; i++)
    {
        float angle = RandomFloat(0.0f, 6.283185f);
        float speed = RandomFloat(speed_min, speed_max);
        float vx = cosf(angle) * speed;
        float vy = sinf(angle) * speed;
        float life = RandomFloat(0.22f, 0.62f);
        int size = 3 + (rand() % 5);
        SpawnParticle(game, x, y, vx, vy, life, size, color);
    }
}

static void SpawnPickup(Game* game)
{
    // GUVENLI ALAN (SAFE ZONE) EKLENDI
    // Toplar artik ekranin dibine yapisip kaybolamaz
    float safe_margin = 60.0f; 
    float min_x = PICKUP_RADIUS + safe_margin;
    float max_x = screen_w_f - PICKUP_RADIUS - safe_margin;
    float min_y = PLAY_TOP_F + PICKUP_RADIUS + safe_margin;
    float max_y = screen_h_f - PICKUP_RADIUS - safe_margin;

    for (int attempt = 0; attempt < 40; attempt++)
    {
        float x = RandomFloat(min_x, max_x);
        float y = RandomFloat(min_y, max_y);
        float dx = x - game->player.x;
        float dy = y - game->player.y;
        float dist_sq = (dx * dx) + (dy * dy);

        if (dist_sq > 10000.0f)
        {
            game->pickup.x = x;
            game->pickup.y = y;
            game->pickup.radius = PICKUP_RADIUS;
            game->pickup.pulse = 0.0f;
            return;
        }
    }

    game->pickup.x = RandomFloat(min_x, max_x);
    game->pickup.y = RandomFloat(min_y, max_y);
    game->pickup.radius = PICKUP_RADIUS;
    game->pickup.pulse = 0.0f;
}

static int CountEnemies(const Game* game)
{
    int result = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (game->enemies[i].active != 0) { result++; }
    }
    return result;
}

static void SpawnEnemy(Game* game)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (game->enemies[i].active == 0)
        {
            Enemy* enemy = &game->enemies[i];
            int side = rand() % 4;
            float radius = RandomFloat(13.0f, 22.0f);
            float speed = ENEMY_START_SPEED + ((float)game->score * 7.0f) + (game->time_alive * 2.4f);
            float dx, dy, angle;

            if (speed > ENEMY_MAX_SPEED) { speed = ENEMY_MAX_SPEED; }

            enemy->active = 1;
            enemy->radius = radius;
            enemy->speed = speed;

            if (side == 0)
            {
                enemy->x = RandomFloat(radius, screen_w_f - radius);
                enemy->y = PLAY_TOP_F + radius;
            }
            else if (side == 1)
            {
                enemy->x = screen_w_f - radius;
                enemy->y = RandomFloat(PLAY_TOP_F + radius, screen_h_f - radius);
            }
            else if (side == 2)
            {
                enemy->x = RandomFloat(radius, screen_w_f - radius);
                enemy->y = screen_h_f - radius;
            }
            else
            {
                enemy->x = radius;
                enemy->y = RandomFloat(PLAY_TOP_F + radius, screen_h_f - radius);
            }

            dx = game->player.x - enemy->x;
            dy = game->player.y - enemy->y;
            angle = atan2f(dy, dx) + RandomFloat(-0.75f, 0.75f);
            enemy->vx = cosf(angle) * speed;
            enemy->vy = sinf(angle) * speed;
            return;
        }
    }
}

static void StartRound(Game* game)
{
    game->mode = MODE_PLAYING;
    game->score = 0;
    game->new_high_score = 0;
    game->time_alive = 0.0f;
    game->spawn_timer = 0.0f;
    game->game_over_timer = 0.0f;
    game->control_mode = 0;
    game->mouse_ready = 0;
    game->player.x = screen_w_f * 0.5f;
    game->player.y = PLAY_TOP_F + ((screen_h_f - PLAY_TOP_F) * 0.5f);
    game->player.vx = 0.0f;
    game->player.vy = 0.0f;
    game->player.radius = PLAYER_RADIUS;

    ClearEnemies(game);
    ClearParticles(game);
    SpawnPickup(game);

    for (int i = 0; i < 3; i++) { SpawnEnemy(game); }
}

static void InitGame(Game* game)
{
    game->mode = MODE_TITLE;
    game->score = 0;
    game->high_score = LoadHighScore();
    game->new_high_score = 0;
    game->control_mode = 0;
    game->mouse_ready = 0;
    game->last_mouse_x = screen_w_f * 0.5f;
    game->last_mouse_y = screen_h_f * 0.5f;
    game->time_alive = 0.0f;
    game->title_time = 0.0f;
    game->spawn_timer = 0.0f;
    game->game_over_timer = 0.0f;
    game->player.x = screen_w_f * 0.5f;
    game->player.y = PLAY_TOP_F + ((screen_h_f - PLAY_TOP_F) * 0.5f);
    game->player.vx = 0.0f;
    game->player.vy = 0.0f;
    game->player.radius = PLAYER_RADIUS;
    game->pickup.x = screen_w_f * 0.5f;
    game->pickup.y = screen_h_f * 0.5f;
    game->pickup.radius = PICKUP_RADIUS;
    game->pickup.pulse = 0.0f;

    ClearEnemies(game);
    ClearParticles(game);
}

static void TriggerGameOver(Game* game)
{
    if (game->mode == MODE_PLAYING)
    {
        game->mode = MODE_GAME_OVER;
        game->game_over_timer = 0.0f;
        SpawnBurst(game, game->player.x, game->player.y, ColorRGB(255u, 45u, 45u), 70, 80.0f, 520.0f);

        if (game->score > game->high_score)
        {
            game->high_score = game->score;
            game->new_high_score = 1;
            SaveHighScore(game->high_score);
        }
        else
        {
            game->new_high_score = 0;
        }
    }
}

static void UpdateParticles(Game* game, float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle* particle = &game->particles[i];
        if (particle->active != 0)
        {
            particle->life -= dt;
            if (particle->life <= 0.0f) { particle->active = 0; }
            else
            {
                particle->x += particle->vx * dt;
                particle->y += particle->vy * dt;
                particle->vx *= 1.0f - (2.4f * dt);
                particle->vy *= 1.0f - (2.4f * dt);
            }
        }
    }
}

static void UpdatePlayer(Game* game, float dt)
{
    float mouse_x = game->player.x;
    float mouse_y = game->player.y;
    int mouse_valid = GetMouseClientPosition(&mouse_x, &mouse_y);
    int key_x = 0;
    int key_y = 0;
    int keyboard_active;
    float desired_vx;
    float desired_vy;

    if ((IsDown('A') != 0) || (IsDown(VK_LEFT) != 0)) { key_x -= 1; }
    if ((IsDown('D') != 0) || (IsDown(VK_RIGHT) != 0)) { key_x += 1; }
    if ((IsDown('W') != 0) || (IsDown(VK_UP) != 0)) { key_y -= 1; }
    if ((IsDown('S') != 0) || (IsDown(VK_DOWN) != 0)) { key_y += 1; }

    keyboard_active = ((key_x != 0) || (key_y != 0)) ? 1 : 0;

    if (mouse_valid != 0)
    {
        if (game->mouse_ready == 0)
        {
            game->last_mouse_x = mouse_x;
            game->last_mouse_y = mouse_y;
            game->mouse_ready = 1;
        }
        else
        {
            float mdx = mouse_x - game->last_mouse_x;
            float mdy = mouse_y - game->last_mouse_y;
            float mouse_move_sq = (mdx * mdx) + (mdy * mdy);

            if (mouse_move_sq > 9.0f) { game->control_mode = 0; }
            game->last_mouse_x = mouse_x;
            game->last_mouse_y = mouse_y;
        }
    }

    if (keyboard_active != 0) { game->control_mode = 1; }

    if ((game->control_mode == 0) && (mouse_valid != 0))
    {
        float target_x = ClampFloat(mouse_x, PLAYER_RADIUS, screen_w_f - PLAYER_RADIUS);
        float target_y = ClampFloat(mouse_y, PLAY_TOP_F + PLAYER_RADIUS, screen_h_f - PLAYER_RADIUS);
        float follow = PLAYER_MOUSE_FOLLOW * dt;

        if (follow > 1.0f) { follow = 1.0f; }

        game->player.vx = (target_x - game->player.x) * PLAYER_MOUSE_FOLLOW;
        game->player.vy = (target_y - game->player.y) * PLAYER_MOUSE_FOLLOW;
        game->player.x += (target_x - game->player.x) * follow;
        game->player.y += (target_y - game->player.y) * follow;
    }
    else
    {
        desired_vx = 0.0f;
        desired_vy = 0.0f;

        if (keyboard_active != 0)
        {
            float fx = (float)key_x;
            float fy = (float)key_y;
            float len = sqrtf((fx * fx) + (fy * fy));

            if (len > 0.0f)
            {
                desired_vx = (fx / len) * PLAYER_KEY_SPEED;
                desired_vy = (fy / len) * PLAYER_KEY_SPEED;
            }
        }

        {
            float accel = PLAYER_KEY_ACCEL * dt;
            if (accel > 1.0f) { accel = 1.0f; }
            game->player.vx += (desired_vx - game->player.vx) * accel;
            game->player.vy += (desired_vy - game->player.vy) * accel;
        }

        game->player.x += game->player.vx * dt;
        game->player.y += game->player.vy * dt;
    }

    game->player.x = ClampFloat(game->player.x, PLAYER_RADIUS, screen_w_f - PLAYER_RADIUS);
    game->player.y = ClampFloat(game->player.y, PLAY_TOP_F + PLAYER_RADIUS, screen_h_f - PLAYER_RADIUS);
}

static void UpdateEnemies(Game* game, float dt)
{
    int target_count = 3 + (game->score / 3) + (int)(game->time_alive / 8.0f);
    float spawn_interval = 0.78f - ((float)game->score * 0.018f);

    if (target_count > MAX_ENEMIES) { target_count = MAX_ENEMIES; }
    if (spawn_interval < 0.16f) { spawn_interval = 0.16f; }

    game->spawn_timer += dt;

    if ((CountEnemies(game) < target_count) && (game->spawn_timer >= spawn_interval))
    {
        SpawnEnemy(game);
        game->spawn_timer = 0.0f;
    }

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy* enemy = &game->enemies[i];
        if (enemy->active != 0)
        {
            float dx = game->player.x - enemy->x;
            float dy = game->player.y - enemy->y;
            float dist_sq = (dx * dx) + (dy * dy);
            float max_speed = enemy->speed;

            if (dist_sq > 0.0001f)
            {
                float dist = sqrtf(dist_sq);
                float homing = 12.0f + ((float)game->score * 0.45f);
                if (homing > 42.0f) { homing = 42.0f; }
                enemy->vx += (dx / dist) * homing * dt;
                enemy->vy += (dy / dist) * homing * dt;
            }

            {
                float speed_sq = (enemy->vx * enemy->vx) + (enemy->vy * enemy->vy);
                float max_speed_sq = max_speed * max_speed;

                if ((speed_sq > max_speed_sq) && (speed_sq > 0.0001f))
                {
                    float scale = max_speed / sqrtf(speed_sq);
                    enemy->vx *= scale;
                    enemy->vy *= scale;
                }
            }

            enemy->x += enemy->vx * dt;
            enemy->y += enemy->vy * dt;

            if ((enemy->x - enemy->radius) < 0.0f)
            {
                enemy->x = enemy->radius;
                enemy->vx = fabsf(enemy->vx);
            }
            if ((enemy->x + enemy->radius) > screen_w_f)
            {
                enemy->x = screen_w_f - enemy->radius;
                enemy->vx = -fabsf(enemy->vx);
            }
            if ((enemy->y - enemy->radius) < PLAY_TOP_F)
            {
                enemy->y = PLAY_TOP_F + enemy->radius;
                enemy->vy = fabsf(enemy->vy);
            }
            if ((enemy->y + enemy->radius) > screen_h_f)
            {
                enemy->y = screen_h_f - enemy->radius;
                enemy->vy = -fabsf(enemy->vy);
            }
        }
    }
}

static void UpdateCollisions(Game* game)
{
    float pickup_dx = game->player.x - game->pickup.x;
    float pickup_dy = game->player.y - game->pickup.y;
    float pickup_r = game->player.radius + game->pickup.radius;
    float pickup_dist_sq = (pickup_dx * pickup_dx) + (pickup_dy * pickup_dy);

    if (pickup_dist_sq <= (pickup_r * pickup_r))
    {
        game->score += 1;
        SpawnBurst(game, game->pickup.x, game->pickup.y, ColorRGB(42u, 255u, 123u), 34, 60.0f, 360.0f);
        SpawnPickup(game);
        game->spawn_timer += 0.20f;
    }

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy* enemy = &game->enemies[i];
        if (enemy->active != 0)
        {
            float dx = game->player.x - enemy->x;
            float dy = game->player.y - enemy->y;
            float radius = game->player.radius + enemy->radius;
            float dist_sq = (dx * dx) + (dy * dy);

            if (dist_sq <= (radius * radius))
            {
                TriggerGameOver(game);
                return;
            }
        }
    }
}

static void DrawBackground(const Game* game)
{
    int grid_offset = ((int)(game->title_time * 44.0f)) % 48;

    Engine_DrawFillRect(0, 0, screen_w, screen_h, ColorRGB(3u, 6u, 20u));

    for (int y = HUD_H + grid_offset; y < screen_h; y += 48)
    {
        Engine_DrawFillRect(0, y, screen_w, 1, ColorRGB(8u, 24u, 48u));
    }

    for (int x = grid_offset; x < screen_w; x += 48)
    {
        Engine_DrawFillRect(x, HUD_H, 1, screen_h - HUD_H, ColorRGB(8u, 24u, 48u));
    }

    Engine_DrawFillRect(0, 0, screen_w, HUD_H, ColorRGB(5u, 9u, 28u));
    Engine_DrawFillRect(0, HUD_H - 2, screen_w, 2, ColorRGB(0u, 229u, 255u));
}

static void DrawHUD(const Game* game)
{
    char text[128];

    sprintf_s(text, sizeof(text), "SKOR %d", game->score);
    Engine_DrawText(text, 24, 18, ColorRGB(42u, 255u, 123u), 2, 999);

    sprintf_s(text, sizeof(text), "EN YUKSEK %d", game->high_score);
    Engine_DrawText(text, 230, 18, ColorRGB(0u, 229u, 255u), 2, 999);

    // Duzenli Ortalama
    Engine_DrawText("YESIL TOPLA", screen_w - 560, 18, ColorRGB(42u, 255u, 123u), 2, 999);
    Engine_DrawText("KIRMIZIDAN KAC", screen_w - 280, 18, ColorRGB(255u, 45u, 45u), 2, 999);
}

static void DrawParticles(const Game* game)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const Particle* particle = &game->particles[i];
        if (particle->active != 0)
        {
            float life_t = particle->life / particle->max_life;
            int size = (int)(((float)particle->size * life_t) + 1.0f);
            int x = RoundFloatToInt(particle->x) - (size / 2);
            int y = RoundFloatToInt(particle->y) - (size / 2);
            Engine_DrawFillRect(x, y, size, size, particle->color);
        }
    }
}

static void DrawGameObjects(const Game* game)
{
    float pulse = (sinf(game->pickup.pulse * 7.0f) + 1.0f) * 0.5f;
    float pickup_radius = game->pickup.radius + (pulse * 4.0f);

    DrawGlowCircle(game->pickup.x, game->pickup.y, pickup_radius, ColorRGB(210u, 255u, 225u), ColorRGB(42u, 255u, 123u), ColorRGB(0u, 88u, 58u));

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        const Enemy* enemy = &game->enemies[i];
        if (enemy->active != 0)
        {
            DrawGlowCircle(enemy->x, enemy->y, enemy->radius, ColorRGB(255u, 225u, 225u), ColorRGB(255u, 45u, 45u), ColorRGB(90u, 0u, 20u));
        }
    }

    DrawGlowCircle(game->player.x, game->player.y, game->player.radius, ColorRGB(255u, 255u, 255u), ColorRGB(0u, 229u, 255u), ColorRGB(0u, 55u, 96u));
}

static void DrawTitle(const Game* game)
{
    char text[128];
    float bob = sinf(game->title_time * 3.0f) * 10.0f;
    // Ekrana Gore Dinamik Ortalama
    int cy = screen_h / 2;
    int y = cy - 180 + RoundFloatToInt(bob);

    DrawTextCentered("NEON REAKSIYON", screen_w / 2, y, ColorRGB(0u, 229u, 255u), 4);
    DrawTextCentered("YESIL TOPLA   KIRMIZIDAN KAC", screen_w / 2, y + 88, ColorRGB(255u, 255u, 255u), 2);

    sprintf_s(text, sizeof(text), "EN YUKSEK SKOR %d", game->high_score);
    DrawTextCentered(text, screen_w / 2, y + 138, ColorRGB(42u, 255u, 123u), 2);

    DrawTextCentered("BASLAMAK ICIN TIKLA VEYA SPACE", screen_w / 2, y + 214, ColorRGB(255u, 207u, 64u), 2);
    DrawTextCentered("FARE VEYA WASD ILE OYNA", screen_w / 2, y + 256, ColorRGB(0u, 229u, 255u), 1);
}

static void DrawGameOver(const Game* game)
{
    char text[128];
    int cy = screen_h / 2;

    Engine_DrawFillRect(0, 0, screen_w, screen_h, ColorRGB(18u, 0u, 12u));
    DrawParticles(game);
    DrawTextCentered("OYUN BITTI", screen_w / 2, cy - 146, ColorRGB(255u, 45u, 45u), 5);

    sprintf_s(text, sizeof(text), "SKOR %d", game->score);
    DrawTextCentered(text, screen_w / 2, cy - 34, ColorRGB(42u, 255u, 123u), 3);

    sprintf_s(text, sizeof(text), "EN YUKSEK %d", game->high_score);
    DrawTextCentered(text, screen_w / 2, cy + 26, ColorRGB(0u, 229u, 255u), 2);

    if (game->new_high_score != 0)
    {
        DrawTextCentered("YENI REKOR", screen_w / 2, cy + 84, ColorRGB(255u, 207u, 64u), 3);
    }

    DrawTextCentered("TEKRAR ICIN TIKLA VEYA SPACE", screen_w / 2, cy + 196, ColorRGB(255u, 255u, 255u), 2);
}

static void UpdateAndRender(float dt)
{
    static int space_was_down = 0;
    static int enter_was_down = 0;
    static int mouse_was_down = 0;
    Game* game = &global_game;
    int start_pressed = 0;

    if (dt < 0.0f) { dt = 0.0f; }
    if (dt > 0.05f) { dt = 0.05f; }

    game->title_time += dt;
    start_pressed |= PressedNow(VK_SPACE, &space_was_down);
    start_pressed |= PressedNow(VK_RETURN, &enter_was_down);
    start_pressed |= PressedNow(VK_LBUTTON, &mouse_was_down);

    if (game->mode == MODE_TITLE)
    {
        DrawBackground(game);
        DrawHUD(game);
        DrawTitle(game);

        if (start_pressed != 0) { StartRound(game); }
    }
    else if (game->mode == MODE_PLAYING)
    {
        game->time_alive += dt;
        game->pickup.pulse += dt;

        UpdatePlayer(game, dt);
        UpdateEnemies(game, dt);
        UpdateCollisions(game);
        UpdateParticles(game, dt);

        DrawBackground(game);
        DrawGameObjects(game);
        DrawParticles(game);
        DrawHUD(game);
    }
    else
    {
        game->game_over_timer += dt;
        UpdateParticles(game, dt);
        DrawGameOver(game);

        if ((game->game_over_timer > 0.35f) && (start_pressed != 0))
        {
            StartRound(game);
        }
    }
}

int main(void)
{
    
    RECT work_area;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work_area, 0);
    
    screen_w = work_area.right - work_area.left;
    screen_h = work_area.bottom - work_area.top;
    screen_w_f = (float)screen_w;
    screen_h_f = (float)screen_h;

    srand((unsigned int)GetTickCount());
    
 
    Engine_InitWindow(screen_w, screen_h, "Neon Reaksiyon");
    
    InitGame(&global_game);
    Engine_Start(UpdateAndRender);
    
    return 0;
}