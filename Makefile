CC=gcc
CCX=g++-4.8
CPPFLAGS=-std=c++11 -Wall -I.
LDLIBS=-lstdc++ -lhwloc -lpthread -O3

all:  exp

exp: tbbexp stdexp

debug: CPPFLAGS += -DDEBUG -g
debug: exp

stddebug: CPPFLAGS += -DDEBUG -g
stddebug: stdexp

tbbdebug: CPPFLAGS += -DDEBUG -g
tbbdebug: tbbexp


stdexp:
	$(CCX) $(CPPFLAGS) std_queue_numa_split.cpp -o std_queue_numa_split $(LDLIBS) 
	$(CCX) $(CPPFLAGS) std_queue_numa_split_nodequeue.cpp -o std_queue_numa_split_nodequeue $(LDLIBS) 
	$(CCX) $(CPPFLAGS) std_queue_shared_next_numa_split.cpp -o std_queue_shared_next_numa_split $(LDLIBS) 
	$(CCX) $(CPPFLAGS) std_queue_single_prod_corequeue.cpp -o std_queue_single_prod_corequeue  $(LDLIBS) 

tbbexp:
	$(CCX) $(CPPFLAGS) tbb_queue_numa_split.cpp -o tbb_queue_numa_split $(LDLIBS) -ltbb
	$(CCX) $(CPPFLAGS) tbb_queue_numa_split_nodequeue.cpp -o tbb_queue_numa_split_nodequeue $(LDLIBS) -ltbb
	$(CCX) $(CPPFLAGS) tbb_queue_shared_next_numa_split.cpp -o tbb_queue_shared_next_numa_split $(LDLIBS) -ltbb
	$(CCX) $(CPPFLAGS) tbb_queue_single_prod_corequeue.cpp -o tbb_queue_single_prod_corequeue $(LDLIBS) -ltbb

