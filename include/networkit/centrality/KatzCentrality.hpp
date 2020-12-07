/*
 * KatzCentrality.hpp
 *
 *  Created on: 09.01.2015
 *      Author: Henning
 */

#ifndef NETWORKIT_CENTRALITY_KATZ_CENTRALITY_HPP_
#define NETWORKIT_CENTRALITY_KATZ_CENTRALITY_HPP_

#include <networkit/centrality/Centrality.hpp>

namespace NetworKit {

/**
 * @ingroup centrality
 * Computes the Katz centrality of the graph.
 * NOTE: There is an inconsistency in the definition in Newman's book (Ch. 7) regarding
 * directed graphs; we follow the verbal description, which requires to sum over the incoming
 * edges (as opposed to outgoing ones).
 */
class KatzCentrality: public Centrality {
protected:
    const double alpha; // damping
    const double beta; // constant centrality amount
    const double tol; // error tolerance

public:
    /**
     * Constructs a KatzCentrality object for the given Graph @a G. @a tol defines the tolerance for convergence.
     * Each iteration of the algorithm requires O(m) time. The number of iterations depends on how long it takes to reach the convergence.
     *
     * @param[in] G The graph.
     * @param[in] alpha Damping of the matrix vector product result
     * @param[in] beta Constant value added to the centrality of each vertex
     * @param[in] tol The tolerance for convergence.
     */
    KatzCentrality(const Graph& G, double alpha = 5e-4, double beta = 0.1, double tol = 1e-8);

    /**
     * Computes katz centrality on the graph passed in constructor.
     */
    void run() override;
};

} /* namespace NetworKit */
#endif // NETWORKIT_CENTRALITY_KATZ_CENTRALITY_HPP_
