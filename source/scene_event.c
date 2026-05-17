#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "scene_event.h"
#include "konwencik_api.h"
#include "drawing.h"
#include "cwav_shit.h"
#include "data.h"
#include "cJSON.h"
#include "utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef enum {
    TAB_INFO = 0,
    TAB_PROGRAM,
    TAB_MAP
} EventTab;

static EventTab current_tab = TAB_INFO;

static bool program_parsed = false;
KonwencikProgramItem* program_db = NULL;
static size_t total_program_items = 0;

static char search_query[64] = "";
static bool is_searching = false;
static int* filtered_indices = NULL;
static size_t filtered_count = 0;

static float program_scrollY = 0.0f;
static float program_targetScrollY = 0.0f; 
static float program_maxScroll = 0.0f;
#define PROGRAM_CARD_HEIGHT 90.0f

static GFX_TEXT descText;
static bool descCached = false;
static float desc_scrollY = 0.0f;
static float desc_targetScrollY = 0.0f;
static float desc_maxScroll = 0.0f;

static u64 event_lastTick = 0; 
static bool p_touchActive = false; 
static int p_lastTouchY = 0;
static int p_lastTouchX = 0;
static int p_touchStartY = 0;
static int p_touchStartX = 0;
static bool p_hasMoved = false;

static GFX_TEXTBUF textBuf;
static GFX_TEXTBUF detailBuf;
bool was_in_qr = false;
bool like_parsed = false;

static int program_selectedIndex = 0;
static float program_stickTimer = 0.0f;


static int selected_map_idx = 0;
static float map_cam_x = 0.0f;
static float map_cam_y = 0.0f;
static float map_zoom = 1.0f;
static float map_list_scrollY = 0.0f;
static float map_list_targetScrollY = 0.0f;
static ResponseBuffer map_download_buf = {NULL, 0, 0, false};
static bool map_is_processing = false;
static bool map_is_downloading = false;

static void str_to_lower(char* dest, const char* src, size_t max_len) {
    if (!src) { dest[0] = '\0'; return; }
    size_t i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = tolower((unsigned char)src[i]);
        i++;
    }
    dest[i] = '\0';
}

static void update_search_filter(void) {
    if (!program_parsed || total_program_items == 0) {
        filtered_count = 0;
        return;
    }
    if (filtered_indices == NULL) {
        filtered_indices = (int*)malloc(total_program_items * sizeof(int));
    }
    filtered_count = 0;
    for (size_t i = 0; i < total_program_items; i++) {
        if (!is_searching || search_query[0] == '\0') {
            filtered_indices[filtered_count++] = i;
        } else {
            char lower_title[256];
            char lower_speaker[256];
            char lower_query[64];
            str_to_lower(lower_title, program_db[i].title, sizeof(lower_title));
            str_to_lower(lower_speaker, program_db[i].speaker, sizeof(lower_speaker));
            str_to_lower(lower_query, search_query, sizeof(lower_query));
            if (strstr(lower_title, lower_query) != NULL || strstr(lower_speaker, lower_query) != NULL) {
                filtered_indices[filtered_count++] = i;
            }
        }
    }
    program_maxScroll = (filtered_count * (PROGRAM_CARD_HEIGHT + 5.0f)) - 200.0f;
    if (program_maxScroll < 0) program_maxScroll = 0;
    if (program_selectedIndex >= (int)filtered_count) {
        program_selectedIndex = filtered_count > 0 ? filtered_count - 1 : 0;
    }
}

const char* dawajDzien(const char* dateStr) {
    int y, m, d;
    int parsed = 0;
    if (sscanf(dateStr, "%d-%d-%d", &y, &m, &d) == 3) parsed = 1;
    else if (sscanf(dateStr, "%d.%d.%d", &d, &m, &y) == 3) parsed = 1;
    if (!parsed) return "Nieznany"; 
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    y -= m < 3;
    int dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    const char* weekdays[] = {"Niedziela", "Poniedzialek", "Wtorek", "Sroda", "Czwartek", "Piatek", "Sobota"};
    return weekdays[dow];
}

