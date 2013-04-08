#ifndef _CALYPSO_UTIL_H_
#define _CALYPSO_UTIL_H_

#include <string>

int read_all_text( const char* file_path, std::string& text );
bool need_reload(time_t last_time);
bool need_stop();
bool need_restart_app();
void set_reload_time();
void set_stop_sig();
void set_restart_app_sig();
void clear_reload_time();
void clear_stop_sig();
void clear_restart_app_sig();

void init_rand();
// return rand int in [rbeg, rend]
int nrand(int rbeg, int rend);
#endif