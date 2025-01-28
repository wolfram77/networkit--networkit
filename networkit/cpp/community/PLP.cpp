/*
 * PLP.cpp
 *
 *  Created on: 07.12.2012
 *      Author: Christian Staudt
 */

#include <cstdio>
#include <cmath>
#include <tuple>
#include <vector>
#include <chrono>
#include <random>
#include <omp.h>

#include <networkit/Globals.hpp>
#include <networkit/auxiliary/Log.hpp>
#include <networkit/auxiliary/Random.hpp>
#include <networkit/auxiliary/Timer.hpp>
#include <networkit/community/PLP.hpp>

using namespace std;




namespace NetworKit {
// Represents an edge.
typedef tuple<node, node> Edge;


// Get current time.
inline auto timeNow() {
  return chrono::high_resolution_clock::now();
}


// Get duration between two time points, in milliseconds.
template <class T>
inline float duration(const T &start, const T &end) {
  auto a = chrono::duration<float>(end - start);
  return a.count() * 1000.0f;
}


// Generate edge deletions.
inline vector<Edge> generateEdgeDeletions(const Graph &G, int batchSize, bool isSymmetric) {
  int retries = 5;
  index n = G.numberOfNodes();
  vector<Edge> deletions;
  random_device dev;
  default_random_engine rnd(dev());
  uniform_int_distribution<node> dist(1, n);
  for (index b=0; b<batchSize; ++b) {
    for (int r=0; r<retries; ++r) {
      node  u = dist(rnd);
      count degree = G.degree(u);
      if (degree == 0) continue;
      index j = dist(rnd) % degree;
      node  v = G.getIthNeighbor(u, j);
      deletions.push_back({u, v});
      if (isSymmetric) deletions.push_back({v, u});
      break;
    }
  }
  return deletions;
}


// Generate edge insertions.
inline vector<Edge> generateEdgeInsertions(const Graph &G, int batchSize, bool isSymmetric) {
  int retries = 5;
  index n = G.numberOfNodes();
  vector<Edge> insertions;
  random_device dev;
  default_random_engine rnd(dev());
  uniform_int_distribution<node> dist(1, n);
  for (index b=0; b<batchSize; ++b) {
    for (int r=0; r<retries; ++r) {
      node  u = dist(rnd);
      node  v = dist(rnd);
      if (G.hasEdge(u, v)) continue;
      insertions.push_back({u, v});
      if (isSymmetric) insertions.push_back({v, u});
      break;
    }
  }
  return insertions;
}




PLP::PLP(const Graph &G, count theta, count maxIterations)
    : CommunityDetectionAlgorithm(G), updateThreshold(theta), maxIterations(maxIterations) {}


PLP::PLP(const Graph &G, const Partition &baseClustering, count theta)
    : CommunityDetectionAlgorithm(G, baseClustering), updateThreshold(theta) {}


void PLP::run() {
  bool symmetric = false;
  printf("Running from inside PLP ...\n");
  printf("Accessing loaded graph ...\n");
  printf("Nodes: %zu, Edges: %zu\n", G->numberOfNodes(), G->numberOfEdges());
  printf("Elapsed time: %.2f ms\n", 0.0f);
  printf("\n");
  // Perform batch updates of varying sizes.
  for (int batchPower=-7; batchPower<=-1; ++batchPower) {
    count m = G->numberOfEdges();
    double batchFraction = pow(10.0, batchPower);
    count batchSize = count(round(m * batchFraction));
    printf("Batch fraction: %.1e [%zu edges]\n", batchFraction, batchSize);
    // Perform edge deletions.
    {
      vector<Edge> deletions = generateEdgeDeletions(*G, batchSize, symmetric);
      printf("Cloning graph ...\n");
      auto  t0 = timeNow();
      Graph *H = new Graph(*G);
      auto  t1 = timeNow();
      printf("Nodes: %zu, Edges: %zu\n", H->numberOfNodes(), H->numberOfEdges());
      printf("Elapsed time: %.2f ms\n", duration(t0, t1));
      printf("Deleting edges [%zu edges] ...\n", deletions.size());
      auto  t2 = timeNow();
      size_t D = deletions.size();
      #pragma omp parallel for schedule(dynamic, 2048)
      for (index i=0; i<D; ++i) {
        node u = get<0>(deletions[i]);
        node v = get<1>(deletions[i]);
        H->removeEdge(u, v);  // HEY: Is this thread-safe?
      }
      auto t3 = timeNow();
      printf("Nodes: %zu, Edges: %zu\n", H->numberOfNodes(), H->numberOfEdges());
      printf("Elapsed time: %.2f ms\n", duration(t2, t3));
      for (index i=0; i<D; ++i) {
        node u = get<0>(deletions[i]);
        node v = get<1>(deletions[i]);
        assert(!H->hasEdge(u, v));
      }
    }
    // Perform edge insertions.
    {
      vector<Edge> insertions = generateEdgeInsertions(*G, batchSize, symmetric);
      printf("Cloning graph ...\n");
      auto  t0 = timeNow();
      Graph *H = new Graph(*G);
      auto  t1 = timeNow();
      printf("Nodes: %zu, Edges: %zu\n", H->numberOfNodes(), H->numberOfEdges());
      printf("Elapsed time: %.2f ms\n", duration(t0, t1));
      printf("Inserting edges [%zu edges] ...\n", insertions.size());
      auto  t2 = timeNow();
      size_t I = insertions.size();
      #pragma omp parallel for schedule(dynamic, 2048)
      for (index i=0; i<I; ++i) {
        node u = get<0>(insertions[i]);
        node v = get<1>(insertions[i]);
        H->addEdge(u, v);  // HEY: Is this thread-safe?
      }
      H->removeMultiEdges();
      auto t3 = timeNow();
      printf("Nodes: %zu, Edges: %zu\n", H->numberOfNodes(), H->numberOfEdges());
      printf("Elapsed time: %.2f ms\n", duration(t2, t3));
      for (index i=0; i<I; ++i) {
        node u = get<0>(insertions[i]);
        node v = get<1>(insertions[i]);
        assert(H->hasEdge(u, v));
    }
    printf("\n");
  }
}


void PLP::setUpdateThreshold(count th) {
    this->updateThreshold = th;
}


count PLP::numberOfIterations() {
    assureFinished();
    return this->nIterations;
}


const std::vector<count> &PLP::getTiming() const {
    assureFinished();
    return this->timing;
}

} /* namespace NetworKit */
