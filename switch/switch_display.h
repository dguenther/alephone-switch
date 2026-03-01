#pragma once
#ifdef __SWITCH__
// Gets current Switch display resolution.
// Returns false if the resolution query fails.
bool switch_get_display_resolution(int *w, int *h);

// Returns true and fills *w/*h when display resolution changed since last call.
bool switch_check_display_change(int *w, int *h);
#endif
