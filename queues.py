"""EXPERIMENTS = [
"std_queue_numa_split",
"std_queue_numa_split_nodequeue",
"std_queue_shared_next_numa_split",
"std_queue_single_prod_corequeue",
"tbb_queue_numa_split",
"tbb_queue_numa_split_nodequeue",
"tbb_queue_shared_next_numa_split",
"tbb_queue_single_prod_corequeue"
]
"""
EXPERIMENTS = [
"std_queue_single_prod_corequeue",
"tbb_queue_single_prod_corequeue"]

import re
import os
import subprocess
import time
import numpy as np
import matplotlib as mpl
import psutil
mpl.use('pdf')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import sys

HYRISE_PORT = 6001
PCM_PATH = "/home/Oliver.Xylander/numaexp/IntelPerformanceCounterMonitorV2.6"
EXPERIMENT_PATH = "/home/Oliver.Xylander/numaexp/experiments"
RESULT_PATH = "/home/Oliver.Xylander/numaexp/experiments/results"

def execute(experiment, threads, workitems):
  print "Running ", experiment, ". Threads: ", threads, "."
  #fname = "/home/Oliver.Xylander/numaexp/temp.log"
  #with open(fname,'w') as fhandle:
  print 'sudo  %s/pcm-numa.x "%s/%s %d %d" -c' % (PCM_PATH, EXPERIMENT_PATH, experiment, threads, workitems)
  p1 = subprocess.Popen('sudo  %s/pcm-numa.x "%s/%s %d %d" -c' % (PCM_PATH, EXPERIMENT_PATH, experiment, threads, workitems), shell=True, stdout=subprocess.PIPE)
  #p1 = subprocess.Popen(["/home/Oliver.Xylander/hyrise_nvm/build/hyrise-server_release","-t",str(hyrise_threads), "-s", "CoreBoundQueuesScheduler"])
  #p1 = subprocess.Popen('sudo /home/Oliver.Xylander/numaexp/IntelPerformanceCounterMonitorV2.6/pcm-numa.x "/home/Oliver.Xylander/hyrise_nvm/build/hyrise-server_release -t %d - s CoreTBBQueueScheduler -p %d " -c' % (hyrise_threads, port), shell=True, stdout=fhandle)
  time.sleep(2);
  p1_out, p1_err = p1.communicate()
  #p1.terminate()`
  #pid = [p.pid for p in psutil.process_iter() if ('hyrise' in p.name and str(port) in p.cmdline)]
  #os.system("sudo kill -9 %s"%(pid[0]))

  time.sleep(2.0)
  runtime = re.search("Time elapsed: ([0-9]+) ms", p1_out).group(1)
  _, avg_ipc, sum_instr, sum_cycles, sum_local, sum_remote, _ = p1_out.split('\n')[-4].split(',')

  sum_local = 0
  sum_remote = 0
    #for line in p1_out.split('\n'):
    #  if line[:1].isalpha(): continue
    #  core, ipc, instructions, cycles, local, remote = line.split(',')
    #  total_remote += remote
    #  total_local += local
  #with open(fname,'r') as fhandle:
  #  _, avg_ipc, sum_instr, sum_cycles, sum_local, sum_remote, _ = fhandle.read().split('\n')[-4].split(',')
  return (int(runtime), int(sum_local), int(sum_remote))


workitems = 100000
thread_max = 2
runs = 2

try:
  thread_max = int(sys.argv[1])
except IndexError:
  pass
finally:
  print "threads max = %d" % thread_max

x = []
y_runtime = []
y_local_dram = []
y_remote_dram = []

#Init
p0 = subprocess.Popen(["sudo", "echo", "sudo password accepted"])
p0.wait()

for exp in EXPERIMENTS:
  try:
    for threads in range(1, thread_max):
      runtime = []
      total_local = []
      total_remote = []
      for i in range(runs):
        res = execute(exp, threads, workitems)
        runtime.append(res[0])
        total_local.append(res[1])
        total_remote.append(res[2])

      x.append(threads)
      y_runtime.append(np.mean(runtime))
      y_local_dram.append(np.mean(total_local))
      y_remote_dram.append(np.mean(total_remote))
  except KeyboardInterrupt:
    raise
  finally:
    fig = plt.figure(1, figsize=(10, 30))

    plt.subplot(3, 1, 1)
    plt.title('Total Runtime, %d Items' % workitems)
    plt.xlabel('#Worker Threads')
    plt.ylabel('#requests/sec')
    plt.plot(x, y_runtime, 'ro-')

    plt.subplot(3, 1, 2)
    plt.xlabel('#Worker Threads')
    plt.ylabel('Local DRAM Access [MB]')
    plt.title('Local DRAM Access across all cores')
    plt.plot(x, y_local_dram, 'ro-')

    plt.subplot(3, 1, 3)
    plt.xlabel('#Worker Threads')
    plt.ylabel('Remote DRAM Access [MB]')
    plt.title('Remote DRAM Access across all cores')
    plt.plot(x, y_remote_dram, 'ro-')

    name = "%s/%s_%d.pdf" % (RESULT_PATH, exp, workitems)
    print "Saving results in file: %s" % name
    pp = PdfPages(name)
    pp.savefig(fig)
    pp.close()
