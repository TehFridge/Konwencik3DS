#include "request.h"

#define MAX_QUEUE 10
bool doing_debug_logs;
static Request request_queue[MAX_QUEUE];
static int request_count = 0;
static LightLock request_lock;
static bool request_running = true;
static Thread request_thread;
static LightEvent request_event;

bool youfuckedup = false;
bool czasfuckup = false;
bool requestdone = false;
bool need_to_load_image = false;
bool loadingshit = false;
long response_code = 0;

LightLock global_response_lock;

// the stuff where the converted png/jpg goes
C2D_SpriteSheet kuponobraz; 
C2D_Image kuponkurwa;
bool obrazekdone = false;

static inline u32 next_pow2(u32 n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
static inline u32 clamp(u32 n, u32 lower, u32 upper) {
    return n < lower ? lower : (n > upper ? upper : n);
}
static inline u32 rgba_to_abgr(u32 px) {
    u8 r = px & 0xff;
    u8 g = (px >> 8) & 0xff;
    u8 b = (px >> 16) & 0xff;
    u8 a = (px >> 24) & 0xff;
    return (a << 24) | (b << 16) | (g << 8) | r;
}


size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    if (!ptr || !userdata) return 0;

    size_t total_size = size * nmemb;
    ResponseBuffer *buf = (ResponseBuffer *)userdata;

    // Explicitly keep it not done while we are still receiving chunks
    buf->done = false; 

    size_t needed_size = buf->size + total_size + 1;
    char *new_data = realloc(buf->data, needed_size);
    if (!new_data) {
        log_to_file("[write_callback] ERROR: realloc failed");
        return 0;
    }

    buf->data = new_data;
    memcpy(buf->data + buf->size, ptr, total_size);
    buf->size += total_size;
    buf->data[buf->size] = '\0'; 

    return total_size;
}

void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void log_request_to_file(const char *url, const char *data, struct curl_slist *headers, char *response) {
    FILE *log_file = fopen("request_log.txt", "a");
    if (log_file) {
        fprintf(log_file, "URL: %s\n", url);
        fprintf(log_file, "Data: %s\n", data ? data : "(None)");
        fprintf(log_file, "Headers:\n");
        struct curl_slist *header = headers;
        while (header) {
            fprintf(log_file, "  %s\n", header->data);
            header = header->next;
        }
        fprintf(log_file, "Response: %s\n", response ? response : "(None)");
        fprintf(log_file, "------------------------\n");
        fclose(log_file);
    } else {
        printf("Failed to open log file for writing.\n");
    }
}

int my_curl_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
    switch (type) {
        case CURLINFO_TEXT:
        case CURLINFO_HEADER_IN:
        case CURLINFO_HEADER_OUT:
        case CURLINFO_DATA_IN:
        case CURLINFO_DATA_OUT:
            if (!doing_debug_logs) {
                log_to_file("[curl_debug] %.*s", (int)size, data);
            }
            break;
        default:
            break;
    }
    return 0;
}

// func that actually pushes the request
bool refresh_data(CURL *curl, const char *url, const char *data, struct curl_slist *headers, ResponseBuffer *response_buf)  {
    bool request_failed = false;
    bool retried_after_401 = false;

retry_request:
    youfuckedup = false;
    czasfuckup = false;
    requestdone = false;
    loadingshit = true;

    if (!url || url[0] == '\0' || !response_buf) {
        if (!doing_debug_logs) {
            log_to_file("[refresh_data] ERROR: Missing URL or response buffer.");
        }
        return true;
    }
    if (!doing_debug_logs) {
        log_to_file("[refresh_data] --- HTTP REQUEST BEGIN ---");
        log_to_file("[refresh_data] URL: %s", url);
    }

    if (!doing_debug_logs) {
        for (struct curl_slist *h = headers; h != NULL; h = h->next)
            log_to_file("  %s", h->data);
    }

    if (!doing_debug_logs) {
        if (data && strcmp(url, "https://zabka-snrs.zabka.pl/v4/server/time") != 0) {
            log_to_file("[refresh_data] Body:\n%s", data);
        } else {
            log_to_file("[refresh_data] Body: (none or skipped)");
        }
    }

    if (response_buf->data) {
        free(response_buf->data);
        response_buf->data = NULL;
    }
    response_buf->size = 0;

    curl_easy_reset(curl);

    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);

    if (data && (strstr(url, "v1") || strstr(url, "collect"))) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buf);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (res != CURLE_OK) {
        request_failed = true;
        response_buf->done = false; // Ensure it's not marked done on failure
    } else {
        // Only here is the data TRULY downloaded and the buffer full
        response_buf->done = true; 
    }

    loadingshit = false;
    requestdone = true; // Global flag

    if (response_code == 0) czasfuckup = true;

    // if (response_code == 401 && !retried_after_401) {
    //     if (!doing_debug_logs) {
    //         log_to_file("[refresh_data] Got 401, refreshing token and retrying...");
    //     }

    //     struct curl_slist *refresh_headers = NULL;
    //     refresh_headers = curl_slist_append(refresh_headers, "Content-Type: application/json");
    //     refresh_headers = curl_slist_append(refresh_headers, "User-Agent: InPost-Mobile/3.46.0(34600200) (Horizon 11.17.0-50U; AW715988204; Nintendo 3DS; pl)");

    //     char refresh_request[256];
    //     snprintf(refresh_request, sizeof(refresh_request), "{\"refreshToken\":\"%s\"}", refreshToken);

    //     if (get_refresh_token.data) {
    //         free(get_refresh_token.data);
    //         get_refresh_token.data = NULL;
    //         get_refresh_token.size = 0;
    //     }

    //     refresh_data("https://api-inmobile-pl.easypack24.net/v1/authenticate", refresh_request, refresh_headers, &get_refresh_token);

    //     curl_slist_free_all(refresh_headers);

    //     if (get_refresh_token.data) {
    //         parseRefreshedToken(get_refresh_token.data);
    //     }


    //     headers = NULL; 
    //     struct curl_slist *retry_headers = NULL;
    //     char authheader[2048];
        

    //     snprintf(authheader, sizeof(authheader), "Authorization: %s", authToken);
        
    //     retry_headers = curl_slist_append(retry_headers, "Content-Type: application/json");
    //     retry_headers = curl_slist_append(retry_headers, "User-Agent: InPost-Mobile/3.46.0(34600200) (Horizon 11.17.0-50U; AW715988204; Nintendo 3DS; pl)");
    //     retry_headers = curl_slist_append(retry_headers, authheader);
        
    //     headers = retry_headers;

    //     retried_after_401 = true;
    //     goto retry_request;
    // }

    if ((strstr(url, ".png") || strstr(url, ".jpeg") || strstr(url, ".jpg")) &&
        response_buf->data && response_buf->size > 0) {
        need_to_load_image = true;
    }

    if (retried_after_401 && headers) {
        curl_slist_free_all(headers);
    }
    if (!doing_debug_logs) {
        log_to_file("[refresh_data] Request Done");
    }
    return request_failed;
}

