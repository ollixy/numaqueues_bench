#include <queue>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <hwloc.h>
#include <condition_variable>
#include <helper.h>


struct Workitem {
    int _id;
    std::string _msg;
    Workitem(int id, std::string msg) {
        _id = id;
        _msg = msg;
    }
};
typedef std::queue<std::shared_ptr<Workitem>> queue_type;

class Scheduler;
class Producer;

class Worker {
    queue_type queue;
    std::mutex mutex;
    Producer &producer;
    int _node;
    int _sum;
    friend class Producer;
    friend class Scheduler;

public:
    Worker(Producer &p, int node);
    void work();
};

class Producer {
    std::vector<Worker*> _worker_instances;
    std::vector<std::thread> _worker_threads;
    std::atomic_int _status;
    std::atomic_size_t _next;
    std::mutex _reg_mutex;
    int _iterations;
    int _node;
    friend class Worker;
    friend class Scheduler;

public:
    Producer(int threads, int iterations, int node);
    ~Producer();
    void register_worker(Worker* worker);

    void run(std::condition_variable& start, std::mutex& start_mutex);
};

Worker::Worker(Producer &p, int node) : producer(p), _node(node), _sum(0) {
    //std::cout << "Worker: registering Worker " << this << std::endl;
    p.register_worker(this);
    //bindToNode(node);
    //pin_to_core(10);
    pinToNode(node);
}

void Worker::work() {
    //std::cout << "Worker: Starting" << std::endl;

    while (producer._status != 0) {
        mutex.lock();
        if(!queue.empty()) {
            int i = (queue.front())->_id;
            _sum += i;
            //std::cout << i << std::endl;
            queue.pop();
            mutex.unlock();
        }
        else{
            mutex.unlock();
            std::this_thread::yield();
        }
    }
    //while(true) {
    //    mutex.lock();
        while(!queue.empty()) {
            int i = queue.front()->_id;
            _sum += i;
            //std::cout << i << std::endl;
            queue.pop();
    //        mutex.unlock();
        }
        std::cout << _sum << std::endl;
    //}
}

Producer::Producer(int threads, int iterations, int node) : _next(0), _iterations(iterations), _node(node){
    _status = -1;
    //bindToNode(node);

    //pinToNode(node);
    for(int i = 0; i < threads; i++) {
        std::thread thread([this] {
            Worker worker(*this, this->_node);
            worker.work();
        });
        _worker_threads.push_back(std::move(thread));
    }
    while(_worker_threads.size() > _worker_instances.size()) {};
    //std::cout << "Producer on Node" << node << ": All Workers registered!" << std::endl;
    _status = 1;

}

Producer::~Producer() {
    if(_worker_threads.size() > 0) {
        _worker_threads.clear();
    }
}

void Producer::register_worker(Worker* worker) {
    std::lock_guard<std::mutex> lock(_reg_mutex);
    _worker_instances.push_back(worker);
    //std::cout << "Producer: registered Worker " << worker << std::endl;
}

void Producer::run(std::condition_variable& start, std::mutex& start_mutex) {
     pinToNode(_node);
    //std::cout << "Producer: Waiting" << std::endl;
    {
    std::unique_lock<std::mutex> lk(start_mutex);
    start.wait(lk);
    }
    //std::cout << "Producer: Starting" << std::endl;
    //int _sum = 0;
    //while (true) {_sum += 1;}
    for (int i = 0; i < _iterations ; ++i) {
        Worker* nextWorker = _worker_instances[_next.fetch_add(1) % _worker_instances.size()];
        nextWorker->mutex.lock();
        nextWorker->queue.push(std::make_shared<Workitem>(i,"Workitem"));
        nextWorker->mutex.unlock();

    }
    //std::cout << "Producer: Finished Generating!" << std::endl;
    _status = 0;
    for(size_t i = 0; i < _worker_threads.size(); i++){
            _worker_threads[i].join();
            //std::cout << "Producer: Thread " << i << "  joined!" << std::endl;
        }
}

class Scheduler{
};



int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout<< "usage: " << argv[0] << " <num_workitems> " << std::endl;
        exit(1);
    }
    int total_items = std::atoi(argv[1]);
    std::condition_variable start_cv;
    std::mutex start_mutex;
    //pin_to_core(1);
    //pinToNode(0);

    //std::vector<Producer> producers;
    //std::vector<std::thread> prod_threads;


    Producer p0(9,total_items/4,0);
    Producer p1(9,total_items/4,1);
    Producer p2(9,total_items/4,2);
    Producer p3(9,total_items/4,3);

    std::thread t0(&Producer::run, &p0, std::ref(start_cv), std::ref(start_mutex));
    std::thread t1(&Producer::run, &p1, std::ref(start_cv), std::ref(start_mutex));
    std::thread t2(&Producer::run, &p2, std::ref(start_cv), std::ref(start_mutex));
    std::thread t3(&Producer::run, &p3, std::ref(start_cv), std::ref(start_mutex));

    sleep(2);

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now() ;
    start_cv.notify_all();
    t0.join();
    t1.join();
    t2.join();
    t3.join();
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() ;
    typedef std::chrono::duration<int,std::milli> millisecs_t ;
    millisecs_t duration( std::chrono::duration_cast<millisecs_t>(end-start) ) ;
    sleep(2);
    std::cout << duration.count() << " ms.\n" ;
  return 0;
}
