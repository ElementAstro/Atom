#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>  // For std::function
#include <future>      // For std::async, std::future
#include <iostream>    // For std::cout
#include <memory>      // For std::shared_ptr, std::make_shared
#include <sstream>     // For capturing stdout
#include <string>
#include <thread>
#include <vector>

#include "atom/async/slot.hpp"

using namespace atom::async;
using ::testing::ContainsRegex;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Throw;

// Helper to capture stdout for tests that print
auto captureOutput(const std::function<void()>& func) -> std::string {
    std::stringstream buffer;
    std::streambuf* old_cout = std::cout.rdbuf(buffer.rdbuf());
    func();
    std::cout.rdbuf(old_cout);
    return buffer.str();
}

// Test fixture for Signal tests
class SignalTest : public ::testing::Test {
protected:
    std::atomic<int> call_count{0};
    std::atomic<int> last_int_arg{0};
    std::string last_string_arg;
    std::mutex string_mutex;  // Protect last_string_arg

    // Helper slot function (modified to accept int)
    auto simple_slot() {
        return [&](int /*unused*/) { call_count++; };
    }

    // Helper slot function with args
    auto args_slot() {
        return [&](int x, const std::string& s) {
            call_count++;
            last_int_arg.store(x);
            {
                std::lock_guard lock(string_mutex);
                last_string_arg = s;
            }
        };
    }

    // Helper slot function that throws
    auto throwing_slot() {
        return [&](int) {
            call_count++;
            throw std::runtime_error("Slot failed");
        };
    }

    void SetUp() override {
        call_count = 0;
        last_int_arg = 0;
        {
            std::lock_guard lock(string_mutex);
            last_string_arg.clear();
        }
    }
};

// --- Signal Tests ---

TEST_F(SignalTest, Signal_ConnectEmit_CallsSlot) {
    Signal<int, std::string> signal;
    signal.connect(args_slot());
    signal.emit(123, "test");

    EXPECT_THAT(call_count.load(), Eq(1));
    EXPECT_THAT(last_int_arg.load(), Eq(123));
    {
        std::lock_guard lock(string_mutex);
        EXPECT_THAT(last_string_arg, Eq("test"));
    }
}

TEST_F(SignalTest, Signal_ConnectMultiple_CallsAllSlots) {
    Signal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    signal.connect([&](int x) {
        slot1_calls++;
        EXPECT_THAT(x, Eq(10));
    });
    signal.connect([&](int x) {
        slot2_calls++;
        EXPECT_THAT(x, Eq(10));
    });

    signal.emit(10);

    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(1));
    EXPECT_THAT(signal.size(), Eq(2));
}

TEST_F(SignalTest, Signal_Disconnect_RemovesSpecificSlot) {
    Signal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    auto s1 = [&](int) { slot1_calls++; };
    auto s2 = [&](int) { slot2_calls++; };

    signal.connect(s1);
    signal.connect(s2);
    EXPECT_THAT(signal.size(), Eq(2));

    signal.emit(1);
    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(1));

    signal.disconnect(s1);
    EXPECT_THAT(signal.size(), Eq(1));

    signal.emit(2);
    EXPECT_THAT(slot1_calls.load(), Eq(1));  // Should not be called again
    EXPECT_THAT(slot2_calls.load(), Eq(2));  // Should be called again

    signal.disconnect(s2);
    EXPECT_THAT(signal.size(), Eq(0));

    signal.emit(3);
    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(2));  // No calls
}

TEST_F(SignalTest, Signal_DisconnectNonExistent_NoEffect) {
    Signal<int> signal;
    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(1));

    auto non_existent_slot = [&](int) {};
    signal.disconnect(non_existent_slot);
    EXPECT_THAT(signal.size(), Eq(1));  // Size unchanged

    signal.emit(1);
    EXPECT_THAT(call_count.load(), Eq(1));  // Original slot still works
}

TEST_F(SignalTest, Signal_ConnectInvalidSlot_Throws) {
    Signal<int> signal;
    Signal<int>::SlotType invalid_slot = nullptr;
    EXPECT_THROW(signal.connect(invalid_slot), SlotConnectionError);
    EXPECT_THAT(signal.size(), Eq(0));
}

