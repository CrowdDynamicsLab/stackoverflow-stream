/**
 * @file cluster_sequences.cpp
 * @author Chase Geigle
 *
 * Fits a hidden Markov model with sequence observations to the extracted
 * sequences for users from a StackExchange data dump.
 */

#include <fstream>
#include <iostream>

#include "meta/io/filesystem.h"
#include "meta/logging/logger.h"
#include "meta/sequence/hmm/hmm.h"
#include "meta/sequence/hmm/sequence_observations.h"

using namespace meta;

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " sequences.bin num_states"
                  << std::endl;
        return 1;
    }

    if (!filesystem::file_exists(argv[1]))
    {
        std::cerr << argv[1] << " not found" << std::endl;
        return 1;
    }

    using namespace sequence;
    using namespace hmm;
    using action_sequence_type = std::vector<state_id>;
    using sequence_type = std::vector<action_sequence_type>;
    using training_data_type = std::vector<sequence_type>;

    logging::set_cerr_logging();

    uint64_t num_states = std::stoull(argv[2]);
    std::ifstream input{argv[1], std::ios::binary};

    LOG(info) << "Reading training data..." << ENDLG;
    training_data_type training;
    io::packed::read(input, training);

    std::mt19937 rng{47};

    const uint64_t num_actions = 8;
    const double smoothing_constant = 1e-6;

    sequence_observations obs_dist{
        num_states, num_actions, rng,
        stats::dirichlet<state_id>{smoothing_constant, num_actions}};

    parallel::thread_pool pool;
    hidden_markov_model<sequence_observations> hmm{
        num_states, rng, std::move(obs_dist),
        stats::dirichlet<state_id>{smoothing_constant, num_states}};

    decltype(hmm)::training_options options;
    options.delta = 1e-4;
    options.max_iters = 50;

    LOG(info) << "Beginning training..." << ENDLG;
    hmm.fit(training, pool, options);

    LOG(info) << "Saving model..." << ENDLG;
    std::ofstream output{"hmm-model.bin", std::ios::binary};
    hmm.save(output);

    return 0;
}
