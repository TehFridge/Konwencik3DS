#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include "scene_title.h"
#include "scene_manager.h"

#include "cwav_shit.h"
#include "sprites.h"
#include "drawing.h"
#include "data.h"


bool isScrolling = false;
float elapsed = 0.0f;
float duration = 7.0f; 
float deltaTime = 0.1f;
float currentY = 0.0f;
float timer = 0.0f;
static GFX_TEXTBUF tit_textBuf;
static GFX_TEXT titText;
void sceneTitleInit(void) {
    tit_textBuf = GFX_TextBufNew(4096);

    GFX_TextParse(&titText, tit_textBuf, "Wciśnij A");
    GFX_TextOptimize(&titText);
    #ifndef PYRKON
        playCwav(1, false);
    #else
        playCwav(4, true);
    #endif
    
    currentY = -2000.0f;
}
float easeOutQuad(float t, float start, float end, float duration) {
    t /= duration;
    return start + (end - start) * (1 - (1 - t) * (1 - t));
}
void sceneTitleUpdate(uint32_t kDown, uint32_t kHeld) {
    
    
    
    
    timer += 0.2f ;
    if (timer > 7.0f) {
        isScrolling = true;
    }

    if (isScrolling) {
        if (elapsed < duration) {
            currentY = easeOutQuad(elapsed, -400.0f, 0.0f, duration);
            elapsed += deltaTime;
        }
    }
    if (kDown & KEY_A) {
        if (timer > 7.0f) {
            sceneManagerSwitchTo(SCENE_QUERY);
        }
    }
}
void sceneTitleRender(void) {
    GFX_BeginSceneTop_WithColor(0, C2D_Color32(255, 255, 255, 255));
 
    GFX_DrawImageAt(logo3ds, 0.0f, currentY, 0.0f, NULL, 1.0f, 1.0f);
    GFX_BeginSceneBottom_WithColor(C2D_Color32(255, 255, 255, 255));
	
    GFX_DrawShadowedText(&titText, 160.0f, -currentY + 100.0f, 0.5f, 1.5f, 1.5f, GFX_ALIGN_CENTER, C2D_Color32(76, 25, 102, 220), C2D_Color32(0xff, 0xff, 0xff, 0xff));
}

void sceneTitleExit(void) {
    
}