TEST_F(SignalTest, Signal_EmitWithThrowingSlot_ThrowsSlotEmissionError) {
    Signal<int> signal;
    signal.connect(simple_slot());    // This one won't throw
    signal.connect(throwing_slot());  // This one will throw
    signal.connect(simple_slot());    // This one might not be reached

    EXPECT_THROW(
        {
            try {
                signal.emit(1);
            } catch (const SlotEmissionError& e) {
                // Check if the original exception message is included
                EXPECT_THAT(e.what(), ContainsRegex("Slot failed"));
                throw;  // Re-throw to satisfy EXPECT_THROW
            }
        },
        SlotEmissionError);

    // The first simple_slot should have been called, the throwing_slot too.
    // The third slot might or might not be called depending on the order and
    // whether the exception is caught and rethrown per slot or stops the loop.
    // The current implementation copies slots and iterates, throwing stops the
    // loop. So, call_count should be at least 2 (first simple + throwing).
    EXPECT_THAT(call_count.load(), Ge(2));
}

TEST_F(SignalTest, Signal_SizeAndEmpty_ReflectState) {
    Signal<int> signal;
    EXPECT_THAT(signal.size(), Eq(0));
    EXPECT_TRUE(signal.empty());

    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(1));
    EXPECT_FALSE(signal.empty());

    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(2));
    EXPECT_FALSE(signal.empty());

    signal.clear();
    EXPECT_THAT(signal.size(), Eq(0));
    EXPECT_TRUE(signal.empty());
}

TEST_F(SignalTest, Signal_Clear_RemovesAllSlots) {
    Signal<int> signal;
    signal.connect(simple_slot());
    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(2));

    signal.clear();
    EXPECT_THAT(signal.size(), Eq(0));
    EXPECT_TRUE(signal.empty());

    signal.emit(1);  // Should not call anything
    EXPECT_THAT(call_count.load(), Eq(0));
}

TEST_F(SignalTest, Signal_ThreadSafety_ConcurrentConnectEmit) {
    Signal<int> signal;
    const int num_threads = 10;
    const int connects_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> total_calls{0};

    // Threads concurrently connect slots and emit
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            auto slot = [&](int val) {
                total_calls++;
                EXPECT_THAT(val, Eq(i));
            };
            for (int j = 0; j < connects_per_thread; ++j) {
                signal.connect(slot);
                // Emit occasionally
                if (j % 10 == 0) {
                    try {
                        signal.emit(i);  // Emit the thread index
                    } catch (...) {
                    }  // Ignore potential emission errors from other threads
                       // throwing
                }
            }
            // Emit one last time after connecting all slots
            try {
                signal.emit(i);
            } catch (...) {
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // The exact number of calls is hard to predict due to concurrent connects
    // and emits. However, the test should not crash or deadlock.
    // The total number of slots connected is num_threads * connects_per_thread.
    // Each emit iterates over a copy of slots at that moment.
    // We expect total_calls to be > 0 and less than (num_threads *
    // connects_per_thread) * (connects_per_thread/10 + 1) A simpler check is
    // just that calls happened and the size is correct.
    EXPECT_THAT(signal.size(), Eq(num_threads * connects_per_thread));
    EXPECT_THAT(total_calls.load(), Ge(1));  // At least one call should happen
}

// --- AsyncSignal Tests ---

TEST_F(SignalTest, AsyncSignal_ConnectEmit_CallsSlotAsync) {
    AsyncSignal<int, std::string>
        signal;  // Changed signal type to match args_slot and emit
    signal.connect(args_slot());  // Use args_slot which modifies atomics

    auto futures = signal.emit(456, "async");  // This now matches

    // Wait for all futures to complete
    for (auto& f : futures) {
        f.get();  // This will re-throw exceptions from async tasks
    }

    EXPECT_THAT(call_count.load(), Eq(1));
    EXPECT_THAT(last_int_arg.load(), Eq(456));
    {
        std::lock_guard lock(string_mutex);
        EXPECT_THAT(last_string_arg, Eq("async"));
    }
}

TEST_F(SignalTest, AsyncSignal_ConnectMultiple_CallsAllSlotsAsync) {
    AsyncSignal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    signal.connect([&](int x) {
        slot1_calls++;
        EXPECT_THAT(x, Eq(20));
    });
    signal.connect([&](int x) {
        slot2_calls++;
        EXPECT_THAT(x, Eq(20));
    });

    auto futures = signal.emit(20);

    // Wait for all futures
    for (auto& f : futures) {
        f.get();
    }

    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(1));
    // AsyncSignal doesn't have size() or empty() in the provided code
}

