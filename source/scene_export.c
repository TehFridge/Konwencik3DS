#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "scene_export.h"
#include "scene_query.h"
#include "scene_event.h"
#include "scene_manager.h"
#include "main.h"
#include "sprites.h"
#include "data.h"
#include "drawing.h"
#include "konwencik_api.h"
#include "qrcodegen.h"

#define QR_BORDER 2
#define QR_SCALE 4.0f

static GFX_TEXTBUF textBuf;
static GFX_TEXT ziomekText;

static C3D_Tex qrTex;
static Tex3DS_SubTexture qrSubTex;
static GFX_IMAGE qrImage;
static bool qrReady = false;

static uint8_t qrBuffer[qrcodegen_BUFFER_LEN_MAX];
static uint8_t qrTempBuffer[qrcodegen_BUFFER_LEN_MAX];
static char qrPayload[512];


static inline u32 mortonInterleave(u32 x, u32 y)
{
    u32 z = 0;
    for (u32 i = 0; i < 3; i++) {
        z |= ((x & (1 << i)) << i) | ((y & (1 << i)) << (i + 1));
    }
    return z;
}

static void swizzleRGBA8(u32* dst, const u32* src, int width, int height)
{
    int blocksPerRow = width / 8;
    
    for (int by = 0; by < height / 8; by++) {
        for (int bx = 0; bx < width / 8; bx++) {
            int blockIndex = by * blocksPerRow + bx;
            
            for (int py = 0; py < 8; py++) {
                for (int px = 0; px < 8; px++) {
                    int srcIndex = (by * 8 + py) * width + (bx * 8 + px);
                    int dstIndex = (blockIndex * 64) + mortonInterleave(px, py); 
                    
                    dst[dstIndex] = src[srcIndex];
                }
            }
        }
    }
}

static void BuildQRFromCurrentEvent(void)
{
    if (!global_selected_event || !program_db || program_count == 0) {
        qrPayload[0] = '\0';
        return;
    }

    int liked_ids[256];
    int liked_count = 0;

    for (size_t i = 0; i < program_count; i++) {
        if (program_db[i].liked) {
            liked_ids[liked_count++] = program_db[i].id;
        }
    }

    if (liked_count == 0) {
        qrPayload[0] = '\0';
        return;
    }

    Konwencik_QRDaneEncode(
        global_selected_event->code,
        liked_ids,
        liked_count,
        qrPayload,
        sizeof(qrPayload)
    );
}


static void GenerateQRTexture(void)
{
    if (strlen(qrPayload) == 0)
        return;

    bool ok = qrcodegen_encodeText(
        qrPayload,
        qrTempBuffer,
        qrBuffer,
        qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true
    );

    if (!ok)
        return;

    int size = qrcodegen_getSize(qrBuffer);
    int texSize = size + QR_BORDER * 2;

    int texSizePow2 = 64; 
    while (texSizePow2 < texSize) {
        texSizePow2 *= 2;
    }

    if (qrReady) {
        C3D_TexDelete(&qrTex);
        qrReady = false;
    }

    if (!C3D_TexInit(&qrTex, texSizePow2, texSizePow2, GPU_RGBA8))
        return;

    C3D_TexSetFilter(&qrTex, GPU_NEAREST, GPU_NEAREST);

    u32* linear = malloc(texSizePow2 * texSizePow2 * sizeof(u32));
    if (!linear) {
        C3D_TexDelete(&qrTex);
        return;
    }

    for (int y = 0; y < texSizePow2; y++) {
        for (int x = 0; x < texSizePow2; x++) {
            if (x < texSize && y < texSize) {
                int qrX = x - QR_BORDER;
                int qrY = y - QR_BORDER;
                bool black = (qrX >= 0 && qrY >= 0 && qrX < size && qrY < size) ?
                              qrcodegen_getModule(qrBuffer, qrX, qrY) : false;
                
                linear[y * texSizePow2 + x] = black ? 0xFF000000 : 0xFFFFFFFF;
            } else {
                linear[y * texSizePow2 + x] = 0xFFFFFFFF; 
            }
        }
    }

    u32* gpuPtr = (u32*)qrTex.data; 

    if (gpuPtr != NULL) {
        swizzleRGBA8(gpuPtr, linear, texSizePow2, texSizePow2);
    }
    
    free(linear);
    C3D_TexFlush(&qrTex); 
    float ratio = (float)texSize / texSizePow2;
    
    float offset = 0.5f / texSizePow2;

    qrSubTex.width  = texSize;
    qrSubTex.height = texSize;
    
    qrSubTex.left   = offset;
    qrSubTex.right  = ratio - offset;

    qrSubTex.top    = 1.0f - offset;
    qrSubTex.bottom = 1.0f - ratio + offset;

    qrImage.sheet = NULL;
    qrImage.image.tex = &qrTex;
    qrImage.image.subtex = &qrSubTex;
    qrImage.width = texSize;
    qrImage.height = texSize;
    qrImage.cacheIndex = -1;

    qrReady = true;
}

void sceneExportInit(void)
{
    textBuf = GFX_TextBufNew(4096);

    GFX_TextParse(&ziomekText, textBuf,
        "Kolega stworzył świetny plan konwentu i chcesz razem z nim chodzić na te same punkty programu?\n"
        "A może przygotowałeś swój idealny plan na tablecie, a chcesz go teraz przenieść na 3DSa?\n"
        "To proste - wystarczy zeskanować kod QR!");
    GFX_TextOptimize(&ziomekText);

    memset(qrPayload, 0, sizeof(qrPayload));

    BuildQRFromCurrentEvent();
    GenerateQRTexture();
}

void sceneExportUpdate(uint32_t kDown, uint32_t kHeld)
{
    if (kDown & KEY_B) {
        sceneManagerSwitchTo(SCENE_EVENT);
        return;
    }
}

void sceneExportRender(void)
{
    GFX_BeginSceneTop(0, true);

    if (qrReady) {
        float texSize = (float)qrImage.width; 
        

        float scale = floorf(240.0f / texSize);
        
        if (scale < 1.0f) {
            scale = 240.0f / texSize;
        }

        float drawSize = texSize * scale;
        float x = (400.0f - drawSize) / 2.0f;
        float y = (240.0f - drawSize) / 2.0f;

        GFX_DrawImageAt(
            &qrImage,
            x,
            y,
            0.5f,
            NULL,
            scale,
            scale
        );
    }

    GFX_BeginSceneBottom();
    GFX_DrawRectSolid(0, 0, 0.4f, 320, 240,
        GFX_COLOR_RGBA(25,25,25,255));

    GFX_DrawTextWrapped(
        &ziomekText,
        10,
        20,
        0.6f,
        0.45f,
        0.45f,
        GFX_ALIGN_LEFT,
        GFX_COLOR_RGBA(255,255,255,255),
        300
    );
}

void sceneExportExit(void)
{
    if (qrReady) {
        C3D_TexDelete(&qrTex);
        qrReady = false;
    }

    if (textBuf)
        GFX_TextBufDelete(textBuf);
}