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
print("Memory usage (initial): {} GB (max)".format(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/(1024*1024)), flush=True)
file = os.path.expanduser(sys.argv[1])
print("Reading graph from file: {}".format(file))
G    = nk.readGraph(file, nk.graphio.Format.EdgeListSpaceOne)
print("Read graph from file: {}".format(file))
print("Nodes: {}, Edges: {}".format(G.numberOfNodes(), G.numberOfEdges()))
print("Directed: {}, Weighted: {}".format(G.isDirected(), G.isWeighted()))
print("Memory usage (after readGraph): {} GB".format(psutil.Process().memory_info().rss/(1024*1024*1024)), flush=True)
print("Memory usage (after readGraph): {} GB (max)".format(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/(1024*1024)), flush=True)

# Find communities using PLM (Parallel Louvain Method)
print("Finding communities using PLM ...")
# nk.engineering.setLogLevel("INFO")
PL = nk.community.PLM(G)
start = time.time()
PL.run()
stop  = time.time()
print("Memory usage (after PLM): {} GB".format(psutil.Process().memory_info().rss/(1024*1024*1024)), flush=True)
print("Memory usage (after PLM): {} GB (max)".format(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/(1024*1024)), flush=True)

# Print runtime and modularity
partition   = PL.getPartition()
modularity  = nk.community.Modularity().getQuality(partition, G)
total_time  = 0
timing      = PL.getTiming()
for t in timing[b'coarsen']:
    total_time += t
for t in timing[b'move']:
    total_time += t
print("PLM: Runtime: {}ms, Modularity: {}".format(total_time, modularity))
print("Total time: {}".format(stop - start))
print("Memory usage (after getPartition): {} GB".format(psutil.Process().memory_info().rss/(1024*1024*1024)), flush=True)
print("Memory usage (after getPartition): {} GB (max)".format(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/(1024*1024)), flush=True)
