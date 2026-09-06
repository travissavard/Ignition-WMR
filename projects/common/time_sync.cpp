#include "time_sync.h"
#include "rpc_core.h"
#include "rpc_enums.h"
#include "time_c.h"

#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

std::atomic<bool> TimeSync::is_calibrated_{false};
std::atomic<uint64_t> TimeSync::qpc_base_{0};
std::atomic<uint64_t> TimeSync::linux_base_ns_{0};
std::atomic<uint64_t> TimeSync::qpc_freq_{10000000ULL};

void TimeSync::InitializeServer() {
#ifdef _WIN32
    LARGE_INTEGER freq;
    if (QueryPerformanceFrequency(&freq) && freq.QuadPart > 0) {
        qpc_freq_.store(static_cast<uint64_t>(freq.QuadPart), std::memory_order_relaxed);
    } else {
        qpc_freq_.store(10000000ULL, std::memory_order_relaxed);
    }

    RecalibrateServer();
    SendSync();
#endif
}

void TimeSync::RecalibrateServer() {
#ifdef _WIN32
    uint64_t best_qpc = 0;
    uint64_t best_linux = 0;
    uint64_t min_diff = UINT64_MAX;

    for (int i = 0; i < 15; ++i) {
        LARGE_INTEGER qpc1, qpc2;
        QueryPerformanceCounter(&qpc1);
        uint64_t linux_ns = ignition_bridge_get_linux_ticks();
        QueryPerformanceCounter(&qpc2);

        uint64_t diff = static_cast<uint64_t>(qpc2.QuadPart - qpc1.QuadPart);
        if (diff < min_diff && linux_ns > 0) {
            min_diff = diff;
            best_qpc = static_cast<uint64_t>((qpc1.QuadPart + qpc2.QuadPart) / 2);
            best_linux = linux_ns;
        }
    }

    if (best_linux > 0 && best_qpc > 0) {
        qpc_base_.store(best_qpc, std::memory_order_release);
        linux_base_ns_.store(best_linux, std::memory_order_release);
        is_calibrated_.store(true, std::memory_order_release);
    }
#endif
}

void TimeSync::InitializeDriver() {
    RpcSystem::RegisterFunction(RPCFunction_SyncTime, [](const auto& args) {
        if (!args.empty() && args[0].isByteArray()) {
            auto data = args[0].asByteArray();
            if (data.size() >= sizeof(uint64_t) * 3) {
                uint64_t qpc_base, linux_base_ns, qpc_freq;
                memcpy(&qpc_base, data.data(), sizeof(uint64_t));
                memcpy(&linux_base_ns, data.data() + sizeof(uint64_t), sizeof(uint64_t));
                memcpy(&qpc_freq, data.data() + sizeof(uint64_t) * 2, sizeof(uint64_t));
                TimeSync::SetCalibration(qpc_base, linux_base_ns, qpc_freq);
            }
        }
        return RpcValue(true);
    });
}

void TimeSync::SendSync() {
    if (!is_calibrated_.load(std::memory_order_acquire)) {
        return;
    }

    uint64_t qpc = qpc_base_.load(std::memory_order_relaxed);
    uint64_t linux_ns = linux_base_ns_.load(std::memory_order_relaxed);
    uint64_t freq = qpc_freq_.load(std::memory_order_relaxed);

    try {
        std::vector<char> sync_buf(sizeof(uint64_t) * 3);
        memcpy(sync_buf.data(), &qpc, sizeof(uint64_t));
        memcpy(sync_buf.data() + sizeof(uint64_t), &linux_ns, sizeof(uint64_t));
        memcpy(sync_buf.data() + sizeof(uint64_t) * 2, &freq, sizeof(uint64_t));

        RpcSystem::CallWithTimeout(RPCFunction_SyncTime, std::chrono::seconds(5), RpcValue(sync_buf.data(), sync_buf.size()));
    } catch (const std::exception& e) {
        std::cerr << "Failed to send sync: " << e.what() << std::endl;
    }
}

void TimeSync::SetCalibration(uint64_t qpc_base, uint64_t linux_base_ns, uint64_t qpc_freq) {
    if (qpc_freq == 0) qpc_freq = 10000000ULL;
    qpc_base_.store(qpc_base, std::memory_order_release);
    linux_base_ns_.store(linux_base_ns, std::memory_order_release);
    qpc_freq_.store(qpc_freq, std::memory_order_release);
    is_calibrated_.store(true, std::memory_order_release);
}

uint64_t TimeSync::QpcToLinuxTicks(uint64_t qpc_ticks) {
    if (!is_calibrated_.load(std::memory_order_acquire)) {
        // Likely on Windows, pass the time as is.
        return qpc_ticks;
    }

    uint64_t qpc_base = qpc_base_.load(std::memory_order_relaxed);
    uint64_t linux_base = linux_base_ns_.load(std::memory_order_relaxed);
    uint64_t freq = qpc_freq_.load(std::memory_order_relaxed);
    if (freq == 0) freq = 10000000ULL;

    // Sanity check: if qpc_ticks is already in nanoseconds close to linux_base
    // (within +/- 60 seconds), do not double-convert.
    int64_t diff_from_linux = static_cast<int64_t>(qpc_ticks - linux_base);
    if (diff_from_linux > -60000000000LL && diff_from_linux < 60000000000LL) {
        return qpc_ticks;
    }

    int64_t delta_qpc = static_cast<int64_t>(qpc_ticks - qpc_base);
    int64_t delta_ns;

    if (freq == 10000000ULL) {
        delta_ns = delta_qpc * 100LL;
    } else {
        int64_t sec = delta_qpc / static_cast<int64_t>(freq);
        int64_t rem = delta_qpc % static_cast<int64_t>(freq);
        delta_ns = sec * 1000000000LL + (rem * 1000000000LL) / static_cast<int64_t>(freq);
    }

    return static_cast<uint64_t>(static_cast<int64_t>(linux_base) + delta_ns);
}