#include "mpp_allocator.h"

int MppAllocator::instance(MppAllocator** out) {
    static MppAllocator obj;
    *out = &obj;
    return 0;
}

int MppAllocator::alloc(const std::string& name, const PoolConfig& cfg) {
    auto it = pools.find(name);
    if (it != pools.end()) return -1;
    
    auto pool = std::make_unique<BufferPool>();
    int ret = pool->init(cfg);

    if (ret != 0)
    return ret;

    pools.emplace(name, std::move(pool));
    return 0;
}

int MppAllocator::free(const std::string& name) {
    auto it = pools.find(name);
    if (it == pools.end()) return -1;

    pools.erase(name);
    return 0;
}

int BufferPool::init(const PoolConfig& cfg) {

    if (group) return -1;
    
    this->cfg = cfg;
    MPP_RET ret = mpp_buffer_group_get_internal(&group, cfg.type);
    if (ret != MPP_OK) return -1;

    bufs.resize(cfg.count, nullptr);
    for (int i = 0; i < cfg.count; ++i) {
        ret = mpp_buffer_get(group, &bufs[i], cfg.buffer_size);
        if (ret != MPP_OK) return -1;
    }

    return 0;
}

void* BufferPool::ptr(int id) const {
    return mpp_buffer_get_ptr(bufs[id]);
}

int BufferPool::fd(int id) const {
    return mpp_buffer_get_fd(bufs[id]);
}

int BufferPool::count() const {
    return cfg.count;
}

MppBuffer BufferPool::buffer(int id) const {
    return bufs.at(id);
}

BufferPool* MppAllocator::get(const std::string& name) {
    auto it = pools.find(name);
    if (it == pools.end()) return nullptr;
    return it->second.get();
}

BufferPool::~BufferPool() {
    for (auto& b : bufs) {
        if (b) mpp_buffer_put(b);
    }
    bufs.clear();

    if (group) {
        mpp_buffer_group_put(group);
        group = nullptr;
    }
}

// BufferHandle::~BufferHandle() {
//    // mpp_buffer_put()
// }