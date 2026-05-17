#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quirc.h"

#include "scene_event.h"
#include "drawing.h"
#include "konwencik_api.h"

#define CAM_WIDTH  400
#define CAM_HEIGHT 240
#define CAM_SCREEN_SIZE (CAM_WIDTH * CAM_HEIGHT * 2)

static u8* cam_buffer = NULL;
static u32 cam_bufSize = 0;
static Handle camReceiveEvent = 0;

static struct quirc* qr_ctx = NULL;
static bool is_scanning = false;
static char scanned_payload[2048] = "";

static bool is_matching_event = false;
static int scanned_ids[2200];
static int scanned_id_count = 0;

static GFX_TEXTBUF textBuf;

static C3D_Tex cam_tex;
static C2D_Image cam_image;
static Tex3DS_SubTexture cam_subtex;



static Handle camErrorEvent = 0;
static u32 transferUnit = 0;

void sceneQrInit(void) {
    textBuf = GFX_TextBufNew(4096);
    camInit();

    CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
    CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
    CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_30);
    CAMU_SetNoiseFilter(SELECT_OUT1, true);
    CAMU_SetAutoExposure(SELECT_OUT1, true);
    CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
    CAMU_Activate(SELECT_OUT1);


    CAMU_GetBufferErrorInterruptEvent(&camErrorEvent, PORT_CAM1);
    CAMU_SetTrimming(PORT_CAM1, false);
    CAMU_GetMaxBytes(&transferUnit, 400, 240);
    CAMU_SetTransferBytes(PORT_CAM1, transferUnit, 400, 240);

    cam_bufSize = 400 * 240 * sizeof(u16);
    cam_buffer = linearAlloc(cam_bufSize);
    
    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_SetReceiving(&camReceiveEvent, cam_buffer, PORT_CAM1, cam_bufSize, (s16)transferUnit);
    CAMU_StartCapture(PORT_CAM1);

    C3D_TexInit(&cam_tex, 512, 256, GPU_RGB565);
    cam_subtex = (Tex3DS_SubTexture){ 400, 240, 0.0f, 1.0f, 400.0f/512.0f, 1.0f - (240.0f/256.0f) };
    cam_image = (C2D_Image){ &cam_tex, &cam_subtex };

    qr_ctx = quirc_new();
    quirc_resize(qr_ctx, 400, 240);
    is_scanning = true;
}


static u32 frame_timeout_counter = 0;

