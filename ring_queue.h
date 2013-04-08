#ifndef _RING_QUEUE_H_
#define _RING_QUEUE_H_

class ring_queue_t
{
public:
#pragma pack(push) //����ԭ����״̬
#pragma pack(1)
    struct ring_block_t
    {
        unsigned stx_:2;
        unsigned len_:30;
    };
#pragma pack(pop)//�ָ�����״̬

    ring_queue_t();
    ring_queue_t(const ring_queue_t& c);
    virtual ~ring_queue_t();
    int initialize(int len);
    void finalize();
    // Ԥ�����ݿ飬�ȴ�append������ <0����
    int produce_reserve(int len);
    // �����������ݵ�����
    int produce_append(const char* data, int len);
    // �������ݵ����У����ش�����
    int produce(const char* data, int len);
    // �Ӷ��п������ݣ�����ʵ�����ѳ���
    int consume(char* data, int len);
    // ��ȡ��һ���������ݿ�ĳ��ȣ�������sizeof(ring_queue_t)
    int get_consume_len() const;
    // �����¸��������ݰ������������ĳ���
    int skip_consume();
    int get_free_len() const;
    int get_used_len() const;
    void clear();
private:
    void produce_bytes(int pos, const char* data, int len);
    void consume_bytes(char* data, int len);
    void skip_consume_bytes(int len);
    // �����������ƶ�writeָ��
    void produce_finish();
    void extract_rb(ring_block_t& rb, int from) const;
    inline int normalize_pos(int pos) const
    {
        pos =  pos > buffer_len_? (pos - buffer_len_): pos;
        if (pos == buffer_len_) pos = 0;
        return pos;
    }

    int buffer_len_;
    char* buffer_;  // buffer_len_��������buffer_len_-1
    int read_;
    int write_;
};

#endif
