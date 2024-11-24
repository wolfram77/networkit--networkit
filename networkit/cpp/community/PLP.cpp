/*
 * PLP.cpp
 *
 *  Created on: 07.12.2012
 *      Author: Christian Staudt
 */

#include <omp.h>

#include <networkit/Globals.hpp>
#include <networkit/auxiliary/Log.hpp>
#include <networkit/auxiliary/Random.hpp>
#include <networkit/auxiliary/Timer.hpp>
#include <networkit/community/PLP.hpp>

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <atomic>


/**
 * Measure the memory usage of the current process.
 * @returns memory usage in gigabytes
 */
inline float measureMemoryUsage() {
  char buf[128];
  FILE *file = fopen("/proc/self/status", "r");
  size_t rss = 0;
  while  (fgets(buf, 128, file)) {
    if (strncmp(buf, "VmRSS:", 6) != 0) continue;
    if (sscanf (buf, "VmRSS: %ld kB", &rss) != 1) { rss = 0; break; }
    break;
  }
  fclose(file);
  return rss / (1024.0f * 1024.0f);
}




namespace NetworKit {

PLP::PLP(const Graph &G, count theta, count maxIterations)
    : CommunityDetectionAlgorithm(G), updateThreshold(theta), maxIterations(maxIterations) {}

PLP::PLP(const Graph &G, const Partition &baseClustering, count theta)
    : CommunityDetectionAlgorithm(G, baseClustering), updateThreshold(theta) {}

void PLP::run() {
    std::atomic<float> memoryUsage = measureMemoryUsage();
    printf("Memory usage (in PLP initial): %8.4f GB\n", memoryUsage.load());
    if (hasRun) {
        throw std::runtime_error("The algorithm has already run on the graph.");
    }

    // set unique label for each node if no baseClustering was given
    index z = G->upperNodeIdBound();
    if (result.numberOfElements() != z) {
        result = Partition(z);
        result.allToSingletons();
    }

    using label = index; // a label is the same as a cluster id

    count n = G->numberOfNodes();
    // update threshold heuristic
    if (updateThreshold == none) {
        updateThreshold = (count)(n / 1e5);
    }

    // number of nodes which have been updated in last iteration
    count nUpdated = n; // all nodes have new labels -> first loop iteration runs

    nIterations = 0; // number of iterations

    /**
     * == Dealing with isolated nodes ==
     *
     * The pseudocode published does not deal with isolated nodes (and therefore does not terminate
     * if they are present). Isolated nodes stay singletons. They can be ignored in the while loop,
     * but the loop condition must compare to the number of non-isolated nodes instead of n.
     *
     * == Termination criterion ==
     *
     * The published termination criterion is: All nodes have got the label of the majority of their
     * neighbors. In general this does not work. It was changed to: No label was changed in last
     * iteration.
     */

    std::vector<bool> activeNodes(z, true); // record if node must be processed

    Aux::Timer runtime;

    // propagate labels
    // as long as a label has changed... or maximum iterations reached
    while ((nUpdated > this->updateThreshold) && (nIterations < maxIterations)) {
        runtime.start();
        nIterations += 1;
        DEBUG("[BEGIN] LabelPropagation: iteration #", nIterations);

        // reset updated
        nUpdated = 0;

        G->balancedParallelForNodes([&](node v) {
            if (!activeNodes[v] || G->degree(v) == 0)
                return; // node is isolated

            // neighborLabelCounts maps label -> frequency in the neighbors
            std::map<label, double> labelWeights;

            // weigh the labels in the neighborhood of v
            G->forNeighborsOf(v, [&](node w, edgeweight weight) {
                label lw = result.subsetOf(w);
                labelWeights[lw] += weight; // add weight of edge {v, w}
            });

            // get heaviest label
            label heaviest = std::max_element(labelWeights.begin(), labelWeights.end(),
                                              [](const std::pair<label, edgeweight> &p1,
                                                 const std::pair<label, edgeweight> &p2) {
                                                  return p1.second < p2.second;
                                              })
                                 ->first;

            if (result.subsetOf(v) != heaviest) { // UPDATE
                result.moveToSubset(heaviest, v); // result[v] = heaviest;
#pragma omp atomic
                nUpdated += 1;
                G->forNeighborsOf(v, [&](node u) { activeNodes[u] = true; });
            } else {
                activeNodes[v] = false;
            }
            // Update memory usage.
            float memoryUsageTh  = measureMemoryUsage();
            float memoryUsageOld = memoryUsage.load();
            while (memoryUsageTh > memoryUsageOld) {
                bool ok = memoryUsage.compare_exchange_strong(memoryUsageOld, memoryUsageTh);
                if  (ok) {
                    // printf("Memory usage (in PLP loop): %8.4f GB\n", memoryUsageTh);
                    break;
                }
                memoryUsageOld = memoryUsage.load();
            }
        });

        // for each while loop iteration...

        runtime.stop();
        this->timing.push_back(runtime.elapsedMilliseconds());
        DEBUG("[DONE] LabelPropagation: iteration #", nIterations, " - updated ", nUpdated,
              " labels, time spent: ", runtime.elapsedTag());

    } // end while
    hasRun = true;
    printf("Memory usage (in PLP final): %8.4f GB\n", measureMemoryUsage());
    printf("Max Memory usage (in PLP final): %8.4f GB\n", memoryUsage.load());
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
