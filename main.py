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

# Predict links using Jaccard Coefficient
print("Predicting links using Jaccard Coefficient ...")
nk.engineering.setLogLevel("INFO")
for batchFraction in [1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2, 1e-1]:
  batchSize = int(batchFraction * G.numberOfEdges())
  if batchSize < 1:
    continue
  print("Batch fraction: {}, Batch size: {}".format(batchFraction, batchSize))
  start = time.time()
  predictions = nk.linkprediction.JaccardIndex(G).runAll()
  predictions = predictions.byCount(1000)
  stop  = time.time()
  total_time = stop - start
  print("JaccardIndex: Runtime: {}".format(total_time))