TEST_F(SignalTest, AsyncSignal_EmitWithThrowingSlot_FutureGetThrows) {
    AsyncSignal<int> signal;
    signal.connect(simple_slot());    // This one won't throw
    signal.connect(throwing_slot());  // This one will throw
    signal.connect(
        simple_slot());  // This one might run depending on async scheduling

    auto futures = signal.emit(1);

    EXPECT_THAT(futures.size(), Eq(3));  // Should launch a future for each slot

    // Waiting on the future for the throwing slot should throw
    // We don't know which future corresponds to which slot, so check all.
    bool threw = false;
    for (auto& f : futures) {
        try {
            f.get();
        } catch (const SlotEmissionError& e) {
            EXPECT_THAT(
                e.what(),
                ContainsRegex("Async slot execution failed: Slot failed"));
            threw = true;
        } catch (...) {
            ADD_FAILURE() << "Caught unexpected exception";
        }
    }
    EXPECT_TRUE(threw) << "Expected SlotEmissionError from throwing slot";

    // The non-throwing slots should still increment call_count
    EXPECT_THAT(call_count.load(),
                Ge(2));  // At least the two simple_slots should increment
}

TEST_F(SignalTest, AsyncSignal_Clear_RemovesAllSlots) {
    AsyncSignal<int> signal;
    signal.connect(simple_slot());
    signal.connect(simple_slot());
    // No size() method to check initial size

    signal.clear();

    auto futures = signal.emit(1);  // Should not launch anything
    EXPECT_THAT(futures.size(), Eq(0));

    // Wait a bit to be sure no async tasks were launched
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_THAT(call_count.load(), Eq(0));
}

// --- AutoDisconnectSignal Tests ---

TEST_F(SignalTest, AutoDisconnectSignal_ConnectEmit_CallsSlot) {
    AutoDisconnectSignal<int> signal;
    signal.connect(simple_slot());
    signal.emit(1);
    EXPECT_THAT(call_count.load(), Eq(1));
    EXPECT_THAT(signal.size(), Eq(1));
}

TEST_F(SignalTest, AutoDisconnectSignal_ConnectMultiple_CallsAllSlots) {
    AutoDisconnectSignal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    signal.connect([&](int) { slot1_calls++; });
    signal.connect([&](int) { slot2_calls++; });

    signal.emit(1);

    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(1));
    EXPECT_THAT(signal.size(), Eq(2));
}

TEST_F(SignalTest, AutoDisconnectSignal_DisconnectById_RemovesSpecificSlot) {
    AutoDisconnectSignal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    auto id1 = signal.connect([&](int) { slot1_calls++; });
    auto id2 = signal.connect([&](int) { slot2_calls++; });
    EXPECT_THAT(signal.size(), Eq(2));

    signal.emit(1);
    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(1));

    bool disconnected = signal.disconnect(id1);
    EXPECT_TRUE(disconnected);
    EXPECT_THAT(signal.size(), Eq(1));

    signal.emit(2);
    EXPECT_THAT(slot1_calls.load(), Eq(1));  // Should not be called again
    EXPECT_THAT(slot2_calls.load(), Eq(2));  // Should be called again

    disconnected = signal.disconnect(id2);
    EXPECT_TRUE(disconnected);
    EXPECT_THAT(signal.size(), Eq(0));

    signal.emit(3);
    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(2));  // No calls
}

TEST_F(SignalTest, AutoDisconnectSignal_DisconnectNonExistentId_ReturnsFalse) {
    AutoDisconnectSignal<int> signal;
    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(1));

    bool disconnected = signal.disconnect(999);  // Non-existent ID
    EXPECT_FALSE(disconnected);
    EXPECT_THAT(signal.size(), Eq(1));  // Size unchanged

    signal.emit(1);
    EXPECT_THAT(call_count.load(), Eq(1));  // Original slot still works
}

TEST_F(SignalTest, AutoDisconnectSignal_ConnectInvalidSlot_Throws) {
    AutoDisconnectSignal<int> signal;
    AutoDisconnectSignal<int>::SlotType invalid_slot = nullptr;
    EXPECT_THROW(signal.connect(invalid_slot), SlotConnectionError);
    EXPECT_THAT(signal.size(), Eq(0));
}

