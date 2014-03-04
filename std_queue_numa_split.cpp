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
#include <cmath>
#include <Workitem.h>

typedef std::queue<std::shared_ptr<Workitem>> queue_type;

class Producer;

class Worker {
    queue_type queue;
    std::mutex mutex;
    Producer &producer;
    int _node;
    int _sum;
    friend class Producer;

public:
    Worker(Producer &p, int node);
    void work();
};

class Producer {
    std::vector<Worker*> _worker_instances;
    std::vector<std::thread> _worker_threads;
    std::atomic_int _status;
    std::atomic_size_t _next;
    std::mutex _mutex;
    int _iterations;
    int _node;
    friend class Worker;
    friend class Scheduler;

public:
    Producer(int threads, int iterations, int node);
    ~Producer();
    void register_worker(Worker* worker);
    void waitForRegistered();
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

Producer::Producer(int threads, int iterations, int node) : _status(-1), _next(0), _iterations(iterations), _node(node){
    for(int i = 0; i < threads; i++) {
        std::thread thread([this] {
            Worker worker(*this, this->_node);
            worker.work();
        });
        _worker_threads.push_back(std::move(thread));
    }
    void waitForRegistered();
    //std::cout << "Producer on Node" << node << ": All Workers registered!" << std::endl;
    _status = 1;

}

Producer::~Producer() {
    if(_worker_threads.size() > 0) {
        _worker_threads.clear();
    }
}

void Producer::register_worker(Worker* worker) {
    std::lock_guard<std::mutex> lock(_mutex);
    _worker_instances.push_back(worker);
    //std::cout << "Producer: registered Worker " << worker << std::endl;
}

void Producer::waitForRegistered() {
    while(true) {
        _mutex.lock();
        if (_worker_instances.size() >= _worker_threads.size()) {
            _mutex.unlock();
            break;
        }
        else {
            _mutex.unlock();
            sleep(0.5);
        }
    }
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


int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cout<< "usage: " << argv[0] << "<num_threads> <num_workitems> " << std::endl;
        exit(1);
    }
    size_t threads = std::atoi(argv[1]);
    size_t total_items = std::atoi(argv[2]);
    std::condition_variable start_cv;
    std::mutex start_mutex;

    std::vector<std::vector<unsigned>> cores  = getCoresForNodes(threads+1);
    //printVV(cores);

    std::vector<Producer*> producers;
    std::vector<std::thread> prod_threads;
    size_t num_nodes = getNumberOfNodes(getTopology());
    if (threads < num_nodes) {num_nodes = threads;}
    for (size_t nodes = num_nodes; nodes > 0; --nodes) {
        size_t threads_for_node = ceil(float(threads)/nodes);
        size_t node = num_nodes - nodes;

#ifdef DEBUG
        std::cout << "Main: Creating " << threads_for_node << " threads on node " << num_nodes - nodes << "..." << std::endl;
#endif
        Producer* prod = new Producer(threads_for_node ,total_items/num_nodes, node);
        producers.push_back(std::move(prod));
        threads -= threads_for_node;
#ifdef DEBUG
        std::cout << "Main: Finished. Remaining threads: " << threads << ", remaining nodes: " << nodes-1 << "." << std::endl;
#endif
    }

    for(auto p : producers) {
        std::thread thread(&Producer::run, p, std::ref(start_cv), std::ref(start_mutex));
        prod_threads.push_back(std::move(thread));
    }
    sleep(2);

#ifdef DEBUG
    std::cout << "Starting clock..." << std::endl;
#endif

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now() ;
    start_cv.notify_all();
    for(auto& p : prod_threads) {p.join();}
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() ;
    typedef std::chrono::duration<int,std::milli> millisecs_t ;
    millisecs_t duration( std::chrono::duration_cast<millisecs_t>(end-start) ) ;
    std::cout << duration.count() << " ms.\n" ;
  return 0;
}
