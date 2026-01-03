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

    std::atomic<bool> _finished;
    std::atomic<int64_t> _active_tasks;

    std::atomic<int64_t> _total_pops{0};
    std::atomic<int64_t> _total_steals{0};
    std::atomic<int64_t> _successful_steals{0};
    std::atomic<int64_t> _total_splits{0};
    std::atomic<int64_t> _total_solves{0};
    std::atomic<int64_t> _pruned_tasks{0};

    std::vector<WSStats> _per_thread_stats;

    void worker_loop(int thread_id)
    {
        WorkStealingDeque<Task> *my_deque = _deques[thread_id];
        int idle_cycles = 0;
        // main logic for a worker
        while (!_finished.load(memory_order_acquire))
        {

            Task *task = nullptr;

            // First the thread check if it has some work
            task = my_deque->pop();
            LOG("Worker " << thread_id << " started");
            if (task)
            {
                _per_thread_stats[thread_id].local_pop++;
                _per_thread_stats[thread_id].tasks_processed++;
                process_task(task, my_deque, thread_id);
                continue;
            }

            // if we have nothing, then we try stealing work from someone else
            if (task == nullptr)
            {
                // cout << "Its time to steal work" << endl;
                LOG("Worker " << thread_id << " trying to steal work");
                task = steal_work(thread_id);
                if (task != nullptr)
                {
                    LOG("Worker " << thread_id << " stole task");
                    _successful_steals++;
                    idle_cycles = 0;
                    _per_thread_stats[thread_id].steal_success++;
                    _per_thread_stats[thread_id].tasks_processed++;
                    process_task(task, my_deque, thread_id);
                    continue;
                }
            }

            // here we have the case where we stole nothing
            if (task == nullptr)
            {
                _per_thread_stats[thread_id].empty_loops++;
                LOG("Worker " << thread_id << " idle -> stole nothing");
                idle_cycles++;
                if (idle_cycles % 10000 == 0)
                {
                    int64_t active = _active_tasks.load();
                    LOG("Worker " << thread_id << " idle (iter=" << idle_cycles
                                  << ", active_tasks=" << active << ")");

                    if (idle_cycles >= 100000)
                    {
                        LOG(" Worker " << thread_id << " STUCK! Dumping state:");
                        dump_state();
                        idle_cycles = 0; // Reset pour ne pas spam
                    }
                }
                // maybe it's over then we return
                if (check_termination())
                {
                    return;
                }

                // if not over then we just yield and try again
                this_thread::yield();
                continue;
            }
            // LOG("Worker " << thread_id << " processing task");
            // process_task(task, my_deque);
        }
    }
    void dump_state()
    {
        LOG_STAT("=== GLOBAL STATE DUMP ===", "");
        LOG_STAT("_active_tasks", _active_tasks.load());
        LOG_STAT("_finished", _finished.load());

        for (int i = 0; i < _num_threads; i++)
        {
            int size = _deques[i]->size();
            LOG_STAT("  deque[" << i << "].size()", size);
        }

        LOG_STAT("Total pops", _total_pops.load());
        LOG_STAT("Total steals attempted", _total_steals.load());
        LOG_STAT("Successful steals", _successful_steals.load());
        LOG_STAT("Total splits", _total_splits.load());
        LOG_STAT("Total solves", _total_solves.load());
        LOG_STAT("=========================", "");
    }

    void process_task(Task *task, WorkStealingDeque<Task> *my_deque, int thread_id)
    {
        TaskStack coll(64);
        LOG("Processing task -> Calling split()");
        int n = task->split(&coll);
        LOG("Processing task, split returned " << n);
        if (n == 0)
        {
            LOG("Solving task");
            _total_solves++;
            task->solve();

            int64_t remaining = _active_tasks.fetch_sub(1, memory_order_acq_rel) - 1;
            if (remaining == 0)
            {
                LOG(" Last task finished! Setting _finished=true");
                _finished.store(true, std::memory_order_release);
            }

            if (remaining < 0)
            {
                LOG(" ERROR: _active_tasks went negative! remaining=" << remaining);
            }
        }
        else
        {
            _total_splits++;

            LOG("Split into " << n << " subtasks, active_tasks="
                              << _active_tasks.load() << " -> "
                              << (_active_tasks.load() + n - 1));

            // il faut incrémenter avant de pusher
            _active_tasks.fetch_add(n - 1, std::memory_order_release);
            for (int i = 0; i < n; i++)
            {
                my_deque->push(coll[i]);
                _per_thread_stats[thread_id].local_push++;
            }

            coll.clear();
        }
    }

    // Stealing the work randomly might not be the best option because we
    // could always try to steal a worker without any work
    // We could try to optimize this (finding a thread with a lot of work (how?), round-robin?,....)
    Task *steal_work(int my_id)
    {
        _per_thread_stats[my_id].steal_attempts++;
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
            // cout << " stealing victim: " << victim_id << endl;
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
                LOG("Termination condition met: active_tasks=0 and all deques empty");
                _finished.store(true, memory_order_release);

                return true;
            }
            else
            {
                LOG(" active_tasks=0 but some deques not empty!");
            }
        }
        return false;
    }

    void print_stats()
    {
        uint64_t sp = 0, lp = 0, sa = 0, ss = 0, el = 0, tp = 0;
        for (int i = 0; i < _num_threads; i++)
        {
            sp += _per_thread_stats[i].local_push;
            lp += _per_thread_stats[i].local_pop;
            sa += _per_thread_stats[i].steal_attempts;
            ss += _per_thread_stats[i].steal_success;
            el += _per_thread_stats[i].empty_loops;
            tp += _per_thread_stats[i].tasks_processed;
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
    WorkStealingRunner(int num_threads, int deque_capacity = 1024) : _num_threads(num_threads), _finished(false), _active_tasks(0)
    {
        _deques.reserve(num_threads);
        for (int i = 0; i < num_threads; i++)
        {
            _deques.push_back(new WorkStealingDeque<Task>(deque_capacity));
        }
        _per_thread_stats.resize(num_threads);
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
        print_stats();

        _workers.clear();
        stopTimer();
    }
};