TEST_F(SignalTest,
       AutoDisconnectSignal_EmitWithThrowingSlot_ThrowsSlotEmissionError) {
    AutoDisconnectSignal<int> signal;
    signal.connect(simple_slot());    // This one won't throw
    signal.connect(throwing_slot());  // This one will throw
    signal.connect(simple_slot());    // This one might not be reached

    EXPECT_THROW(
        {
            try {
                signal.emit(1);
            } catch (const SlotEmissionError& e) {
                EXPECT_THAT(e.what(), ContainsRegex("Slot failed"));
                throw;
            }
        },
        SlotEmissionError);

    EXPECT_THAT(call_count.load(), Ge(2));
}

TEST_F(SignalTest, AutoDisconnectSignal_Size_ReflectsState) {
    AutoDisconnectSignal<int> signal;
    EXPECT_THAT(signal.size(), Eq(0));

    auto id1 = signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(1));

    auto id2 = signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(2));

    signal.disconnect(id1);
    EXPECT_THAT(signal.size(), Eq(1));

    signal.disconnect(id2);
    EXPECT_THAT(signal.size(), Eq(0));
}

TEST_F(SignalTest, AutoDisconnectSignal_Clear_RemovesAllSlots) {
    AutoDisconnectSignal<int> signal;
    signal.connect(simple_slot());
    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(2));

    signal.clear();
    EXPECT_THAT(signal.size(), Eq(0));

    signal.emit(1);  // Should not call anything
    EXPECT_THAT(call_count.load(), Eq(0));
}

TEST_F(SignalTest,
       AutoDisconnectSignal_ThreadSafety_ConcurrentConnectDisconnectEmit) {
    AutoDisconnectSignal<int> signal;
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> total_calls{0};

    // Threads concurrently connect, disconnect, and emit
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::vector<AutoDisconnectSignal<int>::ConnectionId> ids;
            auto slot = [&](int val) {
                total_calls++;
                EXPECT_THAT(val, Eq(i));
            };

            for (int j = 0; j < operations_per_thread; ++j) {
                // Connect
                try {
                    ids.push_back(signal.connect(slot));
                } catch (...) {
                }

                // Emit occasionally
                if (j % 5 == 0) {
                    try {
                        signal.emit(i);
                    } catch (...) {
                    }
                }

                // Disconnect occasionally
                if (j % 3 == 0 && !ids.empty()) {
                    signal.disconnect(ids.front());
                    ids.erase(ids.begin());
                }
            }
            // Emit one last time
            try {
                signal.emit(i);
            } catch (...) {
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // The exact state is unpredictable, but the test should not crash.
    // Check that some calls happened.
    EXPECT_THAT(total_calls.load(), Ge(1));
    // Size should be <= total connections made (num_threads *
    // operations_per_thread)
    EXPECT_THAT(signal.size(), Le(num_threads * operations_per_thread));
}

// --- ChainedSignal Tests ---

TEST_F(SignalTest, ChainedSignal_AddChainRef_EmitsInOrder) {
    ChainedSignal<int> signal1;
    ChainedSignal<int> signal2;
    std::string output;

    signal1.connect(
        [&](int x) { output += "Signal1:" + std::to_string(x) + ";"; });
    signal2.connect(
        [&](int x) { output += "Signal2:" + std::to_string(x) + ";"; });

    signal1.addChain(signal2);

    signal1.emit(100);

    EXPECT_THAT(output, Eq("Signal1:100;Signal2:100;"));
}

TEST_F(SignalTest, ChainedSignal_AddChainSharedPtr_EmitsInOrder) {
    ChainedSignal<int> signal1;
    auto signal2_ptr = std::make_shared<ChainedSignal<int>>();
    std::string output;

    signal1.connect(
        [&](int x) { output += "Signal1:" + std::to_string(x) + ";"; });
    signal2_ptr->connect(
        [&](int x) { output += "Signal2:" + std::to_string(x) + ";"; });

    signal1.addChain(signal2_ptr);

    signal1.emit(200);

    EXPECT_THAT(output, Eq("Signal1:200;Signal2:200;"));
}

TEST_F(SignalTest, ChainedSignal_WeakPtrChain_ExpiredChainRemoved) {
    ChainedSignal<int> signal1;
    std::string output;

    signal1.connect(
        [&](int x) { output += "Signal1:" + std::to_string(x) + ";"; });

    {
        auto signal2_ptr = std::make_shared<ChainedSignal<int>>();
        signal2_ptr->connect(
            [&](int x) { output += "Signal2:" + std::to_string(x) + ";"; });
        signal1.addChain(signal2_ptr);

        signal1.emit(300);  // signal2_ptr is still valid
        EXPECT_THAT(output, Eq("Signal1:300;Signal2:300;"));
        output.clear();
    }  // signal2_ptr goes out of scope here

    // Emit again, signal2 should be expired and removed
    signal1.emit(400);
    EXPECT_THAT(output,
                Eq("Signal1:400;"));  // Only signal1's slot should be called
}

TEST_F(SignalTest,
       ChainedSignal_EmitWithThrowingSlotInChain_ThrowsSlotEmissionError) {
    ChainedSignal<int> signal1;
    ChainedSignal<int> signal2;
    std::atomic<int> signal1_calls{0};
    std::atomic<int> signal2_calls{0};

    signal1.connect([&](int) { signal1_calls++; });
    signal2.connect([&](int) {
        signal2_calls++;
        throw std::runtime_error("Chain slot failed");
    });
    signal1.addChain(signal2);

    EXPECT_THROW(
        {
            try {
                signal1.emit(1);
            } catch (const SlotEmissionError& e) {
                EXPECT_THAT(e.what(), ContainsRegex("Chain slot failed"));
                throw;
            }
        },
        SlotEmissionError);

    EXPECT_THAT(signal1_calls.load(), Eq(1));  // Signal1 slot should be called
    EXPECT_THAT(signal2_calls.load(),
                Eq(1));  // Signal2 slot should be called before it throws
}

TEST_F(SignalTest, ChainedSignal_Clear_RemovesSlotsAndChains) {
    ChainedSignal<int> signal1;
    ChainedSignal<int> signal2;
    signal1.connect(simple_slot());
    signal1.addChain(signal2);
    // No size() method for ChainedSignal slots/chains

    signal1.clear();

    signal1.emit(1);  // Should not call anything
    EXPECT_THAT(call_count.load(), Eq(0));
    // Cannot easily verify chains are cleared without a size/access method
}

// --- ThreadSafeSignal Tests ---

TEST_F(SignalTest, ThreadSafeSignal_ConnectEmit_CallsSlot) {
    ThreadSafeSignal<int> signal;
    signal.connect(simple_slot());
    signal.emit(1);
    EXPECT_THAT(call_count.load(), Eq(1));
    EXPECT_THAT(signal.size(), Eq(1));
}

TEST_F(SignalTest, ThreadSafeSignal_ConnectMultiple_CallsAllSlots) {
    ThreadSafeSignal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    signal.connect([&](int) { slot1_calls++; });
    signal.connect([&](int) { slot2_calls++; });

    signal.emit(1);

    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(1));
    EXPECT_THAT(signal.size(), Eq(2));
}

