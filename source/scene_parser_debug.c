#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <curl/curl.h>
#include "scene_parser_debug.h"
#include "scene_manager.h"
#include "konwencik_api.h"
#include "request.h"
#include "main.h"

// bool parser_parsed = false;
// KonwencikProgramItem* program_dbg = NULL;
// static size_t total_program_items = 0;

void sceneParserDebugInit(void) {
    // // 1. Setup console for debugging visibility
    // consoleInit(GFX_BOTTOM, NULL); 
    // parser_parsed = false;

    // // 2. Prepare the synchronous request
    // CURL *curl = curl_easy_init();
    // if (!curl) {
    //     printf("Failed to init CURL handle\n");
    //     return;
    // }

    // struct curl_slist *headers = NULL;
    // headers = curl_slist_append(headers, "Content-Type: application/json");
    // headers = curl_slist_append(headers, "User-Agent: Firebase/5/21.0.0/33/Horizon");

    // char url[256];
    // snprintf(url, sizeof(url), "https://conference-app-1a289.firebaseio.com/prod/presentation/pyrkon2025.json");

    // printf("Refreshing data via refresh_data...\n");

    // // 3. Execute synchronous request
    // // refresh_data returns true on failure
    // bool failed = refresh_data(curl, url, NULL, headers, &event_program);

    // if (failed) {
    //     printf("Network refresh failed.\n");
    // } else {
    //     printf("Network refresh success! Size: %zu\n", event_program.size);
    //     // refresh_data in request.c sets requestdone = true, but we should 
    //     // ensure event_program.done is set for our update loop.
    //     event_program.done = true; 
    // }

    // // 4. Cleanup local resources
    // curl_slist_free_all(headers);
    // curl_easy_cleanup(curl);
}

void sceneParserDebugUpdate(uint32_t kDown, uint32_t kHeld) {
    // // Follows the scene_event.c pattern: parse once data is ready
    // if (event_program.done && !parser_parsed) {
    //     if (event_program.data != NULL && event_program.size > 0) {
            
    //         printf("Parsing Program JSON...\n");
            
    //         // Call the parser directly (Main thread, no background task)
    //         program_dbg = KonwencikProgramParser(event_program.data, event_program.size, &total_program_items);

    //         if (program_dbg != NULL) {
    //             parser_parsed = true;
    //             printf("Successfully parsed %zu items!\n", total_program_items);
    //         } else {
    //             printf("Parser returned NULL - check JSON format.\n");
    //             parser_parsed = true; // Prevent infinite retry
    //         }
    //     }
    // }    
}

void sceneParserDebugRender(void) {
    // Console is handled by libctru via consoleInit
}

void sceneParserDebugExit(void) {
    // No specific exit logic needed for debug
}