#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <string.h> 
#include "scene_query.h"
#include "konwencik_api.h"
#include "drawing.h"
#include "data.h"
#include "cwav_shit.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define CARD_HEIGHT 70.0f
#define CARD_MARGIN 8.0f
#define CARD_WIDTH 380.0f
#define LIST_TOP_PADDING 10.0f

#define SCROLL_SPEED 300.0f
#define MAX_VISIBLE 8

KonwencikEvent* db = NULL;
static size_t total_events = 0;

static GFX_TEXTBUF textBuf;
static GFX_TEXTBUF detailBuf;


static GFX_TEXT* eventNameTexts = NULL;
static GFX_TEXT* eventDetailsTexts = NULL;

static float scrollY = 0.0f;
static float targetScrollY = 0.0f; 
static float maxScroll = 0.0f;
static int selectedIndex = 0;

static u64 lastTick = 0;
static float dt = 0.0f;

static bool parsed = false;

static float* cardFade = NULL;

static bool* posterLoaded = NULL;
static GFX_IMAGE** posters = NULL;
static int lastSelectedIndex = -1;

static float scrollVelocity = 0.0f;
#define SCROLL_FRICTION 0.92f
#define TOUCH_SCROLL_MULT 1.5f 

static bool touchActive = false;
static int lastTouchY = 0;

static void BuildEventTexts()
{
    eventNameTexts = malloc(sizeof(GFX_TEXT) * total_events);
    eventDetailsTexts = malloc(sizeof(GFX_TEXT) * total_events);
    cardFade = malloc(sizeof(float) * total_events);
    posters = calloc(total_events, sizeof(GFX_IMAGE*));

    for (size_t i = 0; i < total_events; i++)
    {
        char buffer[512];

        GFX_TextParse(&eventNameTexts[i], textBuf, db[i].name);
        GFX_TextOptimize(&eventNameTexts[i]);

        snprintf(buffer, sizeof(buffer),
            "%s - %s\n%s\n%s",
            db[i].start_day,
            db[i].end_day,
            db[i].location_text,
            db[i].price
        );

        GFX_TextParse(&eventDetailsTexts[i], textBuf, buffer);
        GFX_TextOptimize(&eventDetailsTexts[i]);

        cardFade[i] = 0.0f;
        posters[i] = NULL;
    }

    maxScroll = (total_events * (CARD_HEIGHT + CARD_MARGIN)) - 240.0f;
    if (maxScroll < 0) maxScroll = 0;
}

static void DrawCard(float x, float y, int index)
{
    if (index < 0 || index >= total_events) return;

    if (cardFade[index] < 1.0f)
        cardFade[index] += dt * 3.5f;

    if (cardFade[index] > 1.0f)
        cardFade[index] = 1.0f;

    u8 alpha = (u8)(cardFade[index] * 190);

    u32 bg = (index == selectedIndex)
        ? GFX_COLOR_RGBA(90, 160, 105, alpha)
        : GFX_COLOR_RGBA(40, 40, 40, alpha);

    u32 border = GFX_COLOR_RGBA(255, 255, 255, alpha / 4);
    u32 textColor = GFX_COLOR_RGBA(255,255,255,alpha);

    GFX_DrawRectSolid(x, y, 0.5f, CARD_WIDTH, CARD_HEIGHT, bg);

    GFX_DrawRectangle(x, y, 0.49f, CARD_WIDTH, 2, border, border, border, border);
    GFX_DrawRectangle(x, y + CARD_HEIGHT - 2, 0.49f, CARD_WIDTH, 2, border, border, border, border);
    GFX_DrawRectangle(x, y, 0.49f, 2, CARD_HEIGHT, border, border, border, border);
    GFX_DrawRectangle(x + CARD_WIDTH - 2, y, 0.49f, 2, CARD_HEIGHT, border, border, border, border);

    float nameScaleX = 0.45f;
    int nameLen = strlen(db[index].name);
    if (nameLen > 40) {
        nameScaleX = 0.75f * (28.0f / (float)nameLen);
        if (nameScaleX < 0.20f) nameScaleX = 0.20f; 
    }


    GFX_DrawTextWrapped(
        &eventNameTexts[index],
        x + 10,
        y + 8,
        0.6f,
        nameScaleX, 
        0.45f,      
        GFX_ALIGN_LEFT,
        textColor,
        1000.0f 
    );

    GFX_DrawTextWrapped(
        &eventDetailsTexts[index],
        x + 10,
        y + 8 + 16,
        0.6f,
        0.45f,
        0.45f,
        GFX_ALIGN_LEFT,
        textColor,
        CARD_WIDTH - 80
    );
}

bool playedbgm = false;

