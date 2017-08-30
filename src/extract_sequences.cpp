/**
 * @file extract_sequences.cpp
 * @author Chase Geigle
 *
 * Extracts lists of action sequences from a (repacked) StackExchange data
 * dump.
 */

#include <fstream>
#include <iostream>
#include <libxml/xmlreader.h>

#include "date.h"

#include "meta/hashing/probe_map.h"
#include "meta/io/filesystem.h"
#include "meta/io/packed.h"
#include "meta/io/xzstream.h"
#include "meta/logging/logger.h"
#include "meta/stats/running_stats.h"
#include "meta/util/array_view.h"
#include "meta/util/identifiers.h"
#include "meta/util/progress.h"

#include "actions.h"

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
    xml_text_reader(io::xzifstream& input, printing::progress& progress)
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
                self->progress_(self->input_.bytes_read());
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

    io::xzifstream& input_;
    printing::progress& progress_;
    xmlTextReaderPtr reader_;
};

using sys_milliseconds = date::sys_time<std::chrono::milliseconds>;

sys_milliseconds parse_date(const std::string& date)
{
    std::stringstream ss{date};
    sys_milliseconds tp;
    ss >> date::parse("%Y-%m-%dT%H:%M:%S", tp);
    if (ss.fail())
        throw std::runtime_error{"failed to parse date: " + date};
    return tp;
}

struct action
{
    action(action_type atype, const std::string& d)
        : type{atype}, date{parse_date(d)}
    {
        // nothing
    }

    action_type type;
    sys_milliseconds date;
};

bool operator<(const action& a, const action& b)
{
    return a.date < b.date;
}

template <class ActionMap, class PostMap>
void extract_comments(const std::string& folder, ActionMap& actions,
                      PostMap& post_map)
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

        auto pid = reader.attribute("PostId");
        auto uid = reader.attribute("UserId");
        auto dte = reader.attribute("CreationDate");

        if (!pid || !uid || !dte)
            continue;

        post_id post{std::stoul(pid->to_string())};
        user_id user{std::stoul(uid->to_string())};

        // skip comments where we either (a) can't find the parent or (b)
        // can't find the root question
        //
        // this could happen if the parent post(s) have no user id
        // specified and we thus dropped it during post extraction
        auto type = comment_type(post, user, post_map);
        if (!type)
            continue;

        actions[user].emplace_back(*type, dte->to_string());

        ++num_actions;
    }
    progress.end();
    LOG(progress) << "\rFound " << num_actions << " comments\n" << ENDLG;
}

struct post_info
{
    post_info(user_id uid) : op{uid}
    {
        // nothing
    }

    post_info(user_id uid, post_id pid) : op{uid}, parent{pid}
    {
        // nothing
    }

    user_id op;
    util::optional<post_id> parent;
};

template <class ActionMap>
hashing::probe_map<post_id, post_info> extract_posts(const std::string& folder,
                                                     ActionMap& actions)
{
    hashing::probe_map<post_id, post_info> post_map;

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
        auto id = reader.attribute("Id");

        if (!post_type || !date || !uid)
            continue;

        user_id user{std::stoul(uid->to_string())};
        post_id post{std::stoul(id->to_string())};

        action_type type;
        auto parent_id = reader.attribute("ParentId");
        if (parent_id)
        {
            post_id parent{std::stoul(parent_id->to_string())};
            post_map.emplace(post, post_info{user, parent});

            // this is an answer. Was the question our own?
            auto ptype = content(parent, user, post_map);

            // skip answers to questions we weren't able to attach to a
            // user id
            if (!ptype)
                continue;

            type = ptype == content_type::MY_QUESTION ? action_type::ANSWER_MQ
                                                      : action_type::ANSWER_OQ;
        }
        else
        {
            post_map.emplace(post, user);
            type = action_type::QUESTION;
        }

        actions[user].emplace_back(type, date->to_string());
        ++num_actions;
    }
    progress.end();
    LOG(progress) << "\rFound " << num_actions << " posts\n" << ENDLG;
    return post_map;
}

