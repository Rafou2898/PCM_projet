#pragma once
#include <atomic>
#include <vector>
#include <stdexcept>
#include "my_logs.hpp"


using namespace std;
template <typename T>
class WorkStealingDeque
{
private:
    struct CircularBuffer
    {
        int capacity;

        vector<atomic<T *>> buffer;

        CircularBuffer(int cap) : capacity(cap), buffer(cap)
        {
            for (int i = 0; i < cap; i++)
            {
                // We initialize all elements to nullptr with store (because we use atomic)
                buffer[i].store(nullptr, memory_order_relaxed);
            }
        }

        T *get(int64_t index)
        {
            return buffer.at(index % capacity).load(memory_order_acquire);
        }
        void put(int64_t index, T *value)
        {
            buffer.at(index % capacity).store(value, memory_order_release);
        }

        CircularBuffer *resize(int64_t bottom, int64_t top)
        {
            // Like vector, we double the capacity
            CircularBuffer *new_buffer = new CircularBuffer(capacity * 2);
            // Top is the older index, the oldest pushed element where bottom is the newest pushed element so top < bottom
            for (int64_t i = top; i < bottom; i++)
            {
                T *value = get(i);
                new_buffer->put(i, value);
            }
            return new_buffer;
        }
    };

    // Both top and bottom are int64_t to avoid overflow since they can grow indefinitely,
    // hopefully we won't run that long because if it does we could have problems!
    //  Top of stack for thread thieves
    atomic<int64_t> _top;
    // Bottom of stack for thread owner
    atomic<int64_t> _bottom; // Extrémité pour le propriétaire (push/pop)
    // Mains data array
    atomic<CircularBuffer *> _array; // Tableau circulaire

public:
    WorkStealingDeque(int initial_capacity = 1024)
    {
        _top.store(0, memory_order_relaxed);
        _bottom.store(0, memory_order_relaxed);
        _array.store(new CircularBuffer(initial_capacity), memory_order_relaxed);
    }
    ~WorkStealingDeque()
    {
        delete _array.load(memory_order_relaxed);
    }

    // It should be called only by the owner thread which means no concurrency issues
    void push(T *item)
    {
        LOG("PUSH: thread=" << std::this_thread::get_id() << ", item=" << item);
        int64_t bottom = _bottom.load(memory_order_acquire);
        int64_t top = _top.load(memory_order_acquire);
        LOG("PUSH: before push bottom=" << bottom << ", top=" << top);
        // copy of array in case of resize
        CircularBuffer *array = _array.load(memory_order_acquire);

        // Check if we need to resize
        if (bottom - top >= array->capacity)
        {

            array = array->resize(bottom, top);
            _array.store(array, memory_order_release);
        }

        array->put(bottom, item);
        LOG("PUSH: stored item at index " << bottom);
        // The fence ensures that the item is visible before updating bottom
        atomic_thread_fence(memory_order_release);
        // We can now update bottom
        _bottom.store(bottom + 1, memory_order_release);
        LOG("PUSH DONE: new bottom=" << (bottom + 1));
    }

    T *pop()
    {
        LOG("POP: thread=" << std::this_thread::get_id());
        // bottom is always one past the last element so we decrement it first
        int64_t bottom = _bottom.load(memory_order_acquire) - 1;
        LOG("POP: decremented bottom to " << bottom);
        _bottom.store(bottom, memory_order_release);
        
        // the fence ensures that we see the latest value of top
        atomic_thread_fence(memory_order_seq_cst);
        int64_t top = _top.load(memory_order_relaxed);
        LOG("POP: current top=" << top << ", bottom=" << bottom);
        T *item = nullptr;
        // Non-empty queue
        if (top <= bottom)
        {
            LOG("POP: deque non-empty, popping item");
            CircularBuffer *array = _array.load(memory_order_acquire);
            item = array->get(bottom);
            // If it's the last element, we could have problems with concurrent stealers
            if (top == bottom)
            {
                int64_t expected_top = top;
                // we use a cas since we could have concurrent steals
                if (!_top.compare_exchange_strong(expected_top, top + 1,
                                                  memory_order_seq_cst,
                                                  memory_order_relaxed))
                {
                    // if we're here then we lost the race to a stealer so we set item to nullptr
                    item = nullptr;
                }
                _bottom.store(top + 1, memory_order_relaxed);
            }
        }
        else
        {
            LOG("POP: deque empty");
            // Deque empty
            _bottom.store(bottom + 1, memory_order_relaxed);
        }
        LOG("POP: popped item: " << item);
        return item;
    }
    T *steal()
    {
        int64_t top = _top.load(memory_order_acquire);

        // the fence ensures that we see the latest value of bottom
        atomic_thread_fence(memory_order_seq_cst);
        int64_t bottom = _bottom.load(memory_order_acquire);

        T *item = nullptr;
        // Non-empty queue
        if (top < bottom)
        {
            CircularBuffer *array = _array.load(memory_order_acquire);
            item = array->get(top);
            int64_t expected_top = top;
            // We use a cas to increment top
            if (_top.compare_exchange_strong(expected_top, top + 1,
                                              memory_order_seq_cst,
                                              memory_order_relaxed))
            {
                return item;
            }
        }
        return nullptr;
    }

    // Can help for debugging but might not be exact since a size change can happen concurrently
    int size() const
    {
        int64_t bottom = _bottom.load(memory_order_acquire);
        int64_t top = _top.load(memory_order_acquire);

        // We have to cast to int64_t because the 0 is considered as an int and max() between int64_t and int is not defined
        return max((int64_t)0, bottom - top);
    }

    bool empty() const
    {
        return size() == 0;
    }
};