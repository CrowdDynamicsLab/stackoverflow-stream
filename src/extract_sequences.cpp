/**
 * @file extract_sequences.cpp
 * @author Chase Geigle
 *
 * Extracts lists of action sequences from a (repacked) StackExchange data
 * dump.
 */

#include <iostream>
#include <libxml/xmlreader.h>

#include "meta/hashing/probe_map.h"
#include "meta/io/filesystem.h"
#include "meta/io/xzstream.h"
#include "meta/logging/logger.h"
#include "meta/util/identifiers.h"
#include "meta/util/progress.h"

MAKE_NUMERIC_IDENTIFIER(action_id, uint8_t)
MAKE_NUMERIC_IDENTIFIER(user_id, uint64_t)

using namespace meta;

class xml_string
{
  public:
    xml_string(xmlChar* str) : str_{str}
    {
        // nothing
    }

    ~xml_string()
    {
        ::free(str_);
    }

    xml_string(xml_string&& other) : str_(other.str_)
    {
        other.str_ = nullptr;
    }

    xml_string& operator=(xml_string&& other)
    {
        str_ = other.str_;
        other.str_ = nullptr;
        return *this;
    }

    xml_string(const xml_string&) = delete;
    xml_string& operator=(const xml_string&) = delete;

    std::string to_string() const
    {
        return {(const char*)str_};
    }

    util::string_view sv() const
    {
        return {(const char*)str_};
    }

  private:
    xmlChar* str_;
};

class xml_text_reader
{
  public:
    xml_text_reader(std::istream& input, printing::progress& progress)
        : input_(input), progress_(progress), reader_{make_xml_reader()}
    {
    }

    bool read_next()
    {
        return xmlTextReaderRead(reader_);
    }

    util::string_view node_name() const
    {
        return (const char*)xmlTextReaderConstName(reader_);
    }

    util::optional<xml_string> attribute(const char* name) const
    {
        auto str = xmlTextReaderGetAttribute(reader_, (const xmlChar*)name);
        if (!str)
            return util::nullopt;
        return {str};
    }

    operator xmlTextReaderPtr()
    {
        return reader_;
    }

  private:
    xmlTextReaderPtr make_xml_reader()
    {
        return xmlReaderForIO(
            // Read callback
            [](void* context, char* buffer, int len) {
                auto self = static_cast<xml_text_reader*>(context);
                self->input_.read(buffer, len);

                auto num_read = static_cast<int>(self->input_.gcount());
                self->progress_(static_cast<uint64_t>(num_read));
                return num_read;
            },
            // Close callback
            [](void* /* context */) { return 0; },
            // Context
            this,
            // URL
            "",
            // Encoding
            nullptr,
            // Options
            XML_PARSE_NONET | XML_PARSE_NOBLANKS | XML_PARSE_RECOVER);
    }

    std::istream& input_;
    printing::progress& progress_;
    xmlTextReaderPtr reader_;
};

action_id get_action_id(util::string_view name)
{
    static util::string_view actions[]
        = {"post question", "post answer", "comment",  "edit title",
           "edit body",     "edit tags",   "mod vote", "mod action"};

    action_id aid{0};
    for (const auto& action : actions)
    {
        if (name == action)
            return aid;
        ++aid;
    }
    throw std::runtime_error{"unrecognized action: " + name.to_string()};
}

struct action
{
    action(util::string_view name, const std::string& d)
        : id{get_action_id(name)}, date{d}
    {
        // nothing
    }

    action_id id;
    std::string date;
};

bool operator<(const action& a, const action& b)
{
    return a.date < b.date;
}

template <class ActionMap>
void extract_comments(const std::string& folder, ActionMap& actions)
{
    auto filename = folder + "/Comments.xml.xz";

    printing::progress progress{" > Extracting Comments: ",
                                filesystem::file_size(filename)};

    io::xzifstream input{filename};
    xml_text_reader reader{input, progress};

    uint64_t num_actions = 0;
    while (reader.read_next())
    {
        auto node_name = reader.node_name();

        if (node_name == "comments")
            continue;

        if (node_name != "row")
            throw std::runtime_error{"unrecognized XML entity "
                                     + node_name.to_string()};

        auto uid = reader.attribute("UserId");
        auto dte = reader.attribute("CreationDate");

        if (!uid || !dte)
            continue;

        user_id user{std::stoul(uid->to_string())};
        actions[user].emplace_back("comment", dte->to_string());

        ++num_actions;
    }
    progress.end();
    LOG(progress) << "\rFound " << num_actions << " comments\n" << ENDLG;
}

template <class ActionMap>
void extract_posts(const std::string& folder, ActionMap& actions)
{
    auto filename = folder + "/Posts.xml.xz";

    printing::progress progress{" > Extracting Posts: ",
                                filesystem::file_size(filename)};

    io::xzifstream input{filename};
    xml_text_reader reader{input, progress};

    uint64_t num_actions = 0;
    while (reader.read_next())
    {
        auto node_name = reader.node_name();

        if (node_name == "posts")
            continue;

        if (node_name != "row")
            throw std::runtime_error{"unrecognized XML entity "
                                     + node_name.to_string()};

        auto post_type = reader.attribute("PostTypeId");
        auto date = reader.attribute("CreationDate");
        auto uid = reader.attribute("OwnerUserId");

        if (!post_type || !date || !uid)
            continue;

        user_id user{std::stoul(uid->to_string())};
        actions[user].emplace_back((post_type->sv() == "1") ? "post question"
                                                            : "post answer",
                                   date->to_string());
        ++num_actions;
    }
    progress.end();
    LOG(progress) << "\rFound " << num_actions << " posts\n" << ENDLG;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " folder" << std::endl;
        return 1;
    }

    logging::set_cerr_logging();

    std::string folder{argv[1]};
    for (const auto& name : {"Comments.xml.xz", "Posts.xml.xz",
                             "PostHistory.xml.xz", "PostLinks.xml.xz"})
    {
        if (!filesystem::file_exists(folder + "/" + name))
        {
            std::cerr << "File " << folder << '/' << name << " does not exist"
                      << std::endl;
            return 1;
        }
    }

    hashing::probe_map<user_id, std::vector<action>> actions;
    extract_comments(folder, actions);
    extract_posts(folder, actions);
#if 0
    extract_post_history(folder, actions);
    extract_post_links(folder, actions);
#endif

    return 0;
}
