#ifndef SCENE_EVENT_H
#define SCENE_EVENT_H
#include <stdint.h>
#include "konwencik_api.h"

extern KonwencikProgramItem* program_db;
void Zapisz_Polubione();
void Wczytaj_Polubione();
void sceneEventInit(void);
void sceneEventUpdate(uint32_t kDown, uint32_t kHeld);
void sceneEventRender(void);
void sceneEventExit(void);

#endif
