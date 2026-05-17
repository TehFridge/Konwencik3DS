#ifndef SCENE_QUERY_H
#define SCENE_QUERY_H
#include <stdint.h>
#include "konwencik_api.h"

extern KonwencikEvent* db;
void sceneQueryInit(void);
void sceneQueryUpdate(uint32_t kDown, uint32_t kHeld);
void sceneQueryRender(void);
void sceneQueryExit(void);

#endif
