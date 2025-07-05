/*
 * MLPLM.cpp
 *
 *  Created on: 20.11.2013
 *      Author: cls
 */

#include <omp.h>
#include <sstream>
#include <utility>

#include <networkit/auxiliary/Log.hpp>
#include <networkit/auxiliary/SignalHandling.hpp>
#include <networkit/auxiliary/Timer.hpp>
#include <networkit/coarsening/ClusteringProjector.hpp>
#include <networkit/coarsening/ParallelPartitionCoarsening.hpp>
#include <networkit/community/PLM.hpp>

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <atomic>




/** Memory usage structure for Linux systems. */
typedef struct {
    int64_t vmPeak; // Peak virtual memory size
    int64_t vmSize; // Current virtual memory size
    int64_t vmHwm;  // High water mark of virtual memory size
    int64_t vmRss;  // Resident set size (physical memory used)
} MemoryUsage;


/**
 * Measure the memory usage of the current process.
 * @returns memory usage in gigabytes
 */
inline MemoryUsage measureMemoryUsage() {
  char buf[128]; int read = 0;
  FILE *file = fopen("/proc/self/status", "r");
  MemoryUsage usage = {0, 0, 0, 0};
  while  (fgets(buf, 128, file)) {
    if (sscanf(buf, "VmPeak:%ld kB", &usage.vmPeak) == 1) { ++read; continue;}
    if (sscanf(buf, "VmSize:%ld kB", &usage.vmSize) == 1) { ++read; continue;}
    if (sscanf(buf, "VmHWM:%ld kB",  &usage.vmHwm)  == 1) { ++read; continue;}
    if (sscanf(buf, "VmRSS:%ld kB",  &usage.vmRss)  == 1) { ++read; continue;}
  }
  fclose(file);
  if (read != 4) printf("ERROR: Only %d/4 memory usage values read from /proc/self/status\n", read);
  return usage;
}


/**
 * Measure the memory usage of the current process.
 * @param vmPeak Peak virtual memory size
 * @param vmSize Current virtual memory size
 * @param vmHwm High water mark of virtual memory size
 * @param vmRss Resident set size (physical memory used)
 */
inline void measureMemoryUsageW(std::atomic<int64_t>& vmPeak, std::atomic<int64_t>& vmSize, std::atomic<int64_t>& vmHwm, std::atomic<int64_t>& vmRss) {
  char buf[128]; int read = 0;
  FILE *file = fopen("/proc/self/status", "r");
  while (fgets(buf, 128, file)) {
    if (sscanf(buf, "VmPeak:%ld kB", &vmPeak) == 1) { ++read; continue;}
    if (sscanf(buf, "VmSize:%ld kB", &vmSize) == 1) { ++read; continue;}
    if (sscanf(buf, "VmHWM:%ld kB", &vmHwm) == 1) { ++read; continue;}
    if (sscanf(buf, "VmRSS:%ld kB", &vmRss) == 1) { ++read; continue;}
  }
  fclose(file);
  if (read != 4) printf("ERROR: Only %d/4 memory usage values read from /proc/self/status\n", read);
}


/**
 * Update the memory usage of the current process (to the maximum).
 * @param vmPeak Peak virtual memory size
 * @param vmSize Current virtual memory size
 * @param vmHwm High water mark of virtual memory size
 * @param vmRss Resident set size (physical memory used)
 */
