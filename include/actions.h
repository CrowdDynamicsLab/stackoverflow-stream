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
    POST_QUESTION = 0,
    POST_ANSWER,
    POST_COMMENT,
    EDIT,
    MOD_VOTE,
    MOD_ACTION,
    INIT
};

meta::util::string_view action_name(action_type type)
{
    static meta::util::string_view names[]
        = {"question", "answer", "comment", "edit", "mod vote", "mod action"};

    if (static_cast<uint64_t>(type) > sizeof(names))
        throw std::out_of_range{"invalid action_type "
                                + std::to_string(static_cast<uint64_t>(type))};

    return names[static_cast<uint8_t>(type)];
}

action_type action_cast(history_type_id id)
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

action_type action_cast(meta::sequence::state_id id)
{
    return static_cast<action_type>(static_cast<uint64_t>(id));
}
#endif
