#include "scene_manager.h"
#include "sprites.h"
#include "scene_init.h"
#include "scene_query.h"
#include "scene_event.h"
#include "scene_qr.h"
#include "scene_export.h"
#include "scene_intro.h"
#include "scene_title.h"
#include "scene_parser_debug.h"

bool debug;
SceneType currentScene = SCENE_NONE;

void sceneManagerInit(SceneType initialScene) {
    sceneManagerSwitchTo(initialScene);
}

void sceneManagerUpdate(uint32_t kDown, uint32_t kHeld) {
    switch (currentScene) {
        case SCENE_INIT: sceneInitUpdate(kDown, kHeld); break;
        case SCENE_QUERY: sceneQueryUpdate(kDown, kHeld); break;
        case SCENE_EVENT: sceneEventUpdate(kDown, kHeld); break;
        case SCENE_QR: sceneQrUpdate(kDown, kHeld); break;
        case SCENE_EXPORT: sceneExportUpdate(kDown, kHeld); break;
        case SCENE_TITLE: sceneTitleUpdate(kDown, kHeld); break;
        case SCENE_INTRO: sceneIntroUpdate(kDown, kHeld); break;
        case SCENE_PARSER_DEBUG: sceneParserDebugUpdate(kDown, kHeld); break;
        default: break;
    }
}

void sceneManagerRender(void) {
    switch (currentScene) {
        case SCENE_INIT: sceneInitRender(); break;
        case SCENE_QUERY: sceneQueryRender(); break;
        case SCENE_EVENT: sceneEventRender(); break;
        case SCENE_QR: sceneQrRender(); break;
        case SCENE_EXPORT: sceneExportRender(); break;
        case SCENE_TITLE: sceneTitleRender(); break;
        case SCENE_INTRO: sceneIntroRender(); break;
        case SCENE_PARSER_DEBUG: sceneParserDebugRender; break;
        default: break;
    }
}

void sceneManagerSwitchTo(SceneType nextScene) {
    
    switch (currentScene) {
        case SCENE_INIT: sceneInitExit(); break;
        case SCENE_QUERY: sceneQueryExit(); break;
        case SCENE_EVENT: sceneEventExit(); break;
        case SCENE_QR: sceneQrExit(); break;
        case SCENE_EXPORT: sceneExportExit(); break;
        case SCENE_TITLE: sceneTitleExit(); break;
        case SCENE_INTRO: sceneIntroExit(); break;
        case SCENE_PARSER_DEBUG: sceneParserDebugExit(); break;
        default: break;
    }


    currentScene = nextScene;
    switch (currentScene) {
        case SCENE_INIT: sceneInitInit(); break;;
        case SCENE_QUERY: sceneQueryInit(); break;
        case SCENE_EVENT: sceneEventInit(); break;
        case SCENE_QR: sceneQrInit(); break;
        case SCENE_EXPORT: sceneExportInit(); break;
        case SCENE_TITLE: sceneTitleInit(); break;
        case SCENE_INTRO: sceneIntroInit(); break;
        case SCENE_PARSER_DEBUG: sceneParserDebugInit(); break;
        default: break;
    }
}

void sceneManagerExit(void) {
    sceneManagerSwitchTo(SCENE_NONE);
}
