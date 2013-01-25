#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <algorithm>
#include <utility>

// need this type to "erase" the return type of the packaged task
struct any_packaged_base {
  virtual void execute() = 0;
};

template<class R>
struct any_packaged : public any_packaged_base {
  any_packaged(std::packaged_task<R()>&& t) : task(std::move(t)) { }

  void execute() {
    task();
  }

  std::packaged_task<R()> task;
};

class any_packaged_task {
  public:
    template<class R>
    any_packaged_task(std::packaged_task<R()>&& task) : ptr(new any_packaged<R>(std::move(task))) { }

    void operator()() {
      ptr->execute();
    }

  private:
    std::shared_ptr<any_packaged_base> ptr;
};

class Threadpool;

// our worker thread objects
class Worker {
  public:
    Worker(Threadpool& s) : pool(s) { }

    void operator()();

  private:
    Threadpool& pool;
};

// the actual thread pool
class Threadpool {
  public:
    typedef std::vector<std::thread>::size_type size_type;

    Threadpool() : Threadpool(std::max(1u, std::thread::hardware_concurrency())) { }
    Threadpool(size_type);
    ~Threadpool();

    template<class T, class F>
    std::future<T> enqueue(F f, int);

  private:
    friend class Worker;

    // need to keep track of threads so we can join them
    std::vector<std::thread> workers;

    typedef std::pair<int, any_packaged_task> priority_task;

    // emulate 'nice'
    struct task_comp {
      bool operator()(const priority_task& lhs, const priority_task& rhs) const {
        return lhs.first > rhs.first;
      }
    };

    // the prioritized task queue
    std::priority_queue<priority_task, std::vector<priority_task>, task_comp> tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

void Worker::operator()() {
  while(true) {
    std::unique_lock<std::mutex> lock(pool.queue_mutex);

    while(!pool.stop && pool.tasks.empty())
      pool.condition.wait(lock);

    if(pool.stop)
      return;

    any_packaged_task task(pool.tasks.top().second);
    pool.tasks.pop();

    lock.unlock();

    task();
  }
}

// the constructor just launches some amount of workers
Threadpool::Threadpool(Threadpool::size_type threads) : stop(false) {
  workers.reserve(threads);

  for(Threadpool::size_type i = 0; i < threads; ++i)
    workers.emplace_back(Worker(*this));
}

// add new work item to the pool
template<class T, class F>
std::future<T> Threadpool::enqueue(F f, int priority = 0) {
  std::packaged_task<T()> task(f);
  std::future<T> res = task.get_future();

  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    tasks.emplace(std::make_pair(priority, std::move(task)));
  }

  condition.notify_one();

  return res;
}

// the destructor joins all threads
Threadpool::~Threadpool() {
  stop = true;

  condition.notify_all();

  for(Threadpool::size_type i = 0; i < workers.size(); ++i)
    workers[i].join();
}

#endif
