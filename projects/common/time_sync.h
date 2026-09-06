#pragma once

#include <atomic>
#include <cstdint>

class TimeSync {
public:
    // Called on Windows/Wine server to sample QPC and Linux ticks and send calibration to Linux
    static void InitializeServer();
    static void InitializeDriver();
    static void RecalibrateServer();
    static void SendSync();

    // Called on Linux driver shim when calibration is received
    static void SetCalibration(uint64_t qpc_base, uint64_t linux_base_ns, uint64_t qpc_freq);

    // Convert QPC ticks to Linux CLOCK_MONOTONIC_RAW nanoseconds
    static uint64_t QpcToLinuxTicks(uint64_t qpc_ticks);

private:
    static std::atomic<bool> is_calibrated_;
    static std::atomic<uint64_t> qpc_base_;
    static std::atomic<uint64_t> linux_base_ns_;
    static std::atomic<uint64_t> qpc_freq_;
};
