#include "ByteBuffer.hpp"

ByteBufferConstWrapper::ByteBufferConstWrapper(const uint8_t* buffer, size_t len)
    : ReadableStream(buffer, len)
{
}

ByteBufferConstWrapper::~ByteBufferConstWrapper()
{
}

ByteBufferWrapper::ByteBufferWrapper(uint8_t* buffer, size_t len)
    : ReadableStream(buffer, len)
    , WritableStream(buffer, len)
{
}

ByteBufferWrapper::~ByteBufferWrapper()
{
}

ByteBufferArrayWrapper::ByteBufferArrayWrapper(const wb::Array<uint8_t>& array)
    : ByteBufferConstWrapper(array.begin(), array.size())
{
}

ByteBufferArrayWrapper::~ByteBufferArrayWrapper()
{
}