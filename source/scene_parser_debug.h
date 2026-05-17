#ifndef SCENE_PARSER_DEBUG_H
#define SCENE_PARSER_DEBUG_H
#include <stdint.h>

void sceneParserDebugInit(void);
void sceneParserDebugUpdate(uint32_t kDown, uint32_t kHeld);
void sceneParserDebugRender(void);
void sceneParserDebugExit(void);

#endif