void Zapisz_Polubione() {
    if (!global_selected_event || !program_db || program_count <= 0) return;
    fsInit();
    FS_Archive sdmcArchive;
    FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    char folderBuf[256];
    snprintf(folderBuf, sizeof(folderBuf), "/3ds/Konwencik3DS/%s", global_selected_event->id);
    czyFolderIstnieje(sdmcArchive, fsMakePath(PATH_ASCII, folderBuf));
    FSUSER_CloseArchive(sdmcArchive);
    fsExit();
    char pathBuf[256];
    snprintf(pathBuf, sizeof(pathBuf), "sdmc:/3ds/Konwencik3DS/%s/%s_likes.json", global_selected_event->id, global_selected_event->id);
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < program_count; i++) {
        if (program_db[i].liked) {
            cJSON *item = cJSON_CreateNumber(program_db[i].id);
            cJSON_AddItemToArray(root, item);
        }
    }
    save_json(pathBuf, root);
    cJSON_Delete(root);
}

void Wczytaj_Polubione() {
    if (!global_selected_event || !program_db || program_count <= 0) return;
    char pathBuf[256];
    snprintf(pathBuf, sizeof(pathBuf), "sdmc:/3ds/Konwencik3DS/%s/%s_likes.json", global_selected_event->id, global_selected_event->id);
    cJSON *root = NULL;
    open_json(pathBuf, &root);
    if (!root) return;
    if (cJSON_IsArray(root)) {
        size_t index;
        cJSON *value;
        cJSON_ArrayForEach(value, root) {
            int id = (int)cJSON_GetNumberValue(value);
            for (int i = 0; i < program_count; i++) {
                if (program_db[i].id == id) {
                    program_db[i].liked = true;
                    break;
                }
            }
        }
    }
}

static bool LoadFromCache(const char* filepath, ResponseBuffer* buf) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return false;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    
    if (size <= 0) {
        fclose(f);
        return false;
    }
    
    buf->data = malloc(size + 1);
    if (!buf->data) {
        fclose(f);
        return false;
    }
    
    size_t read_bytes = fread(buf->data, 1, size, f);
    buf->data[read_bytes] = '\0';
    buf->size = read_bytes;
    buf->done = true;
    fclose(f);
    return true;
}

static void SaveToCache(const char* filepath, void* data, size_t size) {
    if (!data || size == 0) return;

    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "/3ds/Konwencik3DS/%s", global_selected_event->id);
    
    fsInit();
    FS_Archive sdmcArchive;
    FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    FS_Path folderPath = fsMakePath(PATH_ASCII, dir_path);
    czyFolderIstnieje(sdmcArchive, folderPath);
    FSUSER_CloseArchive(sdmcArchive);
    fsExit();

    const char* real_path = filepath;
    if (strncmp(filepath, "sdmc:", 5) == 0) {
        real_path = filepath + 5;
    }

    int fd = open(real_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        write(fd, data, size); 
        fsync(fd); 
        close(fd);
    }
}

void sceneEventInit(void) {
    textBuf = GFX_TextBufNew(16384);
    detailBuf = GFX_TextBufNew(32768);

    if (!was_in_qr) {
        current_tab = TAB_INFO;
        program_parsed = false;
        total_program_items = 0;
        program_selectedIndex = 0;
        program_scrollY = 0.0f;
        program_targetScrollY = 0.0f;
        descCached = false;
        like_parsed = false;
        search_query[0] = '\0';
        is_searching = false;
        filtered_count = 0;
        selected_map_idx = 0;
        map_cam_x = 0.0f;
        map_cam_y = 0.0f;
        map_zoom = 1.0f;
        map_list_scrollY = 0.0f;
        map_list_targetScrollY = 0.0f;
        map_is_downloading = false;
        map_is_processing = false;

        if (program_db) {
            KonwencikProgramFree(program_db, total_program_items);
            program_db = NULL;
        }
        if (filtered_indices) {
            free(filtered_indices);
            filtered_indices = NULL;
        }

        if (event_program.data) {
            free(event_program.data);
            event_program.data = NULL;
        }
        event_program.done = false;
        event_program.size = 0;
        program_parsed = false;
        total_program_items = 0;
        
        if (global_selected_event != NULL) {
            char program_cache_path[256];
            snprintf(program_cache_path, sizeof(program_cache_path), "sdmc:/3ds/Konwencik3DS/%s/program.json", global_selected_event->id);
            
            if (LoadFromCache(program_cache_path, &event_program)) {
                if (event_program.data != NULL && event_program.size > 0) {
                    
                    size_t exact_len = strlen((const char*)event_program.data);
                    if (exact_len > 0 && exact_len < event_program.size) {
                        event_program.size = exact_len;
                    }

                    
                    if (program_db) KonwencikProgramFree(program_db, total_program_items);
                    program_db = KonwencikProgramParser(event_program.data, event_program.size, &total_program_items);
                    
                    if (program_db != NULL) {
                        program_parsed = true;
                        Wczytaj_Polubione();
                        like_parsed = true;
                        update_search_filter();
                    }
                }
                
                event_program.done = true; 
            } else {
                
                getKonwencik_EventProgram(global_selected_event->id);
            }
        }
    }
    was_in_qr = false;
    GFX_TextBufClear(detailBuf);
    event_lastTick = svcGetSystemTick();
    p_touchActive = false;
}

