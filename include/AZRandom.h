#pragma once
#include <random>
#include <cstdlib>

namespace AZ {
    // Nếu = true (tắt mặc định), sẽ sử dụng thread_local mt19937 cho NEAT.
    // Nếu = false, sẽ gọi std::rand() để không phá vỡ seed của nhánh RL main.
    extern bool g_UseThreadLocalRNG;

    inline std::mt19937& GetTLS_RNG() {
        thread_local std::mt19937 rng(std::random_device{}());
        return rng;
    }

    inline void SRand(unsigned int seed) {
        if (g_UseThreadLocalRNG) {
            GetTLS_RNG().seed(seed);
        } else {
            std::srand(seed);
        }
    }

    inline int Rand() {
        if (g_UseThreadLocalRNG) {
            return GetTLS_RNG()() & 0x7FFF;
        } else {
            return std::rand();
        }
    }
}
