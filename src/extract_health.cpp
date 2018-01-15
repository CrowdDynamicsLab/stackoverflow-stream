/**
 * @file extract_health.cpp
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
#include "meta/parallel/algorithm.h"
#include "meta/stats/running_stats.h"
#include "meta/util/array_view.h"
#include "meta/util/identifiers.h"
#include "meta/util/progress.h"

#include "actions.h"

using namespace meta;

time_span extract_comments(const std::string& folder)
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
        auto dte = reader.attribute("CreationDate");

        if (!pid || !dte)
            continue;

        auto timestamp = parse_date(dte->to_string());
        if (!span)
            span = time_span{timestamp, timestamp};
        else
            span->update(timestamp);

        ++num_actions;
    }
    progress.end();
    LOG(progress) << "\rFound " << num_actions << " comments\n" << ENDLG;
    return *span;
}

struct post_info
{
    post_info(const std::string& date)
        : timestamp{parse_date(date)}
    {
        // nothing
    }

    post_info(const std::string& date, post_id pid)
        : timestamp{parse_date(date)}, parent{pid}
    {
        // nothing
    }

    bool answered() const
    {
        return static_cast<bool>(first_answer);
    }

    sys_milliseconds timestamp;
    util::optional<post_id> accepted_answer;
    util::optional<post_id> first_answer;
    util::optional<post_id> parent;
};

std::tuple<hashing::probe_map<post_id, post_info>, time_span>
extract_posts(const std::string& folder, std::vector<post_id>& sorted_posts)
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
        auto id = reader.attribute("Id");

        if (!post_type || !date)
            continue;

        auto timestamp = parse_date(date->to_string());
        if (!span)
            span = time_span{timestamp, timestamp};
        else
            span->update(timestamp);

        post_id post{std::stoul(id->to_string())};

        auto parent_id = reader.attribute("ParentId");
        if (parent_id)
        {
            post_id parent{std::stoul(parent_id->to_string())};
            post_info pinfo{date->to_string(), parent};
            post_map.emplace(post, pinfo);

            // this is an answer, so update the first answer timestamp for
            // its associated question (if needed)
            auto parent_info_it = post_map.find(parent);
            if (parent_info_it != post_map.end())
            {
                auto& parent_info = parent_info_it->value();
                if (!parent_info.first_answer
                    || post_map.at(*parent_info.first_answer).timestamp
                           > pinfo.timestamp)
                {
                    parent_info.first_answer = post;
                }
            }
        }
        else
        {
            post_info pinfo{date->to_string()};
            if (auto aans_id = reader.attribute("AcceptedAnswerId"))
                pinfo.accepted_answer
                    = post_id{std::stoul(aans_id->to_string())};
            post_map.emplace(post, pinfo);
        }

        sorted_posts.push_back(post);
        ++num_actions;
    }
    progress.end();
    LOG(progress) << "\rFound " << num_actions << " posts\n" << ENDLG;
    return std::tie(post_map, *span);
}

template <class PostMap>
time_span extract_post_history(const std::string& folder, PostMap& post_map)
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

        history_type_id type_num{
            static_cast<uint32_t>(std::stoul(type->to_string()))};
        post_id post{static_cast<uint32_t>(std::stoul(pid->to_string()))};

        // skip history items where we can't identify the post
        auto it = post_map.find(post);
        if (it == post_map.end())
            continue;

        auto atype = action_cast(type_num, content_type::MY_QUESTION);
        if (atype == action_type::INIT)
            continue;

        ++num_actions;
    }
    progress.end();
    LOG(progress) << "\rFound " << num_actions << " history actions\n" << ENDLG;
    return *span;
}

struct health_info
{
    uint64_t num_questions = 0;
    uint64_t num_answers = 0;
    uint64_t num_with_acc_ans = 0;
    uint64_t num_unanswered = 0;
    stats::running_stats response_time;
};

template <class PostMap>
void compute_health(std::vector<health_info>& slices, const PostMap& post_map,
                    const std::vector<post_id>& sorted_posts,
                    sys_milliseconds birth, date::months step_size)
{
    using namespace std::chrono;

    // partition posts based on step size
    std::size_t slice_num = 0;
    auto it = sorted_posts.begin();
    auto end = sorted_posts.end();

    while (it != end)
    {
        auto slice_end = std::find_if(it, end, [&](post_id p) {
            return (post_map.at(p).timestamp - birth) / step_size
                   > static_cast<long>(slice_num);
        });

        auto& curr_slice = slices.at(slice_num);

        for (; it != slice_end; ++it)
        {
            const auto& post = post_map.at(*it);
            if (!post.parent) // question
            {
                ++curr_slice.num_questions;
                if (post.accepted_answer)
                    ++curr_slice.num_with_acc_ans;

                if (!post.first_answer)
                {
                    ++curr_slice.num_unanswered;
                }
                else
                {
                    const auto& answer = post_map.at(*post.first_answer);

                    auto gap = answer.timestamp - post.timestamp;
                    curr_slice.response_time.add(
                        duration_cast<milliseconds>(gap).count() / 1000.0 / 60.0 / 60.0 / 24.0);
                }
            }
            else // answer
            {
                ++curr_slice.num_answers;
            }
        }

        it = slice_end;
        ++slice_num;
    }
}

void write_slice(std::ofstream& healthout, const health_info& slice)
{
    healthout << slice.num_questions << "," << slice.num_answers << ","
              << slice.num_with_acc_ans << "," << slice.num_unanswered << ","
              << slice.response_time.mean() << ","
              << slice.response_time.stddev() << "\n";
}

int main(int argc, char** argv)
{
    using namespace std::chrono;

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " [--time-slice=N] folder [output-file]" << std::endl;

        std::cerr << "\t--time-slice=N\n"
                  << "\t\tCreate a separate health_info for every N months "
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
        LOG(info) << "Creating one health_info" << ENDLG;
    }
    else
    {
        time_slice = date::months{std::stoi(time_slice_iter->substr(13))};
        LOG(info) << "Creating a separate health_info for every "
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

    std::vector<post_id> sorted_posts;
    auto post_map_and_span = extract_posts(folder, sorted_posts);
    auto& post_map = std::get<0>(post_map_and_span);
    time_span span = std::get<1>(post_map_and_span);

    LOG(info) << "Sorting post ids..." << ENDLG;
    parallel::thread_pool pool;
    parallel::sort(sorted_posts.begin(), sorted_posts.end(), pool,
                   [&](post_id a, post_id b) {
                       return post_map.at(a).timestamp
                              < post_map.at(b).timestamp;
                   });

    auto comment_span = extract_comments(folder);
    span.update(comment_span);

    auto history_span = extract_post_history(folder, post_map);
    span.update(history_span);

    LOG(info) << "Time span: ["
              << date::format("%Y-%m-%dT%H:%M:%S", span.earliest) << ", "
              << date::format("%Y-%m-%dT%H:%M:%S", span.latest) << "]" << ENDLG;

    auto diff = span.latest - span.earliest;
    auto num_files = static_cast<std::size_t>(diff / time_slice + 1);

    std::vector<health_info> slices(num_files);
    LOG(info) << "Computing health..." << ENDLG;
    compute_health(slices, post_map, sorted_posts, span.earliest, time_slice);

    std::ofstream healthout{(argc < 2) ? "sequences" : args.back()};
    healthout << "month,num_questions,num_answers,num_with_acc_ans,num_"
                 "unanswered,avg_response_time,stdev_response_time\n";
    for (std::size_t i = 0; i < num_files; ++i)
    {
        healthout << i << ",";
        write_slice(healthout, slices.at(i));
    }

    LOG(info) << "Done!" << ENDLG;
    return 0;
}
