#ifndef _CALYPSO_INTERFACE_H_
#define _CALYPSO_INTERFACE_H_

#include "app_handler.h"

// ��app�㽻���Ľӿ�

// ����ctx�����Ϣ�ҵ���Ӧ��·����������
int calypso_send_msgpack_by_ctx(void* main_inst, msgpack_context_t ctx, const char* data, size_t len);

// ѡ��ָ��group���һ����·����������
int calypso_send_msgpack_by_group(void* main_inst, int group, const char* data, size_t len);

// �㲥��group���������·
int calypso_broadcast_msgpack_by_group(void* main_inst, int group, const char* data, size_t len);

// TODO: ע�ᶨʱ��

#endif
