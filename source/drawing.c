#include "drawing.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "sprites.h"
C3D_RenderTarget* left;
C3D_RenderTarget* right;
C3D_RenderTarget* bottom;
static u32 GetC2DAlignFlag(GFX_TEXT_ALIGN align) {
    switch (align) {
        case GFX_ALIGN_CENTER: return C2D_AlignCenter;
        case GFX_ALIGN_RIGHT:  return C2D_AlignRight;
        case GFX_ALIGN_LEFT:
        default:               return C2D_AlignLeft;
    }
}

void GFX_DrawImageAt(GFX_IMAGE* texture, float x, float y, float depth, C2D_ImageTint *tint, float scalex, float scaley) {
    C2D_DrawImageAt(texture->image, x, y, depth, tint, scalex, scaley);  
}

void GFX_DrawText(const GFX_TEXT* text, float x, float y, float depth, float scaleX, float scaleY, GFX_TEXT_ALIGN align, u32 color) {
    u32 flags = GetC2DAlignFlag(align) | C2D_WithColor;
    C2D_DrawText(&text->c2d_text, flags, x, y, depth, scaleX, scaleY, color);
}

void GFX_DrawTextWrapped(const GFX_TEXT* text, float x, float y, float depth, float scaleX, float scaleY, GFX_TEXT_ALIGN align, u32 color, float wrapWidth) {
    u32 flags = GetC2DAlignFlag(align) | C2D_WithColor | C2D_WordWrap;
    C2D_DrawText(&text->c2d_text, flags, x, y, depth, scaleX, scaleY, color, wrapWidth);
}

void GFX_DrawShadowedText(const GFX_TEXT* text, float x, float y, float depth, float scaleX, float scaleY, GFX_TEXT_ALIGN align, u32 color, u32 shadowColor) {
    static const float shadowOffsets[4][2] = {
        {0.0f, 1.8f}, {0.0f, -0.7f}, {-1.7f, 0.0f}, {1.8f, 0.0f}
    };

    for (int i = 0; i < 4; i++) {
        GFX_DrawText(text, x + shadowOffsets[i][0], y + shadowOffsets[i][1], depth, scaleX, scaleY, align, shadowColor);
    }
    GFX_DrawText(text, x, y, depth, scaleX, scaleY, align, color);
}

void GFX_DrawShadowedTextWrapped(const GFX_TEXT* text, float x, float y, float depth, float scaleX, float scaleY, GFX_TEXT_ALIGN align, u32 color, u32 shadowColor, float wrapWidth) {
    static const float shadowOffsets[4][2] = {
        {0.0f, 1.8f}, {0.0f, -0.7f}, {-1.7f, 0.0f}, {1.8f, 0.0f}
    };

    for (int i = 0; i < 4; i++) {
        GFX_DrawTextWrapped(text, x + shadowOffsets[i][0], y + shadowOffsets[i][1], depth, scaleX, scaleY, align, shadowColor, wrapWidth);
    }
    GFX_DrawTextWrapped(text, x, y, depth, scaleX, scaleY, align, color, wrapWidth);
}

void GFX_DrawRectSolid(float x, float y, float depth, int w, int h, u32 color){
    C2D_DrawRectSolid(x, y, depth, w, h, color);
}

void GFX_InitGfx(){
    gfxInitDefault();
    gfxSet3D(false);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    
    left = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    right = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
}

void GFX_BeginSceneTop_WithColor(int side, u32 color){
    switch(side){
        case 0:
            C2D_SceneBegin(left);
            C2D_TargetClear(left, color);
            break;
        case 1:
            C2D_SceneBegin(right);
            C2D_TargetClear(right, color);
            break;
    }
}

void GFX_BeginSceneBottom_WithColor(u32 color){
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, color);
}

void GFX_BeginSceneTop(int side, bool should_clear){
    switch(side){
        case 0:
            C2D_SceneBegin(left);
            if (should_clear) {
                C2D_TargetClear(left, C2D_Color32(0, 0, 0, 255));
            }
            break;
        case 1:
            C2D_SceneBegin(right);
            if (should_clear) {
                C2D_TargetClear(right, C2D_Color32(0, 0, 0, 255));
            }
            break;
    }
}

void GFX_BeginSceneBottom(){
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
}

void GFX_RenderFrame(){
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    sceneManagerRender();
    C3D_FrameEnd(0);
}

void GFX_DrawRectangle(float x, float y, float depth, float w, float h, u32 top_left, u32 top_right, u32 bottom_left, u32 bottom_right){
    C2D_DrawRectangle(x, y, depth, w, h, top_left, top_right, bottom_left, bottom_right);
}

void GFX_DrawCircleSolid(float x, float y, float depth, float radius, u32 color){
    C2D_DrawCircleSolid(x, y, depth, radius, color);         
}

void GFX_DrawLine(float x1, float y1, u32 color0, float x2, float y2, u32 color1, float thick, float depth){
    C2D_DrawLine(x1, y1, color0, x2, y2, color1, thick, depth);
}
typedef struct {
    float x, y;
    float speed;
    float radius;
    bool onTopScreen;
} Bubble;
#define MAX_BUBBLES 37
Bubble bubbles[MAX_BUBBLES];

