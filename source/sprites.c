#include "sprites.h"
#include <3ds.h>
#include <stdlib.h> 
#include "main.h"


extern size_t linear_bytes_used;



GFX_IMAGE* test;
GFX_IMAGE* bgtop;
GFX_IMAGE* fridge_image;
GFX_IMAGE* logo3ds;

u64 lastFrameTime = 0;
int currentFrame = 0;


void spritesInit() {
    #ifndef PYRKON
        bgtop = GFX_LoadTexture("romfs:/gfx/bg.t3x", 0); 
    #else
        bgtop = GFX_LoadTexture("romfs:/gfx/pyrbg.t3x", 0);
    #endif
    srand(osGetTime());

    if (rand() % 2 == 0) {
        fridge_image = GFX_LoadTexture("romfs:/gfx/tehfridge2.t3x", 0);
    } else {
        fridge_image = GFX_LoadTexture("romfs:/gfx/tehfridge.t3x", 0);
    }
    #ifndef PYRKON
        logo3ds = GFX_LoadTexture("romfs:/gfx/logo.t3x", 1);
    #endif

    #ifdef PYRKON
        logo3ds = GFX_LoadTexture("romfs:/gfx/logo.t3x", 0);
    #endif
}