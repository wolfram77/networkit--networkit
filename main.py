#!/usr/bin/env python3
import os
import sys
import time
import psutil
import resource
import networkit as nk




# Read graph from file
print("Networkit version: {}".format(nk.__version__), flush=True)
print("Memory usage (initial): {} GB".format(psutil.Process().memory_info().rss/(1024*1024*1024)), flush=True)
print("Max memory usage (initial): {} GB".format(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/(1024*1024)), flush=True)
file = os.path.expanduser(sys.argv[1])
print("Reading graph from file: {}".format(file), flush=True)
G    = nk.readGraph(file, nk.graphio.Format.EdgeListSpaceOne)
print("Read graph from file: {}".format(file), flush=True)
print("Nodes: {}, Edges: {}".format(G.numberOfNodes(), G.numberOfEdges()), flush=True)
print("Directed: {}, Weighted: {}".format(G.isDirected(), G.isWeighted()), flush=True)
print("Memory usage (after readGraph): {} GB".format(psutil.Process().memory_info().rss/(1024*1024*1024)), flush=True)
print("Max memory usage (after readGraph): {} GB".format(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/(1024*1024)), flush=True)

# Find communities using Parallel LPA
print("Finding communities using Parallel LPA ...", flush=True)
PL = nk.community.PLP(G)
start = time.time()
PL.run()
stop  = time.time()
print("Memory usage (after PLP): {} GB".format(psutil.Process().memory_info().rss/(1024*1024*1024)), flush=True)
print("Max memory usage (after PLP): {} GB".format(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/(1024*1024)), flush=True)

# Print runtime and modularity
partition   = PL.getPartition()
modularity  = nk.community.Modularity().getQuality(partition, G)
timing      = PL.getTiming()
print(timing)
total_time  = 0
for t in timing:
    total_time += t
print("PLP: Runtime: {}ms, Modularity: {}".format(total_time, modularity), flush=True)
print("Total time: {}".format(stop - start), flush=True)
print("Memory usage (after getPartition): {} GB".format(psutil.Process().memory_info().rss/(1024*1024*1024)), flush=True)
print("Max memory usage (after getPartition): {} GB".format(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/(1024*1024)), flush=True)

# Save community memberships to file
print("Number of communities: {}".format(partition.numberOfSubsets()), flush=True)
comm = os.path.expanduser(sys.argv[2])
print("Saving communities to file {} ...".format(comm), flush=True)
with open(comm, "w") as f:
  vector = partition.getVector()
  for i in range(len(vector)):
    f.write("{} {}\n".format(i, vector[i]))
