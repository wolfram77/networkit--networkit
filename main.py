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

# Find ranks of vertices using PageRank
print("Finding ranks of vertices using PageRank ...")
PL = nk.centrality.PageRank(G, tol=1e-10)
start = time.time()
PL.run()
stop  = time.time()

# Print runtime
print("Total time: {}".format(stop - start))
