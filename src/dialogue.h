#pragma once
#ifndef DIALOGUE_H
#define DIALOGUE_H

#include "npc.h"
#include "npc_class.h"
#include "output.h"
#include "game.h"
#include "map.h"
#include "npctalk.h"
#include "dialogue_win.h"

#include <memory>
#include <vector>
#include <string>
#include <functional>

class JsonObject;
class mission;
class time_point;
class npc;
class item;
struct tripoint;
class player;
template<typename T>
class string_id;

struct dialogue;

enum talk_trial_type : unsigned char {
    TALK_TRIAL_NONE, // No challenge here!
    TALK_TRIAL_LIE, // Straight up lying
    TALK_TRIAL_PERSUADE, // Convince them
    TALK_TRIAL_INTIMIDATE, // Physical intimidation
    NUM_TALK_TRIALS
};

enum dialogue_consequence : unsigned char {
    none = 0,
    hostile,
    helpless,
    action
};

using talkfunction_ptr = std::add_pointer<void ( npc & )>::type;
using dialogue_fun_ptr = std::add_pointer<void( npc & )>::type;

using trial_mod = std::pair<std::string, int>;

/**
 * If not TALK_TRIAL_NONE, it defines how to decide whether the responses succeeds (e.g. the
 * NPC believes the lie). The difficulty is a 0...100 percent chance of success (!), 100 means
 * always success, 0 means never. It is however affected by mutations/traits/bionics/etc. of
 * the player character.
 */
struct talk_trial {
    talk_trial_type type = TALK_TRIAL_NONE;
    int difficulty = 0;

    int calc_chance( const dialogue &d ) const;
    /**
     * Returns a user-friendly representation of @ref type
     */
    const std::string &name() const;
    operator bool() const {
        return type != TALK_TRIAL_NONE;
    }
    /**
     * Roll for success or failure of this trial.
     */
    bool roll( dialogue &d ) const;

    talk_trial() = default;
    talk_trial( JsonObject );
};

struct talk_topic {
    explicit talk_topic( const std::string &i ) : id( i ) { }

    std::string id;
    /** If we're talking about an item, this should be its type. */
    itype_id item_type = "null";
    /** Reason for denying a request. */
    std::string reason;
};

/**
 * This defines possible responses from the player character.
 */
struct talk_response {
        /**
         * What the player character says (literally). Should already be translated and will be
         * displayed.
         */
        std::string text;
        talk_trial trial;
        /**
         * The following values are forwarded to the chatbin of the NPC (see @ref npc_chatbin).
         */
        mission *mission_selected = nullptr;
        skill_id skill = skill_id::NULL_ID();
        matype_id style = matype_id::NULL_ID();
        struct effect_fun_t {
            private:
                std::function<void( const dialogue &d )> function;

            public:
                effect_fun_t() = default;
                effect_fun_t( talkfunction_ptr effect );
                effect_fun_t( std::function<void( npc & )> effect );
                void set_companion_mission( std::string &role_id );
                void set_u_add_effect( std::string &new_effect, std::string &duration );
                void set_npc_add_effect( std::string &new_effect, std::string &duration );
                void set_u_add_trait( std::string &new_trait );
                void set_npc_add_trait( std::string &new_trait );
                void set_u_buy_item( std::string &new_trait, int cost, int count, std::string &container_name );
                void set_u_spend_cash( int amount );
                void set_npc_change_faction( std::string &faction_name );
                void set_change_faction_rep( int amount );
                void operator()( const dialogue &d ) const {
                    if( !function ) {
                        return;
                    }
                    return function( d );
                }
        };
        /**
         * Defines what happens when the trial succeeds or fails. If trial is
         * TALK_TRIAL_NONE it always succeeds.
         */
        struct effect_t {
                /**
                 * How (if at all) the NPCs opinion of the player character (@ref npc::op_of_u) will change.
                 */
                npc_opinion opinion;
                /**
                 * Topic to switch to. TALK_DONE ends the talking, TALK_NONE keeps the current topic.
                 */
                talk_topic next_topic = talk_topic( "TALK_NONE" );

                talk_topic apply( dialogue &d ) const;
                dialogue_consequence get_consequence( const dialogue &d ) const;

                /**
                 * Sets an effect and consequence based on function pointer.
                 */
                void set_effect( talkfunction_ptr effect );
                void set_effect( effect_fun_t effect );
                /**
                 * Sets an effect to a function object and consequence to explicitly given one.
                 */
                void set_effect_consequence( effect_fun_t eff, dialogue_consequence con );
                void set_effect_consequence( std::function<void( npc &p )> ptr, dialogue_consequence con );


                void load_effect( JsonObject &jo );
                void parse_sub_effect( JsonObject jo );

                effect_t() = default;
                effect_t( JsonObject );

            private:
                /**
                 * Functions that are called when the response is chosen.
                 */
                std::vector<effect_fun_t> effects;
                dialogue_consequence guaranteed_consequence = dialogue_consequence::none;
        };
        effect_t success;
        effect_t failure;

        talk_data create_option_line( const dialogue &d, char letter );
        std::set<dialogue_consequence> get_consequences( const dialogue &d ) const;

        talk_response() = default;
        talk_response( JsonObject );
};

struct dialogue {
        /**
         * The player character that speaks (always g->u).
         * TODO: make it a reference, not a pointer.
         */
        player *alpha = nullptr;
        /**
         * The NPC we talk to. Never null.
         * TODO: make it a reference, not a pointer.
         */
        npc *beta = nullptr;
        /**
         * If true, we are done talking and the dialog ends.
         */
        bool done = false;
        std::vector<talk_topic> topic_stack;

