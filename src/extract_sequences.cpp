/**
 * @file extract_sequences.cpp
 * @author Chase Geigle
 *
 * Extracts lists of action sequences from a (repacked) StackExchange data
 * dump.
 */

#include <fstream>
#include <iostream>

#include "date.h"
#include "parsing.h"

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
time_span extract_comments(const std::string& folder, ActionMap& actions,
                           PostMap& post_map)
{
    auto filename = folder + "/Comments.xml.xz";

    printing::progress progress{" > Extracting Comments: ",
                                filesystem::file_size(filename)};

    io::xzifstream input{filename};
    xml_text_reader reader{input, progress};

    util::optional<time_span> span;
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

        if (!pid || !dte)
            continue;

        auto timestamp = parse_date(dte->to_string());
        if (!span)
            span = time_span{timestamp, timestamp};
        else
            span->update(timestamp);

        if (!uid)
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
    return *span;
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
std::tuple<hashing::probe_map<post_id, post_info>, time_span>
extract_posts(const std::string& folder, ActionMap& actions)
{
    hashing::probe_map<post_id, post_info> post_map;

    auto filename = folder + "/Posts.xml.xz";

    printing::progress progress{" > Extracting Posts: ",
                                filesystem::file_size(filename)};

    io::xzifstream input{filename};
    xml_text_reader reader{input, progress};

    util::optional<time_span> span;
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

        if (!post_type || !date)
            continue;

        auto timestamp = parse_date(date->to_string());
        if (!span)
            span = time_span{timestamp, timestamp};
        else
            span->update(timestamp);

        if (!uid)
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
    return std::tie(post_map, *span);
}

template <class ActionMap, class PostMap>
time_span extract_post_history(const std::string& folder, ActionMap& actions,
                          PostMap& post_map)
{
    auto filename = folder + "/PostHistory.xml.xz";

    printing::progress progress{" > Extracting PostHistory: ",
                                filesystem::file_size(filename)};

    io::xzifstream input{filename};
    xml_text_reader reader{input, progress};

    util::optional<time_span> span;
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

        if (!type || !date)
            continue;

        auto timestamp = parse_date(date->to_string());
        if (!span)
            span = time_span{timestamp, timestamp};
        else
            span->update(timestamp);

        if (!uid)
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
    return *span;
}

struct sequence_stats
{
    stats::running_stats sequence_length;
    stats::running_stats num_sequences;
    stats::running_stats gap_length;
};

using session_actions = util::array_view<const action>;
using session_list = std::vector<session_actions>;
using slice = std::vector<session_list>;

void partition_sequences(std::vector<slice>& slices,
                         const std::vector<action>& actions,
                         sequence_stats& stats, sys_milliseconds birth,
                         date::months step_size)
{
    using namespace std::chrono;
    const action* begin = &actions[0];
    const action* last = begin;

    std::vector<util::array_view<const action>> sequences;
    for (const auto& act : actions)
    {
        auto gap = act.date - last->date;
        if (gap > hours{6})
        {
            last = &act;
            sequences.emplace_back(begin, last);
            begin = last;
        }
        else
        {
            if (&act != last)
            {
                stats.gap_length.add(duration_cast<minutes>(gap).count());
            }
            last = &act;
        }
    }
    sequences.emplace_back(begin, last + 1);

    stats.num_sequences.add(sequences.size());

    // partition sequences based on step size
    std::size_t slice_num = 0;
    auto it = sequences.begin();
    auto end = sequences.end();

    while (it != end)
    {
        auto slice_end = std::find_if(
            it, end, [=](const util::array_view<const action>& sequence) {
                return (sequence.begin()->date - birth) / step_size
                       > static_cast<long>(slice_num);
            });

        auto& curr_slice = slices.at(slice_num);
        curr_slice.emplace_back();

        for (; it != slice_end; ++it)
        {
            stats.sequence_length.add(it->size());
            curr_slice.back().emplace_back(*it);
        }
        it = slice_end;
        ++slice_num;
    }
}

void write_slice(std::ostream& out, const slice& slice)
{
    io::packed::write(out, slice.size());
    for (const auto& sessions : slice)
    {
        io::packed::write(out, sessions.size());
        for (const auto& session : sessions)
        {
            io::packed::write(out, session.size());
            for (const auto& act : session)
            {
                io::packed::write(out, act.type);
            }
        }
    }
}

int main(int argc, char** argv)
{
    using namespace std::chrono;

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " [--time-slice=N] folder [output-file]" << std::endl;

        std::cerr << "\t--time-slice=N\n"
                  << "\t\tCreate a separate sequence file for every N months "
                     "after network birth"
                  << std::endl;

        std::cerr << "\toutput-file: defaults to \"sequences.bin\""
                  << std::endl;
        return 1;
    }

    logging::set_cerr_logging();

    std::vector<std::string> args{argv, argv + argc};

    auto folder_name_iter = std::find_if(
        args.begin() + 1, args.end(),
        [](const std::string& arg) { return !arg.empty() && arg[0] != '-'; });
    if (folder_name_iter == args.end())
    {
        LOG(fatal) << "Could not determine folder argument" << ENDLG;
        return 1;
    }

    auto time_slice_iter
        = std::find_if(args.begin() + 1, args.end(), [](util::string_view arg) {
              return arg.size() > 13 && arg.substr(0, 13) == "--time-slice=";
          });

    date::months time_slice{std::numeric_limits<date::months::rep>::max()};
    if (time_slice_iter == args.end())
    {
        LOG(info) << "Creating one output file" << ENDLG;
    }
    else
    {
        time_slice = date::months{std::stoi(time_slice_iter->substr(13))};
        LOG(info) << "Creating a separate output file for every "
                  << time_slice.count() << " months since birth" << ENDLG;
    }

    const auto& folder = *folder_name_iter;
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
    util::optional<time_span> span;
    {
        auto post_map_and_span = extract_posts(folder, user_map);
        auto& post_map = std::get<0>(post_map_and_span);
        span = std::get<1>(post_map_and_span);

        auto comment_span = extract_comments(folder, user_map, post_map);
        span->update(comment_span);

        auto history_span = extract_post_history(folder, user_map, post_map);
        span->update(history_span);
    }

    auto actions = std::move(user_map).extract();
    using value_type = decltype(actions)::value_type;
    std::sort(actions.begin(), actions.end(),
              [](const value_type& a, const value_type& b) {
                  return a.first < b.first;
              });

    LOG(info) << "Sorting sequences..." << ENDLG;
    for (auto& pr : actions)
        std::sort(pr.second.begin(), pr.second.end());
    LOG(info) << "Time span: ["
              << date::format("%Y-%m-%dT%H:%M:%S", span->earliest) << ", "
              << date::format("%Y-%m-%dT%H:%M:%S", span->latest) << "]" << ENDLG;

    auto diff = span->latest - span->earliest;
    auto num_files = static_cast<std::size_t>(diff / time_slice + 1);

    std::vector<slice> slices(num_files);
    sequence_stats stats;
    for (const auto& pr : actions)
    {
        partition_sequences(slices, pr.second, stats, span->earliest, time_slice);
    }

    for (std::size_t i = 0; i < num_files; ++i)
    {
        std::stringstream filename;
        if (argc < 2)
            filename << "sequences";
        else
            filename << args.back();
        filename << "." << std::setw(3) << std::setfill('0') << i << ".bin";
        std::ofstream output{filename.str(), std::ios::binary};

        write_slice(output, slices.at(i));
    }

    LOG(info) << "Sequence length: " << stats.sequence_length.mean() << " +/- "
              << stats.sequence_length.stddev() << ENDLG;
    LOG(info) << "Gap length: " << stats.gap_length.mean() << " +/- "
              << stats.gap_length.stddev() << ENDLG;
    LOG(info) << "Num sequences/user: " << stats.num_sequences.mean() << " +/- "
              << stats.num_sequences.stddev() << ENDLG;

    return 0;
}
