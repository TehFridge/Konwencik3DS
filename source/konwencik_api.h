#ifndef KONWENCIK_H
#define KONWENCIK_H

#include <citro3d.h>
#include <citro2d.h>
#include "request.h"

extern int program_count;
extern ResponseBuffer konwencik_baza;
extern ResponseBuffer event_stoisko_polubienia;
extern ResponseBuffer event_stoiska;
extern ResponseBuffer event_ogloszenia;
extern ResponseBuffer event_program;
extern ResponseBuffer event_goscie;
extern ResponseBuffer testreq;

typedef struct {
    int day;
    int month;
    int year;
    int index;
    char text[32]; 
} Dzien_wyd;

typedef struct {
    C3D_Tex tex;
    Tex3DS_SubTexture subtex;
    int width;
    int height;
    int x_offset;
    int y_offset;
} MapChunk;

typedef struct {
    MapChunk* chunks;
    int num_chunks_x;
    int num_chunks_y;
    int total_width;
    int total_height;
    bool loaded;
} MapkaData;

typedef struct {
    char* title; 
    char* file;  
    MapkaData runtime_data; // Holds the downloaded and split textures
} Mapka;

typedef struct {
    char id[64];              
    char code[64];
    char name[128];
    char start_day[16];     
    char end_day[16];
    int year;
    char location_text[128];
    char price[128];
    bool isTimetablePublished;

    char* description;
    char* location_url;
    char* poster_file_url;
    char* logo_file_url;

    Dzien_wyd* days;
    size_t days_count;

    Mapka* venue_maps;
    size_t venue_map_count;
} KonwencikEvent;

#define MAX_PROGRAM_ITEMS 2048

typedef struct {
    int id;
    bool program_exists;
    char title[128];
    char start[16];
    char end[16];
    char room[64];
    char speaker[128];
    char type[64];
    char day[32];
    int date_val;
    char description[1024];
    char like_count[64];
    int count_program;
    bool liked;
} KonwencikProgramItem;

extern KonwencikProgramItem konwencik_program_buffer[MAX_PROGRAM_ITEMS];

void testrequest();
void getKonwencikBaza();
void getKonwencik_EventProgram(const char* event_id);
void getKonwencik_EventOgloszenia(const char* event_id);
void getKonwencik_EventStoiska(const char* event_id);
void getKonwencik_EventStoiskoPolubienia(const char* event_id, const char* stoisko_uuid);
void getKonwencik_EventGoscie(const char* event_id);
void getKonwencik_ProgramEntry_LikeCount(const char* event_id, KonwencikProgramItem* program_entry);

KonwencikEvent* KonwencikBazaParser(const char* json_string, size_t* out_count);
void KonwencikBazaFree(KonwencikEvent* events, size_t count);

int Konwencik_QRDaneDecode(const char* input, int* out_ids, int max_ids);
void Konwencik_QRDaneEncode(const char* header, int* ids, int count, char* out_buf, size_t buf_size);

KonwencikProgramItem* KonwencikProgramParser(const char* json_string, size_t json_length, size_t* out_count);
void KonwencikProgramFree(KonwencikProgramItem* items, size_t count);
void Konwencik_ApplyLikesFromQR(int* scanned_ids, int scanned_count, KonwencikProgramItem* items, size_t program_count);

// Map API processing
void Konwencik_ProcessMapData(Mapka* map, const uint8_t* image_data, size_t size);
void Konwencik_FreeMapData(Mapka* map);

extern KonwencikEvent* global_selected_event;

void FS_CreateKonwencikBase();

#endif