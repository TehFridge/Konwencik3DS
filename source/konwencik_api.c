#include <3ds.h>
#include <citro2d.h>
#include "konwencik_api.h"
#include "scene_manager.h"
#include "main.h"
#include "cwav_shit.h"
#include "sprites.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <curl/curl.h>
#include "request.h"
#include "utils.h"
#include "cJSON.h"

#include <png.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <webp/decode.h>

typedef enum {
    FMT_NONE,
    FMT_PNG,
    FMT_JPEG,
    FMT_WEBP
} ImgFmt;

typedef struct {
    const uint8_t* data;
    size_t size;
    size_t offset;
} mem_source;

struct my_jpeg_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};


typedef struct ImageStream {
    mem_source mem;

    int width, height;
    int format;

    
    int stride;
    int bpp;

    
    png_structp png;
    png_infop info;

    
    struct jpeg_decompress_struct cinfo;
    struct my_jpeg_error_mgr jerr;
    uint8_t* jpeg_row_buf; 
    
    WebPDecoderConfig webp_cfg;
    uint8_t* webp_rgba;
    int webp_row;
} ImageStream;


static void png_mem_read_cb(png_structp png_ptr, png_bytep data, png_size_t length)
{
    mem_source* src = (mem_source*)png_get_io_ptr(png_ptr);

    if (src->offset + length > src->size)
        png_error(png_ptr, "Unexpected end of PNG data");

    memcpy(data, src->data + src->offset, length);
    src->offset += length;
}

static void my_jpeg_error_exit(j_common_ptr cinfo)
{
    struct my_jpeg_error_mgr* myerr =
        (struct my_jpeg_error_mgr*)cinfo->err;

    longjmp(myerr->setjmp_buffer, 1);
}

static void ImageStream_Close(ImageStream* ctx)
{
    if (ctx->format == FMT_PNG)
    {
        png_destroy_read_struct(&ctx->png, &ctx->info, NULL);
    }
    else if (ctx->format == FMT_JPEG)
    {
        jpeg_destroy_decompress(&ctx->cinfo);
    }
    else if (ctx->format == FMT_WEBP)
    {
        if (ctx->webp_rgba)
            free(ctx->webp_rgba);

        WebPFreeDecBuffer(&ctx->webp_cfg.output);
    }

    memset(ctx, 0, sizeof(ImageStream));
}

