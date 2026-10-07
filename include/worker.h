#ifndef WORKER_H
#define WORKER_H

#include <concepts>
#include <thread>

class worker {
public:
    template <std::invocable<std::stop_token> F>
    explicit worker(F&& worker):thread(std::forward<F>(worker)) {}

    worker(const worker&) = delete;
    worker& operator=(const worker&) = delete;
    worker(worker&&) = delete;
    worker& operator=(worker&&) = delete;

    void stop() {
        thread.request_stop();
        thread.join();
    }
    
private:
    std::jthread thread{};
};

#endif // WORKER_H