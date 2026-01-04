#pragma once
#include "task.hpp"
#include "work_stealing_deque.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <algorithm>
#include "my_logs.hpp"

using namespace std;

int64_t factorial(int n)
{
    int64_t f = 1;
    for (int i = 2; i <= n; i++)
    {
        f *= i;
    }
    return f;
}

struct WSStats
{
    uint64_t local_push = 0;
    uint64_t local_pop = 0;
    uint64_t steal_attempts = 0;
    uint64_t steal_success = 0;
    uint64_t empty_loops = 0;
    uint64_t tasks_processed = 0;
};

class WorkStealingRunner : public TaskRunner
{
private:
    int _num_threads;
    std::vector<WorkStealingDeque<Task> *> _deques;
    std::vector<std::thread> _workers;

    atomic<int64_t> leaves_remaining;
    atomic<int64_t> factorial_array[21];

    std::vector<WSStats> _threads_stats;

    void worker_loop(int thread_id)
    {
        WorkStealingDeque<Task> *my_deque = _deques[thread_id];
        int idle_cycles = 0;
        // main logic for a worker
        while (leaves_remaining.load() > 0)
        {

            Task *task = nullptr;

            // First the thread check if it has some work
            task = my_deque->pop();
            LOG("Worker " << thread_id << " started");
            if (task)
            {
                _threads_stats[thread_id].local_pop++;
                _threads_stats[thread_id].tasks_processed++;
                process_task(task, my_deque, thread_id);
                continue;
            }

            // if we have nothing, then we try stealing work from someone else
            if (task == nullptr)
            {
                LOG("Worker " << thread_id << " trying to steal work");
                task = steal_work(thread_id);
                if (task != nullptr)
                {
                    LOG("Worker " << thread_id << " stole task");
                    idle_cycles = 0;
                    _threads_stats[thread_id].steal_success++;
                    _threads_stats[thread_id].tasks_processed++;
                    process_task(task, my_deque, thread_id);
                    continue;
                }
            }

            // here we have the case where we stole nothing
            if (task == nullptr)
            {
                _threads_stats[thread_id].empty_loops++;
                LOG("Worker " << thread_id << " idle -> stole nothing");
                idle_cycles++;
                if (idle_cycles % 10000 == 0)
                {

                    if (idle_cycles >= 100000)
                    {
                        idle_cycles = 0; // Reset pour ne pas spam
                    }
                }

                // if not over then we just yield and try again
                this_thread::yield();
                continue;
            }
        }
    }

    void process_task(Task *task, WorkStealingDeque<Task> *my_deque, int thread_id)
    {
        TaskStack coll(64);
        LOG("Processing task -> Calling split()");
        int n = task->split(&coll);
        LOG("Processing task, split returned " << n);

        int remaining = static_cast<TSPTask *>(task)->remaining();
        if (n == 0)
        {
            LOG("Solving task");
            task->solve();

            leaves_remaining.fetch_sub(factorial_array[remaining].load());

            if (remaining < 0)
            {
                LOG(" ERROR: _active_tasks went negative! remaining=" << remaining);
            }
        }
        else
        {

            for (int i = 0; i < n; i++)
            {
                my_deque->push(coll[i]);
                _threads_stats[thread_id].local_push++;
            }

            coll.clear();
        }
    }

    // Stealing the work randomly might not be the best option because we
    // could always try to steal a worker without any work
    // We could try to optimize this (finding a thread with a lot of work (how?), round-robin?,....)
    Task *steal_work(int my_id)
    {
        _threads_stats[my_id].steal_attempts++;
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

    void print_stats()
    {
        uint64_t sp = 0, lp = 0, sa = 0, ss = 0, el = 0, tp = 0;
        for (int i = 0; i < _num_threads; i++)
        {
            sp += _threads_stats[i].local_push;
            lp += _threads_stats[i].local_pop;
            sa += _threads_stats[i].steal_attempts;
            ss += _threads_stats[i].steal_success;
            el += _threads_stats[i].empty_loops;
            tp += _threads_stats[i].tasks_processed;
        }
        std::cout << "WS stats: pushes=" << sp
                  << " pops=" << lp
                  << " steal_attempts=" << sa
                  << " steal_success=" << ss
                  << " empty_loops=" << el
                  << " tasks_processed=" << tp
                  << "\n";
    }

public:
    WorkStealingRunner(int num_threads, int deque_capacity = 1024) : _num_threads(num_threads)
    {
        _deques.reserve(num_threads);
        for (int i = 0; i < num_threads; i++)
        {
            _deques.push_back(new WorkStealingDeque<Task>(deque_capacity));
        }
        _threads_stats.resize(num_threads);
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
        startTimer();

        int64_t total_leaves = factorial(TSPPath::full() - 1);
        leaves_remaining.store(total_leaves);

        for (int i = 0; i < TSPPath::full(); i++)
        {
            factorial_array[i].store(factorial(i));
        }

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
        print_stats();

        _workers.clear();
        stopTimer();
    }
};