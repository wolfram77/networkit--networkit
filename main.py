#!/usr/bin/env python3
import os
import sys
import time
import networkit as nk




# Read graph from file
file = os.path.expanduser(sys.argv[1])
print("Reading graph from file: {}".format(file))
G    = nk.readGraph(file, nk.graphio.Format.EdgeListSpaceOne)
print("Read graph from file: {}".format(file))
print("Nodes: {}, Edges: {}".format(G.numberOfNodes(), G.numberOfEdges()))
print("Directed: {}, Weighted: {}".format(G.isDirected(), G.isWeighted()))

# Find communities using Parallel Louvain
print("Finding communities using Parallel Louvain ...")
PL = nk.community.PLM(G)
start = time.time()
PL.run()
stop  = time.time()

# Print runtime and modularity
partition   = PL.getPartition()
modularity  = nk.community.Modularity().getQuality(partition, G)
timing      = PL.getTiming()
total_time  = 0
for t in timing[b'coarsen']:
    total_time += t
for t in timing[b'move']:
    total_time += t
print("PLM: Runtime: {}ms, Modularity: {}".format(total_time, modularity))
print("Total time: {}".format(stop - start))
