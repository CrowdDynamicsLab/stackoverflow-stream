/**
 * @file actions.h
 * @author Chase Geigle
 *
 * Defines the set of actions to be extracted from the StackExchange data
 * and functions for converting from ids to names.
 */

#ifndef STACKEXCHANGE_ACTIONS_H_
#define STACKEXCHANGE_ACTIONS_H_

#include "meta/meta.h"
#include "meta/sequence/markov_model.h"
#include "meta/util/optional.h"
#include "meta/util/string_view.h"

MAKE_NUMERIC_IDENTIFIER(user_id, uint64_t)
MAKE_NUMERIC_IDENTIFIER(post_id, uint64_t)
MAKE_NUMERIC_IDENTIFIER(history_type_id, uint64_t)

/**
 * Action space for users on StackExchange channels.
 *
 * Notation:
 * - MQ: My Question
 * - OQ: Other's Question
 * - MA: My Answer
 * - OA: Other's Answer
 * - MA_MQ: My Answer to My Question
 * - MA_OQ: My Answer to Other's Question
 * - OA_MQ: Other's Answer to My Question
 * - OA_OQ: Other's Answer to Other's Question
 */
enum class action_type : uint8_t
{
    QUESTION = 0,
    ANSWER_MQ,
    ANSWER_OQ,
    COMMENT_MQ,
    COMMENT_OQ,
    COMMENT_MA_MQ,
    COMMENT_MA_OQ,
    COMMENT_OA_MQ,
    COMMENT_OA_OQ,
    EDIT_MQ,
    EDIT_OQ,
    EDIT_MA,
    EDIT_OA,
    MOD_VOTE,
    MOD_ACTION,
    INIT
};

inline meta::util::string_view action_name(action_type type)
{
    switch (type)
    {
        case action_type::QUESTION:
            return "question";
        case action_type::ANSWER_MQ:
            return "answer (mq)";
        case action_type::ANSWER_OQ:
            return "answer (oq)";
        case action_type::COMMENT_MQ:
            return "comment (mq)";
        case action_type::COMMENT_OQ:
            return "comment (oq)";
        case action_type::COMMENT_MA_MQ:
            return "comment (ma-mq)";
        case action_type::COMMENT_MA_OQ:
            return "comment (ma-oq)";
        case action_type::COMMENT_OA_MQ:
            return "comment (oa-mq)";
        case action_type::COMMENT_OA_OQ:
            return "comment (oa-oq)";
        case action_type::EDIT_MQ:
            return "edit (mq)";
        case action_type::EDIT_OQ:
            return "edit (oq)";
        case action_type::EDIT_MA:
            return "edit (ma)";
        case action_type::EDIT_OA:
            return "edit (oa)";
        case action_type::MOD_VOTE:
            return "mod vote";
        case action_type::MOD_ACTION:
            return "mod action";
        case action_type::INIT:
            throw std::out_of_range{"INIT cannot be converted to string"};
    }
    return "unreachable";
}

enum class content_type : uint8_t
{
    MY_QUESTION = 0,
    OTHER_QUESTION,
    MY_ANSWER,
    OTHER_ANSWER
};

template <class PostMap>
meta::util::optional<content_type> content(post_id post, user_id user,
                                           PostMap& post_map)
{
    auto it = post_map.find(post);
    if (it == post_map.end())
        return meta::util::nullopt;

    if (it->value().op == user)
    {
        return it->value().parent ? content_type::MY_ANSWER
                                  : content_type::MY_QUESTION;
    }
    else
    {
        return it->value().parent ? content_type::OTHER_ANSWER
                                  : content_type::OTHER_QUESTION;
    }
}

template <class PostMap>
meta::util::optional<action_type> comment_type(post_id post, user_id user,
                                               PostMap& post_map)
{
    auto parent_type = content(post, user, post_map);
    if (!parent_type)
        return meta::util::nullopt;

    switch (*parent_type)
    {
        case content_type::MY_QUESTION:
            return action_type::COMMENT_MQ;
        case content_type::OTHER_QUESTION:
            return action_type::COMMENT_OQ;
        case content_type::MY_ANSWER:
        case content_type::OTHER_ANSWER:
            // comment was on an answer. Was the question our own?
            {
                auto qtype = content(*post_map.at(post).parent, user, post_map);
                if (!qtype)
                    return meta::util::nullopt;

                if (*qtype == content_type::MY_QUESTION)
                {
                    return *parent_type == content_type::MY_ANSWER
                               ? action_type::COMMENT_MA_MQ
                               : action_type::COMMENT_OA_MQ;
                }
                else
                {
                    return *parent_type == content_type::MY_ANSWER
                               ? action_type::COMMENT_MA_OQ
                               : action_type::COMMENT_OA_OQ;
                }
            }
        default:
            return meta::util::nullopt;
    }
}

inline action_type action_cast(history_type_id id, content_type type)
{
    switch (id)
    {
        case 1:
        case 2:
        case 3:
            return action_type::INIT;
        // title edits
        case 4:
        case 7:
        // body edits
        case 5:
        case 8:
        // tag edits
        case 6:
        case 9:
            switch (type)
            {
                case content_type::MY_QUESTION:
                    return action_type::EDIT_MQ;
                case content_type::OTHER_QUESTION:
                    return action_type::EDIT_OQ;
                case content_type::MY_ANSWER:
                    return action_type::EDIT_MA;
                case content_type::OTHER_ANSWER:
                    return action_type::EDIT_OA;
                default:
                    throw std::out_of_range{"invalid edit type"};
            }
        // vote for moderation
        case 10:
        case 11:
        case 12:
        case 13:
            return action_type::MOD_VOTE;
        // moderation action
        default:
            return action_type::MOD_ACTION;
    }
}

inline action_type action_cast(meta::sequence::state_id id)
{
    return static_cast<action_type>(static_cast<uint64_t>(id));
}
#endif
