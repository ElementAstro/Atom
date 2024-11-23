#include "atom/async/slot.hpp"

#include <iostream>
#include <memory>
#include <thread>

using namespace atom::async;

// Example slot function
void exampleSlot(int value) {
    std::cout << "Slot called with value: " << value << std::endl;
}

int main() {
    // Signal example
    Signal<int> signal;
    signal.connect(exampleSlot);
    signal.emit(42);
    signal.disconnect(exampleSlot);

    // AsyncSignal example
    AsyncSignal<int> asyncSignal;
    asyncSignal.connect(exampleSlot);
    asyncSignal.emit(84);
    asyncSignal.disconnect(exampleSlot);

    // AutoDisconnectSignal example
    AutoDisconnectSignal<int> autoDisconnectSignal;
    int slotId = autoDisconnectSignal.connect(exampleSlot);
    autoDisconnectSignal.emit(126);
    autoDisconnectSignal.disconnect(slotId);

    // ChainedSignal example
    ChainedSignal<int> chainedSignal1;
    ChainedSignal<int> chainedSignal2;
    chainedSignal1.connect(exampleSlot);
    chainedSignal2.connect([](int value) {
        std::cout << "Chained slot called with value: " << value << std::endl;
    });
    chainedSignal1.addChain(chainedSignal2);
    chainedSignal1.emit(168);

    // TemplateSignal example
    TemplateSignal<int> templateSignal;
    templateSignal.connect(exampleSlot);
    templateSignal.emit(210);
    templateSignal.disconnect(exampleSlot);

    // ThreadSafeSignal example
    ThreadSafeSignal<int> threadSafeSignal;
    threadSafeSignal.connect(exampleSlot);
    std::thread thread1([&]() { threadSafeSignal.emit(252); });
    std::thread thread2([&]() { threadSafeSignal.emit(294); });
    thread1.join();
    thread2.join();
    threadSafeSignal.disconnect(exampleSlot);

    // BroadcastSignal example
    BroadcastSignal<int> broadcastSignal1;
    BroadcastSignal<int> broadcastSignal2;
    broadcastSignal1.connect(exampleSlot);
    broadcastSignal2.connect([](int value) {
        std::cout << "Broadcast slot called with value: " << value << std::endl;
    });
    broadcastSignal1.addChain(broadcastSignal2);
    broadcastSignal1.emit(336);

    // LimitedSignal example
    LimitedSignal<int> limitedSignal(2);
    limitedSignal.connect(exampleSlot);
    limitedSignal.emit(378);
    limitedSignal.emit(420);
    limitedSignal.emit(462);  // This will not be emitted due to limit

    // DynamicSignal example
    DynamicSignal<int> dynamicSignal;
    dynamicSignal.connect(exampleSlot);
    dynamicSignal.emit(504);
    dynamicSignal.disconnect(exampleSlot);

    // ScopedSignal example
    ScopedSignal<int> scopedSignal;
    auto slotPtr = std::make_shared<ScopedSignal<int>::SlotType>(exampleSlot);
    scopedSignal.connect(slotPtr);
    scopedSignal.emit(546);
    slotPtr.reset();         // Disconnects the slot
    scopedSignal.emit(588);  // This will not call the slot

    return 0;
}