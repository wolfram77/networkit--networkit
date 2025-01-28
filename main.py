#!/usr/bin/env python3
import os
import sys
import time
import networkit as nk


# Read graph from file
print("Networkit version: {}".format(nk.__version__), flush=True)
file = os.path.expanduser(sys.argv[1])
print("Reading graph from file: {}".format(file), flush=True)
G    = nk.readGraph(file, nk.graphio.Format.EdgeListSpaceOne)
print("Read graph from file: {}".format(file), flush=True)
print("Nodes: {}, Edges: {}".format(G.numberOfNodes(), G.numberOfEdges()), flush=True)
print("Directed: {}, Weighted: {}".format(G.isDirected(), G.isWeighted()), flush=True)

# Find communities using Parallel LPA
print("Invoking PLP to perform the experiment ...", flush=True)
PL = nk.community.PLP(G)
start = time.time()
PL.run()
stop  = time.time()
