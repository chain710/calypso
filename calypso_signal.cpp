#include "calypso_signal.h"

// Ϊ����app�߳�Ҳ�ܻ�ȡ�ź���Ϣ�������¼���ϴλ�ȡ�����źŵ�ʱ�䡣ͬʱ���߳�����Ҫ��¼����ʱ��
time_t reload_sig_time_ = 0;
time_t restart_app_sig_time_ = 0;
int stop_sig_ = 0;
int restart_sig_ = 0;

bool need_reload( time_t last_time )
{
    return reload_sig_time_ > last_time;
}

bool need_stop()
{
    return stop_sig_ > 0;
}

void set_reload_time()
{
    reload_sig_time_ = time(NULL);
}

void set_stop_sig()
{
    stop_sig_ = 1;
}

void clear_reload_time()
{
    reload_sig_time_ = 0;
}

void clear_stop_sig()
{
    stop_sig_ = 0;
}


