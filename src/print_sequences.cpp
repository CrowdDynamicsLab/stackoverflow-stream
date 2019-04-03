/**
 * @file print_sequences.cpp
 * Prints the extracted sequences as a big JSON file.
 */

#include <fstream>
#include <iostream>

#include "json.hpp"

#include "actions.h"
#include "meta/io/packed.h"

using namespace nlohmann;
using namespace meta;

using action_sequence_type = std::vector<uint8_t>;
using sessions_list = std::vector<action_sequence_type>;

struct user_sessions
{
    user_id user;
    sessions_list sessions;
};

template <class InputStream>
uint64_t packed_read(InputStream& is, user_sessions& sessions)
{
    auto bytes = io::packed::read(is, sessions.user);
    bytes += io::packed::read(is, sessions.sessions);
    return bytes;
}

using training_data_type = std::vector<user_sessions>;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " sequences.bin" << std::endl;
        return 1;
    }

    std::ifstream input{argv[1], std::ios::binary};
    training_data_type training;
    io::packed::read(input, training);

    json output;

    for (const auto& sessions : training)
    {
        json obj;
        obj["user"] = static_cast<uint64_t>(sessions.user);
        obj["sessions"] = sessions.sessions;
        output.push_back(obj);
    }

    std::cout << output << std::endl;

    return 0;
}
