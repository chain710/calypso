#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <string>
#include <netinet/in.h>
#include <time.h>

// read text from file
int read_all_text( const char* file_path, std::string& text );
// init rand with now time
void init_rand();
// return rand int in [rbeg, rend]
int nrand(int rbeg, int rend);
// get ip addr string
const char* get_addr_str(sockaddr_in addr, char* buf, int size);
// from time to string
void format_time(time_t time, const char* fmt, char *buf, size_t length);
// from string to time
time_t format_time(const char* time_str, const char* fmt);
#endif
