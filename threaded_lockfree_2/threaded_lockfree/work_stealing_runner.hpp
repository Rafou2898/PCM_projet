#pragma once
#include "task.hpp"
#include "work_stealing_deque.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <algorithm>

using namespace std;
class WorkStealingRunner : public TaskRunner
{
private:
    int _num_threads;
    std::vector<WorkStealingDeque<Task> *> _deques;
    std::vector<std::thread> _workers;

    std::atomic<bool> _finished;
    std::atomic<int64_t> _active_tasks;

    void worker_loop(int thread_id)
    {
        WorkStealingDeque<Task> *my_deque = _deques[thread_id];

        // main logic for a worker
        while (!_finished.load(memory_order_acquire))
        {

            Task *task = nullptr;

            // First the thread check if it has some work
            task = my_deque->pop();

            // if we have nothing, then we try stealing work from someone else
            if (task == nullptr)
            {
                task = steal_work(thread_id);
            }

            // here we have the case where we stole nothing
            if (task == nullptr)
            {

                // maybe it's over then we return
                if (check_termination())
                {
                    return;
                }

                // if not over then we just yield and try again
                this_thread::yield();
                continue;
            }

            process_task(task, my_deque);
        }
    }

    void process_task(Task *task, WorkStealingDeque<Task> *my_deque)
    {
        TaskStack coll(64);
        int n = task->split(&coll);

        if (n == 0)
        {
            task->solve();

            int64_t remaining = _active_tasks.fetch_sub(1, memory_order_acq_rel) - 1;
            if (remaining == 0)
            {
                _finished.store(true, memory_order_release);
            }
        }
        else
        {
            _active_tasks.fetch_add(n - 1);
            for (int i = 0; i < n; i++)
            {
                my_deque->push(coll[i]);
            }

            coll.clear();
        }
    }

    // Stealing the work randomly might not be the best option because we
    // could always try to steal a worker without any work
    // We could try to optimize this (finding a thread with a lot of work (how?), round-robin?,....)
    Task *steal_work(int my_id)
    {
        // We try stealing another thread randomly
        static thread_local std::random_device rd;
        static thread_local std::mt19937 random(rd());

        vector<int> victims;
        // -1 because we are not parts of the victims
        victims.reserve(_num_threads - 1);

        for (int i = 0; i < _num_threads; i++)
        {
            // Check it's not thread id
            if (i != my_id)
            {
                victims.push_back(i);
            }
        }

        shuffle(victims.begin(), victims.end(), random);

        for (int victim_id : victims)
        {
            Task *stolen = _deques[victim_id]->steal();
            if (stolen != nullptr)
            {
                return stolen;
            }
        }

        // nothing to steal
        return nullptr;
    }
    bool check_termination()
    {

        if (_active_tasks.load(memory_order_acquire) == 0)
        {
            bool all_empty = true;

            for (auto *deque : _deques)
            {
                if (!deque->empty())
                {
                    all_empty = false;
                    break;
                }
            }

            if (all_empty)
            {
                _finished.store(true, memory_order_release);

                return true;
            }
        }
        return false;
    }

public:
    WorkStealingRunner(int num_threads, int deque_capacity = 1024) : _num_threads(num_threads), _finished(false), _active_tasks(0)
    {
        _deques.reserve(num_threads);
        for (int i = 0; i < num_threads; i++)
        {
            _deques.push_back(new WorkStealingDeque<Task>(deque_capacity));
        }
    }
    ~WorkStealingRunner()
    {
        for (auto *d : _deques)
        {
            delete d;
        }
    }
    void run(Task *root) override
    {
        _finished.store(false, std::memory_order_relaxed);
        _active_tasks.store(1, std::memory_order_relaxed);

        startTimer();
        // Root task is pushed to deque 0 (so for thread 0)
        _deques[0]->push(root);

        // We create worker threads
        _workers.reserve(_num_threads);
        for (int tid = 0; tid < _num_threads; tid++)
        {
            _workers.emplace_back([this, tid]()
                                  { worker_loop(tid); });
        }

        for (auto &worker : _workers)
        {
            worker.join();
        }

        _workers.clear();
        stopTimer();
    }
};