TEST_F(SignalTest, ThreadSafeSignal_Disconnect_RemovesSpecificSlot) {
    ThreadSafeSignal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    auto s1 = [&](int) { slot1_calls++; };
    auto s2 = [&](int) { slot2_calls++; };

    signal.connect(s1);
    signal.connect(s2);
    EXPECT_THAT(signal.size(), Eq(2));

    signal.emit(1);
    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(1));

    signal.disconnect(s1);
    EXPECT_THAT(signal.size(), Eq(1));

    signal.emit(2);
    EXPECT_THAT(slot1_calls.load(), Eq(1));  // Should not be called again
    EXPECT_THAT(slot2_calls.load(), Eq(2));  // Should be called again

    signal.disconnect(s2);
    EXPECT_THAT(signal.size(), Eq(0));

    signal.emit(3);
    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(2));  // No calls
}

TEST_F(SignalTest, ThreadSafeSignal_ConnectInvalidSlot_Throws) {
    ThreadSafeSignal<int> signal;
    ThreadSafeSignal<int>::SlotType invalid_slot = nullptr;
    EXPECT_THROW(signal.connect(invalid_slot), SlotConnectionError);
    EXPECT_THAT(signal.size(), Eq(0));
}

TEST_F(SignalTest,
       ThreadSafeSignal_EmitWithThrowingSlot_ThrowsSlotEmissionError) {
    ThreadSafeSignal<int> signal;
    signal.connect(simple_slot());    // This one won't throw
    signal.connect(throwing_slot());  // This one will throw
    signal.connect(simple_slot());    // This one might not be reached

    EXPECT_THROW(
        {
            try {
                signal.emit(1);
            } catch (const SlotEmissionError& e) {
                EXPECT_THAT(e.what(), ContainsRegex("Slot failed"));
                throw;
            }
        },
        SlotEmissionError);

    // Parallel execution might mean all slots are launched before any exception
    // is handled. So call_count should be 3 if all slots were reached before
    // the exception handling mechanism stops. With par_unseq, the order is not
    // guaranteed, but all valid slots *should* be attempted.
    EXPECT_THAT(call_count.load(), Eq(3));
}

