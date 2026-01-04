#pragma once
#include "task.hpp"
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include "tsptask.hpp"

using namespace std;

class StackTaskRunner : public TaskRunner
{
private:
    mutex global_mutex;
    vector<Task *> work_queue;
    atomic<int64_t> leaves_remaining;
    int nb_threads;

public:
    StackTaskRunner(int n) : nb_threads(n)
    {
    }

    void run(Task *root) override
    {
        int64_t total_leaves = factorial(TSPPath::full() - 1);
        leaves_remaining.store(total_leaves);

        {
            lock_guard<mutex> lock(global_mutex);
            work_queue.push_back(root);
        }

        startTimer();
        std::vector<std::thread> workers;
        for (int i = 0; i < nb_threads; i++)
        {
            workers.emplace_back([this]
                                 { worker_loop(); });
        }

        for (auto &w : workers)
        {
            w.join();
        }
        stopTimer();
    }

private:
    int64_t factorial(int n)
    {
        int64_t f = 1;
        for (int i = 2; i <= n; i++)
        {
            f *= i;
        }
        return f;
    }
    void worker_loop()
    {
        while (leaves_remaining.load() > 0)
        {
            Task *t = nullptr;
            {
                lock_guard<mutex> lock(global_mutex);
                if (!work_queue.empty())
                {
                    t = work_queue.back();
                    work_queue.pop_back();
                }
            }
            if (!t)
            {
                std::this_thread::yield();
                continue;
            }

            while (t)
            {
                TaskStack children(32);
                int n = t->split(&children);

                int remaining = static_cast<TSPTask *>(t)->remaining();

                if (n == 0)
                {
                    t->solve();
                    static_cast<TSPTask *>(t)->recycle();
                    leaves_remaining.fetch_sub(factorial(remaining));
                    break;
                }
                else
                {
                    Task *next = children[children.size() - 1];
                    children.pop();

                    for (int i = 0; i < children.size(); i++)
                    {
                        {
                            lock_guard<mutex> lock(global_mutex);
                            work_queue.push_back(children[i]);
                        }
                    }

                    static_cast<TSPTask *>(t)->recycle();
                    t = next;
                }
            }
        }
    }
};