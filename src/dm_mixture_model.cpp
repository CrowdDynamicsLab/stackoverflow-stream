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
#include <random>

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

    template <class RandomNumberEngine>
    void run(const training_data_type& training, uint64_t num_iters,
             RandomNumberEngine&& rng)
    {
        LOG(progress) << "> Iteration 0 log joint likelihood: "
                      << log_joint_likelihood() << "\n"
                      << ENDLG;

        for (uint64_t iter = 1; iter <= num_iters; ++iter)
        {
            printing::progress progress{" > Iteration " + std::to_string(iter)
                                            + ": ",
                                        topic_assignments_.size()};
            progress.print_endline(false);

            perform_iteration(progress, training,
                              std::forward<RandomNumberEngine>(rng));
            progress.clear();
            LOG(progress) << "> Iteration " << iter
                          << " log joint likelihood: " << log_joint_likelihood()
                          << "\n"
                          << ENDLG;
        }
    }

    void save(const std::string& prefix) const
    {
        if (!filesystem::exists(prefix))
            filesystem::make_directories(prefix);

        std::ofstream topics_file{prefix + "/topics.bin", std::ios::binary};
        io::packed::write(topics_file, topics_);

        std::ofstream topic_proportions_file{prefix + "/topic-proportions.bin",
                                             std::ios::binary};
        io::packed::write(topic_proportions_file, topic_proportions_);
    }

  private:
    template <class RandomNumberEngine>
    void initialize(const training_data_type& training,
                    RandomNumberEngine&& rng)
    {
        printing::progress progress{" > Initialization: ",
                                    topic_assignments_.size()};

        // proceed like a normal sampling pass, but without removing any counts
        uint64_t x = 0;
        for (network_id i{0}; i < training.size(); ++i)
        {
            for (doc_id j{0}; j < training[i].size(); ++j)
            {
                auto z = sample_topic(i, training[i][j],
                                      std::forward<RandomNumberEngine>(rng));
                topic_assignments_[x] = z;
                topic_proportions_[i].increment(z, 1.0);
                for (const auto& pr : training[i][j])
                {
                    topics_[z].increment(pr.first, pr.second);
                }
                progress(++x);
            }
        }
    }

    template <class RandomNumberEngine>
    void perform_iteration(printing::progress& progress,
                           const training_data_type& training,
                           RandomNumberEngine&& rng)
    {
        uint64_t x = 0;
        for (network_id i{0}; i < training.size(); ++i)
        {
            for (doc_id j{0}; j < training[i].size(); ++j)
            {
                // remove counts
                auto old_z = topic_assignments_[x];
                topic_proportions_[i].decrement(old_z, 1.0);
                for (const auto& pr : training[i][j])
                {
                    topics_[old_z].decrement(pr.first, pr.second);
                }

                // sample new topic
                auto z = sample_topic(i, training[i][j],
                                      std::forward<RandomNumberEngine>(rng));
                topic_assignments_[x] = z;

                // update counts
                topic_proportions_[i].increment(z, 1.0);
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
            {
                result = z;
                max_value = log_prob + gumbel_noise;
            }
        }

        return result;
    }

    double log_joint_likelihood() const
    {
        // log p(w, z) = log p(w | z)p(z) = log p(w|z) + log p(z)
        //
        // both p(w|z) and p(z) are Dirichlet-multinomial distributions
        auto log_likelihood = 0.0;

        // log p(w|z)
        for (const auto& topic : topics_)
        {
            log_likelihood += dm_log_likelihood(topic);
        }

        // log p(z)
        for (const auto& theta : topic_proportions_)
        {
            log_likelihood += dm_log_likelihood(theta);
        }

        return log_likelihood;
    }

    template <class T>
    double dm_log_likelihood(const stats::multinomial<T>& dist) const
    {
        auto log_likelihood = std::lgamma(dist.prior().pseudo_counts());
        log_likelihood -= std::lgamma(dist.counts());

        dist.each_seen_event([&](const T& val) {
            log_likelihood += std::lgamma(dist.counts(val));
            log_likelihood -= std::lgamma(dist.prior().pseudo_counts(val));
        });

        return log_likelihood;
    }

    /// the topic assignment for each session
    std::vector<topic_id> topic_assignments_;

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
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " config.toml network1 [network2] [network3]..."
                  << std::endl;
        return 1;
    }

    using training_data_type = dm_mixture_model::training_data_type;
    using dm_sequences_type = dm_mixture_model::sequences_type;
    using dm_session_type = dm_mixture_model::session_type;

    using network_session_type = std::vector<action_type>;
    using network_sequence_type = std::vector<network_session_type>;
    using network_sequences_type = std::vector<network_sequence_type>;

    logging::set_cerr_logging();

    // set up options for the model
    auto config = cpptoml::parse_file(argv[1]);
    auto mix_config = config->get_table("dm-mixture-model");
    if (!mix_config)
    {
        LOG(fatal) << "Missing dm-mixture-model table in " << argv[1] << ENDLG;
        return 1;
    }

    dm_mixture_model::options_type options;

    options.num_topics
        = mix_config->get_as<uint8_t>("topics").value_or(options.num_topics);
    options.alpha = mix_config->get_as<double>("alpha").value_or(options.alpha);
    options.beta = mix_config->get_as<double>("beta").value_or(options.beta);

    uint64_t total_sessions = 0;
    training_data_type training;
    for (int a = 2; a < argc; ++a)
    {
        if (!filesystem::file_exists(argv[a]))
        {
            LOG(fatal) << argv[a] << " not found!" << ENDLG;
            return 1;
        }

        std::ifstream input{argv[a], std::ios::binary};

        // need to convert ordered sequences -> histograms
        // could write a separate extractor program for this, but why bother
        network_sequences_type sequences;
        auto bytes = io::packed::read(input, sequences);
        if (bytes == 0 || !input)
        {
            LOG(fatal) << "Failed to read file " << argv[a] << ENDLG;
            return 1;
        }

        dm_sequences_type network;
        for (const auto& user : sequences)
        {
            for (const auto& session : user)
            {
                dm_session_type dm_session;
                for (const auto& action : session)
                    dm_session[action] += 1;
                network.emplace_back(dm_session);
            }
        }
        network.shrink_to_fit();

        total_sessions += network.size();
        training.emplace_back(std::move(network));
    }

    LOG(info) << "Read " << total_sessions << " sessions from "
              << training.size() << " networks" << ENDLG;

    std::mt19937 rng;
    dm_mixture_model model{training, options, rng};
    model.run(training, mix_config->get_as<uint64_t>("max-iter").value_or(1000),
              rng);

    LOG(info) << "Saving estimate based on final chain sample..." << ENDLG;
    model.save(
        mix_config->get_as<std::string>("prefix").value_or("dmmm-model"));

    return 0;
}
