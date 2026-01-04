#pragma once
#include "task.hpp"
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>


using namespace std;

class MutexTaskRunner : public TaskRunner
{
private:
    mutex global_mutex;
    vector<Task *> work_queue;
    atomic<bool> finished;
    atomic<int64_t> active_tasks;
    int nb_threads;

public:
    MutexTaskRunner(int n) : finished(false), active_tasks(0), nb_threads(n)
    {}

    void run(Task *root) override
    {
        {
            lock_guard<mutex> lock(global_mutex);
            work_queue.push_back(root);
        }
		active_tasks.store(1);
        startTimer();

        vector<thread> workers;
		workers.reserve(nb_threads);
        for (int i = 0; i < nb_threads; i++)
        {

            workers.emplace_back([this]()
                                 { worker_loop(); });
        }

        for (auto &t : workers)
        {

            t.join();
        }

        stopTimer();
    }

private:
    void worker_loop()
    {
        while (!finished.load(std::memory_order_relaxed))
        {
            Task *t = nullptr;

            // We extract a task from the global work queue
            // It is a critical section
            // So we use a mutex to protect it
            // lock_guard is unlocked when going out of scope
            // thats why we put it in a separate block
            {
                lock_guard<mutex> lock(global_mutex);
                if (!work_queue.empty())
                {
                    t = work_queue.back();
                    work_queue.pop_back();
                }
            }

            if (t == nullptr)
            {
                // We check if we are finished
                if (active_tasks.load(std::memory_order_relaxed) == 0)
                {
                    finished.store(true, std::memory_order_relaxed);
                    return;
                }

                this_thread::yield();
                continue;
            }

            TaskStack coll(64);
            int n = t->split(&coll);
			//cout << "split created n:" << n << endl;

            if (n == 0)
            {
                t->solve();

                // One less leaf
               int64_t remaining = active_tasks.fetch_sub(1, std::memory_order_acq_rel) - 1;

                if (remaining == 0)
                {
                    finished.store(true, std::memory_order_relaxed);
                }
            }
            else
            {
                {
                    lock_guard<mutex> lock(global_mutex);
                    for (int i = 0; i < n; i++)
                        work_queue.push_back(coll[i]);
                }
				 active_tasks.fetch_add(n - 1, std::memory_order_release);
				
                coll.clear();
            }
        }
    }
};
