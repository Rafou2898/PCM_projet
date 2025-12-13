#pragma once
#include "task.hpp"
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>


using namespace std;

int factorial(int n){
    int f = 1;
    for(int i = 2; i <= n; i++){
        f *= i;
    }
    return f;
}

class StackTaskRunner : public TaskRunner
{
private:
    mutex global_mutex;
    vector<Task *> work_queue;
    atomic<int> leaves_remaining;
    int nb_threads;

public:
    StackTaskRunner(int n, int number_cities) : nb_threads(n), finished(false), active_tasks(0)
    {}

    void run(Task *root) override
    {
        int total_leaves = factorial(TSPPath::full() - 1);
        leaves_remaining.store(total_leaves);

        {
            lock_guard<mutex> lock(global_mutex);
            work_queue.push_back(root);
        }

        startTimer():
        std::vector<std::threads> workers;
        for(int i = 0; i < nb_threads; i++){
            workers.emplace_back(worker_loop);
        }

        for(auto& w : workers){
            w.join();
        }
        stopTimer();
    }

private:
    void worker_loop()
    {
        while (leaves_remaining.load() > 0)
        {
            {
                lock_guard<mutex> lock(global_mutex);
                if (!work_queue.empty())
                {
                    Task* t = work_queue.back();
                    work_queue.pop_back();
                }
            }
            if(!t){
                std::this_thread::yield();
                continue;
            }

            while(t){
                TaskStack children(32);
                int n = t->split(&children);

                int remaining = TSPPath::full() - static_cast<TSPTask*>(t)->_path.size();

                if(n == 0){
                    t->solve();
                    reusefree(static_cast<TSPTask*>(t));
                    leaves_remaining.fetch_sub(factorial(remaining));
                    break;
                }else{
                    Task* next = children[children.size() - 1];
                    children.pop();

                    for(int i = 0; i < children.size(); i++){
                        {
                            lock_guard<mutex> lock(global_mutex);
                            work_queue.push(children[i]);
                        }    
                    }

                    reusefree(static_cast<TSPTask*>(t));
                    t = next;
                }
            }
        }
    }
};