// atom/extra/spdlog/sampling/test_sampler.cpp

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "sampler.h"

using modern_log::LogSampler;
using modern_log::SamplingStrategy;

TEST(LogSamplerTest, DefaultIsNoSampling) {
    LogSampler sampler;
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(sampler.should_sample());
    }
    EXPECT_EQ(sampler.get_dropped_count(), 0u);
    EXPECT_DOUBLE_EQ(sampler.get_current_rate(), 1.0);
}

TEST(LogSamplerTest, UniformSamplingFullRate) {
    LogSampler sampler(SamplingStrategy::uniform, 1.0);
    int kept = 0, dropped = 0;
    for (int i = 0; i < 100; ++i) {
        if (sampler.should_sample())
            ++kept;
        else
            ++dropped;
    }
    EXPECT_EQ(kept, 100);
    EXPECT_EQ(dropped, 0);
    EXPECT_EQ(sampler.get_dropped_count(), 0u);
}

TEST(LogSamplerTest, UniformSamplingZeroRate) {
    LogSampler sampler(SamplingStrategy::uniform, 0.0);
    int kept = 0, dropped = 0;
    for (int i = 0; i < 10; ++i) {
        if (sampler.should_sample())
            ++kept;
        else
            ++dropped;
    }
    EXPECT_EQ(kept, 0);
    EXPECT_EQ(dropped, 10);
    EXPECT_EQ(sampler.get_dropped_count(), 10u);
}

TEST(LogSamplerTest, UniformSamplingPartialRate) {
    LogSampler sampler(SamplingStrategy::uniform, 0.2);
    int kept = 0, dropped = 0;
    for (int i = 0; i < 100; ++i) {
        if (sampler.should_sample())
            ++kept;
        else
            ++dropped;
    }
    // Should keep about 20 logs
    EXPECT_NEAR(kept, 20, 2);
    EXPECT_NEAR(dropped, 80, 2);
    EXPECT_EQ(sampler.get_dropped_count(), dropped);
}

TEST(LogSamplerTest, AdaptiveSamplingAdjustsRate) {
    LogSampler sampler(SamplingStrategy::adaptive, 0.5);
    int kept = 0, dropped = 0;
    for (int i = 0; i < 100; ++i) {
        if (sampler.should_sample())
            ++kept;
        else
            ++dropped;
    }
    // Should keep less than 50 logs due to simulated load
    EXPECT_LT(kept, 60);
    EXPECT_GT(kept, 0);
    EXPECT_EQ(sampler.get_dropped_count(), dropped);
    double rate = sampler.get_current_rate();
    EXPECT_GE(rate, 0.0);
    EXPECT_LE(rate, 0.5);
}

TEST(LogSamplerTest, BurstSamplingLimitsPerSecond) {
    LogSampler sampler(SamplingStrategy::burst, 0.3);  // max_burst = 3
    int kept = 0, dropped = 0;
    for (int i = 0; i < 10; ++i) {
        if (sampler.should_sample())
            ++kept;
        else
            ++dropped;
    }
    EXPECT_EQ(kept, 3);
    EXPECT_EQ(dropped, 7);
    EXPECT_EQ(sampler.get_dropped_count(), 7u);

    // Wait for burst window to reset
    std::this_thread::sleep_for(std::chrono::seconds(1));
    kept = dropped = 0;
    for (int i = 0; i < 10; ++i) {
        if (sampler.should_sample())
            ++kept;
        else
            ++dropped;
    }
    EXPECT_EQ(kept, 3);
}

TEST(LogSamplerTest, SetStrategyAndRate) {
    LogSampler sampler(SamplingStrategy::uniform, 1.0);
    sampler.set_strategy(SamplingStrategy::uniform, 0.1);
    int kept = 0;
    for (int i = 0; i < 20; ++i) {
        if (sampler.should_sample())
            ++kept;
    }
    EXPECT_NEAR(kept, 2, 1);
    sampler.set_strategy(SamplingStrategy::none, 1.0);
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(sampler.should_sample());
    }
}

TEST(LogSamplerTest, ResetStatsResetsCounters) {
    LogSampler sampler(SamplingStrategy::uniform, 0.0);
    for (int i = 0; i < 5; ++i)
        sampler.should_sample();
    EXPECT_EQ(sampler.get_dropped_count(), 5u);
    sampler.reset_stats();
    EXPECT_EQ(sampler.get_dropped_count(), 0u);
}

TEST(LogSamplerTest, ThreadSafety) {
    LogSampler sampler(SamplingStrategy::uniform, 0.5);
    std::vector<std::thread> threads;
    std::atomic<int> kept{0}, dropped{0};
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 100; ++i) {
                if (sampler.should_sample())
                    ++kept;
                else
                    ++dropped;
            }
        });
    }
    for (auto& th : threads)
        th.join();
    EXPECT_NEAR(kept, 200, 20);
    EXPECT_NEAR(dropped, 200, 20);
    EXPECT_EQ(sampler.get_dropped_count(), dropped);
}
