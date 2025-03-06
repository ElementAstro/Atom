// gc.cpp
#include "gc.h"

namespace tsx {

void GarbageCollector::collectGarbage() {
#ifdef DEBUG_GC
    std::cout << "-- gc begin" << std::endl;
    size_t before = bytesAllocated;
#endif

    markRoots();
    traceReferences();
    sweep();

    nextGC = bytesAllocated * 2;

#ifdef DEBUG_GC
    std::cout << "-- gc end" << std::endl;
    std::cout << "   collected " << before - bytesAllocated << " bytes (from "
              << before << " to " << bytesAllocated << ") next at " << nextGC
              << std::endl;
#endif
}

void GarbageCollector::markRoots() {
    // Mark all values on the VM stack
    for (const auto& value : vm.stack) {
        markValue(value);
    }

    // Mark all globals
    for (const auto& value : vm.globals) {
        markValue(value);
    }

    // Mark the current closure
    if (vm.currentClosure) {
        markObject(vm.currentClosure);
    }

    // Mark all open upvalues
    for (Upvalue* upvalue : vm.openUpvalues) {
        markObject(upvalue);
    }

    // TODO: Mark compiler roots if needed
}

void GarbageCollector::traceReferences() {
    size_t oldSize;

    do {
        oldSize = markedObjects.size();

        for (ObjectBase* object : markedObjects) {
            object->markReferences(*this);
        }
    } while (markedObjects.size() > oldSize);
}

void GarbageCollector::sweep() {
    for (auto it = objects.begin(); it != objects.end();) {
        if (markedObjects.find(*it) == markedObjects.end()) {
            // Object is not marked, so it's unreachable
            delete *it;
            it = objects.erase(it);
        } else {
            ++it;
        }
    }

    markedObjects.clear();
}

void GarbageCollector::markObject(ObjectBase* object) {
    if (object == nullptr)
        return;

    // If already marked, we're done
    if (markedObjects.find(object) != markedObjects.end())
        return;

    // Mark the object
    markedObjects.insert(object);

#ifdef DEBUG_GC
    std::cout << "marked " << object->toString() << std::endl;
#endif
}

void GarbageCollector::markValue(const Value& value) {
    if (value.getType() == Value::Type::Object ||
        value.getType() == Value::Type::Function ||
        value.getType() == Value::Type::Closure ||
        value.getType() == Value::Type::NativeFunction ||
        value.getType() == Value::Type::Class ||
        value.getType() == Value::Type::Instance ||
        value.getType() == Value::Type::Array) {
        markObject(value.getObject());
    }
}

}  // namespace tsx