#include "cwav_shit.h"
#include "logs.h"
#include <stdlib.h> 
#include <stdio.h> 
#include <3ds.h>
#include <citro2d.h> 

#define FILE_COUNT (sizeof(fileList) / sizeof(fileList[0]))

const char* fileList[] = {
    "romfs:/sfx/collectedall.bcwav", 
    "romfs:/bgm/bgm_app.bcwav",        
    "romfs:/sfx/change.bcwav",      
    "romfs:/sfx/go.bcwav",    
    "romfs:/bgm/pyr_app.bcwav",  
    "romfs:/bgm/intro.bcwav", 
};

const u8 maxSPlayList[] = {
    2, 2, 2, 2, 2, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1
};

typedef struct {
    const char* filename;
    CWAV* cwav;
    void* buffer;
    bool loaded;
    u32 size; 
} CWAVEntry;

static CWAVEntry cwavList[FILE_COUNT];

const char* cwavGetStatusString(cwavStatus_t status) {
    switch (status) {
        case CWAV_NOT_ALLOCATED:           return "CWAV_NOT_ALLOCATED";
        case CWAV_SUCCESS:                 return "CWAV_SUCCESS";
        case CWAV_INVALID_ARGUMENT:        return "CWAV_INVALID_ARGUMENT";
        case CWAV_FILE_OPEN_FAILED:        return "CWAV_FILE_OPEN_FAILED";
        case CWAV_FILE_READ_FAILED:        return "CWAV_FILE_READ_FAILED";
        case CWAV_UNKNOWN_FILE_FORMAT:     return "CWAV_UNKNOWN_FILE_FORMAT";
        case CWAV_INVAID_INFO_BLOCK:       return "CWAV_INVAID_INFO_BLOCK";
        case CWAV_INVAID_DATA_BLOCK:       return "CWAV_INVAID_DATA_BLOCK";
        case CWAV_UNSUPPORTED_AUDIO_ENCODING: return "CWAV_UNSUPPORTED_AUDIO_ENCODING";
        case CWAV_INVALID_CWAV_CHANNEL:    return "CWAV_INVALID_CWAV_CHANNEL";
        case CWAV_NO_CHANNEL_AVAILABLE:    return "CWAV_NO_CHANNEL_AVAILABLE";
        default:                           return "UNKNOWN_STATUS";
    }
}

bool loadCwavIndex(u32 index) {
    if (index >= FILE_COUNT) return false;

    CWAVEntry* entry = &cwavList[index];
    if (entry->loaded) {
        log_to_file("[CWAV] Index %d already loaded at PTR: %p\n", index, entry->cwav);
        return true; 
    }

    FILE* file = fopen(entry->filename, "rb");
    if (!file) {
        log_to_file("[WARN] Failed to open %s\n", entry->filename);
        return false;
    }

    fseek(file, 0, SEEK_END);
    u32 fileSize = ftell(file);
    rewind(file);

    void* buffer = linearAlloc(fileSize);
    if (!buffer) {
        fclose(file);
        log_to_file("[WARN] linearAlloc failed for %s (Size: %lu)\n", entry->filename, fileSize);
        return false;
    }
    
    linear_bytes_used += fileSize;
    entry->size = fileSize; 

    fread(buffer, 1, fileSize, file);
    fclose(file);

    CWAV* cwav = (CWAV*)calloc(1, sizeof(CWAV)); 
    if (!cwav) {
        linearFree(buffer);
        log_to_file("[WARN] malloc failed for CWAV struct %s\n", entry->filename);
        return false;
    }

    cwavLoad(cwav, buffer, maxSPlayList[index]);
    

    log_to_file("[CWAV] Load '%s' status: %s | Buffer: %p | Struct: %p\n", 
                entry->filename, 
                cwavGetStatusString(cwav->loadStatus),
                buffer,
                cwav);

    if (cwav->loadStatus != CWAV_SUCCESS) {
        log_to_file("[WARN] Load Status Failed for %s\n", entry->filename);
        cwavFree(cwav);
        free(cwav);
        
        linearFree(buffer); 
        linear_bytes_used -= fileSize; 
        
        return false;
    }

    entry->cwav = cwav;
    entry->buffer = buffer;
    entry->loaded = true;

    return true;
}

void unloadCwavIndex(u32 index) {
    if (index >= FILE_COUNT) return;
    CWAVEntry* entry = &cwavList[index];
    if (!entry->loaded) return;

    if (entry->cwav) {
        cwavStop(entry->cwav, -1, -1);    
        svcSleepThread(60 * 1000 * 1000); // Wait 60ms for DSP to let go

        // Now safe to free internal structs because dataBuffer is NULL
        cwavFree(entry->cwav); 
        free(entry->cwav);
    }
    
    if (entry->buffer) {
        linearFree(entry->buffer); // We manually free the buffer here
        linear_bytes_used -= entry->size;
    }

    entry->cwav = NULL;
    entry->buffer = NULL;
    entry->loaded = false;
    entry->size = 0;
}

void initCwavSystem(void) {
    for (u32 i = 0; i < FILE_COUNT; i++) {
        cwavList[i].filename = fileList[i];
        cwavList[i].cwav = NULL;
        cwavList[i].buffer = NULL;
        cwavList[i].loaded = false;
        cwavList[i].size = 0; 
    }
    log_to_file("[INFO] CWAV system initialized (%d entries)\n", FILE_COUNT);
}

void freeAllCwavs(void) {
    for (u32 i = 0; i < FILE_COUNT; i++) {
        unloadCwavIndex(i);
    }
    log_to_file("[INFO] All CWAVs unloaded.\n");
}

CWAV* getCwav(u32 index) {
    if (index >= FILE_COUNT) return NULL;
    if (!cwavList[index].loaded) loadCwavIndex(index);
    return cwavList[index].cwav;
}

void playCwav(u32 index, bool stereo) {
    CWAV* cwav = getCwav(index);
    
    log_to_file("[CWAV] Request Play Index: %d | PTR: %p\n", index, cwav);

    if (cwav) {
        
        if (cwav->loadStatus != CWAV_SUCCESS) {
             log_to_file("[ERR] Attempted to play invalid CWAV (Status: %s)\n", cwavGetStatusString(cwav->loadStatus));
             return;
        }

        cwavPlayResult result;

        if (stereo)
            result = cwavPlay(cwav, 0, 1);
        else
            result = cwavPlay(cwav, 0, -1);
        
        log_to_file("[CWAV] Play '%s' Result: %s (L:%d R:%d)\n", 
                    fileList[index], 
                    cwavGetStatusString(result.playStatus),
                    result.monoLeftChannel,
                    result.rightChannel);
        
    } else {
        log_to_file("[CWAV] Play failed: Index %d returned NULL pointer.\n", index);
    }
}

void stopCwav(u32 index) {
    if (index >= FILE_COUNT) return;
    CWAVEntry* entry = &cwavList[index];
    
    if (!entry->loaded || !entry->cwav) {
        log_to_file("[CWAV] Stop ignored: Index %d not loaded.\n", index);
        return;
    }

    cwavStop(entry->cwav, -1, -1);
    log_to_file("[CWAV] Stopped '%s' | PTR: %p\n", entry->filename, entry->cwav);
}