        /** Missions that have been assigned by this npc to the player they currently speak to. */
        std::vector<mission *> missions_assigned;

        talk_topic opt( dialogue_window &d_win, const talk_topic &topic );

        dialogue() = default;

        std::string dynamic_line( const talk_topic &topic ) const;

        /**
         * Possible responses from the player character, filled in @ref gen_responses.
         */
        std::vector<talk_response> responses;
        void gen_responses( const talk_topic &topic );

        void add_topic( const std::string &topic );
        void add_topic( const talk_topic &topic );

    private:
        /**
         * Add a simple response that switches the topic to the new one.
         */
        talk_response &add_response( const std::string &text, const std::string &r );
        /**
         * Add a response with the result TALK_DONE.
         */
        talk_response &add_response_done( const std::string &text );
        /**
         * Add a response with the result TALK_NONE.
         */
        talk_response &add_response_none( const std::string &text );
        /**
         * Add a simple response that switches the topic to the new one and executes the given
         * action. The response always succeeds. Consequence is based on function used.
         */
        talk_response &add_response( const std::string &text, const std::string &r,
                                     dialogue_fun_ptr effect_success );

        /**
         * Add a simple response that switches the topic to the new one and executes the given
         * action. The response always succeeds. Consequence must be explicitly specified.
         */
        talk_response &add_response( const std::string &text, const std::string &r,
                                     std::function<void( npc & )> effect_success,
                                     dialogue_consequence consequence );
        /**
         * Add a simple response that switches the topic to the new one and sets the currently
         * talked about mission to the given one. The mission pointer must be valid.
         */
        talk_response &add_response( const std::string &text, const std::string &r, mission *miss );
        /**
         * Add a simple response that switches the topic to the new one and sets the currently
         * talked about skill to the given one.
         */
        talk_response &add_response( const std::string &text, const std::string &r, const skill_id &skill );
        /**
         * Add a simple response that switches the topic to the new one and sets the currently
         * talked about martial art style to the given one.
         */
        talk_response &add_response( const std::string &text, const std::string &r,
                                     const martialart &style );
        /**
         * Add a simple response that switches the topic to the new one and sets the currently
         * talked about item type to the given one.
         */
        talk_response &add_response( const std::string &text, const std::string &r,
                                     const itype_id &item_type );
};

/**
 * A dynamically generated line, spoken by the NPC.
 * This struct only adds the constructors which will load the data from json
 * into a lambda, stored in the std::function object.
 * Invoking the function operator with a dialog reference (so the function can access the NPC)
 * returns the actual line.
 */
struct dynamic_line_t {
    private:
        std::function<std::string( const dialogue & )> function;

    public:
        dynamic_line_t() = default;
        dynamic_line_t( const std::string &line );
        dynamic_line_t( JsonObject jo );
        dynamic_line_t( JsonArray ja );
        static dynamic_line_t from_member( JsonObject &jo, const std::string &member_name );

        std::string operator()( const dialogue &d ) const {
            if( !function ) {
                return std::string{};
            }
            return function( d );
        }
};

/**
 * A condition for a response spoken by the player.
 * This struct only adds the constructors which will load the data from json
 * into a lambda, stored in the std::function object.
 * Invoking the function operator with a dialog reference (so the function can access the NPC)
 * returns whether the response is allowed.
 */
struct conditional_t {
    private:
        std::function<bool ( const dialogue & )> condition;

    public:
        conditional_t() = default;
        conditional_t( const std::string &type );
        conditional_t( JsonObject jo );
        static conditional_t from_member( JsonObject &jo, const std::string &member_name );

        bool operator()( const dialogue &d ) const {
            if( !condition ) {
                return false;
            }
            return condition( d );
        }
};

/**
 * An extended response. It contains the response itself and a condition, so we can include the
 * response if, and only if the condition is met.
 */
class json_talk_response
{
    private:
        talk_response actual_response;
        std::function<bool( const dialogue & )> condition;

        void load_condition( JsonObject &jo );
        bool test_condition( const dialogue &d ) const;

    public:
        json_talk_response( JsonObject jo );

        /**
         * Callback from @ref json_talk_topic::gen_responses, see there.
         */
        void gen_responses( dialogue &d ) const;
};

/**
 * Talk topic definitions load from json.
 */
class json_talk_topic
{
    public:

    private:
        bool replace_built_in_responses = false;
        std::vector<json_talk_response> responses;
        dynamic_line_t dynamic_line;

    public:
        json_talk_topic() = default;
        /**
         * Load data from json.
         * This will append responses (not change existing ones).
         * It will override dynamic_line and replace_built_in_responses if those entries
         * exist in the input, otherwise they will not be changed at all.
         */
        void load( JsonObject &jo );

        std::string get_dynamic_line( const dialogue &d ) const;
        void check_consistency() const;
        /**
         * Callback from @ref dialogue::gen_responses, it should add the response from here
         * into the list of possible responses (that will be presented to the player).
         * It may add an arbitrary number of responses (including none at all).
         * @return true if built in response should excluded (not added). If false, built in
         * responses will be added (behind those added here).
         */
        bool gen_responses( dialogue &d ) const;
};

void unload_talk_topics();
void load_talk_topic( JsonObject &jo );

#endif
