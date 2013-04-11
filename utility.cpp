#include "utility.h"
#include "log_interface.h"
#include <fstream>
#include <time.h>

using namespace std;

int read_all_text( const char* file_path, std::string& text )
{
    if (NULL == file_path)
    {
        C_FATAL("config_path is %p", file_path);
        return -1;
    }

    fstream fin(file_path, ios_base::in);
    if (!fin.is_open())
    {
        C_ERROR("netlink config %s not opened, maybe not exist?", file_path);
        return -1;
    }

    char buf[1024];
    while (!fin.eof() && !fin.fail())
    {
        fin.read(buf, sizeof(buf));
        text.append(buf, fin.gcount());
    }

    fin.close();
    return 0;
}

int nrand(int rbeg, int rend)
{
    // incorrect, but enough for now
    if (rend <= rbeg)
    {
        return rbeg;
    }

    return (rand() % (rend - rbeg + 1)) + rbeg;
}

void init_rand()
{
    srand(time(NULL));
}