template <class ActionMap, class PostMap>
void extract_post_history(const std::string& folder, ActionMap& actions,
                          PostMap& post_map)
{
    auto filename = folder + "/PostHistory.xml.xz";

    printing::progress progress{" > Extracting PostHistory: ",
                                filesystem::file_size(filename)};

    io::xzifstream input{filename};
    xml_text_reader reader{input, progress};

    uint64_t num_actions = 0;
    while (reader.read_next())
    {
        auto node_name = reader.node_name();

        if (node_name == "posthistory")
            continue;

        if (node_name != "row")
            throw std::runtime_error{"unrecognized XML entity "
                                     + node_name.to_string()};

        auto uid = reader.attribute("UserId");
        auto type = reader.attribute("PostHistoryTypeId");
        auto date = reader.attribute("CreationDate");
        auto pid = reader.attribute("PostId");

        if (!uid || !type || !date)
            continue;

        user_id user{std::stoul(uid->to_string())};
        history_type_id type_num{std::stoul(type->to_string())};
        post_id post{std::stoul(pid->to_string())};

        // skip history items where we can't identify the post
        auto it = post_map.find(post);
        if (it == post_map.end())
            continue;

        auto ctype = content(post, user, post_map);
        if (!ctype)
            continue;

        auto atype = action_cast(type_num, *ctype);
        if (atype == action_type::INIT)
            continue;

        actions[user].emplace_back(atype, date->to_string());
        ++num_actions;
    }
    progress.end();
    LOG(progress) << "\rFound " << num_actions << " history actions\n" << ENDLG;
}

struct sequence_stats
{
    stats::running_stats sequence_length;
    stats::running_stats num_sequences;
    stats::running_stats gap_length;
};

void write_sequences(std::ostream& out, const std::vector<action>& actions,
                     sequence_stats& stats)
{
    using namespace std::chrono;
    const action* begin = &actions[0];
    const action* last = begin;

    std::vector<util::array_view<const action>> sequences;
    for (const auto& act : actions)
    {
        auto gap = act.date - last->date;
        if (&act != last)
        {
            stats.gap_length.add(duration_cast<seconds>(gap).count());
        }

        if (act.date - last->date > hours{6})
        {
            last = &act;
            sequences.emplace_back(begin, last);
            begin = last;
        }
        else
        {
            last = &act;
        }
    }
    sequences.emplace_back(begin, last + 1);

    stats.num_sequences.add(sequences.size());
    io::packed::write(out, sequences.size());
    for (const auto& av : sequences)
    {
        stats.sequence_length.add(av.size());
        io::packed::write(out, av.size());
        for (const auto& act : av)
        {
            io::packed::write(out, act.type);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " folder [output-file]"
                  << std::endl;
        std::cerr << "\toutput-file: defaults to \"sequences.bin\""
                  << std::endl;
        return 1;
    }

    logging::set_cerr_logging();

    std::string folder{argv[1]};
    for (const auto& name :
         {"Comments.xml.xz", "Posts.xml.xz", "PostHistory.xml.xz"})
    {
        if (!filesystem::file_exists(folder + "/" + name))
        {
            std::cerr << "File " << folder << '/' << name << " does not exist"
                      << std::endl;
            return 1;
        }
    }

    hashing::probe_map<user_id, std::vector<action>> user_map;
    {
        auto post_map = extract_posts(folder, user_map);
        extract_comments(folder, user_map, post_map);
        extract_post_history(folder, user_map, post_map);
    }

    auto actions = std::move(user_map).extract();
    using value_type = decltype(actions)::value_type;
    std::sort(actions.begin(), actions.end(),
              [](const value_type& a, const value_type& b) {
                  return a.first < b.first;
              });

    sequence_stats stats;
    std::ofstream sequences{argc > 2 ? argv[2] : "sequences.bin",
                            std::ios::binary};
    io::packed::write(sequences, actions.size());
    for (auto& pr : actions)
    {
        std::sort(pr.second.begin(), pr.second.end());

        write_sequences(sequences, pr.second, stats);
    }

    LOG(info) << "Sequence length: " << stats.sequence_length.mean() << " +/- "
              << stats.sequence_length.stddev() << ENDLG;
    LOG(info) << "Gap length: " << stats.gap_length.mean() << " +/- "
              << stats.gap_length.stddev() << ENDLG;
    LOG(info) << "Num sequences/user: " << stats.num_sequences.mean() << " +/- "
              << stats.num_sequences.stddev() << ENDLG;

    return 0;
}
