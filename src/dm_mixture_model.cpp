/**
 * @file dm_mixture_model.cpp
 * @author Chase Geigle
 *
 * Fits a variant of a Dirichlet-Multinomial Mixture Model to a collection
 * of sequences extracted from StackExchange data. Each website is
 * characterized with a latent multinomial variable indicating the
 * distribution over K possible latent "roles" that users take when
 * generating the actions within one sequence. These two sets of
 * distributions (the latent role action distributions and the role
 * proportions) have Dirichlet priors.
 */

#include <fstream>
#include <iostream>

#include "actions.h"
#include "cpptoml.h"
#include "meta/io/filesystem.h"
#include "meta/logging/logger.h"
#include "meta/math/fastapprox.h"
#include "meta/sequence/hmm/hmm.h"
#include "meta/sequence/hmm/sequence_observations.h"
#include "meta/stats/multinomial.h"
#include "meta/util/progress.h"
#include "meta/util/random.h"
#include "meta/util/sparse_vector.h"

using namespace meta;

MAKE_NUMERIC_IDENTIFIER(network_id, uint64_t)
MAKE_NUMERIC_IDENTIFIER(topic_id, uint64_t)

class dm_mixture_model
{
  public:
    // session[i] == # of times action i was taken in that session
    using session_type = util::sparse_vector<action_type, uint64_t>;
    // sequences[i] == one session in a specific network
    using sequences_type = std::vector<session_type>;
    // training_data[i] == one network in the collection
    using training_data_type = std::vector<sequences_type>;

    struct options_type
    {
        uint8_t num_topics = 5;
        double alpha = 0.1;
        double beta = 0.1;
    };

    template <class RandomNumberEngine>
    dm_mixture_model(const training_data_type& training, options_type opts,
                     RandomNumberEngine&& rng)
        : topic_assignments_(
              std::accumulate(std::begin(training), std::end(training), 0ul,
                              [](uint64_t accum, const sequences_type& seqs) {
                                  return accum + seqs.size();
                              })),
          // initialize all topics with the same prior pseudo-counts
          topics_(opts.num_topics,
                  stats::dirichlet<action_type>(
                      opts.beta, static_cast<uint64_t>(action_type::INIT))),
          topic_proportions_(training.size(), stats::dirichlet<topic_id>(
                                                  opts.alpha, opts.num_topics))
    {
        initialize(training, std::forward<RandomNumberEngine>(rng));
    };

  private:
    template <class RandomNumberEngine>
    void initialize(const training_data_type& training,
                    RandomNumberEngine&& rng)
    {
        printing::progress progress{" > Initialization: ",
                                    topic_assignments_.size()};

        uint64_t x = 0;
        for (network_id i{0}; i < training.size(); ++i)
        {
            for (doc_id j{0}; j < training[i].size(); ++j)
            {
                auto z = sample_topic(i, training[i][j],
                                      std::forward<RandomNumberEngine>(rng));
                topic_assignments_[x] = z;
                topic_proportions_[i].increment(z);
                for (const auto& pr : training[i][j])
                {
                    topics_[z].increment(pr.first, pr.second);
                }
                progress(++x);
            }
        }
    }

    template <class RandomNumberEngine>
    topic_id sample_topic(network_id i, const session_type& session,
                          RandomNumberEngine&& rng)
    {
        //
        // compute sample using the Gumbel-max trick:
        // https://stats.stackexchange.com/questions/64081
        //
        // This is done to avoid underflow issues due to the |d|
        // multiplications of probabilities in the second term of the
        // sampling proportion equation for the Gibbs sampler.
        //
        topic_id result{0};
        auto max_value = std::numeric_limits<float>::lowest();
        std::uniform_int_distribution<> dist{0, 1};

        const auto num_topics = topics_.size();
        for (topic_id z{0}; z < num_topics; ++z)
        {
            // compute the sampling probability (up to proportionality) in
            // log-space to avoid underflow
            auto denom = static_cast<float>(topics_[z].counts());
            auto log_prob = fastapprox::fastlog(
                static_cast<float>(topic_proportions_[i].probability(z)));
            uint64_t j = 0;
            for (const auto& pr : session)
            {
                const auto& word = pr.first;
                const auto& count = pr.second;

                for (uint64_t i = 0; i < count; ++i)
                {
                    log_prob += fastapprox::fastlog(
                        static_cast<float>(topics_[z].counts(word)) + i);
                    log_prob -= fastapprox::fastlog(denom + j);
                    ++j;
                }
            }

            // apply the Gumbel-max trick to update the sample
            auto rnd = dist(rng);
            auto gumbel_noise = -fastapprox::fastlog(-fastapprox::fastlog(rnd));

            if (log_prob + gumbel_noise > max_value)
                result = z;
        }

        return result;
    }

    /// the topic assignment for each session
    std::vector<uint8_t> topic_assignments_;

    /**
     * The action distributions for each role. Doubles as storage for the
     * count information for each role.
     */
    std::vector<stats::multinomial<action_type>> topics_;

    /**
     * The topic distributions for each network. Doubles as storage for the
     * count information for each network.
     */
    std::vector<stats::multinomial<topic_id>> topic_proportions_;
};

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " config.toml" << std::endl;
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

#if 0
    std::ifstream input{argv[1], std::ios::binary};
    LOG(info) << "Reading training data..." << ENDLG;
    training_data_type training;
    io::packed::read(input, training);
#endif
    return 0;
}
