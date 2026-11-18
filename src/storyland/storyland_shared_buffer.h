#pragma once
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

// World instances share geometry until an editor actually changes its contents.
// Mutable access detaches, keeping edits independent from the other instances.
template<class T> class StorylandSharedBuffer {
    using Storage = std::vector<T>;
    std::shared_ptr<Storage> storage;
    const Storage& read() const {
        static const Storage empty;
        return storage ? *storage : empty;
    }
    Storage& write() {
        if (!storage) storage = std::make_shared<Storage>();
        else if (!storage.unique()) storage = std::make_shared<Storage>(*storage);
        return *storage;
    }
public:
    StorylandSharedBuffer() = default;
    StorylandSharedBuffer(std::initializer_list<T> values) : storage(std::make_shared<Storage>(values)) {}
    StorylandSharedBuffer& operator=(std::initializer_list<T> values) {
        storage = std::make_shared<Storage>(values); return *this;
    }
    StorylandSharedBuffer& operator=(Storage values) {
        storage = std::make_shared<Storage>(std::move(values)); return *this;
    }
    operator const Storage&() const { return read(); }
    operator Storage&() { return write(); }
    friend bool operator==(const StorylandSharedBuffer& a, const StorylandSharedBuffer& b) {
        return a.storage == b.storage || a.read() == b.read();
    }
    friend bool operator!=(const StorylandSharedBuffer& a, const StorylandSharedBuffer& b) { return !(a == b); }
    size_t size() const { return read().size(); }
    size_t capacity() const { return read().capacity(); }
    bool empty() const { return read().empty(); }
    const T* data() const { return read().data(); }
    T* data() { return write().data(); }
    const T& operator[](size_t index) const { return read()[index]; }
    T& operator[](size_t index) { return write()[index]; }
    const T& front() const { return read().front(); }
    T& front() { return write().front(); }
    const T& back() const { return read().back(); }
    T& back() { return write().back(); }
    auto begin() const { return read().begin(); }
    auto end() const { return read().end(); }
    auto begin() { return write().begin(); }
    auto end() { return write().end(); }
    void clear() { storage.reset(); }
    void reserve(size_t count) { write().reserve(count); }
    void resize(size_t count) { write().resize(count); }
    void assign(size_t count, const T& value) { write().assign(count, value); }
    template<class It> void assign(It first, It last) { write().assign(first, last); }
    void push_back(const T& value) { write().push_back(value); }
    void push_back(T&& value) { write().push_back(std::move(value)); }
};