void initBubbles() {
    for (int i = 0; i < MAX_BUBBLES; i++) {
        bubbles[i].x = rand() % 340;
        bubbles[i].y = 240;
        bubbles[i].speed = 0.5f + (rand() % 5) * 0.1f;
        bubbles[i].radius = 5 + (rand() % 10);
        bubbles[i].onTopScreen = false;
    }
}

void updateBubbles() {
    for (int i = 0; i < MAX_BUBBLES; i++) {
        bubbles[i].y -= bubbles[i].speed;

        // Switch to top screen when off bottom screen
        if (!bubbles[i].onTopScreen && bubbles[i].y < -bubbles[i].radius) {
            bubbles[i].onTopScreen = true;
            bubbles[i].y = 240;  // Start from bottom of top screen
        }

        // Reset completely when reaching a certain height on top screen
        if (bubbles[i].onTopScreen && bubbles[i].y < 120.0f) {
            bubbles[i].y = 270;
            bubbles[i].x = rand() % 340;
            bubbles[i].speed = 0.5f + (rand() % 5) * 0.1f;
            bubbles[i].radius = 5 + (rand() % 10);
            bubbles[i].onTopScreen = false;
        }
    }
}

void drawBubblesTop() {
    for (int i = 0; i < MAX_BUBBLES; i++) {
        if (bubbles[i].onTopScreen) {
            float alpha = (bubbles[i].y - 120.0f) / (240.0f - 120.0f); // 1.0 at y=240, 0.0 at y=120
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            u32 bubbleColor;
            #ifndef PYRKON
                bubbleColor = C2D_Color32(255, 255, 255, (u8)(alpha * 124)); // original alpha was 124
            #else
                bubbleColor = C2D_Color32(137, 90, 164, (u8)(alpha * 124)); // original alpha was 124
            #endif
            

            C2D_DrawCircle(bubbles[i].x + 40.0f, bubbles[i].y, 0.5f, bubbles[i].radius,
                bubbleColor, bubbleColor, bubbleColor, bubbleColor);
        }
    }
}

void drawBubblesBottom() {
    for (int i = 0; i < MAX_BUBBLES; i++) {
        if (!bubbles[i].onTopScreen) {
            u32 bubbleColor;
            #ifndef PYRKON
                bubbleColor = C2D_Color32(255, 255, 255, 124); // original alpha was 124
            #else
                bubbleColor = C2D_Color32(137, 90, 164, 124); // original alpha was 124
            #endif
            C2D_DrawCircle(bubbles[i].x, bubbles[i].y, 0.5f, bubbles[i].radius,
                bubbleColor, bubbleColor, bubbleColor, bubbleColor);
        }
    }
}


typedef struct {
    float x, y;
} WavePoint;

WavePoint wave[NUM_POINTS];
float phaseOffsets[NUM_POINTS]; // for local randomness

void initWaveOffsets() {
    srand(time(NULL)); // seed RNG
    for (int i = 0; i < NUM_POINTS; ++i) {
        phaseOffsets[i] = ((rand() % 1000) / 1000.0f) * 2.0f * M_PI;
    }
}



void updateWave(u64 tick)
{
    // convert ticks → seconds (or ms) once, for all the phases
    float time = (float)tick / CPU_TICKS_PER_MSEC / 1000.0f;

    const float t1 = time * 20.0f;
    const float t2 = time * 12.0f;
    const float t3 = time * 7.0f;
    const float waveFreq2 = WAVE_FREQUENCY * 2.0f;
    const float waveFreq05 = WAVE_FREQUENCY * 0.5f;

    for (int i = 0; i < NUM_POINTS; ++i) {
        float norm = (float)i / (NUM_POINTS - 1);
        float x = norm * SCREEN_WIDTH;
        float phase = x + phaseOffsets[i];

        float y = BASE_HEIGHT
                + sinf(WAVE_FREQUENCY * (phase + t1)) * WAVE_AMPLITUDE
                + sinf(waveFreq2 * (x + t2)) * (WAVE_AMPLITUDE * 0.5f)
                + sinf(waveFreq05 * (x + t3)) * (WAVE_AMPLITUDE * 0.3f);

        y += (((float)(rand() % 100) / 100.0f) - 0.5f) * 1.5f; // organic jitter

        wave[i].x = x;
        wave[i].y = y;
    }
}

void drawWaveFill() {
    u32 fillColor; 
    #ifndef PYRKON
        fillColor = C2D_Color32(255, 251, 122, 82);
    #else
        fillColor = C2D_Color32(76, 25, 102, 220);
    #endif

    for (int i = 1; i < NUM_POINTS; ++i) {
        float x0 = wave[i - 1].x, y0 = wave[i - 1].y;
        float x1 = wave[i].x,     y1 = wave[i].y;

        // One quad split into two triangles
        C2D_DrawTriangle(x0, y0, fillColor,
                         x1, y1, fillColor,
                         x0, SCREEN_HEIGHT, fillColor, 0.3f);

        C2D_DrawTriangle(x1, y1, fillColor,
                         x1, SCREEN_HEIGHT, fillColor,
                         x0, SCREEN_HEIGHT, fillColor, 0.3f);
    }
}
float x = 0.0f;
float y = 0.0f;
void drawKwadraty(){
    //updateBubbles();
    if (y > -40.0f) {
        x -= 0.25f;
        y -= 0.25f;
    } else {
        x = 0.0f;
        y = 0.0f;
    }

    GFX_DrawImageAt(bgtop, x, y, 0.0f, NULL, 1.0f, 1.0f);
}