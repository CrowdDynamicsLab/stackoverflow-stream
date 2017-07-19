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
#include "meta/util/string_view.h"

MAKE_NUMERIC_IDENTIFIER(history_type_id, uint64_t)

enum class action_type : uint8_t
{
    QUESTION = 0,
    ANSWER,
    COMMENT_OWN_QUESTION,
    COMMENT_ANSWER_OWN_QUESTION,
    COMMENT_OTHER_QUESTION,
    COMMENT_ANSWER_OTHER_QUESTION,
    EDIT,
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
        case action_type::ANSWER:
            return "answer";
        case action_type::COMMENT_OWN_QUESTION:
            return "comment (mq)";
        case action_type::COMMENT_ANSWER_OWN_QUESTION:
            return "comment (a2mq)";
        case action_type::COMMENT_OTHER_QUESTION:
            return "comment (oq)";
        case action_type::COMMENT_ANSWER_OTHER_QUESTION:
            return "comment (a2oq)";
        case action_type::EDIT:
            return "edit";
        case action_type::MOD_VOTE:
            return "mod vote";
        case action_type::MOD_ACTION:
            return "mod action";
        case action_type::INIT:
            throw std::out_of_range{"INIT cannot be converted to string"};
    }
    return "unreachable";
}

inline action_type action_cast(history_type_id id)
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
            return action_type::EDIT;
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
