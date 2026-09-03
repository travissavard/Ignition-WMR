#pragma once

#include <openvr_driver.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ignition {

inline vr::DistortionCoordinates_t Lerp(const vr::DistortionCoordinates_t& a, const vr::DistortionCoordinates_t& b, float t) {
    vr::DistortionCoordinates_t r{};
    r.rfRed[0] = a.rfRed[0] + (b.rfRed[0] - a.rfRed[0]) * t;
    r.rfRed[1] = a.rfRed[1] + (b.rfRed[1] - a.rfRed[1]) * t;
    r.rfGreen[0] = a.rfGreen[0] + (b.rfGreen[0] - a.rfGreen[0]) * t;
    r.rfGreen[1] = a.rfGreen[1] + (b.rfGreen[1] - a.rfGreen[1]) * t;
    r.rfBlue[0] = a.rfBlue[0] + (b.rfBlue[0] - a.rfBlue[0]) * t;
    r.rfBlue[1] = a.rfBlue[1] + (b.rfBlue[1] - a.rfBlue[1]) * t;
    return r;
}

inline vr::HmdVector2_t Lerp(const vr::HmdVector2_t& a, const vr::HmdVector2_t& b, float t) {
    vr::HmdVector2_t r{};
    r.v[0] = a.v[0] + (b.v[0] - a.v[0]) * t;
    r.v[1] = a.v[1] + (b.v[1] - a.v[1]) * t;
    return r;
}

template <typename T>
struct DistortionGridCache {
    bool valid = false;
    bool has_sample_0_0 = false;
    T sample_0_0{};
    uint32_t resolution = 0;
    std::vector<T> grid;

    void Reset() {
        valid = false;
        has_sample_0_0 = false;
        resolution = 0;
        grid.clear();
    }

    T Sample(float fU, float fV) const {
        if (grid.empty() || resolution < 2) {
            return T{};
        }

        float u_clamped = std::clamp(fU, 0.0f, 1.0f);
        float v_clamped = std::clamp(fV, 0.0f, 1.0f);

        float u_pos = u_clamped * static_cast<float>(resolution - 1);
        float v_pos = v_clamped * static_cast<float>(resolution - 1);

        int u0 = std::clamp(static_cast<int>(u_pos), 0, static_cast<int>(resolution - 1));
        int u1 = std::min(u0 + 1, static_cast<int>(resolution - 1));
        float u_frac = u_pos - static_cast<float>(u0);

        int v0 = std::clamp(static_cast<int>(v_pos), 0, static_cast<int>(resolution - 1));
        int v1 = std::min(v0 + 1, static_cast<int>(resolution - 1));
        float v_frac = v_pos - static_cast<float>(v0);

        const auto& p00 = grid[v0 * resolution + u0];
        const auto& p10 = grid[v0 * resolution + u1];
        const auto& p01 = grid[v1 * resolution + u0];
        const auto& p11 = grid[v1 * resolution + u1];

        if (u_frac <= 0.0f && v_frac <= 0.0f) {
            return p00;
        }

        auto row0 = Lerp(p00, p10, u_frac);
        auto row1 = Lerp(p01, p11, u_frac);
        return Lerp(row0, row1, v_frac);
    }

    template <typename FetchSingleFunc, typename FetchBatchFunc>
    bool GetOrFetch(float fU, float fV, T* out_result, FetchSingleFunc&& fetch_single, FetchBatchFunc&& fetch_batch) {
        if (!out_result) return false;

        bool atOrigin = std::abs(fU) < 1e-6f && std::abs(fV) < 1e-6f;

        if (atOrigin) {
            // We will consider the cache invalid, as this is likely for a new distortion grid calculation.
            // This assumes the first sample is at (0,0). SteamVR does this, so we should be mostly safe.
            valid = false;
        }

        if (valid) {
            *out_result = Sample(fU, fV);
            return true;
        }

        // Starting point at (0,0)
        if (atOrigin) {
            if (fetch_single(0.0f, 0.0f, out_result)) {
                sample_0_0 = *out_result;
                has_sample_0_0 = true;
                return true;
            }
            return false;
        }

        // We can figure out the grid resolution by the delta from the first point to the second point.
        // SteamVR moves along V and then U. Other runtimes that can load SteamVR drivers may try along U first.
        // We will handle both cases of stepping along the U or V axis. Hopefully we won't have a case with unconventional sampling...
        if (!atOrigin && has_sample_0_0) {
            // We assume either fU or fV is zero, but not neither.
            float delta = fU + fV;
            uint32_t res = static_cast<uint32_t>(std::round(1.0f / delta)) + 1;

            std::vector<T> batch_grid;
            if (fetch_batch(res, batch_grid) && batch_grid.size() == res * res) {
                resolution = res;
                grid = std::move(batch_grid);
                valid = true;
                *out_result = Sample(fU, fV);
                return true;
            }
        }

        // Fallback for when we do not have the grid yet.
        return fetch_single(fU, fV, out_result);
    }
};

} // namespace ignition