void sceneEventUpdate(uint32_t kDown, uint32_t kHeld) {
    if (!global_selected_event) return;

    u64 now = svcGetSystemTick();
    float dt = (float)(now - event_lastTick) / CPU_TICKS_PER_MSEC / 1000.0f;
    event_lastTick = now;

    updateWave(now * 3.0f); 

    
    if (kDown & KEY_L) {
        if (current_tab > 0) current_tab--;
        else current_tab = TAB_MAP;
        playCwav(2, true);
    }
    if (kDown & KEY_R) {
        if (current_tab < TAB_MAP) current_tab++;
        else current_tab = TAB_INFO;
        
        
        if (current_tab == TAB_PROGRAM && !program_parsed && !event_program.done && event_program.data == NULL) {
            getKonwencik_EventProgram(global_selected_event->id);
        }
        playCwav(2, true);
    }

    
    if (kDown & KEY_SELECT) {
        was_in_qr = true;
        sceneManagerSwitchTo(SCENE_QR);
    }
    if (kDown & KEY_X && current_tab == TAB_PROGRAM) {
        was_in_qr = true;
        sceneManagerSwitchTo(SCENE_EXPORT);
    }
    if (kDown & KEY_B) {
        if (current_tab == TAB_PROGRAM && is_searching) {
            is_searching = false;
            search_query[0] = '\0';
            update_search_filter();
            playCwav(2, true);
        } else {
            sceneManagerSwitchTo(SCENE_QUERY);
            return; 
        }
    }
    
    
    if (current_tab == TAB_MAP) {
        if (kHeld & KEY_A) map_zoom += 1.5f * dt;
        if (kHeld & KEY_Y) map_zoom -= 1.5f * dt;
        if (map_zoom < 0.25f) map_zoom = 0.25f;
        if (map_zoom > 4.0f) map_zoom = 4.0f;
    }

    if (kDown & KEY_START) {
        if (current_tab == TAB_PROGRAM && osGetWifiStrength() > 0) {
            
            program_parsed = false;
            event_program.done = false;
            event_program.size = 0; 
            if (event_program.data) {
                free(event_program.data);
                event_program.data = NULL;
            }
            getKonwencik_EventProgram(global_selected_event->id); 
            playCwav(2, true);
        } 
        else if (current_tab == TAB_MAP) {
            if (map_is_downloading || map_is_processing) return;

            Mapka* sel_map = &global_selected_event->venue_maps[selected_map_idx];
            Konwencik_FreeMapData(sel_map); 
            
            map_download_buf.done = false;
            if (map_download_buf.data) {
                free(map_download_buf.data);
                map_download_buf.data = NULL;
            }
            map_is_downloading = true;
            queue_request(sel_map->file, NULL, NULL, &map_download_buf, false); 
            playCwav(2, true);
        }
    }

    if (map_download_buf.done && !map_is_processing && map_download_buf.data != NULL) {
        map_is_processing = true;
        
        char map_cache_path[256];
        snprintf(map_cache_path, sizeof(map_cache_path), "sdmc:/3ds/Konwencik3DS/%s/map_%d.dat", global_selected_event->id, selected_map_idx);
        SaveToCache(map_cache_path, map_download_buf.data, map_download_buf.size);

        Konwencik_ProcessMapData(&global_selected_event->venue_maps[selected_map_idx], 
                                 (const uint8_t*)map_download_buf.data, 
                                 map_download_buf.size);
        
        free(map_download_buf.data);
        map_download_buf.data = NULL;
        map_download_buf.size = 0;
        map_download_buf.done = false;
        map_is_downloading = false;
        map_is_processing = false;
    }

    if (current_tab == TAB_PROGRAM && program_parsed && (kDown & KEY_A)) {
        SwkbdState swkbd;
        char mybuf[64];
        swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(mybuf) - 1);
        swkbdSetInitialText(&swkbd, search_query);
        swkbdSetHintText(&swkbd, "Szukaj w tytule lub prelegencie...");
        swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Anuluj", false);
        swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Szukaj", true);
        
        SwkbdButton button = swkbdInputText(&swkbd, mybuf, sizeof(mybuf));
        if (button != SWKBD_BUTTON_NONE && button != SWKBD_BUTTON_LEFT) {
            strncpy(search_query, mybuf, sizeof(search_query) - 1);
            search_query[sizeof(search_query) - 1] = '\0';
            is_searching = (strlen(search_query) > 0);
            update_search_filter();
            playCwav(2, true);
        }
    }

    
    if (event_program.done && !program_parsed) {
        if (event_program.data != NULL && event_program.size > 0) {
            char program_cache_path[256];
            snprintf(program_cache_path, sizeof(program_cache_path), "sdmc:/3ds/Konwencik3DS/%s/program.json", global_selected_event->id);
            
            
            size_t true_len = strlen((const char*)event_program.data);
            if (true_len > 0 && true_len < event_program.size) {
                event_program.size = true_len;
            }

            SaveToCache(program_cache_path, event_program.data, event_program.size);

            if (program_db) KonwencikProgramFree(program_db, total_program_items);
            program_db = KonwencikProgramParser(event_program.data, event_program.size, &total_program_items);
            if (program_db != NULL) {
                program_parsed = true;
                Wczytaj_Polubione();
                like_parsed = true;
                update_search_filter();
            }
        }
    }

    if (!descCached && global_selected_event->description) {
        const char* original_desc = global_selected_event->description;
        size_t desc_len = strlen(original_desc);
        
        
        #define MAX_SAFE_DESC_LEN 3500
        
        if (desc_len > MAX_SAFE_DESC_LEN) {
            
            char* truncated_desc = malloc(MAX_SAFE_DESC_LEN + 128);
            if (truncated_desc) {
                strncpy(truncated_desc, original_desc, MAX_SAFE_DESC_LEN);
                truncated_desc[MAX_SAFE_DESC_LEN] = '\0';
                
                
                strcat(truncated_desc, "\n\n... [ Tekst skrócony - koniec pamięci :( ]");
                
                GFX_TextParse(&descText, detailBuf, truncated_desc);
                
                
                float est_height = 0;
                int chars_in_line = 0;
                for(int i = 0; truncated_desc[i] != '\0'; i++) {
                    chars_in_line++;
                    if (truncated_desc[i] == '\n' || chars_in_line > 50) { 
                        est_height += 18.0f; 
                        chars_in_line = 0;
                    }
                }
                desc_maxScroll = est_height - 200.0f;
                free(truncated_desc);
            } else {
                
                GFX_TextParse(&descText, detailBuf, original_desc);
            }
        } else {
            
            GFX_TextParse(&descText, detailBuf, original_desc);
            
            float est_height = 0;
            int chars_in_line = 0;
            for(int i = 0; original_desc[i] != '\0'; i++) {
                chars_in_line++;
                if (original_desc[i] == '\n' || chars_in_line > 50) { 
                    est_height += 18.0f; 
                    chars_in_line = 0;
                }
            }
            desc_maxScroll = est_height - 200.0f;
        }

        GFX_TextOptimize(&descText);
        if (desc_maxScroll < 0) desc_maxScroll = 0;
        descCached = true;
    }

    touchPosition touch;
    hidTouchRead(&touch);
    circlePosition pos;
    hidCircleRead(&pos);

    
    if (kDown & KEY_TOUCH) {
        p_touchActive = true;
        p_lastTouchY = touch.py;
        p_lastTouchX = touch.px;
        p_touchStartY = touch.py;
        p_touchStartX = touch.px;
        p_hasMoved = false;
    } else if ((kHeld & KEY_TOUCH) && p_touchActive) {
        int deltaY = touch.py - p_lastTouchY;
        if (abs(touch.py - p_touchStartY) > 8 || abs(touch.px - p_touchStartX) > 8) {
            p_hasMoved = true;
        }
        
        if (current_tab == TAB_PROGRAM) {
            program_targetScrollY -= deltaY * 1.5f; 
        } else if (current_tab == TAB_INFO) {
            desc_targetScrollY -= deltaY * 1.5f; 
        } else if (current_tab == TAB_MAP) {
            map_list_targetScrollY -= deltaY * 1.5f; 
        }
        p_lastTouchY = touch.py;
        p_lastTouchX = touch.px;
    } else if (p_touchActive) { 
        if (current_tab == TAB_PROGRAM && !p_hasMoved && program_parsed) {
            float relativeTouchY = p_lastTouchY - 30.0f + program_scrollY;
            int tappedIndex = (int)(relativeTouchY / (PROGRAM_CARD_HEIGHT + 5.0f));
            if (tappedIndex >= 0 && tappedIndex < (int)filtered_count) {
                program_selectedIndex = tappedIndex;
                playCwav(2, true);
            }
        }
        else if (current_tab == TAB_MAP && !p_hasMoved && global_selected_event->venue_map_count > 0) {
            if (!map_is_downloading && !map_is_processing) {
                int buttonHeight = 35;
                for (int i = 0; i < global_selected_event->venue_map_count; i++) {
                    int by = 10 + (i * (buttonHeight + 5)) - map_list_scrollY;
                    if (p_lastTouchY >= by && p_lastTouchY <= by + buttonHeight && p_lastTouchX > 10 && p_lastTouchX < 310) {
                        if (selected_map_idx != i) {
                            Konwencik_FreeMapData(&global_selected_event->venue_maps[selected_map_idx]);
                            selected_map_idx = i;
                            map_cam_x = 0;
                            map_cam_y = 0;
                            map_zoom = 1.0f;
                        }
                        
                        playCwav(2, true);

                        Mapka* sel_map = &global_selected_event->venue_maps[selected_map_idx];
                        if (!sel_map->runtime_data.loaded && !map_download_buf.done && !map_is_processing) {
                            char map_cache_path[256];
                            snprintf(map_cache_path, sizeof(map_cache_path), "sdmc:/3ds/Konwencik3DS/%s/map_%d.dat", global_selected_event->id, selected_map_idx);
                            
                            if (!LoadFromCache(map_cache_path, &map_download_buf)) {
                                if (osGetWifiStrength() > 0) 
                                    map_is_downloading = true;
                                    queue_request(sel_map->file, NULL, NULL, &map_download_buf, false);
                            } else {
                                map_is_downloading = true;
                            }
                        }
                        break;
                    }
                }
            }
        }
        p_touchActive = false;
    }

    
    if (current_tab == TAB_PROGRAM && program_parsed) {
        program_stickTimer -= dt;
        bool moveUp = (pos.dy > 40) || (kDown & KEY_UP);
        bool moveDown = (pos.dy < -40) || (kDown & KEY_DOWN);
        if (program_selectedIndex < 0) program_selectedIndex = 0;
        if (program_selectedIndex >= (int)filtered_count) program_selectedIndex = (int)filtered_count - 1;
        float itemTop = (program_selectedIndex * (PROGRAM_CARD_HEIGHT + 5.0f));
        if (moveUp || moveDown) program_targetScrollY = itemTop - 60.0f; 

        if (kDown & KEY_Y && program_db && filtered_count > 0) {
            int actual_idx = filtered_indices[program_selectedIndex];
            program_db[actual_idx].liked = !program_db[actual_idx].liked;
            Zapisz_Polubione();
            playCwav(2, true);
        }

        if (program_targetScrollY < 0) program_targetScrollY = 0;
        if (program_targetScrollY > program_maxScroll) program_targetScrollY = program_maxScroll;
        program_scrollY += (program_targetScrollY - program_scrollY) * 10.0f * dt;
        int newIndex = (int)((program_scrollY + 60.0f) / (PROGRAM_CARD_HEIGHT + 5.0f));
        if (newIndex >= 0 && newIndex < (int)filtered_count) program_selectedIndex = newIndex;

    } else if (current_tab == TAB_INFO) {
        if (pos.dy > 40) desc_targetScrollY -= 450.0f * dt;
        if (pos.dy < -40) desc_targetScrollY += 450.0f * dt;
        if (desc_targetScrollY < 0) desc_targetScrollY = 0;
        if (desc_targetScrollY > desc_maxScroll) desc_targetScrollY = desc_maxScroll;
        desc_scrollY += (desc_targetScrollY - desc_scrollY) * 10.0f * dt;

    } else if (current_tab == TAB_MAP) {
        if (pos.dy > 40) map_list_targetScrollY -= 450.0f * dt;
        if (pos.dy < -40) map_list_targetScrollY += 450.0f * dt;
        float max_map_scroll = (global_selected_event->venue_map_count * 40.0f) - 180.0f;
        if (max_map_scroll < 0) max_map_scroll = 0;
        if (map_list_targetScrollY < 0) map_list_targetScrollY = 0;
        if (map_list_targetScrollY > max_map_scroll) map_list_targetScrollY = max_map_scroll;
        map_list_scrollY += (map_list_targetScrollY - map_list_scrollY) * 10.0f * dt;

        if (abs(pos.dx) > 10) map_cam_x -= pos.dx * 1.5f * dt;
        if (abs(pos.dy) > 10) map_cam_y += pos.dy * 1.5f * dt;
        
        if (kHeld & KEY_LEFT) map_cam_x -= 300.0f * dt;
        if (kHeld & KEY_RIGHT) map_cam_x += 300.0f * dt;
        if (kHeld & KEY_UP) map_cam_y -= 300.0f * dt;
        if (kHeld & KEY_DOWN) map_cam_y += 300.0f * dt;
        
        if (global_selected_event->venue_map_count > 0) {
            Mapka* cur_map = &global_selected_event->venue_maps[selected_map_idx];
            if (cur_map->runtime_data.loaded) {
                float max_x = (cur_map->runtime_data.total_width * map_zoom) - 400.0f;
                float max_y = (cur_map->runtime_data.total_height * map_zoom) - 210.0f;
                if (max_x < 0) max_x = 0;
                if (max_y < 0) max_y = 0;
                if (map_cam_x < 0) map_cam_x = 0;
                if (map_cam_y < 0) map_cam_y = 0;
                if (map_cam_x > max_x) map_cam_x = max_x;
                if (map_cam_y > max_y) map_cam_y = max_y;
            }
        }
    }
}

