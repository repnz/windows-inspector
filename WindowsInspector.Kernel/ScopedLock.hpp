#pragma once

template <typename T>
struct ScopedLock {
	T& item;

    ScopedLock(T& item) : item(item) {
        item.Lock();
    }

    ~ScopedLock() {
        item.Unlock();
    }
};
