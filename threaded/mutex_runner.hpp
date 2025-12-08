#include "task.hpp"
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

using namespace std;

class MutexTaskRunner : public TaskRunner
{
private:
    std::mutex global_mutex;
    std::vector<Task *> work_queue;
    std::atomic<bool> finished;
    int nb_threads;
    int tree_size;

public:
    MutexTaskRunner(int n, int number_cities) : nb_threads(n), finished(false)
    {
        tree_size = factorial(number_cities - 1);
    }

    void run(Task *root) override
    {
        // 1. initialiser la queue avec la tâche root
        {
            std::lock_guard<std::mutex> lock(global_mutex);
            work_queue.push_back(root);
        }

        startTimer();

        // 2. créer threads
        std::vector<std::thread> workers;
        for (int i = 0; i < nb_threads; i++)
            workers.emplace_back([this]()
                                 { worker_loop(); });

        // 3. attendre fin des threads
        for (auto &t : workers)
            t.join();

        stopTimer();
    }

private:
    void worker_loop()
    {
        while (!finished)
        {
            Task *t = nullptr;

            // extraire une tâche
            {
                std::lock_guard<std::mutex> lock(global_mutex);
                if (!work_queue.empty())
                {
                    t = work_queue.back();
                    work_queue.pop_back();
                }
            }

            if (t == nullptr)
            {
                std::this_thread::yield();
                continue;
            }

            TaskStack coll(32);
            int n = t->split(&coll);

            if (n == 0)
            {
                t->solve();
                // détecter la terminaison : à définir

            }
            else
            {
                // insérer les sous‐tâches dans la file
                std::lock_guard<std::mutex> lock(global_mutex);
                for (int i = 0; i < n; i++)
                    work_queue.push_back(coll[i]);
                t->merge(&coll);
            }
        }
    }

    unsigned int factorial(unsigned int n)
    {
        if (n == 0)
            return 1;
        return n * factorial(n - 1);
    }
};