inline void updateMemoryUsageU(std::atomic<int64_t>& vmPeak, std::atomic<int64_t>& vmSize, std::atomic<int64_t>& vmHwm, std::atomic<int64_t>& vmRss) {
  MemoryUsage usage = measureMemoryUsage();
  while (1) {
    int64_t vmPeakOld = vmPeak.load();
    if (usage.vmPeak <= vmPeakOld) break;
    if (vmPeak.compare_exchange_strong(vmPeakOld, usage.vmPeak)) break;
  }
  while (1) {
    int64_t vmSizeOld = vmSize.load();
    if (usage.vmSize <= vmSizeOld) break;
    if (vmSize.compare_exchange_strong(vmSizeOld, usage.vmSize)) break;
  }
  while (1) {
    int64_t vmHwmOld = vmHwm.load();
    if (usage.vmHwm <= vmHwmOld) break;
    if (vmHwm.compare_exchange_strong(vmHwmOld, usage.vmHwm)) break;
  }
  while (1) {
    int64_t vmRssOld = vmRss.load();
    if (usage.vmRss <= vmRssOld) break;
    if (vmRss.compare_exchange_strong(vmRssOld, usage.vmRss)) break;
  }
}




namespace NetworKit {

PLM::PLM(const Graph &G, bool refine, double gamma, std::string par, count maxIter, bool turbo,
         bool recurse)
    : CommunityDetectionAlgorithm(G), parallelism(std::move(par)), refine(refine), gamma(gamma),
      maxIter(maxIter), turbo(turbo), recurse(recurse) {}

PLM::PLM(const Graph &G, const PLM &other)
    : CommunityDetectionAlgorithm(G), parallelism(other.parallelism), refine(other.refine),
      gamma(other.gamma), maxIter(other.maxIter), turbo(other.turbo), recurse(other.recurse) {}

void PLM::run() {
    if (hasRun) {
        throw std::runtime_error("The algorithm has already run on the graph.");
    }

    std::atomic<int64_t> vmPeak(0), vmSize(0), vmHwm(0), vmRss(0);
    measureMemoryUsageW(vmPeak, vmSize, vmHwm, vmRss);
    printf("Memory usage in PLM: VmPeak %8.4f GB, VmSize %8.4f GB, VmHwm %8.4f GB, VmRss %8.4f GB (initial)\n",
        vmPeak / (1024.0 * 1024.0),
        vmSize / (1024.0 * 1024.0),
        vmHwm / (1024.0 * 1024.0),
        vmRss / (1024.0 * 1024.0)
    );
    Aux::SignalHandler handler;

    count z = G->upperNodeIdBound();

    // init communities to singletons
    Partition zeta(z);
    zeta.allToSingletons();
    index o = zeta.upperBound();

    // init graph-dependent temporaries
    std::vector<double> volNode(z, 0.0);
    // $\omega(E)$
    edgeweight total = G->totalEdgeWeight();
    DEBUG("total edge weight: ", total);
    edgeweight divisor = (2 * total * total); // needed in modularity calculation

    G->parallelForNodes([&](node u) { // calculate and store volume of each node
        volNode[u] += G->weightedDegree(u);
        volNode[u] += G->weight(u, u); // consider self-loop twice
    });

    // init community-dependent temporaries
    std::vector<double> volCommunity(o, 0.0);
    zeta.parallelForEntries([&](node u, index C) { // set volume for all communities
        if (C != none)
            volCommunity[C] = volNode[u];
    });

    // first move phase
    bool moved = false;  // indicates whether any node has been moved in the last pass
    bool change = false; // indicates whether the communities have changed at all

    // stores the affinity for each neighboring community (index), one vector per thread
    std::vector<std::vector<edgeweight>> turboAffinity;
    // stores the list of neighboring communities, one vector per thread
    std::vector<std::vector<index>> neigh_comm;

    if (turbo) {
        // initialize arrays for all threads only when actually needed
        if (this->parallelism != "none" && this->parallelism != "none randomized") {
            turboAffinity.resize(omp_get_max_threads());
            neigh_comm.resize(omp_get_max_threads());
            for (auto &it : turboAffinity) {
                // resize to maximum community id
                it.resize(zeta.upperBound());
            }
        } else { // initialize array only for first thread
            turboAffinity.emplace_back(zeta.upperBound());
            neigh_comm.emplace_back();
        }
    }

    // try to improve modularity by moving a node to neighboring clusters
    auto tryMove = [&](node u) {
        // trying to move node u
        index tid = omp_get_thread_num();

        // collect edge weight to neighbor clusters
        std::map<index, edgeweight> affinity;

        if (turbo) {
            neigh_comm[tid].clear();
            // set all to -1 so we can see when we get to it the first time
            G->forNeighborsOf(u, [&](node v) { turboAffinity[tid][zeta[v]] = -1; });
            turboAffinity[tid][zeta[u]] = 0;
            G->forNeighborsOf(u, [&](node v, edgeweight weight) {
                if (u != v) {
                    index C = zeta[v];
                    if (turboAffinity[tid][C] == -1) {
                        // found the neighbor for the first time, initialize to 0 and add to list of
                        // neighboring communities
                        turboAffinity[tid][C] = 0;
                        neigh_comm[tid].push_back(C);
                    }
                    turboAffinity[tid][C] += weight;
                }
            });
        } else {
            G->forNeighborsOf(u, [&](node v, edgeweight weight) {
                if (u != v) {
                    index C = zeta[v];
                    affinity[C] += weight;
                }
            });
        }

        // sub-functions

        // $\vol(C \ {x})$ - volume of cluster C excluding node x
        auto volCommunityMinusNode = [&](index C, node x) {
            double volC = 0.0;
            double volN = 0.0;
            volC = volCommunity[C];
            if (zeta[x] == C) {
                volN = volNode[x];
                return volC - volN;
            } else {
                return volC;
            }
        };

        auto modGain = [&](node u, index C, index D, edgeweight affinityC, edgeweight affinityD) {
            double volN = 0.0;
            volN = volNode[u];
            double delta =
                (affinityD - affinityC) / total
                + this->gamma * ((volCommunityMinusNode(C, u) - volCommunityMinusNode(D, u)) * volN)
                      / divisor;
            return delta;
        };

        index best = none;
        index C = none;
        double deltaBest = -1;

        C = zeta[u];

        if (turbo) {
            edgeweight affinityC = turboAffinity[tid][C];

            for (index D : neigh_comm[tid]) {

                // consider only nodes in other clusters (and implicitly only nodes other than u)
                if (D != C) {
                    double delta = modGain(u, C, D, affinityC, turboAffinity[tid][D]);

                    if (delta > deltaBest) {
                        deltaBest = delta;
                        best = D;
                    }
                }
            }
        } else {
            edgeweight affinityC = affinity[C];

            for (auto it : affinity) {
                index D = it.first;
                // consider only nodes in other clusters (and implicitly only nodes other than u)
                if (D != C) {
                    double delta = modGain(u, C, D, affinityC, it.second);
                    if (delta > deltaBest) {
                        deltaBest = delta;
                        best = D;
                    }
                }
            }
        }

        if (deltaBest > 0) {                   // if modularity improvement possible
            assert(best != C && best != none); // do not "move" to original cluster

            zeta[u] = best; // move to best cluster
            // node u moved

            // mod update
            double volN = 0.0;
            volN = volNode[u];
// update the volume of the two clusters
#pragma omp atomic
            volCommunity[C] -= volN;
#pragma omp atomic
            volCommunity[best] += volN;

            moved = true; // change to clustering has been made
        }
    };

    // performs node moves
    auto movePhase = [&]() {
        count iter = 0;
        do {
            moved = false;
            // apply node movement according to parallelization strategy
            if (this->parallelism == "none") {
                G->forNodes(tryMove);
            } else if (this->parallelism == "simple") {
                G->parallelForNodes(tryMove);
            } else if (this->parallelism == "balanced") {
                G->balancedParallelForNodes(tryMove);
            } else if (this->parallelism == "none randomized") {
                G->forNodesInRandomOrder(tryMove);
            } else {
                ERROR("unknown parallelization strategy: ", this->parallelism);
                throw std::runtime_error("unknown parallelization strategy");
            }
            if (moved)
                change = true;

            if (iter == maxIter) {
                WARN("move phase aborted after ", maxIter, " iterations");
            }
            iter += 1;
            // Update memory usage.
            updateMemoryUsageU(vmPeak, vmSize, vmHwm, vmRss);
        } while (moved && (iter <= maxIter) && handler.isRunning());
        DEBUG("iterations in move phase: ", iter);
    };
    handler.assureRunning();
    // first move phase
    Aux::Timer timer;
    timer.start();

    movePhase();

    timer.stop();
    timing["move"].push_back(timer.elapsedMilliseconds());
    handler.assureRunning();
    if (recurse && change) {
        DEBUG("nodes moved, so begin coarsening and recursive call");

        timer.start();

        // coarsen graph according to communities
        std::pair<Graph, std::vector<node>> coarsened = coarsen(*G, zeta);

        timer.stop();
        timing["coarsen"].push_back(timer.elapsedMilliseconds());

        PLM onCoarsened(coarsened.first, this->refine, this->gamma, this->parallelism,
                        this->maxIter, this->turbo);
        onCoarsened.run();
        Partition zetaCoarse = onCoarsened.getPartition();
        // Update memory usage.
        updateMemoryUsageU(vmPeak, vmSize, vmHwm, vmRss);

        // get timings
        auto tim = onCoarsened.getTiming();
        for (count t : tim["move"]) {
            timing["move"].push_back(t);
        }
        for (count t : tim["coarsen"]) {
            timing["coarsen"].push_back(t);
        }
        for (count t : tim["refine"]) {
            timing["refine"].push_back(t);
        }

        DEBUG("coarse graph has ", coarsened.first.numberOfNodes(), " nodes and ",
              coarsened.first.numberOfEdges(), " edges");
        // unpack communities in coarse graph onto fine graph
        zeta = prolong(coarsened.first, zetaCoarse, *G, coarsened.second);
        // refinement phase
        if (refine) {
            DEBUG("refinement phase");
            // reinit community-dependent temporaries
            o = zeta.upperBound();
            volCommunity.clear();
            volCommunity.resize(o, 0.0);
            zeta.parallelForEntries([&](node u, index C) { // set volume for all communities
                if (C != none) {
                    edgeweight volN = volNode[u];
#pragma omp atomic
                    volCommunity[C] += volN;
                }
            });
            // second move phase
            timer.start();

            movePhase();

            timer.stop();
            timing["refine"].push_back(timer.elapsedMilliseconds());
        }
        // Update memory usage.
        updateMemoryUsageU(vmPeak, vmSize, vmHwm, vmRss);
    }
    result = std::move(zeta);
    hasRun = true;
    printf("Memory usage in PLM: VmPeak %8.4f GB, VmSize %8.4f GB, VmHwm %8.4f GB, VmRss %8.4f GB (final)\n",
        vmPeak / (1024.0 * 1024.0),
        vmSize / (1024.0 * 1024.0),
        vmHwm / (1024.0 * 1024.0),
        vmRss / (1024.0 * 1024.0)
    );
}

std::pair<Graph, std::vector<node>> PLM::coarsen(const Graph &G, const Partition &zeta) {
    ParallelPartitionCoarsening parCoarsening(G, zeta);
    parCoarsening.run();
    return {parCoarsening.getCoarseGraph(), parCoarsening.getFineToCoarseNodeMapping()};
}

Partition PLM::prolong(const Graph &, const Partition &zetaCoarse, const Graph &Gfine,
                       std::vector<node> nodeToMetaNode) {
    Partition zetaFine(Gfine.upperNodeIdBound());
    zetaFine.setUpperBound(zetaCoarse.upperBound());

    Gfine.forNodes([&](node v) {
        node mv = nodeToMetaNode[v];
        index cv = zetaCoarse[mv];
        zetaFine[v] = cv;
    });

    return zetaFine;
}

const std::map<std::string, std::vector<count>> &PLM::getTiming() const {
    assureFinished();
    return timing;
}

} /* namespace NetworKit */
