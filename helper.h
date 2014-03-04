#include <hwloc.h>
#include <iostream>

std::ostream& operator<<(std::ostream& os, const hwloc_bitmap_t& bm)
{
    size_t j;
    hwloc_bitmap_foreach_begin(j, bm)
    {
        os << hwloc_bitmap_isset(bm, j);
    }
    hwloc_bitmap_foreach_end();
    return os;
}

void pin_to_core(size_t core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

static hwloc_topology_t& getTopology() {
  static hwloc_topology_t topology = [] () {
    hwloc_topology_t t;
    hwloc_topology_init(&t);
    hwloc_topology_load(t);
    return t;
  }();
  return topology;
}

inline hwloc_obj_t getNode(const hwloc_topology_t& topo, std::size_t node) {
  if (auto o = hwloc_get_obj_by_type(topo, HWLOC_OBJ_NODE, node)) { return o; }
  else { return nullptr; }
}

inline hwloc_obj_t getCPU(const hwloc_topology_t& topo, std::size_t cpu) {
  if (auto o = hwloc_get_obj_by_type(topo, HWLOC_OBJ_CORE, cpu)) { return o; }
  else { return nullptr; }
}

std::vector<unsigned> getCoresForNode(hwloc_topology_t topology, unsigned node){
  std::vector<unsigned> children;
  unsigned number_of_cores;
  // get hwloc obj for node
  hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NODE, node);
  // get all cores and check whether core is in subtree of node, if yes, push to vector
  // get number of cores by type
  number_of_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
  // iterate over cores and check whether in subtree
  hwloc_obj_t core;
  for(unsigned i = 0; i < number_of_cores; i++){
    core = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, i);
    if(hwloc_obj_is_in_subtree(topology, core, obj)){
      children.push_back(core->logical_index);
    }
  }
  return children;
}

void* memoryAt(int node, std::size_t size) {
  const hwloc_topology_t& topo = getTopology();
  if (auto obj = getNode(topo, node)) {
    void* memory = hwloc_alloc_membind_nodeset(topo,
                                               size,
                                               obj->nodeset,
                                               HWLOC_MEMBIND_BIND,
                                               HWLOC_MEMBIND_NOCPUBIND);
    return memory;
  } else {
    exit(-1);
  }
}

void bindToNode(int node) {
  hwloc_topology_t topology = getTopology();
  hwloc_cpuset_t cpuset;
  hwloc_obj_t obj;

  // The actual core
  obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, node);
  cpuset = hwloc_bitmap_dup(obj->cpuset);
  hwloc_bitmap_singlify(cpuset);

  // bind
  if (hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_STRICT | HWLOC_CPUBIND_NOMEMBIND | HWLOC_CPUBIND_THREAD)) {
    char *str;
    int error = errno;
    hwloc_bitmap_asprintf(&str, obj->cpuset);
    printf("Couldn't bind to cpuset %s: %s\n", str, strerror(error));
    free(str);
    throw std::runtime_error(strerror(error));
  }

  // free duplicated cpuset
  hwloc_bitmap_free(cpuset);

  // assuming single machine system
  obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_MACHINE, 0);
  // set membind policy interleave for this thread
  if (hwloc_set_membind_nodeset(topology, obj->nodeset, HWLOC_MEMBIND_INTERLEAVE, HWLOC_MEMBIND_STRICT | HWLOC_MEMBIND_THREAD)) {
    char *str;
    int error = errno;
    hwloc_bitmap_asprintf(&str, obj->nodeset);
    fprintf(stderr, "Couldn't membind to nodeset  %s: %s\n", str, strerror(error));
    fprintf(stderr, "Continuing as normal, however, no guarantees\n");
    free(str);
  }
}

void checkNode (char** str) {
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    hwloc_topology_t topology = getTopology();
    hwloc_get_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);
    hwloc_bitmap_asprintf(str, cpuset);
}

