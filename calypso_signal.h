#ifndef _CALYPSO_SIGNAL_H_
#define _CALYPSO_SIGNAL_H_
#include <time.h>

bool need_reload(time_t last_time);
bool need_stop();
void set_reload_time();
void set_stop_sig();
void clear_reload_time();
void clear_stop_sig();
#endif