static bool ImageStream_Open(ImageStream* ctx, const uint8_t* data, size_t size)
{
    memset(ctx, 0, sizeof(ImageStream));

    ctx->mem.data = data;
    ctx->mem.size = size;
    ctx->mem.offset = 0;

    ctx->bpp = 4; 
    ctx->stride = 0;

    
    
    
    if (size >= 4 && !memcmp(data, "\x89PNG", 4))
    {
        ctx->format = FMT_PNG;

        ctx->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        ctx->info = png_create_info_struct(ctx->png);

        if (setjmp(png_jmpbuf(ctx->png))) {
            ImageStream_Close(ctx); 
            return false;
        }

        png_set_read_fn(ctx->png, &ctx->mem, png_mem_read_cb);
        png_read_info(ctx->png, ctx->info);

        int color_type = png_get_color_type(ctx->png, ctx->info);
        int bit_depth  = png_get_bit_depth(ctx->png, ctx->info);

        if (bit_depth == 16)
            png_set_strip_16(ctx->png);

        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(ctx->png);

        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(ctx->png);

        
        png_set_gray_to_rgb(ctx->png);
        png_set_add_alpha(ctx->png, 0xFF, PNG_FILLER_AFTER);

        png_read_update_info(ctx->png, ctx->info);

        ctx->width  = png_get_image_width(ctx->png, ctx->info);
        ctx->height = png_get_image_height(ctx->png, ctx->info);
        ctx->stride = ctx->width * 4;

        return true;
    }

    
    
    
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xD8)
    {
        ctx->format = FMT_JPEG;
        ctx->cinfo.err = jpeg_std_error(&ctx->jerr.pub);
        ctx->jerr.pub.error_exit = my_jpeg_error_exit;

        
        jpeg_create_decompress(&ctx->cinfo);

        if (setjmp(ctx->jerr.setjmp_buffer)) {
            ImageStream_Close(ctx); 
            return false;
        }

        jpeg_mem_src(&ctx->cinfo, data, size);
        jpeg_read_header(&ctx->cinfo, TRUE);
        ctx->cinfo.out_color_space = JCS_RGB;
        jpeg_start_decompress(&ctx->cinfo);

        ctx->width  = ctx->cinfo.output_width;
        ctx->height = ctx->cinfo.output_height;
        ctx->stride = ctx->width * 4;

        
        ctx->jpeg_row_buf = (uint8_t*)malloc(ctx->width * 3);
        if (!ctx->jpeg_row_buf) {
            ImageStream_Close(ctx);
            return false;
        }

        return true;
    }
    
    
    
    if (size >= 12 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WEBP", 4))
    {
        ctx->format = FMT_WEBP;
        WebPDecoderConfig* cfg = &ctx->webp_cfg;

        if (!WebPInitDecoderConfig(cfg)) return false;

        if (WebPGetFeatures(data, size, &cfg->input) != VP8_STATUS_OK) return false;

        ctx->width  = cfg->input.width;
        ctx->height = cfg->input.height;
        ctx->stride = ctx->width * 4;

        size_t buffer_size = ctx->width * ctx->height * 4;
        ctx->webp_rgba = (uint8_t*)malloc(buffer_size);
        if (!ctx->webp_rgba) {
            ImageStream_Close(ctx); 
            return false;
        }

        cfg->output.colorspace = MODE_RGBA;
        cfg->output.u.RGBA.rgba   = ctx->webp_rgba;
        cfg->output.u.RGBA.stride = ctx->stride;
        cfg->output.u.RGBA.size   = buffer_size;
        cfg->output.is_external_memory = 1;

        if (WebPDecode(data, size, cfg) != VP8_STATUS_OK) {
            ImageStream_Close(ctx); 
            return false;
        }

        ctx->webp_row = 0;
        return true;
    }
    return false;
}

static bool ImageStream_ReadRows(ImageStream* ctx, uint8_t* buffer, int num_rows)
{
    if (ctx->format == FMT_PNG)
    {
        if (setjmp(png_jmpbuf(ctx->png)))
            return false;

        for (int i = 0; i < num_rows; i++)
        {
            png_bytep row = buffer + (i * ctx->stride);
            png_read_row(ctx->png, row, NULL);
        }

        return true;
    }

    if (ctx->format == FMT_JPEG)
    {
        if (setjmp(ctx->jerr.setjmp_buffer))
            return false;

        for (int i = 0; i < num_rows; i++)
        {
            uint8_t* row = buffer + (i * ctx->stride);

            
            JSAMPROW ptr = ctx->jpeg_row_buf;
            jpeg_read_scanlines(&ctx->cinfo, &ptr, 1);

            for (int x = 0; x < ctx->width; x++)
            {
                row[x * 4 + 0] = ctx->jpeg_row_buf[x * 3 + 0];
                row[x * 4 + 1] = ctx->jpeg_row_buf[x * 3 + 1];
                row[x * 4 + 2] = ctx->jpeg_row_buf[x * 3 + 2];
                row[x * 4 + 3] = 0xFF;
            }
        }
        return true;
    }

    if (ctx->format == FMT_WEBP)
    {
        for (int i = 0; i < num_rows; i++)
        {
            if (ctx->webp_row >= ctx->height)
                return false;

            memcpy(
                buffer + (i * ctx->stride),
                ctx->webp_rgba + (ctx->webp_row * ctx->stride),
                ctx->stride
            );

            ctx->webp_row++;
        }

        return true;
    }

    return false;
}


