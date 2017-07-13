/**
 * @file print_hmm.cpp
 * Prints the distributions for a HMM model file.
 */

#include <fstream>
#include <iostream>

#include "actions.h"

#include "meta/io/filesystem.h"
#include "meta/io/packed.h"
#include "meta/logging/logger.h"
#include "meta/stats/multinomial.h"

using namespace meta;

MAKE_NUMERIC_IDENTIFIER(network_id, uint64_t)
MAKE_NUMERIC_IDENTIFIER(topic_id, uint64_t)

int main(int argc, char** argv)
{
    logging::set_cerr_logging();

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " dmmm-prefix network1 [network2] [network3]..."
                  << std::endl;
        return 1;
    }

    std::vector<std::string> args(argv, argv + argc);
    for (const auto& filename :
         {args[1], args[1] + "/topics.bin", args[1] + "/topic-proportions.bin"})
    {
        if (!filesystem::exists(filename))
        {
            LOG(fatal) << filename << " does not exist" << ENDLG;
            return 1;
        }
    }

    std::ifstream topics_file{args[1] + "/topics.bin", std::ios::binary};
    auto topics
        = io::packed::read<std::vector<stats::multinomial<action_type>>>(
            topics_file);

    // write topics to CSV
    for (std::size_t i = 0; i < topics.size(); ++i)
    {
        std::ofstream topics_csv{"topic" + std::to_string(i + 1) + ".csv"};
        topics_csv << "action,probability\n";
        topics[i].each_seen_event([&](action_type a) {
            topics_csv << action_name(a) << "," << topics[i].probability(a)
                       << "\n";
        });
    }

    std::ifstream theta_file{args[1] + "/topic-proportions.bin",
                             std::ios::binary};
    auto theta = io::packed::read<std::vector<stats::multinomial<topic_id>>>(
        theta_file);

    // write topic proportions to CSV
    for (std::size_t i = 0; i < theta.size(); ++i)
    {
        auto filename = args[2 + i].substr(
                args[2 + i].find_last_of("/") + 1);
        filename = filename.substr(0, filename.find_last_of("-"));
        std::ofstream topic_prop_csv{filename + "-proportions.csv"};
        topic_prop_csv << "topic,probability\n";
        theta[i].each_seen_event([&](topic_id k) {
            topic_prop_csv << k + 1 << "," << theta[i].probability(k) << "\n";
        });
    }

    return 0;
}