// queue stuff

void request_worker(void* arg) {
    ResponseBuffer local_buf = {NULL, 0, false};
    
    CURL *worker_curl = curl_easy_init();
    if (!worker_curl) {
        log_to_file("[request_worker] FATAL: Failed to init cURL");
        return;
    }

    while (request_running) {
        LightLock_Lock(&request_lock);
        if (request_count == 0) {
            LightLock_Unlock(&request_lock);
            LightEvent_Wait(&request_event);
            continue;
        }

        Request req = request_queue[0];
        memmove(request_queue, request_queue + 1, (request_count - 1) * sizeof(Request));
        request_count--;
        LightLock_Unlock(&request_lock);

        local_buf.data = NULL;
        local_buf.size = 0;
        local_buf.done = false;

        ResponseBuffer *buf = req.response_buf;
        if (!buf) {
            buf = &local_buf;
        }

        if (req.url[0] == '\0') {
            log_to_file("[request_worker] ERROR: request has no URL");
            continue;
        }

        refresh_data(worker_curl, req.url, req.data, req.headers, buf);

        if (req.response && req.response_size && buf->data) {
            *req.response = buf->data;
            *req.response_size = buf->size;
        } else if (!req.response_buf && buf->data) {
            free(buf->data);
        }

        if (req.owns_data && req.data) free(req.data);
        if (req.headers) curl_slist_free_all(req.headers);

        
        if (requestdone && need_to_load_image && buf->data && buf->size > 0) {
            // tu był kod w inpost oryginalnie ale teraz jest gdzie indziej :p
        }

        if (req.response_buf) {
            req.response_buf->done = true;
        }
    }
    curl_easy_cleanup(worker_curl);
}

//pretty self-explanatory
void queue_request(const char *url, const char *data, struct curl_slist *headers,
                   ResponseBuffer *response_buf, bool is_binary) {
    if (!url || url[0] == '\0' || !response_buf) {
        log_to_file("[queue_request] ERROR: Missing critical args");
        return;
    }

    if (response_buf) {
        response_buf->done = false;
        // Optional: free old data here to ensure a clean slate
        if(response_buf->data) {
            free(response_buf->data);
            response_buf->data = NULL;
            response_buf->size = 0;
        }
    }
    LightLock_Lock(&request_lock);

    if (request_count >= MAX_QUEUE) {
        log_to_file("[queue_request] ERROR: Request queue full");
        LightLock_Unlock(&request_lock);
        return;
    }

    Request *req = &request_queue[request_count];
    memset(req, 0, sizeof(Request));

    strncpy(req->url, url, sizeof(req->url) - 1);
    req->url[sizeof(req->url) - 1] = '\0';

    if (data) {
        req->data = strdup(data);
        if (!req->data) {
            log_to_file("[queue_request] ERROR: strdup(data) failed");
            LightLock_Unlock(&request_lock);
            return;
        }
        req->owns_data = true;
    }

    req->headers = headers;
    req->response_buf = response_buf;
    req->is_binary = is_binary;

    request_count++;
    LightEvent_Signal(&request_event);
    LightLock_Unlock(&request_lock);
}

// thread stuff (if u want threaded requests, run this at the beggining/end of ur app)
void start_request_thread() {
    LightLock_Init(&request_lock);
    LightLock_Init(&global_response_lock);
    LightEvent_Init(&request_event, RESET_ONESHOT);
    request_thread = threadCreate(request_worker, NULL, 32 * 1024, 0x30, -2, false);
}

void stop_request_thread() {
    LightLock_Lock(&request_lock);
    request_running = false;
    LightEvent_Signal(&request_event);
    LightLock_Unlock(&request_lock);

    threadJoin(request_thread, UINT64_MAX);
    threadFree(request_thread);
}