void sceneEventRender(void) {
    if (!global_selected_event) return;

    GFX_BeginSceneTop(0, true);
    GFX_TextBufClear(textBuf); 
    drawKwadraty();
    drawWaveFill();
    drawBubblesTop();

    
    if (current_tab == TAB_INFO) {
        char infoStr[512];
        snprintf(infoStr, sizeof(infoStr), "Event: %s\nData: %s - %s\nMiejsce: %s\nCena: %s",
            global_selected_event->name, global_selected_event->start_day,
            global_selected_event->end_day, global_selected_event->location_text,
            global_selected_event->price);

        GFX_TEXT infoText;
        GFX_TextParse(&infoText, textBuf, infoStr);
        GFX_TextOptimize(&infoText);
        GFX_DrawShadowedTextWrapped(&infoText, 10, 40, 0.6f, 0.6f, 0.6f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,255), 380);
    } 
    else if (current_tab == TAB_PROGRAM) {
        if (!event_program.done || !program_parsed) {
            float cx = 200.0f, cy = 120.0f, r = 15.0f;
            u64 ticks = svcGetSystemTick();
            int active = ((u64)(ticks / (CPU_TICKS_PER_MSEC * 100))) % 8;
            for (int i = 0; i < 8; i++) {
                float angle = i * (3.14159f / 4.0f);
                float px = cx + cosf(angle) * r;
                float py = cy + sinf(angle) * r;
                u32 color = (i == active) ? GFX_COLOR_RGBA(60, 90, 160, 255) : GFX_COLOR_RGBA(60, 60, 60, 255);
                GFX_DrawRectSolid(px - 2, py - 2, 0.5f, 4, 4, color);
            }
        } else if (program_parsed && total_program_items > 0) {
            if (filtered_count == 0) {
                GFX_TEXT noResText;
                GFX_TextParse(&noResText, textBuf, "Brak wynikow wyszukiwania.");
                GFX_TextOptimize(&noResText);
                GFX_DrawTextWrapped(&noResText, 10, 40, 0.5f, 0.6f, 0.6f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255,255,255,255), 380);
            } else {
                int firstVisible = (int)(program_scrollY / (PROGRAM_CARD_HEIGHT + 5.0f));
                int maxVisible = (240 / (PROGRAM_CARD_HEIGHT + 5.0f)) + 2;
                for (int i = 0; i < maxVisible; i++) {
                    int idx = firstVisible + i;
                    if (idx >= filtered_count) break;
                    int actual_idx = filtered_indices[idx];
                    float y = 30.0f + (idx * (PROGRAM_CARD_HEIGHT + 5.0f)) - program_scrollY;
                    if (y > 240 || y + PROGRAM_CARD_HEIGHT < 30) continue;
                    u32 cardColor = (idx == program_selectedIndex) ? 
                        (program_db[actual_idx].liked ? GFX_COLOR_RGBA(130, 70, 100, 255) : GFX_COLOR_RGBA(60, 120, 90, 255)) : 
                        (program_db[actual_idx].liked ? GFX_COLOR_RGBA(100, 40, 70, 190) : GFX_COLOR_RGBA(30, 30, 30, 190));       
                    GFX_DrawRectSolid(10, y, 0.5f, 380, PROGRAM_CARD_HEIGHT, cardColor);
                    char progStr[256];
                    snprintf(progStr, sizeof(progStr), "[%s - %s] %s\n%s | Prowadzący: %s", 
                        program_db[actual_idx].start, program_db[actual_idx].end, program_db[actual_idx].title, 
                        program_db[actual_idx].room, program_db[actual_idx].speaker);
                    GFX_TEXT pText;
                    GFX_TextParse(&pText, textBuf, progStr);
                    GFX_TextOptimize(&pText);
                    GFX_DrawTextWrapped(&pText, 15, y + 5, 0.5f, 0.5f, 0.5f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255,255,255,255), 370);
                }
                if (firstVisible >= 0 && firstVisible < filtered_count) {
                    const char* dayName = dawajDzien(program_db[filtered_indices[firstVisible]].day);
                    GFX_DrawRectSolid(290, 30, 0.8f, 110, 20, GFX_COLOR_RGBA(20, 20, 20, 230));
                    GFX_TEXT dayText;
                    GFX_TextParse(&dayText, textBuf, dayName);
                    GFX_TextOptimize(&dayText);
                    GFX_DrawText(&dayText, 295, 33, 0.9f, 0.5f, 0.5f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255, 215, 0, 255)); 
                }
            }
        }
    } 
    else if (current_tab == TAB_MAP) {
        if (global_selected_event->venue_map_count > 0) {
            Mapka* cur_map = &global_selected_event->venue_maps[selected_map_idx];
            if (cur_map->runtime_data.loaded) {
                for(int i = 0; i < (cur_map->runtime_data.num_chunks_x * cur_map->runtime_data.num_chunks_y); i++) {
                    MapChunk* chunk = &cur_map->runtime_data.chunks[i];
                    
                    if (!chunk->tex.data) continue; 
                    
                    float scaled_w = chunk->width * map_zoom;
                    float scaled_h = chunk->height * map_zoom;
                    float draw_x = (chunk->x_offset * map_zoom) - map_cam_x;
                    float draw_y = (chunk->y_offset * map_zoom) - map_cam_y + 30.0f;

                    if (draw_x + scaled_w < -1.0f || draw_x > 401.0f) continue;
                    if (draw_y + scaled_h < 29.0f || draw_y > 241.0f) continue;
                    
                    C2D_Image cimg;
                    cimg.tex = &chunk->tex;
                    cimg.subtex = &chunk->subtex;
                    
                    C2D_DrawImageAt(cimg, floorf(draw_x), floorf(draw_y), 0.5f, NULL, map_zoom, map_zoom);
                }
            } else if (map_download_buf.data || map_is_processing || map_is_downloading) {
                float cx = 200.0f, cy = 120.0f, r = 15.0f;
                u64 ticks = svcGetSystemTick();
                int active = ((u64)(ticks / (CPU_TICKS_PER_MSEC * 100))) % 8;
                for (int i = 0; i < 8; i++) {
                    float angle = i * (3.14159f / 4.0f);
                    float px = cx + cosf(angle) * r;
                    float py = cy + sinf(angle) * r;
                    u32 color = (i == active) ? GFX_COLOR_RGBA(60, 90, 160, 255) : GFX_COLOR_RGBA(60, 60, 60, 255);
                    GFX_DrawRectSolid(px - 2, py - 2, 0.5f, 4, 4, color);
                }
            } else {
                GFX_TEXT loadingText;
                GFX_TextParse(&loadingText, textBuf, "Kliknij by załadować mapę...");
                GFX_TextOptimize(&loadingText);
                GFX_DrawText(&loadingText, 200, 120, 0.5f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(150, 150, 150, 255));
            }
        } else {
            GFX_TEXT noMapText;
            GFX_TextParse(&noMapText, textBuf, "Brak map dla tego wydarzenia.");
            GFX_TextOptimize(&noMapText);
            GFX_DrawText(&noMapText, 200, 120, 0.5f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
        }
    }

    
    GFX_DrawRectSolid(0, 0, 0.6f, 400, 30, GFX_COLOR_RGBA(20, 20, 20, 255)); 
    u32 tabActiveColor = GFX_COLOR_RGBA(60, 90, 160, 255);
    u32 tabInactiveColor = GFX_COLOR_RGBA(40, 40, 40, 255);
    
    GFX_DrawRectSolid(0, 0, 0.7f, 133, 25, (current_tab == TAB_INFO) ? tabActiveColor : tabInactiveColor);
    GFX_DrawRectSolid(133, 0, 0.7f, 134, 25, (current_tab == TAB_PROGRAM) ? tabActiveColor : tabInactiveColor);
    GFX_DrawRectSolid(267, 0, 0.7f, 133, 25, (current_tab == TAB_MAP) ? tabActiveColor : tabInactiveColor);

    GFX_TEXT infoTabLabel, progTabLabel, mapTabLabel;
    GFX_TextParse(&infoTabLabel, textBuf, "INFO");
    GFX_TextParse(&progTabLabel, textBuf, "PROGRAM");
    GFX_TextParse(&mapTabLabel, textBuf, "MAPY");
    
    GFX_DrawText(&infoTabLabel, 66, 5, 0.8f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
    GFX_DrawText(&progTabLabel, 200, 5, 0.8f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
    GFX_DrawText(&mapTabLabel, 333, 5, 0.8f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));

    
    GFX_BeginSceneBottom();
    GFX_DrawRectSolid(0, 0, 0.4f, 320, 240, GFX_COLOR_RGBA(25,25,25,255));

    if (current_tab == TAB_INFO) {
        if (descCached) {
            float y_pos = 10.0f - desc_scrollY; 
            GFX_DrawTextWrapped(&descText, 10, y_pos, 0.5f, 0.4f, 0.4f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(200,200,200,255), 300);
        }
    } else if (current_tab == TAB_PROGRAM) {
        char bottomHelp[256];
        if (is_searching) snprintf(bottomHelp, sizeof(bottomHelp), "(SELECT) - Import planu\n(X) - Export planu\n(A) - Szukaj (Aktywne: %s)\n(B) - Anuluj wyszukiwanie\n(Y) - Polub", search_query);
        else snprintf(bottomHelp, sizeof(bottomHelp), "(SELECT) - Import planu\n(X) - Export planu\n(A) - Szukaj\n(Y) - Polub\n(START) - Odśwież");
        
        GFX_TEXT importText;
        GFX_TextParse(&importText, textBuf, bottomHelp);
        GFX_TextOptimize(&importText);
        GFX_DrawTextWrapped(&importText, 10, 10, 0.5f, 0.6f, 0.6f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255,255,255,255), 380);
    } else if (current_tab == TAB_MAP) {
        if (global_selected_event->venue_map_count > 0) {
            for (int i = 0; i < global_selected_event->venue_map_count; i++) {
                float by = 10.0f + (i * 40.0f) - map_list_scrollY;
                if (by > 240 || by + 35 < 0) continue;

                u32 col = (i == selected_map_idx) ? GFX_COLOR_RGBA(60, 90, 160, 255) : GFX_COLOR_RGBA(60, 60, 60, 255);
                GFX_DrawRectSolid(10, by, 0.5f, 300, 35, col);
                
                GFX_TEXT btnText;
                GFX_TextParse(&btnText, textBuf, global_selected_event->venue_maps[i].title);
                GFX_TextOptimize(&btnText);
                GFX_DrawText(&btnText, 160, by + 8, 0.6f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
            }
            
            GFX_TEXT helpText;
            GFX_TextParse(&helpText, textBuf, "(A/Y) - Przybliż/Oddal mapę\n(D-Pad/Krzyżak) - Przesuwanie mapy");
            GFX_TextOptimize(&helpText);
            GFX_DrawText(&helpText, 160, 210, 0.5f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(180, 180, 180, 255));

        } else {
            GFX_TEXT noDataTxt;
            GFX_TextParse(&noDataTxt, textBuf, "Brak map...");
            GFX_TextOptimize(&noDataTxt);
            GFX_DrawText(&noDataTxt, 160, 120, 0.6f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(150, 150, 150, 255));
        }
    }
}

void sceneEventExit(void) {
    if (!was_in_qr) {
        if (program_db) {
            KonwencikProgramFree(program_db, total_program_items);
            program_db = NULL;
        }
        if (filtered_indices) {
            free(filtered_indices);
            filtered_indices = NULL;
        }
        
        if (map_download_buf.data) {
            free(map_download_buf.data);
            map_download_buf.data = NULL;
            map_download_buf.size = 0;
            map_download_buf.done = false;
        }

        
        if (global_selected_event && global_selected_event->venue_maps) {
            for (size_t i = 0; i < global_selected_event->venue_map_count; i++) {
                Konwencik_FreeMapData(&global_selected_event->venue_maps[i]);
            }
        }

        program_parsed = false;
        total_program_items = 0;
        descCached = false;
        like_parsed = false;
        is_searching = false;
        search_query[0] = '\0';
        map_is_downloading = false;
        map_is_processing = false;
    }

    GFX_TextBufDelete(textBuf);
    GFX_TextBufDelete(detailBuf);
}