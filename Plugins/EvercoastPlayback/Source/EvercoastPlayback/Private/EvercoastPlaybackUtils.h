#pragma once

#include <inttypes.h>
#include <thread>
#include <mutex>
#include <condition_variable>

int64_t GetFrameIndex(double timestamp, double frameRate);


class CountingSemaphore {
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;  // Tracks available permits

public:
    explicit CountingSemaphore(int initial = 0) : count(initial) {}

    void acquire() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return count > 0; });  // Wait until count > 0
        --count;  // Consume a permit
    }

    void release() {
        std::lock_guard<std::mutex> lock(mtx);
        ++count;  // Add a permit
        cv.notify_one();  // Wake up one waiting thread
    }
};