int program_count;
ResponseBuffer konwencik_baza = {NULL, 0, 0, false};
ResponseBuffer event_stoisko_polubienia = {NULL, 0, 0, false};
ResponseBuffer event_stoiska = {NULL, 0, 0, false};
ResponseBuffer event_ogloszenia = {NULL, 0, 0, false};
ResponseBuffer event_program = {NULL, 0, 0, false};
ResponseBuffer event_goscie = {NULL, 0, 0, false};
ResponseBuffer testreq = {NULL, 0, 0, false};
ResponseBuffer event_programentry_polubienia = {NULL, 0, 0, false};

static const char* get_json_string(cJSON *obj, const char *key) {
    if (!obj) return "";
    cJSON *val = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(val) && (val->valuestring != NULL)) {
        return val->valuestring;
    }
    return ""; 
}

static int compare_newest_first(const void *a, const void *b) {
    KonwencikEvent *eventA = (KonwencikEvent *)a;
    KonwencikEvent *eventB = (KonwencikEvent *)b;
    return strcmp(eventB->start_day, eventA->start_day);
}

char* sanitize_markdown(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = src[i];
        if (c == '[') {
            i++;
            while (src[i] && src[i] != ']' && j < len) {
                out[j++] = src[i++];
            }
            while (src[i] && src[i] != ')') i++;
            continue;
        }
        if (c == '*' || c == '#' || c == '`') continue;
        if (c == '\n' && src[i+1] == '\n') {
            out[j++] = '\n';
            i++;
            continue;
        }
        out[j++] = c;
    }
    out[j] = '\0';
    return out;
}

void testrequest(){
    queue_request("https://inpost.pl/404", NULL, NULL, &testreq, false);
}

void getKonwencikBaza(){
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-Firebase-GMPID: 1:442888522259:android:ea4afff029cdccac");
    headers = curl_slist_append(headers, "User-Agent: Firebase/5/21.0.0/33/Horizon");
    queue_request("https://conference-app-1a289.firebaseio.com/prod/conference.json", NULL, headers, &konwencik_baza, false);
}

void getKonwencik_EventProgram(const char* event_id) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-Firebase-GMPID: 1:442888522259:android:ea4afff029cdccac");
    headers = curl_slist_append(headers, "User-Agent: Firebase/5/21.0.0/33/Horizon");
    char url[256];
    snprintf(url, sizeof(url), "https://conference-app-1a289.firebaseio.com/prod/presentation/%s.json", event_id);
    queue_request(url, NULL, headers, &event_program, false);
}

void getKonwencik_EventOgloszenia(const char* event_id) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-Firebase-GMPID: 1:442888522259:android:ea4afff029cdccac");
    headers = curl_slist_append(headers, "User-Agent: Firebase/5/21.0.0/33/Horizon");
    char url[256];
    snprintf(url, sizeof(url), "https://conference-app-1a289.firebaseio.com/prod/announcement/%s.json", event_id);
    queue_request(url, NULL, headers, &event_ogloszenia, false);
}

void getKonwencik_EventStoiska(const char* event_id) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-Firebase-GMPID: 1:442888522259:android:ea4afff029cdccac");
    headers = curl_slist_append(headers, "User-Agent: Firebase/5/21.0.0/33/Horizon");
    char url[256];
    snprintf(url, sizeof(url), "https://conference-app-1a289.firebaseio.com/prod/stand/%s.json", event_id);
    queue_request(url, NULL, headers, &event_stoiska, false);
}

void getKonwencik_EventStoiskoPolubienia(const char* event_id, const char* stoisko_uuid) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-Firebase-GMPID: 1:442888522259:android:ea4afff029cdccac");
    headers = curl_slist_append(headers, "User-Agent: Firebase/5/21.0.0/33/Horizon");
    char url[256];
    snprintf(url, sizeof(url), "https://conference-app-1a289.firebaseio.com/prod/stand_feedback/%s/%s/love_count.json", event_id, stoisko_uuid);
    queue_request(url, NULL, headers, &event_stoisko_polubienia, false);
}

