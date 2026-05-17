#ifndef SCENE_QR_H
#define SCENE_QR_H
#include <stdint.h>
#include "konwencik_api.h"
void sceneQrInit(void);
void sceneQrUpdate(uint32_t kDown, uint32_t kHeld);
void sceneQrRender(void);
void sceneQrExit(void);

#endif