void sceneQueryInit(void)
{
    FS_CreateKonwencikBase();
        
    textBuf = GFX_TextBufNew(16384);
    detailBuf = GFX_TextBufNew(4096);
    GFX_TextBufClear(textBuf);

    lastTick = svcGetSystemTick();

    if (db == NULL) {
        parsed = false;
        
        
        const char* base_cache_path = "/3ds/Konwencik3DS/conference.json";
        
        
        if (osGetWifiStrength() > 0) {
            
            konwencik_baza.done = false;
            konwencik_baza.size = 0;
            if (konwencik_baza.data) {
                free(konwencik_baza.data);
                konwencik_baza.data = NULL;
            }
            getKonwencikBaza();
        } else {
            
            
            FILE* f = fopen(base_cache_path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                if (size > 0) {
                    konwencik_baza.data = malloc(size + 1);
                    if (konwencik_baza.data) {
                        size_t read_bytes = fread(konwencik_baza.data, 1, size, f);
                        konwencik_baza.data[read_bytes] = '\0';
                        konwencik_baza.size = read_bytes;
                        konwencik_baza.done = true;
                    }
                }
                fclose(f);
            }
            
            
            if (!konwencik_baza.done) {
                getKonwencikBaza();
            }
        }
    } else {
        parsed = true;
        BuildEventTexts();
    }
    
    scrollY = 0;
    targetScrollY = 0; 
    selectedIndex = 0;
    lastSelectedIndex = -1; 
}

static float stickTimer = 0.0f;
#define STICK_THRESHOLD 40
#define STICK_REPEAT_DELAY 0.12f 

void sceneQueryUpdate(uint32_t kDown, uint32_t kHeld)
{
    u64 now = svcGetSystemTick();
    dt = (float)(now - lastTick) / CPU_TICKS_PER_MSEC / 1000.0f;
    lastTick = now;

    updateWave(now * 3.0f); 
    if (!parsed && konwencik_baza.done)
    {
        if (konwencik_baza.data != NULL && konwencik_baza.size > 0)
        {
            
            const char* base_cache_path = "sdmc:/3ds/Konwencik3DS/conference.json";
            
            
            const char* real_path = base_cache_path;
            if (strncmp(base_cache_path, "sdmc:", 5) == 0) {
                real_path = base_cache_path + 5;
            }

            int fd = open(real_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd >= 0) {
                write(fd, konwencik_baza.data, konwencik_baza.size);
                fsync(fd); 
                close(fd);
            }
        }

        db = KonwencikBazaParser(konwencik_baza.data, &total_events);
        BuildEventTexts();
        parsed = true;
    }

    if (!parsed) return;


    circlePosition pos;
    hidCircleRead(&pos);

    stickTimer -= dt;
    
    bool moveUp = pos.dy > STICK_THRESHOLD || (kDown & KEY_UP) || (kHeld & KEY_UP);
    bool moveDown = pos.dy < -STICK_THRESHOLD || (kDown & KEY_DOWN) || (kHeld & KEY_DOWN);

    if (moveUp) {
        if (stickTimer <= 0) {
            selectedIndex--;
            playCwav(2, true);
            stickTimer = STICK_REPEAT_DELAY; 
        }
    } else if (moveDown) {
        if (stickTimer <= 0) {
            selectedIndex++;
            playCwav(2, true);
            stickTimer = STICK_REPEAT_DELAY;
        }
    } else {
        stickTimer = 0; 
    }

    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= (int)total_events) selectedIndex = total_events - 1;


    float itemTop = (selectedIndex * (CARD_HEIGHT + CARD_MARGIN));
    float itemBottom = itemTop + CARD_HEIGHT;
    
    if (moveUp || moveDown) {
        if (itemTop < targetScrollY) 
            targetScrollY = itemTop;
        else if (itemBottom > targetScrollY + 240.0f - LIST_TOP_PADDING)
            targetScrollY = itemBottom - 240.0f + LIST_TOP_PADDING;
    }


    touchPosition touch;
    hidTouchRead(&touch);

    if (kDown & KEY_TOUCH) {
        touchActive = true;
        lastTouchY = touch.py;
    } else if ((kHeld & KEY_TOUCH) && touchActive) {
        float dy = touch.py - lastTouchY;
        targetScrollY -= dy * TOUCH_SCROLL_MULT; 
        lastTouchY = touch.py;
    } else {
        touchActive = false;
    }

    if (targetScrollY < 0) targetScrollY = 0;
    if (targetScrollY > maxScroll) targetScrollY = maxScroll;

    scrollY += (targetScrollY - scrollY) * 15.0f * dt;

    if (selectedIndex != lastSelectedIndex)
    {
        lastSelectedIndex = selectedIndex;
        if (selectedIndex >= 0 && selectedIndex < total_events && db[selectedIndex].poster_file_url != NULL)
        {
            if (posters[selectedIndex] == NULL)
                posters[selectedIndex] = GFX_LoadTexture(db[selectedIndex].poster_file_url, 0);
        }
    }

    if (kDown & KEY_A) {
        if (selectedIndex >= 0 && selectedIndex < total_events) {
            global_selected_event = &db[selectedIndex];
            event_program.done = false; 
            playCwav(3, true);
            sceneManagerSwitchTo(SCENE_EVENT);
            return; 
        }
    }
}