void getKonwencik_EventGoscie(const char* event_id) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-Firebase-GMPID: 1:442888522259:android:ea4afff029cdccac");
    headers = curl_slist_append(headers, "User-Agent: Firebase/5/21.0.0/33/Horizon");
    char url[256];
    snprintf(url, sizeof(url), "https://conference-app-1a289.firebaseio.com/prod/guest/%s.json", event_id);
    queue_request(url, NULL, headers, &event_goscie, false);
}

KonwencikEvent* KonwencikBazaParser(const char* json_string, size_t* out_count) {
    cJSON *root = cJSON_Parse(json_string);
    if (!root) {
        *out_count = 0;
        return NULL;
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        *out_count = 0;
        return NULL;
    }

    size_t total_items = cJSON_GetArraySize(root);
    KonwencikEvent *events = malloc(total_items * sizeof(KonwencikEvent));
    if (!events) {
        cJSON_Delete(root);
        *out_count = 0;
        return NULL;
    }
    memset(events, 0, total_items * sizeof(KonwencikEvent));

    cJSON *value = NULL;
    size_t filtered_index = 0;

    cJSON_ArrayForEach(value, root) {
        if (cJSON_IsTrue(cJSON_GetObjectItem(value, "show_conference")) != 1) continue; 
        const char* current_code = get_json_string(value, "code");
        
        #ifdef PYRKON
            if (current_code == NULL || strstr(current_code, "pyrkon") == NULL) continue; 
        #endif

        if (value->string) strncpy(events[filtered_index].id, value->string, sizeof(events[filtered_index].id) - 1);
        
        strncpy(events[filtered_index].name, get_json_string(value, "name"), sizeof(events[filtered_index].name) - 1);
        strncpy(events[filtered_index].code, current_code, sizeof(events[filtered_index].code) - 1);
        strncpy(events[filtered_index].start_day, get_json_string(value, "start_day"), sizeof(events[filtered_index].start_day) - 1);
        strncpy(events[filtered_index].end_day, get_json_string(value, "end_day"), sizeof(events[filtered_index].end_day) - 1);
        strncpy(events[filtered_index].location_text, get_json_string(value, "location_text"), sizeof(events[filtered_index].location_text) - 1);
        strncpy(events[filtered_index].price, get_json_string(value, "price"), sizeof(events[filtered_index].price) - 1);

        const char* desc = get_json_string(value, "description");
        events[filtered_index].description = (desc && strlen(desc) > 0) ? sanitize_markdown(desc) : NULL;
        
        const char* loc_url = get_json_string(value, "location_url");
        events[filtered_index].location_url = (loc_url && strlen(loc_url) > 0) ? strdup(loc_url) : NULL;

        const char* poster_url = get_json_string(value, "poster_file_url");
        events[filtered_index].poster_file_url = (poster_url && strlen(poster_url) > 0) ? strdup(poster_url) : NULL;

        cJSON *j_tt = cJSON_GetObjectItemCaseSensitive(value, "isTimetablePublished");
        if (cJSON_IsBool(j_tt)) events[filtered_index].isTimetablePublished = cJSON_IsTrue(j_tt);

        cJSON *j_days = cJSON_GetObjectItemCaseSensitive(value, "days");
        if (cJSON_IsArray(j_days)) {
            events[filtered_index].days_count = cJSON_GetArraySize(j_days);
            events[filtered_index].days = malloc(events[filtered_index].days_count * sizeof(Dzien_wyd));
            size_t d_idx = 0;
            cJSON *d_val = NULL;
            cJSON_ArrayForEach(d_val, j_days) {
                cJSON *j_day = cJSON_GetObjectItemCaseSensitive(d_val, "day");
                if(cJSON_IsNumber(j_day)) events[filtered_index].days[d_idx].day = j_day->valueint;
                strncpy(events[filtered_index].days[d_idx].text, get_json_string(d_val, "text"), 31);
                d_idx++;
            }
        }

        cJSON *j_maps = cJSON_GetObjectItemCaseSensitive(value, "venue_map");
        if (cJSON_IsArray(j_maps)) {
            events[filtered_index].venue_map_count = cJSON_GetArraySize(j_maps);
            events[filtered_index].venue_maps = malloc(events[filtered_index].venue_map_count * sizeof(Mapka));
            size_t m_idx = 0;
            cJSON *m_val = NULL;
            cJSON_ArrayForEach(m_val, j_maps) {
                const char* title = get_json_string(m_val, "title");
                events[filtered_index].venue_maps[m_idx].title = (title && strlen(title) > 0) ? strdup(title) : NULL;
                const char* file = get_json_string(m_val, "file");
                events[filtered_index].venue_maps[m_idx].file = (file && strlen(file) > 0) ? strdup(file) : NULL;
                events[filtered_index].venue_maps[m_idx].runtime_data.loaded = false;
                m_idx++;
            }
        }
        filtered_index++;
    }

    *out_count = filtered_index;
    if (filtered_index > 0) {
        qsort(events, filtered_index, sizeof(KonwencikEvent), compare_newest_first);
    } else {
        free(events);
        events = NULL;
    }

    cJSON_Delete(root);
    return events;
}

