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

# Find communities using Parallel Leiden
print("Finding communities using Parallel Leiden ...")
nk.engineering.setLogLevel("INFO")
PL = nk.community.ParallelLeiden(G, iterations=10)
start = time.time()
PL.run()
stop  = time.time()

# Print runtime and modularity
partition   = PL.getPartition()
modularity  = nk.community.Modularity().getQuality(partition, G)
total_time  = 0
print("ParallelLeiden: Runtime: {}ms, Modularity: {}".format(total_time, modularity))
print("Total time: {}".format(stop - start))

# Count number of disconnected communities
print("Number of communities: {}".format(partition.numberOfSubsets()))

# Save communities to file
comm = os.path.expanduser(sys.argv[2])
print("Saving communities to file {} ...".format(comm))
with open(comm, "w") as f:
  vector = partition.getVector()
  for i in range(len(vector)):
    f.write("{} {}\n".format(i, vector[i]))