void sceneQrUpdate(uint32_t kDown, uint32_t kHeld) {
    if (kDown & KEY_B) { sceneManagerSwitchTo(SCENE_EVENT); return; }
    if (kDown & KEY_A) { is_scanning = true; }

    if (svcWaitSynchronization(camErrorEvent, 0) == 0) {
        if (camReceiveEvent) svcCloseHandle(camReceiveEvent);
        CAMU_ClearBuffer(PORT_CAM1);
        CAMU_SetReceiving(&camReceiveEvent, cam_buffer, PORT_CAM1, cam_bufSize, (s16)transferUnit);
        CAMU_StartCapture(PORT_CAM1);
        return;
    }

    if (camReceiveEvent != 0 && svcWaitSynchronization(camReceiveEvent, 0) == 0) {
        svcCloseHandle(camReceiveEvent);
        camReceiveEvent = 0;

        GSPGPU_InvalidateDataCache(cam_buffer, cam_bufSize);

        uint16_t* src = (uint16_t*)cam_buffer;
        uint16_t* dst = (uint16_t*)cam_tex.data;
        for(u32 y = 0; y < 240; y++) {
            for(u32 x = 0; x < 400; x++) {
                u32 dstPos = ((((y >> 3) * (512 >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3)));
                dst[dstPos] = src[y * 400 + x];
            }
        }
        GSPGPU_FlushDataCache(cam_tex.data, 512 * 256 * 2);

        if (is_scanning && qr_ctx) {
            int w, h;
            u8* q_buf = quirc_begin(qr_ctx, &w, &h);
            for (int i = 0; i < 400 * 240; i++) {
                u16 px = src[i];
                q_buf[i] = (u8)(((((px >> 11) & 0x1F) << 3) + (((px >> 5) & 0x3F) << 2) + ((px & 0x1F) << 3)) / 3);
            }
            quirc_end(qr_ctx);
            if (quirc_count(qr_ctx) > 0) {
                struct quirc_code code; struct quirc_data data;
                quirc_extract(qr_ctx, 0, &code);
                if (!quirc_decode(&code, &data)) {
                    strncpy(scanned_payload, (char*)data.payload, sizeof(scanned_payload)-1);
                    is_scanning = false;
                    
                    is_matching_event = false;
                    
                    if (global_selected_event != NULL) {
                        if (strncmp(scanned_payload, global_selected_event->id, strlen(global_selected_event->id)) == 0) {
                            is_matching_event = true;
                        }
                    }
                    
                    scanned_id_count = Konwencik_QRDaneDecode(scanned_payload, scanned_ids, 2200);

                    if (is_matching_event && program_db != NULL) {
                        Konwencik_ApplyLikesFromQR(scanned_ids, scanned_id_count, program_db, program_count);
                        Zapisz_Polubione();
                    }
                }
            }
        }

        CAMU_SetReceiving(&camReceiveEvent, cam_buffer, PORT_CAM1, cam_bufSize, (s16)transferUnit);
    }
}
void sceneQrRender(void)
{
    GFX_BeginSceneTop(0, true);
    C2D_DrawImageAt(cam_image, 0, 0, 0.5f, NULL, 1.0f, 1.0f);
    C2D_DrawRectSolid(10 + (rand()%5), 10, 1.0f, 20, 20, C2D_Color32(255, 0, 0, 255));

    GFX_BeginSceneBottom();
    GFX_TextBufClear(textBuf);

    GFX_DrawRectSolid(0, 0, 0.4f, 320, 240,
        GFX_COLOR_RGBA(25,25,25,255));

    GFX_TEXT t;

    if (is_scanning)
    {
        GFX_TextParse(&t, textBuf,
            "Skanowanie kodu QR...\n(R) - Export planu.");
        GFX_DrawTextWrapped(&t, 10, 20, 0.6f,
            0.5f,0.5f,GFX_ALIGN_LEFT,
            GFX_COLOR_RGBA(200,200,200,255),300);
    }
    else
    {
        GFX_TextParse(&t, textBuf, "Znaleziono QR!");
        GFX_DrawTextWrapped(&t, 10, 20, 0.6f,
            0.6f,0.6f,GFX_ALIGN_LEFT,
            GFX_COLOR_RGBA(60,255,60,255),300);


        char status_text[128];
        snprintf(status_text, sizeof(status_text), 
                 "Zgodny event: %s\nZnalezione ID: %d", 
                 is_matching_event ? "TAK" : "NIE", 
                 scanned_id_count > 0 ? scanned_id_count : 0);
                 
        GFX_TEXT st;
        GFX_TextParse(&st, textBuf, status_text);
        
        u32 status_color = is_matching_event ? GFX_COLOR_RGBA(60,255,60,255) : GFX_COLOR_RGBA(255,60,60,255);
        
        GFX_DrawTextWrapped(&st, 10, 100, 0.5f,
            0.5f,0.5f,GFX_ALIGN_LEFT,
            status_color, 300);

        GFX_TextParse(&t, textBuf,
            "Wciśnij (A) by ponownie zeskanować\nWciśnij (B) by wyjść");
        GFX_DrawTextWrapped(&t, 10, 180, 0.5f,
            0.4f,0.4f,GFX_ALIGN_LEFT,
            GFX_COLOR_RGBA(150,150,150,255),300);
    }
}


void sceneQrExit(void) {
    CAMU_StopCapture(PORT_CAM1);
    
    bool busy = false;
    while(R_SUCCEEDED(CAMU_IsBusy(&busy, PORT_CAM1)) && busy) svcSleepThread(1000000);

    if (camReceiveEvent) svcCloseHandle(camReceiveEvent);
    if (camErrorEvent) svcCloseHandle(camErrorEvent);
    
    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_Activate(SELECT_NONE);
    camExit();

    if (qr_ctx) quirc_destroy(qr_ctx);
    if (cam_buffer) linearFree(cam_buffer);
    C3D_TexDelete(&cam_tex);
    GFX_TextBufDelete(textBuf);
}