#ifdef __SWITCH__
#include <switch.h>
#include "switch_display.h"

static bool s_have_last_resolution = false;
static int s_last_width = 0;
static int s_last_height = 0;

bool switch_get_display_resolution(int *w, int *h) {
    s32 display_w = 0;
    s32 display_h = 0;
    Result rc = appletGetDefaultDisplayResolution(&display_w, &display_h);
    if (R_FAILED(rc) || display_w <= 0 || display_h <= 0) {
        return false;
    }

    if (w) *w = static_cast<int>(display_w);
    if (h) *h = static_cast<int>(display_h);
    return true;
}

bool switch_check_display_change(int *w, int *h) {
    int current_w = 0;
    int current_h = 0;
    if (!switch_get_display_resolution(&current_w, &current_h)) return false;

    if (!s_have_last_resolution) {
        s_last_width = current_w;
        s_last_height = current_h;
        s_have_last_resolution = true;
        return false;
    }

    if (current_w == s_last_width && current_h == s_last_height) return false;

    s_last_width = current_w;
    s_last_height = current_h;
    if (w) *w = current_w;
    if (h) *h = current_h;
    return true;
}
#endif
