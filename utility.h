#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <string>
#include <netinet/in.h>

// read text from file
int read_all_text( const char* file_path, std::string& text );
// init rand with now time
void init_rand();
// return rand int in [rbeg, rend]
int nrand(int rbeg, int rend);
// get ip addr string
const char* get_addr_str(sockaddr_in addr, char* buf, int size);
#endif
