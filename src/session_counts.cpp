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
#include <regex>

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

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " network1 [network2] [network3]..." << std::endl;
        return 1;
    }

    using network_session_type = std::vector<action_type>;
    using network_sequence_type = std::vector<network_session_type>;
    using network_sequences_type = std::vector<network_sequence_type>;

    std::regex fname_regex{
        R"(sequences/([\w.]+)-sequences.bin.([0-9]{3}).bin)"};

    filesystem::remove_all("counts-by-month");
    filesystem::make_directory("counts-by-month");

    logging::set_cerr_logging();
    for (int a = 1; a < argc; ++a)
    {
        if (!filesystem::file_exists(argv[a]))
        {
            LOG(fatal) << argv[a] << " not found!" << ENDLG;
            return 1;
        }

        std::smatch fname_match;
        std::string fname{argv[a]};
        if (!std::regex_match(fname, fname_match, fname_regex))
        {
            LOG(fatal) << "Failed to parse filename " << fname << ENDLG;
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

        auto network_sessions = std::accumulate(
            sequences.begin(), sequences.end(), uint64_t{0},
            [](uint64_t num_sessions, const network_sequence_type& sequence) {
                return num_sessions + sequence.size();
            });

        auto output_name = "counts-by-month/" + fname_match[1].str() + ".csv";
        std::ofstream output;
        if (!filesystem::file_exists(output_name))
        {
            output.open(output_name);
            output << "month,num-sessions\n";
        }
        else
        {
            output.open(output_name, std::ios::app);
        }
        output << std::stoi(fname_match[2].str()) << "," << network_sessions
               << "\n";
    }

    return 0;
}