void KonwencikBazaFree(KonwencikEvent* events, size_t count) {
    if (!events) return;
    for (size_t i = 0; i < count; i++) {
        if (events[i].description) free(events[i].description);
        if (events[i].location_url) free(events[i].location_url);
        if (events[i].poster_file_url) free(events[i].poster_file_url);
        if (events[i].logo_file_url) free(events[i].logo_file_url);
        if (events[i].days) free(events[i].days);
        
        if (events[i].venue_maps) {
            for (size_t j = 0; j < events[i].venue_map_count; j++) {
                if (events[i].venue_maps[j].title) free(events[i].venue_maps[j].title);
                if (events[i].venue_maps[j].file) free(events[i].venue_maps[j].file);
                Konwencik_FreeMapData(&events[i].venue_maps[j]);
            }
            free(events[i].venue_maps);
        }
    }
    free(events);
}

int Konwencik_QRDaneDecode(const char* input, int* out_ids, int max_ids) {
    if (!input || !out_ids) return -1;
    int len = strlen(input);
    const char* payload_start = NULL;
    for (int i = 0; i <= len - 4; i++) {
        if (isdigit((unsigned char)input[i]) && isdigit((unsigned char)input[i+1]) && 
            isdigit((unsigned char)input[i+2]) && isdigit((unsigned char)input[i+3])) {
            payload_start = &input[i + 4];
            break;
        }
    }
    if (!payload_start || *payload_start == '\0') return -1;
    int stride = *payload_start - '0';
    if (stride < 1 || stride > 9) return -1;
    const char* data = payload_start + 1;
    int data_len = strlen(data);
    int count = 0;
    char chunk[16]; 
    for (int i = 0; i < data_len && count < max_ids; i += stride) {
        int current_chunk_size = (i + stride <= data_len) ? stride : (data_len - i);
        strncpy(chunk, &data[i], current_chunk_size);
        chunk[current_chunk_size] = '\0';
        char* endptr;
        out_ids[count] = (int)strtol(chunk, &endptr, 36);
        if (endptr != chunk + current_chunk_size) continue;
        count++;
    }
    return count;
}

void Konwencik_QRDaneEncode(const char* header, int* ids, int count, char* out_buf, size_t buf_size) {
    if (!header || !ids || !out_buf || count == 0) return;
    int max_id = 0;
    for (int i = 0; i < count; i++) {
        if (ids[i] > max_id) max_id = ids[i];
    }
    int stride = 1;
    if (max_id >= 36LL * 36 * 36) stride = 4;     
    else if (max_id >= 36 * 36) stride = 3;        
    else if (max_id >= 36) stride = 2;             
    else stride = 1;                               
    snprintf(out_buf, buf_size, "%s%d", header, stride);
    const char vars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char chunk[10];
    for (int i = 0; i < count; i++) {
        int val = ids[i];
        for (int s = stride - 1; s >= 0; s--) {
            chunk[s] = vars[val % 36];
            val /= 36;
        }
        chunk[stride] = '\0';
        strncat(out_buf, chunk, buf_size - strlen(out_buf) - 1);
    }
}

