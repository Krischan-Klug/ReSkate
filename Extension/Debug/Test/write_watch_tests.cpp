// The write watch on this process's own memory: writes from a thread that ran before it was armed
// and from the arming thread are caught, with their values; a moved address is followed; off is off.
#include "Extension/Debug/write_watch.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

using namespace dingosdk;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

alignas(8) volatile std::uint32_t first = 1, second = 7;
std::atomic<std::uintptr_t> target{reinterpret_cast<std::uintptr_t>(&first)};

// A thread that exists before the watch is armed, and writes when told to.
struct Writer {
    std::mutex lock;
    std::condition_variable wake;
    int pending = 0, done = 0;
    bool stop = false;
    std::thread thread{[this] {
        std::unique_lock guard(lock);
        for (;;) {
            wake.wait(guard, [this] { return stop || pending > done; });
            if (stop) return;
            first = first + 1; // 2, 3, ...
            ++done;
            wake.notify_all();
        }
    }};
    void write_once() {
        std::unique_lock guard(lock);
        ++pending;
        wake.notify_all();
        wake.wait(guard, [this] { return done == pending; });
    }
    ~Writer() {
        { std::lock_guard guard(lock); stop = true; }
        wake.notify_all();
        thread.join();
    }
};
}

int main() {
    try {
        Writer writer;
        std::string error;
        check(!write_watch::arm("nothing", [] { return std::uintptr_t{0}; }, 4, error), "nothing to watch is refused");
        check(!write_watch::arm("odd", [] { return reinterpret_cast<std::uintptr_t>(&first) + 1; }, 4, error), "an unaligned address is refused");
        check(write_watch::arm("first", [] { return target.load(); }, 4, error), "arming works");
        writer.write_once();
        writer.write_once();
        first = 10; // from the arming thread
        write_watch::on_client_tick();
        auto s = write_watch::summary();
        check(s.armed && s.writes == 3, "every write is seen, from the earlier thread and the arming one");
        check(s.writers == 2, "two writing places");
        check(s.value && *s.value == 10, "the latest value");

        target = reinterpret_cast<std::uintptr_t>(&second); // the object was rebuilt elsewhere
        write_watch::on_client_tick();
        s = write_watch::summary();
        check(s.address == reinterpret_cast<std::uintptr_t>(&second) && s.value && *s.value == 7, "the watch follows the address");
        second = 8;
        first = 11; // the old address: no longer watched
        write_watch::on_client_tick();
        s = write_watch::summary();
        check(s.writes == 4 && *s.value == 8, "only the new address counts");

        write_watch::disarm();
        second = 9;
        write_watch::on_client_tick();
        check(!write_watch::summary().armed && write_watch::summary().writes == 4, "off is off");
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
    std::cout << "Write watch tests passed.\n";
    return 0;
}