void pinToNode(int node) {
    const hwloc_topology_t& topology = getTopology();
    auto obj = getNode(topology, node);
    if (!obj) {exit(-1);};
    //remove HT
    //hwloc_cpuset_t cpuset = hwloc_bitmap_dup(obj->cpuset);
    //hwloc_bitmap_singlify(cpuset);
    //std::cout << "pin to Node called with Node = " << node << " BM: " << obj-> cpuset << std::endl;
    hwloc_set_cpubind(topology, obj->cpuset, HWLOC_CPUBIND_STRICT | HWLOC_CPUBIND_NOMEMBIND | HWLOC_CPUBIND_THREAD);
    hwloc_set_membind_nodeset(topology, obj->nodeset, HWLOC_MEMBIND_INTERLEAVE, HWLOC_MEMBIND_STRICT | HWLOC_MEMBIND_THREAD);
    //hwloc_bitmap_free(cpuset);
}

void pinToNode(int node, std::thread& thread) {
    const hwloc_topology_t& topology = getTopology();
    auto obj = getNode(topology, node);
    if (!obj) {exit(-1);};
    //remove HT
    //hwloc_cpuset_t cpuset = hwloc_bitmap_dup(obj->cpuset);
    //hwloc_bitmap_singlify(cpuset);
    //std::cout << "pin to Node called with Node = " << node << " BM: " << obj-> cpuset << std::endl;
    hwloc_set_thread_cpubind(topology, thread.native_handle(), obj->cpuset, HWLOC_CPUBIND_STRICT | HWLOC_CPUBIND_NOMEMBIND | HWLOC_CPUBIND_THREAD);
    hwloc_set_membind_nodeset(topology, obj->nodeset, HWLOC_MEMBIND_INTERLEAVE, HWLOC_MEMBIND_STRICT | HWLOC_MEMBIND_THREAD);
    //hwloc_bitmap_free(cpuset);
}

void pinToCore(int core) {
    const hwloc_topology_t& topology = getTopology();
    auto obj = getCPU(topology, core);
    if (!obj) {exit(-1);};
    //remove HT
    //hwloc_cpuset_t cpuset = hwloc_bitmap_dup(obj->cpuset);
    //hwloc_bitmap_singlify(cpuset);
    hwloc_set_cpubind(topology, obj->cpuset, HWLOC_CPUBIND_STRICT | HWLOC_CPUBIND_NOMEMBIND | HWLOC_CPUBIND_PROCESS);
    hwloc_set_membind_nodeset(topology, obj->nodeset, HWLOC_MEMBIND_INTERLEAVE, HWLOC_MEMBIND_STRICT | HWLOC_MEMBIND_THREAD);
    //hwloc_bitmap_free(cpuset);
    //hwloc_bitmap_free(cpuset);
}

unsigned getNumberOfNodes(hwloc_topology_t topology){
  return hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NODE);
}

std::vector<std::vector<unsigned>> getCoresForNodes() {
  std::vector<std::vector<unsigned>> cores;
  hwloc_topology_t topology = getTopology();
  unsigned num_nodes = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NODE);
  for (size_t i =0; i< num_nodes; i++) {
    cores.push_back(std::move(getCoresForNode(topology, i)));
  }
  return cores;
}

std::vector<std::vector<unsigned>> getCoresForNodes(size_t requested) {
  std::vector<std::vector<unsigned>> cores;
  hwloc_topology_t topology = getTopology();
  unsigned num_nodes = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NODE);
  unsigned num_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
  if (requested > num_cores) {requested = num_cores;}
  size_t cur_node = 0;
  std::vector<unsigned> cur_cores, new_cores;
  while (requested > 0 && cur_node < num_nodes) {
    cur_cores = getCoresForNode(topology, cur_node++);
    //for (auto i : cur_cores) {std::cout << i;}
    if (cur_cores.size() < requested) {
      requested -= cur_cores.size();
      cores.push_back(std::move(cur_cores));
    }
    else {
      for (size_t i=0; i < requested; ++i) {
        new_cores.push_back(cur_cores[i]);
      }
      requested -= new_cores.size();
      cores.push_back(std::move(new_cores));
    }
  }
  return cores;
}

void printVV(std::vector<std::vector<unsigned>>& v) {
  for (size_t i=0; i < v.size(); ++i) {
    std::cout << i << ": ";
    for (auto c : v[i]) {
      std::cout << c << ", ";
    }
  std::cout << std::endl;
  }
}
