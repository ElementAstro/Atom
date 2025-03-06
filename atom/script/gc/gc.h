// gc.h
#pragma once

#include <unordered_set>
#include <vector>
#include "../vm/vm.h"

namespace tsx {

class GarbageCollector {
public:
    explicit GarbageCollector(VirtualMachine& vm)
        : vm(vm), nextGC(1024 * 1024) {}

    template <typename T, typename... Args>
    T* allocateObject(Args&&... args) {
        // Check if we need to collect garbage
        if (bytesAllocated > nextGC) {
            collectGarbage();
        }

        size_t size = sizeof(T);
        bytesAllocated += size;

        T* object = new T(std::forward<Args>(args)...);
        objects.push_back(object);
        return object;
    }

    void collectGarbage();
    void markRoots();
    void traceReferences();
    void sweep();

    void markObject(ObjectBase* object);
    void markValue(const Value& value);

private:
    VirtualMachine& vm;
    std::vector<ObjectBase*> objects;
    std::unordered_set<ObjectBase*> markedObjects;

    size_t bytesAllocated = 0;
    size_t nextGC;
};

}  // namespace tsx