void sceneQueryRender(void)
{
    GFX_BeginSceneTop(0, true);
    GFX_BeginSceneBottom(); 
    GFX_BeginSceneTop(0, true); 
    if (parsed){
        drawKwadraty();
        drawWaveFill();
        drawBubblesTop();
    }
    
    if (!parsed)
    {
        float cx = 200.0f;
        float cy = 120.0f;
        float r = 15.0f;
        u64 ticks = svcGetSystemTick();
        int active = ((u64)(ticks / (CPU_TICKS_PER_MSEC * 100))) % 8;
        
        for (int i = 0; i < 8; i++) {
            float angle = i * (3.14159f / 4.0f);
            float px = cx + cosf(angle) * r;
            float py = cy + sinf(angle) * r;
            u32 color = (i == active) ? GFX_COLOR_RGBA(255, 255, 255, 255) : GFX_COLOR_RGBA(80, 80, 80, 255);
            GFX_DrawRectSolid(px - 2, py - 2, 0.5f, 4, 4, color);
        }
        return;
    }

    int firstVisible = (int)(scrollY / (CARD_HEIGHT + CARD_MARGIN));
    int maxVisible = (240 / (CARD_HEIGHT + CARD_MARGIN)) + 3;
    
    for (int i = 0; i < maxVisible; i++)
    {
        int index = firstVisible + i;
        if (index >= total_events) break;

        float y = LIST_TOP_PADDING + (index * (CARD_HEIGHT + CARD_MARGIN)) - scrollY;

        if (y > 240 || y + CARD_HEIGHT < 0)
            continue;

        DrawCard(10.0f, y, index);
    }
    float viewHeight = 240.0f;
    float contentHeight = total_events * (CARD_HEIGHT + CARD_MARGIN);

    if (contentHeight > viewHeight)
    {
        float ratio = viewHeight / contentHeight;
        float barHeight = viewHeight * ratio;
        float barY = (scrollY / contentHeight) * viewHeight;

        GFX_DrawRectSolid(392, 0, 0.4f, 6, 240, GFX_COLOR_RGBA(255,255,255,30));
        GFX_DrawRectSolid(392, barY, 0.5f, 6, barHeight, GFX_COLOR_RGBA(255,255,255,120));
    }

    GFX_BeginSceneBottom();

    GFX_TextBufClear(detailBuf); 
    
    if (parsed){
        drawKwadraty();
        #ifndef PYRKON
            GFX_DrawRectSolid(0, 0, 0.4f, 320, 240, GFX_COLOR_RGBA(255, 251, 122, 89));
        #else
            GFX_DrawRectSolid(0, 0, 0.4f, 320, 240, GFX_COLOR_RGBA(76, 25, 102, 220));
        #endif
        drawBubblesBottom();
    }
    
    if (selectedIndex >= 0 && selectedIndex < total_events)
    {
        KonwencikEvent* e = &db[selectedIndex];

        GFX_TEXT titleText;
        GFX_TextParse(&titleText, detailBuf, e->name); 
        GFX_TextOptimize(&titleText);

     
        float bottomScaleX = 0.6f;
        int nameLen = strlen(e->name);
        if (nameLen > 40) { 
            bottomScaleX = 1.15f * (22.0f / (float)nameLen);
            if (bottomScaleX < 0.25f) bottomScaleX = 0.25f; 
        }


        GFX_DrawShadowedText(&titleText, 10, 8, 0.6f, bottomScaleX, 0.6f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,150));

        char infoBuf[512];
        snprintf(infoBuf, sizeof(infoBuf), "Data: %s %d\nLokalizacja: %s\nCena: %s", e->start_day, e->year, e->location_text, e->price);

        GFX_TEXT infoText;
        GFX_TextParse(&infoText, detailBuf, infoBuf);
        GFX_TextOptimize(&infoText);

        GFX_DrawShadowedTextWrapped(&infoText, 10, 60, 0.6f, 0.45f, 0.45f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,150), 300);
    }
}

void sceneQueryExit(void)
{

    if (eventNameTexts) {
        free(eventNameTexts);
        eventNameTexts = NULL;
    }

    if (eventDetailsTexts) {
        free(eventDetailsTexts);
        eventDetailsTexts = NULL;
    }

    if (cardFade) {
        free(cardFade);
        cardFade = NULL;
    }

    if (posters)
    {
        for (size_t i = 0; i < total_events; i++)
        {
            if (posters[i]) {
                GFX_FreeTexture(posters[i]);
                posters[i] = NULL;
            }
        }
        free(posters);
        posters = NULL;
    }

    GFX_TextBufDelete(textBuf);
    GFX_TextBufDelete(detailBuf); 
}