#pragma once
#include "Stream.hpp"

class ByteBufferConstWrapper : public ReadableStream
{
public:
    ByteBufferConstWrapper(const uint8_t* buffer, size_t len);
    virtual ~ByteBufferConstWrapper();
};

class ByteBufferWrapper : public ReadableStream, public WritableStream
{
public:
    ByteBufferWrapper(uint8_t* buffer, size_t len);
    virtual ~ByteBufferWrapper();
};

template<size_t Size>
class ByteBuffer : public ByteBufferWrapper
{
private:
    uint8_t m_buffer[Size];

public:
    ByteBuffer() : ByteBufferWrapper(m_buffer, Size) {}
    virtual ~ByteBuffer() {}
};
