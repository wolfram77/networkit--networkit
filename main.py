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

# Find communities using Parallel LPA
print("Finding communities using Parallel LPA ...")
PL = nk.community.PLP(G)
start = time.time()
PL.run()
stop  = time.time()

# Print runtime and modularity
partition   = PL.getPartition()
modularity  = nk.community.Modularity().getQuality(partition, G)
timing      = PL.getTiming()
print(timing)
total_time  = 0
for t in timing:
    total_time += t
print("PLP: Runtime: {}ms, Modularity: {}".format(total_time, modularity))
print("Total time: {}".format(stop - start))
