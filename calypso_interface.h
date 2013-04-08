#ifndef _CALYPSO_INTERFACE_H_
#define _CALYPSO_INTERFACE_H_

#include "app_handler.h"

// 和app层交互的接口

// 利用ctx里的信息找到对应链路，发送数据
int calypso_send_msgpack_by_ctx(void* main_inst, msgpack_context_t ctx, const char* data, size_t len);

// 选择指定group里的一个链路，发送数据
int calypso_send_msgpack_by_group(void* main_inst, int group, const char* data, size_t len);

// 广播到group里的所有链路
int calypso_broadcast_msgpack_by_group(void* main_inst, int group, const char* data, size_t len);

// TODO: 注册定时器

#endif
