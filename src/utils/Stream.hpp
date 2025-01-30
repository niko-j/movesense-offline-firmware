#pragma once
#include <wb-resources/resources.h>

class WritableStream
{
private:
    uint8_t* m_write_ptr;
    size_t m_write_pos;
    size_t m_write_size;

public:
    WritableStream(uint8_t* buffer, size_t len);
    virtual ~WritableStream();

    bool write(const void* src, size_t len);
    bool seek_write(size_t pos);
    uint8_t* get_write_ptr() const;
    size_t get_write_pos() const;
    size_t get_write_size() const;
    void reset();
    wb::Array<uint8_t> asArray() const;
};

class ReadableStream
{
private:
    const uint8_t* m_read_ptr;
    size_t m_read_pos;
    size_t m_read_size;

public:
    ReadableStream(const uint8_t* buffer, size_t len);
    virtual ~ReadableStream();

    bool read(void* dst, size_t len);
    bool write_to(WritableStream& stream);
    bool seek_read(size_t pos);
    const uint8_t* get_read_ptr() const;
    size_t get_read_pos() const;
    size_t get_read_size() const;
};
