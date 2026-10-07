#ifndef MPP_ALLOCATOR_H
#define MPP_ALLOCATOR_H

#include <rockchip/mpp_buffer.h>
#include <cstddef>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

struct PoolConfig {
    std::size_t buffer_size{0};
    int count{0};
    MppBufferType type{static_cast<MppBufferType>
        (MPP_BUFFER_TYPE_DRM | MPP_BUFFER_FLAGS_CACHABLE)};
};

class BufferPool {
public:
    int init(const PoolConfig& cfg);
    BufferPool() = default; 
    BufferPool(const BufferPool&) = delete;
    BufferPool(BufferPool&&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&&) = delete;

    ~BufferPool();

    void* ptr(int id) const;
    int fd(int id) const;
    std::size_t size() const;
    MppBuffer buffer(int id) const;
    int count() const;
    
private:
    MppBufferGroup group{nullptr};
    std::vector<MppBuffer> bufs{};
    PoolConfig cfg{};
};

class MppAllocator  {
public:

    MppAllocator(const MppAllocator&) = delete;
    MppAllocator& operator=(const MppAllocator&) = delete;
    MppAllocator(MppAllocator&&) = delete;
    MppAllocator& operator=(MppAllocator&&) = delete;
    static int instance(MppAllocator** out);

    int alloc(const std::string& name, const PoolConfig& cfg);
    int free(const std::string& name);
    BufferPool* get(const std::string& name);

private:
    MppAllocator() = default;
    std::unordered_map<std::string, std::unique_ptr<BufferPool>> pools{};
    MppBufferType type{static_cast<MppBufferType>
        (MPP_BUFFER_TYPE_DRM | MPP_BUFFER_FLAGS_CACHABLE)};
};

#endif // MPP_ALLOCATOR_H