static int compare_program_items(const void* a, const void* b) {
    KonwencikProgramItem* itemA = (KonwencikProgramItem*)a;
    KonwencikProgramItem* itemB = (KonwencikProgramItem*)b;
    if (itemA->date_val != itemB->date_val) return itemA->date_val - itemB->date_val;
    return strcmp(itemA->start, itemB->start);
}

static inline void fast_strcopy(char* dst, const char* src, size_t max_len) {
    if (!src) { dst[0] = '\0'; return; }
    size_t i = 0;
    for (; i < max_len - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0'; 
}

KonwencikEvent* global_selected_event = NULL;
KonwencikProgramItem konwencik_program_buffer[MAX_PROGRAM_ITEMS];

static inline int fast_atoi2(const char* s) { return (s[0] - '0') * 10 + (s[1] - '0'); }
static inline int fast_atoi4(const char* s) {
    return (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
}

KonwencikProgramItem* KonwencikProgramParser(const char* json_string, size_t json_length, size_t* out_count) {
    if (!json_string || !out_count) return NULL;
    size_t out_index = 0;
    const char *ptr = json_string;
    log_to_file("Parsing program JSON, length: %zu\n", json_length);
    while ((ptr = strstr(ptr, "{\"block\":")) != NULL && out_index < MAX_PROGRAM_ITEMS) {
        const char *end_of_obj = strchr(ptr, '}');
        if (!end_of_obj) break;
        size_t obj_len = (end_of_obj - ptr) + 1;
        char *temp_chunk = malloc(obj_len + 1);
        if (temp_chunk) {
            memcpy(temp_chunk, ptr, obj_len);
            temp_chunk[obj_len] = '\0';
            cJSON *entry = cJSON_Parse(temp_chunk);
            if (entry) {
                KonwencikProgramItem *item = &konwencik_program_buffer[out_index];
                item->program_exists = true;
                item->liked = false;
                item->id = 0;
                item->date_val = 0;
                cJSON *child = entry->child;
                while (child) {
                    const char* key = child->string;
                    const char* val = child->valuestring;
                    if (!key) { child = child->next; continue; }
                    switch (key[0]) {
                        case 'i': if (key[1] == 'd') item->id = child->valueint; break;
                        case 't': 
                            if (val) {
                                if (key[1] == 'i') fast_strcopy(item->title, val, sizeof(item->title));
                                
                                else if (key[1] == 'y') fast_strcopy(item->type, val, sizeof(item->type));
                            }
                            break;
                        case 's':
                            if (val) {
                                if (strcmp(key, "start") == 0) fast_strcopy(item->start, val, sizeof(item->start));
                                else if (strcmp(key, "speaker") == 0) fast_strcopy(item->speaker, val, sizeof(item->speaker));
                            }
                            break;
                        case 'e': if (val) fast_strcopy(item->end, val, sizeof(item->end)); break;
                        case 'r': if (val && strcmp(key, "room") == 0) fast_strcopy(item->room, val, sizeof(item->room)); break;
                        case 'd': 
                            if (val) {
                                if (key[1] == 'e') fast_strcopy(item->description, val, sizeof(item->description));
                                else {
                                    fast_strcopy(item->day, val, sizeof(item->day));
                                    int y, m, d;
                                    if (item->day[4] == '-') { 
                                        y = fast_atoi4(item->day); m = fast_atoi2(item->day + 5); d = fast_atoi2(item->day + 8);
                                    } else { 
                                        d = fast_atoi2(item->day); m = fast_atoi2(item->day + 3); y = fast_atoi4(item->day + 6);
                                    }
                                    item->date_val = (y * 10000) + (m * 100) + d;
                                }
                            }
                            break;
                    }
                    child = child->next;
                }
                out_index++;
                cJSON_Delete(entry);
            }
            free(temp_chunk);
        }
        ptr = end_of_obj; 
    }
    log_to_file("Finished parsing program JSON. Total items parsed: %zu\n", out_index);
    if (out_index > 1) qsort(konwencik_program_buffer, out_index, sizeof(KonwencikProgramItem), compare_program_items);
    *out_count = out_index;
    program_count = out_index;
    return konwencik_program_buffer;
}

void KonwencikProgramFree(KonwencikProgramItem* items, size_t count) {
    (void)items;
    (void)count;
}

void Konwencik_ApplyLikesFromQR(int* scanned_ids, int scanned_count, KonwencikProgramItem* items, size_t program_count) {
    if (!scanned_ids || !items || scanned_count <= 0) return;
    for (int i = 0; i < scanned_count; i++) {
        int current_id = scanned_ids[i];
        for (size_t j = 0; j < program_count; j++) {
            if (items[j].id == current_id) {
                items[j].liked = true;
                break;
            } 
        }
    }
}

void FS_CreateKonwencikBase(){
    fsInit();
    FS_Archive sdmcArchive;
    FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    FS_Path folderPath = fsMakePath(PATH_ASCII, "/3ds/Konwencik3DS");
    czyFolderIstnieje(sdmcArchive, folderPath);
    FSUSER_CloseArchive(sdmcArchive);
    fsExit();
}



static inline u32 next_pow2(u32 v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static inline u32 part1by1(u32 x) {
    x &= 0xFF;
    x = (x | (x << 4)) & 0x0F0F;
    x = (x | (x << 2)) & 0x3333;
    x = (x | (x << 1)) & 0x5555;
    return x;
}

static inline u32 get_morton_offset(u32 x, u32 y, u32 width) {
    u32 block_x = x >> 3;
    u32 block_y = y >> 3;

    u32 block_idx = block_y * (width >> 3) + block_x;

    u32 px = x & 7;
    u32 py = y & 7;

    u32 i = (part1by1(px) | (part1by1(py) << 1));

    return block_idx * 64 + i;
}

void Konwencik_ProcessMapData(Mapka* map, const uint8_t* image_data, size_t size) {
    if (!map || !image_data) return;
    log_to_file("Map size: %zu bytes\n", size);

    if (map->runtime_data.loaded) {
        Konwencik_FreeMapData(map);
    }

    ImageStream stream;
    if (!ImageStream_Open(&stream, image_data, size)) {
        log_to_file("Unsupported image format or corrupt header!\n");
        return;
    }

    int w = stream.width;
    int h = stream.height;
    log_to_file("Decoded image dimensions: %dx%d\n", w, h);

    const int CHUNK_MAX = 128; 
    int chunks_x = (w + CHUNK_MAX - 1) / CHUNK_MAX;
    int chunks_y = (h + CHUNK_MAX - 1) / CHUNK_MAX;
    
    log_to_file("Allocating chunks array...\n");

    map->runtime_data.chunks = calloc(chunks_x * chunks_y, sizeof(MapChunk));
    if (!map->runtime_data.chunks) {
        log_to_file("Failed to allocate memory for map chunks!\n");
        ImageStream_Close(&stream);
        return;
    }

    map->runtime_data.num_chunks_x = chunks_x;
    map->runtime_data.num_chunks_y = chunks_y;
    map->runtime_data.total_width = w;
    map->runtime_data.total_height = h;

    uint8_t* strip_buffer = malloc(w * CHUNK_MAX * 4);
    if (!strip_buffer) {
        log_to_file("Failed to allocate memory for strip buffer!\n");
        free(map->runtime_data.chunks); 
        map->runtime_data.chunks = NULL;
        ImageStream_Close(&stream);
        return;
    }

    int current_chunk = 0;
    bool allocation_failed = false;

    for (int cy = 0; cy < chunks_y; cy++) {
        log_to_file("Processing chunk row %d/%d...\n", cy + 1, chunks_y);
        
        int rows_to_read = (cy == chunks_y - 1 && h % CHUNK_MAX != 0) ? (h % CHUNK_MAX) : CHUNK_MAX;
        
        if (!ImageStream_ReadRows(&stream, strip_buffer, rows_to_read)) {
            log_to_file("Failed to read image rows!\n");
            break; 
        }

        for (int cx = 0; cx < chunks_x; cx++) {
            int chunk_w = (cx == chunks_x - 1 && w % CHUNK_MAX != 0) ? (w % CHUNK_MAX) : CHUNK_MAX;
            int chunk_h = rows_to_read; 

            int tex_w = next_pow2(chunk_w);
            int tex_h = next_pow2(chunk_h);
            if (tex_w < 8) tex_w = 8;
            if (tex_h < 8) tex_h = 8;

            MapChunk* chunk = &map->runtime_data.chunks[current_chunk];
            chunk->width = chunk_w;
            chunk->height = chunk_h;
            chunk->x_offset = cx * CHUNK_MAX;
            chunk->y_offset = cy * CHUNK_MAX;

            
            if (allocation_failed) {
                chunk->tex.data = NULL;
                current_chunk++;
                continue;
            }

            if (!C3D_TexInit(&chunk->tex, tex_w, tex_h, GPU_RGB565)) {
                log_to_file("Failed to init texture %d. Out of VRAM/Linear Heap!\n", current_chunk);
                chunk->tex.data = NULL; 
                allocation_failed = true; 
                current_chunk++;
                continue; 
            }

            C3D_TexSetFilter(&chunk->tex, GPU_LINEAR, GPU_LINEAR);
            C3D_TexSetWrap(&chunk->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

            u16* dest = (u16*)chunk->tex.data;

            for (int y = 0; y < tex_h; y++) {
                for (int x = 0; x < tex_w; x++) {
                    u32 morton_idx = get_morton_offset(x, y, tex_w);

                    if (x < chunk_w && y < chunk_h) {
                        int src_x = chunk->x_offset + x;
                        int src_y = y;

                        int src_idx = (src_y * w + src_x) * 4;

                        u8 r = strip_buffer[src_idx + 0];
                        u8 g = strip_buffer[src_idx + 1];
                        u8 b = strip_buffer[src_idx + 2];

                        dest[morton_idx] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                    } else {
                        dest[morton_idx] = 0;
                    }
                }
            }

            chunk->subtex.width = chunk_w;
            chunk->subtex.height = chunk_h;
            chunk->subtex.left = 0.0f;
            chunk->subtex.right = (float)chunk_w / (float)tex_w;
            chunk->subtex.top = (float)chunk_h / (float)tex_h;
            chunk->subtex.bottom = 0.0f;

            current_chunk++;
        }
    }
    
    log_to_file("Finished processing all chunks. Total chunks: %d\n", current_chunk);
    
    free(strip_buffer);
    ImageStream_Close(&stream);
    map->runtime_data.loaded = true;
}

void Konwencik_FreeMapData(Mapka* map) {
    if (!map || !map->runtime_data.loaded || !map->runtime_data.chunks)
        return;

    log_to_file("Freeing map data textures...\n");

    int total = map->runtime_data.num_chunks_x * map->runtime_data.num_chunks_y;

    for (int i = 0; i < total; i++) {
        C3D_Tex* tex = &map->runtime_data.chunks[i].tex;

        
        
        if (tex && tex->data != NULL) {
            C3D_TexDelete(tex);
            memset(tex, 0, sizeof(C3D_Tex));
        }
    }

    free(map->runtime_data.chunks);
    map->runtime_data.chunks = NULL;
    map->runtime_data.loaded = false;
}