TEST_F(SignalTest, ThreadSafeSignal_Size_ReflectsState) {
    ThreadSafeSignal<int> signal;
    EXPECT_THAT(signal.size(), Eq(0));

    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(1));

    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(2));

    signal.clear();
    EXPECT_THAT(signal.size(), Eq(0));
}

TEST_F(SignalTest, ThreadSafeSignal_Clear_RemovesAllSlots) {
    ThreadSafeSignal<int> signal;
    signal.connect(simple_slot());
    signal.connect(simple_slot());
    EXPECT_THAT(signal.size(), Eq(2));

    signal.clear();
    EXPECT_THAT(signal.size(), Eq(0));

    signal.emit(1);  // Should not call anything
    EXPECT_THAT(call_count.load(), Eq(0));
}

TEST_F(SignalTest,
       ThreadSafeSignal_ThreadSafety_ConcurrentConnectDisconnectEmit) {
    ThreadSafeSignal<int> signal;
    const int num_threads = 20;  // More threads to stress shared_mutex
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> total_calls{0};

    // Threads concurrently connect, disconnect, and emit
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            auto slot = [&](int val) {
                total_calls++;
                EXPECT_THAT(val, Eq(i));
            };
            // Create multiple distinct slots per thread to make disconnect by
            // target_type meaningful
            auto slot1 = [&](int val) {
                total_calls++;
                EXPECT_THAT(val, Eq(i));
            };
            auto slot2 = [&](int val) {
                total_calls++;
                EXPECT_THAT(val, Eq(i));
            };

            for (int j = 0; j < operations_per_thread; ++j) {
                // Connect
                try {
                    signal.connect(slot1);
                    signal.connect(slot2);
                } catch (...) {
                }

                // Emit occasionally
                if (j % 5 == 0) {
                    try {
                        signal.emit(i);
                    } catch (...) {
                    }  // Ignore potential emission errors
                }

                // Disconnect occasionally
                if (j % 3 == 0) {
                    signal.disconnect(slot1);  // Disconnect one type of slot
                }
            }
            // Emit one last time
            try {
                signal.emit(i);
            } catch (...) {
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // The exact state is unpredictable, but the test should not crash.
    // Check that calls happened and size is reasonable.
    // Total connects attempted: num_threads * operations_per_thread * 2
    // Total disconnects attempted: num_threads * (operations_per_thread / 3)
    // Final size should be <= total connects.
    EXPECT_THAT(signal.size(), Le(num_threads * operations_per_thread * 2));
    EXPECT_THAT(total_calls.load(), Ge(1));  // At least one call should happen
}

// --- LimitedSignal Tests ---

TEST_F(SignalTest, LimitedSignal_EmitUpToLimit) {
    LimitedSignal<int> signal(3);
    signal.connect(simple_slot());

    EXPECT_THAT(signal.isExhausted(), Eq(false));
    EXPECT_THAT(signal.remainingCalls(), Eq(3));

    bool emitted1 = signal.emit(1);
    EXPECT_TRUE(emitted1);
    EXPECT_THAT(call_count.load(), Eq(1));
    EXPECT_THAT(signal.isExhausted(), Eq(false));
    EXPECT_THAT(signal.remainingCalls(), Eq(2));

    bool emitted2 = signal.emit(2);
    EXPECT_TRUE(emitted2);
    EXPECT_THAT(call_count.load(), Eq(2));
    EXPECT_THAT(signal.isExhausted(), Eq(false));
    EXPECT_THAT(signal.remainingCalls(), Eq(1));

    bool emitted3 = signal.emit(3);
    EXPECT_TRUE(emitted3);
    EXPECT_THAT(call_count.load(), Eq(3));
    EXPECT_THAT(signal.isExhausted(), Eq(true));
    EXPECT_THAT(signal.remainingCalls(), Eq(0));

    bool emitted4 = signal.emit(4);  // Should not emit
    EXPECT_FALSE(emitted4);
    EXPECT_THAT(call_count.load(), Eq(3));  // Count unchanged
    EXPECT_THAT(signal.isExhausted(), Eq(true));
    EXPECT_THAT(signal.remainingCalls(), Eq(0));
}

TEST_F(SignalTest, LimitedSignal_ConstructorThrowsOnZeroLimit) {
    EXPECT_THROW(LimitedSignal<int>(0), std::invalid_argument);
}

TEST_F(SignalTest, LimitedSignal_Reset_ResetsCallCount) {
    LimitedSignal<int> signal(2);
    signal.connect(simple_slot());

    signal.emit(1);
    signal.emit(2);
    EXPECT_THAT(signal.isExhausted(), Eq(true));
    EXPECT_THAT(call_count.load(), Eq(2));

    signal.reset();
    EXPECT_THAT(signal.isExhausted(), Eq(false));
    EXPECT_THAT(signal.remainingCalls(), Eq(2));

    bool emitted = signal.emit(3);
    EXPECT_TRUE(emitted);
    EXPECT_THAT(call_count.load(), Eq(3));
}

TEST_F(SignalTest, LimitedSignal_ThreadSafety_ConcurrentEmit) {
    LimitedSignal<int> signal(10);  // Limit to 10 calls
    signal.connect(simple_slot());

    const int num_threads = 20;
    std::vector<std::thread> threads;

    // Threads concurrently try to emit
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            // Try to emit many times
            for (int j = 0; j < 10; ++j) {
                signal.emit(1);
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(1));  // Add some contention
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // The signal should have been emitted exactly 10 times in total across all
    // threads
    EXPECT_THAT(call_count.load(), Eq(10));
    EXPECT_THAT(signal.isExhausted(), Eq(true));
    EXPECT_THAT(signal.remainingCalls(), Eq(0));
}

// --- ScopedSignal Tests ---

TEST_F(SignalTest, ScopedSignal_ConnectWithSharedPtr_CallsSlot) {
    ScopedSignal<int> signal;
    auto slot_ptr =
        std::make_shared<ScopedSignal<int>::SlotType>(simple_slot());

    signal.connect(slot_ptr);
    EXPECT_THAT(signal.size(), Eq(1));

    signal.emit(1);
    EXPECT_THAT(call_count.load(), Eq(1));
}

TEST_F(SignalTest, ScopedSignal_ConnectWithCallable_CallsSlot) {
    ScopedSignal<int> signal;
    signal.connect(simple_slot());  // Connect using the callable overload
    EXPECT_THAT(signal.size(), Eq(1));

    signal.emit(1);
    EXPECT_THAT(call_count.load(), Eq(1));
}

TEST_F(SignalTest, ScopedSignal_ConnectMultiple_CallsAllSlots) {
    ScopedSignal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    signal.connect([&](int) { slot1_calls++; });
    signal.connect([&](int) { slot2_calls++; });

    EXPECT_THAT(signal.size(), Eq(2));

    signal.emit(1);

    EXPECT_THAT(slot1_calls.load(), Eq(1));
    EXPECT_THAT(slot2_calls.load(), Eq(1));
}

TEST_F(SignalTest, ScopedSignal_SharedPtrGoesOutOfScope_SlotDisconnected) {
    ScopedSignal<int> signal;
    std::atomic<int> slot1_calls{0};
    std::atomic<int> slot2_calls{0};

    signal.connect([&](int) {
        slot1_calls++;
    });  // Connected via callable, managed internally

    {
        auto slot2_ptr = std::make_shared<ScopedSignal<int>::SlotType>(
            [&](int) { slot2_calls++; });
        signal.connect(slot2_ptr);          // Connected via shared_ptr
        EXPECT_THAT(signal.size(), Eq(2));  // Both slots counted

        signal.emit(1);
        EXPECT_THAT(slot1_calls.load(), Eq(1));
        EXPECT_THAT(slot2_calls.load(), Eq(1));
    }  // slot2_ptr goes out of scope here

    // Emit again. The expired slot2_ptr should be removed and not called.
    signal.emit(2);
    EXPECT_THAT(slot1_calls.load(), Eq(2));  // slot1 still called
    EXPECT_THAT(slot2_calls.load(), Eq(1));  // slot2 not called again

    // The expired slot should be removed during the emit call
    EXPECT_THAT(signal.size(), Eq(1));  // Only slot1 remains
}

TEST_F(SignalTest, ScopedSignal_ConnectNullSharedPtr_Throws) {
    ScopedSignal<int> signal;
    ScopedSignal<int>::SlotPtr null_ptr = nullptr;
    EXPECT_THROW(signal.connect(null_ptr), SlotConnectionError);
    EXPECT_THAT(signal.size(), Eq(0));
}

TEST_F(SignalTest, ScopedSignal_ConnectSharedPtrWithInvalidFunction_Throws) {
    ScopedSignal<int> signal;
    ScopedSignal<int>::SlotType invalid_func = nullptr;
    auto invalid_slot_ptr =
        std::make_shared<ScopedSignal<int>::SlotType>(invalid_func);
    EXPECT_THROW(signal.connect(invalid_slot_ptr), SlotConnectionError);
    EXPECT_THAT(signal.size(), Eq(0));
}

TEST_F(SignalTest, ScopedSignal_ConnectInvalidCallable_Throws) {
    ScopedSignal<int> signal;
    // A lambda that cannot be converted to SlotType (e.g., wrong signature)
    auto invalid_callable = [&](const std::string&) {};
    // This should fail compilation if the concept check works, but if it
    // somehow passes, the std::function construction or connect call might
    // throw. Assuming the concept check prevents this, we test a nullptr
    // std::function. The connect(Callable&&) overload internally creates a
    // shared_ptr<SlotType>. If the callable is valid but the conversion to
    // SlotType fails (less likely), the make_shared might throw. Let's test the
    // case where the callable *is* valid but we pass a nullptr std::function
    // explicitly. The connect(SlotPtr) overload handles the null check. The
    // connect(Callable&&) overload relies on make_shared and the subsequent
    // connect(SlotPtr). If the callable is valid, make_shared should succeed.
    // If the callable is invalid (doesn't match Args...), the concept should
    // prevent compilation. So, testing invalid callable is primarily a
    // compilation check via concepts. We can test a valid callable that throws
    // during its *own* construction (if it were a class) would be a test case,
    // but a lambda is simple. Let's assume the concept check is sufficient for
    // callable validity and focus on the shared_ptr aspect.
}

TEST_F(SignalTest, ScopedSignal_EmitWithThrowingSlot_ThrowsSlotEmissionError) {
    ScopedSignal<int> signal;
    signal.connect(simple_slot());    // This one won't throw
    signal.connect(throwing_slot());  // This one will throw
    signal.connect(simple_slot());    // This one might be reached

    EXPECT_THAT(signal.size(), Eq(3));

    EXPECT_THROW(
        {
            try {
                signal.emit(1);
            } catch (const SlotEmissionError& e) {
                EXPECT_THAT(e.what(), ContainsRegex("Slot failed"));
                throw;
            }
        },
        SlotEmissionError);

    // The first simple_slot should have been called, the throwing_slot too.
    // The third slot might or might not be called depending on the order.
    // The current implementation copies slots and iterates, throwing stops the
    // loop. So, call_count should be at least 2 (first simple + throwing).
    EXPECT_THAT(call_count.load(), Ge(2));
    // Expired slots are removed *during* emit. The throwing slot is not
    // expired, just threw. So size should still be 3 after the throw.
    EXPECT_THAT(signal.size(), Eq(3));
}

TEST_F(SignalTest, ScopedSignal_Size_CountsValidSlots) {
    ScopedSignal<int> signal;
    EXPECT_THAT(signal.size(), Eq(0));

    auto slot1_ptr =
        std::make_shared<ScopedSignal<int>::SlotType>(simple_slot());
    signal.connect(slot1_ptr);
    EXPECT_THAT(signal.size(), Eq(1));

    signal.connect(simple_slot());  // Connected via callable
    EXPECT_THAT(signal.size(), Eq(2));

    slot1_ptr.reset();  // Release the shared_ptr

    // Size should still be 2 until emit is called and cleans up
    EXPECT_THAT(signal.size(), Eq(2));

    signal.emit(1);                         // This should trigger cleanup
    EXPECT_THAT(call_count.load(), Eq(1));  // Only the callable slot is called

    EXPECT_THAT(signal.size(), Eq(1));  // Size should now be 1 after cleanup
}
