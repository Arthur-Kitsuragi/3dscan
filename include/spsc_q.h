#ifndef SPSC_Q_H
#define SPSC_Q_H

#include <cstdint>
#include <atomic>
#include <memory>
#include <functional>
#include <concepts>
#include <array>

template <typename T, std::size_t N>
requires (N > 1 && (N & (N - 1)) == 0)
class SPSC_q {
private:
    static constexpr std::size_t mask = N - 1;
    static constexpr std::size_t cache_line = 128;

    enum class state : uint8_t {
        empty,
        ready
    };

    struct alignas(cache_line) slot {
        slot() {}
        ~slot() {}

        union {
            T value;
        };
        std::atomic<state> st{state::empty}; 
    };

public:
    SPSC_q(): storage(std::make_unique<std::array<slot, N>> ()) {}
    ~SPSC_q() {
        for (std::size_t i = read_index; i != write_index; ++i) {
            slot &s = (*storage)[i & mask];
            if (s.st.load(std::memory_order_relaxed) == state::ready) {
                std::destroy_at(&s.value);
            }
        }
    }

    template <std::invocable<T*> F>
    [[nodiscard]]
    bool push(F&& builder) {
        slot& slot = (*storage)[write_index & mask];
        
        if (slot.st.load(std::memory_order_acquire) != state::empty) {
            return false;
        }

        std::invoke(std::forward<F>(builder), &slot.value);
        slot.st.store(state::ready, std::memory_order_release);

        event.fetch_add(2, std::memory_order_release);
        event.notify_one();

        ++write_index;

        return true;
    }

    [[nodiscard]]
    bool push(T value) {
        return push([&](T* place) {
            std::construct_at(place, std::move(value));
        });
    }

    template <std::invocable<T&> Callback>
    [[nodiscard]]
    bool pop(Callback&& cb) {
        slot& slot = (*storage)[read_index & mask];
        if (slot.st.load(std::memory_order_acquire) != state::ready) {
            return false;
        }

        std::invoke(std::forward<Callback>(cb), slot.value);
        std::destroy_at(&slot.value);
        slot.st.store(state::empty, std::memory_order_release);
        ++read_index;
        
        return true;
    }

    template <std::invocable<T&> Callback>
    [[nodiscard]]
    bool wait_pop(Callback&& cb) {
        for (;;) {
            
            auto expected = event.load(std::memory_order_acquire);
            slot& slot = (*storage)[read_index & mask];

            if (slot.st.load(std::memory_order_acquire) == state::ready) {
                std::invoke(std::forward<Callback>(cb), slot.value);
                std::destroy_at(&slot.value);
                slot.st.store(state::empty, std::memory_order_release);
                ++read_index;
                return true;
            }
            
            if (expected & 1) {
                return false;
            }

            event.wait(expected, std::memory_order_acquire);
        }

    }

    //call in producer if wait_pop was used and producer is stopped
    //it is prohbited to push after close()

    void close() {
        event.fetch_or(1, std::memory_order_release);
        event.notify_one();
    }

    template<std::invocable<T&> Callback>
    std::size_t consume(std::size_t max_count, Callback&& cb) {
        std::size_t read = 0;
        while (read < max_count) {
            slot& slot = (*storage)[read_index & mask];
            if (slot.st.load(std::memory_order_acquire) != state::ready) {
                break;
            }

            std::invoke(cb, slot.value);
            std::destroy_at(&slot.value);
            slot.st.store(state::empty, std::memory_order_release);
            ++read;
            ++read_index;
        }

        return read;
    }

private:
    std::unique_ptr<std::array<slot, N>> storage;
    alignas(cache_line) std::size_t write_index{0};
    alignas(cache_line) std::size_t read_index{0};
    alignas(cache_line) std::atomic<std::uint64_t> event{0};
};

#endif // SPSC_Q_H