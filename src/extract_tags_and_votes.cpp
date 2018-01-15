/**
 * @file extract_tags_and_votes.cpp
 * @author Chase Geigle
 *
 * Extracts a CSV containing upvotes, downvotes, and favorites by post
 * (with timestamps), as well as a CSV containing posts (with timestamps),
 * authors, and tags.
 */

#include <fstream>
#include <iostream>

#include "meta/io/filesystem.h"
#include "meta/logging/logger.h"
#include "parsing.h"

using namespace meta;

inline util::string_view sv_or_blank(const util::optional<xml_string>& opt)
{
    return opt ? opt->sv() : "";
}

void extract_votes(const std::string& folder)
{
    auto filename = folder + "/Votes.xml.xz";
    printing::progress progress{" > Extracting Votes: ",
                                filesystem::file_size(filename)};
    io::xzifstream input{filename};
    xml_text_reader reader{input, progress};

    std::ofstream output{"votes.csv"};
    output << "PostId,VoteTypeId,CreationDate\n";
    while (reader.read_next())
    {
        auto node_name = reader.node_name();

        if (node_name == "votes")
            continue;

        if (node_name != "row")
            throw std::runtime_error{"unrecognized XML entity "
                                     + node_name.to_string()};

        auto post_id = reader.attribute("PostId");
        auto vote_type = reader.attribute("VoteTypeId");
        auto creation_date = reader.attribute("CreationDate");

        if (vote_type->sv() == "2" || vote_type->sv() == "3"
            || vote_type->sv() == "5")
        {
            output << post_id->sv() << "," << vote_type->sv() << ","
                   << creation_date->sv() << "\n";
        }
    }
}

void extract_posts(const std::string& folder)
{
    auto filename = folder + "/Posts.xml.xz";
    printing::progress progress{" > Extracting Posts: ",
                                filesystem::file_size(filename)};
    io::xzifstream input{filename};
    xml_text_reader reader{input, progress};

    std::ofstream output{"posts.csv"};
    output << "Id,PostTypeId,ParentId,CreationDate,OwnerUserId,Tags\n";
    while (reader.read_next())
    {
        auto node_name = reader.node_name();

        if (node_name == "posts")
            continue;

        if (node_name != "row")
            throw std::runtime_error{"unrecognized XML entity "
                                     + node_name.to_string()};

        auto id = reader.attribute("Id");
        auto post_type_id = reader.attribute("PostTypeId");
        auto parent_id = reader.attribute("ParentId");
        auto creation_date = reader.attribute("CreationDate");
        auto owner_user_id = reader.attribute("OwnerUserId");
        auto tags = reader.attribute("Tags");

        output << sv_or_blank(id) << "," << sv_or_blank(post_type_id) << ","
               << sv_or_blank(parent_id) << "," << sv_or_blank(creation_date)
               << "," << sv_or_blank(owner_user_id) << "," << sv_or_blank(tags)
               << "\n";
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " folder" << std::endl;
        return 1;
    }

    logging::set_cerr_logging();

    std::string folder{argv[1]};

    extract_votes(folder);
    extract_posts(folder);

    return 0;
}
