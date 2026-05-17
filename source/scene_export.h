#ifndef SCENE_EXPORT_H
#define SCENE_EXPORT_H
#include <stdint.h>

void sceneExportInit(void);
void sceneExportUpdate(uint32_t kDown, uint32_t kHeld);
void sceneExportRender(void);
void sceneExportExit(void);

#endif
