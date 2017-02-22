/**
 * @file print_sequences.cpp
 * Prints the extracted sequences as a big JSON file.
 */

#include <fstream>
#include <iostream>

#include "json.hpp"

#include "meta/io/packed.h"


using namespace nlohmann;
using namespace meta;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " sequences.bin" << std::endl;
        return 1;
    }

    using action_sequence_type = std::vector<uint64_t>;
    using sequence_type = std::vector<action_sequence_type>;
    using training_data_type = std::vector<sequence_type>;

    std::ifstream input{argv[1], std::ios::binary};
    training_data_type training;
    io::packed::read(input, training);

    std::cout << json{training} << std::endl;

    return 0;
}
