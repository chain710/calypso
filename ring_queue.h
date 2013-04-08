#ifndef _RING_QUEUE_H_
#define _RING_QUEUE_H_

class ring_queue_t
{
public:
#pragma pack(push) //保存原对齐状态
#pragma pack(1)
    struct ring_block_t
    {
        unsigned stx_:2;
        unsigned len_:30;
    };
#pragma pack(pop)//恢复对齐状态

    ring_queue_t();
    ring_queue_t(const ring_queue_t& c);
    virtual ~ring_queue_t();
    int initialize(int len);
    void finalize();
    // 预留数据块，等待append，返回 <0错误
    int produce_reserve(int len);
    // 拷贝附加数据到队列
    int produce_append(const char* data, int len);
    // 拷贝数据到队列，返回错误码
    int produce(const char* data, int len);
    // 从队列拷贝数据，返回实际消费长度
    int consume(char* data, int len);
    // 获取下一个消费数据块的长度，不包含sizeof(ring_queue_t)
    int get_consume_len() const;
    // 跳过下个消费数据包，返回跳过的长度
    int skip_consume();
    int get_free_len() const;
    int get_used_len() const;
    void clear();
private:
    void produce_bytes(int pos, const char* data, int len);
    void consume_bytes(char* data, int len);
    void skip_consume_bytes(int len);
    // 结束拷贝，移动write指针
    void produce_finish();
    void extract_rb(ring_block_t& rb, int from) const;
    inline int normalize_pos(int pos) const
    {
        pos =  pos > buffer_len_? (pos - buffer_len_): pos;
        if (pos == buffer_len_) pos = 0;
        return pos;
    }

    int buffer_len_;
    char* buffer_;  // buffer_len_长，可用buffer_len_-1
    int read_;
    int write_;
};

#endif
