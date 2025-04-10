#include "player.h" // IWYU pragma: associated

#include <array>
#include <cstdlib>
#include <memory>

#include "activity_handlers.h"
#include "avatar.h"
#include "character.h"
#include "character_effects.h"
#include "character_functions.h"
#include "damage.h"
#include "effect.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "field_type.h"
#include "game.h"
#include "int_id.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "martialarts.h"
#include "messages.h"
#include "morale_types.h"
#include "mongroup.h"
#include "monster.h"
#include "mutation_data.h"
#include "player_activity.h"
#include "pldata.h"
#include "rng.h"
#include "sdl_wrappers.h"
#include "sounds.h"
#include "stomach.h"
#include "string_formatter.h"
#include "teleport.h"
#include "text_snippets.h"
#include "translations.h"
#include "weather.h"
#include "vitamin.h"
#include <algorithm>
#include <functional>

static const activity_id ACT_FIRSTAID( "ACT_FIRSTAID" );

static const efftype_id effect_accumulated_mutagen( "accumulated_mutagen" );
static const efftype_id effect_adrenaline( "adrenaline" );
static const efftype_id effect_alarm_clock( "alarm_clock" );
static const efftype_id effect_antibiotic( "antibiotic" );
static const efftype_id effect_asthma( "asthma" );
static const efftype_id effect_attention( "attention" );
static const efftype_id effect_bite( "bite" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_blind( "blind" );
static const efftype_id effect_bloated( "bloated" );
static const efftype_id effect_bloodworms( "bloodworms" );
static const efftype_id effect_boomered( "boomered" );
static const efftype_id effect_brainworms( "brainworms" );
static const efftype_id effect_cold( "cold" );
static const efftype_id effect_datura( "datura" );
static const efftype_id effect_dazed( "dazed" );
static const efftype_id effect_dermatik( "dermatik" );
static const efftype_id effect_disabled( "disabled" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_evil( "evil" );
static const efftype_id effect_fearparalyze( "fearparalyze" );
static const efftype_id effect_formication( "formication" );
static const efftype_id effect_frostbite( "frostbite" );
static const efftype_id effect_fungus( "fungus" );
static const efftype_id effect_grabbed( "grabbed" );
static const efftype_id effect_grabbing( "grabbing" );
static const efftype_id effect_hallu( "hallu" );
static const efftype_id effect_hot( "hot" );
static const efftype_id effect_infected( "infected" );
static const efftype_id effect_lying_down( "lying_down" );
static const efftype_id effect_mutating( "mutating" );
static const efftype_id effect_nausea( "nausea" );
static const efftype_id effect_narcosis( "narcosis" );
static const efftype_id effect_onfire( "onfire" );
static const efftype_id effect_paincysts( "paincysts" );
static const efftype_id effect_panacea( "panacea" );
static const efftype_id effect_rat( "rat" );
static const efftype_id effect_recover( "recover" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_slept_through_alarm( "slept_through_alarm" );
static const efftype_id effect_spores( "spores" );
static const efftype_id effect_strong_antibiotic( "strong_antibiotic" );
static const efftype_id effect_tapeworm( "tapeworm" );
static const efftype_id effect_teleglow( "teleglow" );
static const efftype_id effect_toxin_buildup( "toxin_buildup" );
static const efftype_id effect_visuals( "visuals" );
static const efftype_id effect_weak_antibiotic( "weak_antibiotic" );

const vitamin_id vitamin_iron( "iron" );
const vitamin_id vitamin_mutant_toxin( "mutant_toxin" );

static const mongroup_id GROUP_NETHER( "GROUP_NETHER" );

static const mtype_id mon_dermatik_larva( "mon_dermatik_larva" );

static const bionic_id bio_infolink( "bio_infolink" );

static const trait_id trait_CHLOROMORPH( "CHLOROMORPH" );
static const trait_id trait_HEAVYSLEEPER2( "HEAVYSLEEPER2" );
static const trait_id trait_HIBERNATE( "HIBERNATE" );
static const trait_id trait_INFRESIST( "INFRESIST" );
static const trait_id trait_M_IMMUNE( "M_IMMUNE" );
static const trait_id trait_M_SKIN3( "M_SKIN3" );
static const trait_id trait_NOPAIN( "NOPAIN" );
static const trait_id trait_SEESLEEP( "SEESLEEP" );
static const trait_id trait_SCHIZOPHRENIC( "SCHIZOPHRENIC" );
static const trait_id trait_THRESH_MYCUS( "THRESH_MYCUS" );
static const trait_id trait_WATERSLEEP( "WATERSLEEP" );

static void eff_fun_onfire( player &u, effect &it )
{
    const int intense = it.get_intensity();
    u.deal_damage( nullptr, it.get_bp(), damage_instance( DT_HEAT, rng( intense,
                   intense * 2 ) ) );
}
static void eff_fun_spores( player &u, effect &it )
{
    // Equivalent to X in 150000 + health * 100
    const int intense = it.get_intensity();
    if( ( !u.has_trait( trait_M_IMMUNE ) ) && ( one_in( 100 ) &&
            x_in_y( intense, 900 + u.get_healthy() * 0.6 ) ) ) {
        u.add_effect( effect_fungus, 1_turns, bodypart_str_id::NULL_ID() );
    }
}
static void eff_fun_fungus( player &u, effect &it )
{
    const time_duration dur = it.get_duration();
    const int intense = it.get_intensity();
    const int bonus = u.get_healthy() / 10 + ( u.resists_effect( it ) ? 100 : 0 );
    switch( intense ) {
        case 1:
            // First hour symptoms
            if( one_in( 960 + bonus * 6 ) ) {
                u.cough( true );
            }
            if( one_in( 600 ) ) {
                u.add_msg_if_player( m_warning, _( "You feel nauseous." ) );
            }
            if( calendar::once_every( 10_minutes ) ) {
                u.add_msg_if_player( m_warning, _( "You smell and taste mushrooms." ) );
            }
            it.mod_duration( 1_turns );
            if( dur > 1_hours ) {
                it.mod_intensity( 1 );
            }
            break;
        case 2:
            // Five hours of worse symptoms
            if( one_in( 3600 + bonus * 18 ) ) {
                u.add_msg_if_player( m_bad,  _( "You spasm suddenly!" ) );
                u.moves -= 100;
                u.apply_damage( nullptr, bodypart_id( "torso" ), 5 );
            }
            if( x_in_y( character_effects::vomit_mod( u ), ( 4800 + bonus * 24 ) ) ||
                one_in( 12000 + bonus * 60 ) ) {
                u.add_msg_player_or_npc( m_bad, _( "You vomit a thick, gray goop." ),
                                         _( "<npcname> vomits a thick, gray goop." ) );

                const int awfulness = rng( 0, 10 );
                u.moves = -200;
                u.mod_stored_kcal( -10 * awfulness );
                u.mod_thirst( awfulness );
                u.apply_damage( nullptr, bodypart_id( "torso" ), 1 );
            }
            it.mod_duration( 1_turns );
            if( dur > 6_hours ) {
                it.mod_intensity( 1 );
            }
            break;
        case 3: {
            // Permanent symptoms
            bool is_fungal_ter = g->m.has_flag_ter( "FUNGUS", u.pos() );
            if( !is_fungal_ter && one_in( 600 + 4 * bonus ) ) {
                u.add_effect( effect_nausea, 5_minutes );
            }
        }
        break;
    }
}
static void eff_fun_rat( player &u, effect &it )
{
    const int dur = to_turns<int>( it.get_duration() );
    it.set_intensity( dur / 10 );
    if( rng( 0, 100 ) < dur / 10 ) {
        if( !one_in( 5 ) ) {
            u.mutate_category( mutation_category_id( "RAT" ) );
            it.mult_duration( .2 );
        } else {
            u.mutate_category( mutation_category_id( "TROGLOBITE" ) );
            it.mult_duration( .33 );
        }
    } else if( rng( 0, 100 ) < dur / 8 ) {
        if( one_in( 3 ) ) {
            u.vomit();
            it.mod_duration( -1_minutes );
        } else {
            u.add_msg_if_player( m_bad, _( "You feel nauseous!" ) );
            it.mod_duration( 3_turns );
        }
    }
}
static void eff_fun_bleed( player &u, effect &it )
{
    // Presuming that during the first-aid process you're putting pressure
    // on the wound or otherwise suppressing the flow. (Kits contain either
    // QuikClot or bandages per the recipe.)
    const int intense = it.get_intensity();
    if( one_in( 36 / intense ) && u.activity->id() != ACT_FIRSTAID ) {
        u.add_msg_player_or_npc( m_bad, _( "You lose some blood." ),
                                 _( "<npcname> loses some blood." ) );
        // Prolonged hemorrhage is a significant risk for developing anemia
        u.vitamin_mod( vitamin_iron, rng( -1, -4 ) );
        u.mod_pain( 1 );
        u.apply_damage( nullptr, it.get_bp(), 1 );
        u.bleed();
    }
}
static void eff_fun_hallu( player &u, effect &it )
{
    // TODO: Redo this to allow for variable durations
    // Time intervals are drawn from the old ones based on 3600 (6-hour) duration.
    constexpr int maxDuration = 21600;
    constexpr int comeupTime = static_cast<int>( maxDuration * 0.9 );
    constexpr int noticeTime = static_cast<int>( comeupTime + ( maxDuration - comeupTime ) / 2 );
    constexpr int peakTime = static_cast<int>( maxDuration * 0.8 );
    constexpr int comedownTime = static_cast<int>( maxDuration * 0.3 );
    const int dur = to_turns<int>( it.get_duration() );
    // Baseline
    if( dur == noticeTime ) {
        u.add_msg_if_player( m_warning, _( "You feel a little strange." ) );
    } else if( dur == comeupTime ) {
        // Coming up
        if( one_in( 2 ) ) {
            u.add_msg_if_player( m_warning, _( "The world takes on a dreamlike quality." ) );
        } else if( one_in( 3 ) ) {
            u.add_msg_if_player( m_warning, _( "You have a sudden nostalgic feeling." ) );
        } else if( one_in( 5 ) ) {
            u.add_msg_if_player( m_warning, _( "Everything around you is starting to breathe." ) );
        } else {
            u.add_msg_if_player( m_warning, _( "Something feels very, very wrong." ) );
        }
    } else if( dur > peakTime && dur < comeupTime ) {
        if( u.stomach.get_calories() > 0 &&
            ( one_in( 1200 ) || x_in_y( character_effects::vomit_mod( u ), 300 ) ) ) {
            u.add_msg_if_player( m_bad, _( "You feel sick to your stomach." ) );
            u.mod_stored_kcal( 20 );
            if( one_in( 6 ) ) {
                u.vomit();
            }
        }
        if( u.is_npc() && one_in( 1200 ) ) {
            static const std::array<std::string, 4> npc_hallu = {{
                    translate_marker( "\"I think it's starting to kick in.\"" ),
                    translate_marker( "\"Oh God, what's happening?\"" ),
                    translate_marker( "\"Of course… it's all fractals!\"" ),
                    translate_marker( "\"Huh?  What was that?\"" )
                }
            };

            ///\EFFECT_STR_NPC increases volume of hallucination sounds (NEGATIVE)

            ///\EFFECT_INT_NPC decreases volume of hallucination sounds
            int loudness = 20 + u.str_cur - u.int_cur;
            loudness = ( loudness > 5 ? loudness : 5 );
            loudness = ( loudness < 30 ? loudness : 30 );
            sounds::sound( u.pos(), loudness, sounds::sound_t::speech, _( random_entry_ref( npc_hallu ) ),
                           false, "speech",
                           loudness < 15 ? ( u.male ? "NPC_m" : "NPC_f" ) : ( u.male ? "NPC_m_loud" : "NPC_f_loud" ) );
        }
    } else if( dur == peakTime ) {
        // Visuals start
        u.add_msg_if_player( m_bad, _( "Fractal patterns dance across your vision." ) );
        u.add_effect( effect_visuals, time_duration::from_turns( peakTime - comedownTime ) );
    } else if( dur > comedownTime && dur < peakTime ) {
        // Full symptoms
        u.mod_per_bonus( -2 );
        u.mod_int_bonus( -1 );
        u.mod_dex_bonus( -2 );
        u.add_miss_reason( _( "Dancing fractals distract you." ), 2 );
        u.mod_str_bonus( -1 );
        if( u.is_player() && one_in( 50 ) ) {
            g->spawn_hallucination( u.pos() + tripoint( rng( -10, 10 ), rng( -10, 10 ), 0 ) );
        }
    } else if( dur == comedownTime ) {
        if( one_in( 42 ) ) {
            u.add_msg_if_player( _( "Everything looks SO boring now." ) );
        } else {
            u.add_msg_if_player( _( "Things are returning to normal." ) );
        }
    }
}

struct temperature_effect {
    int str_pen;
    int dex_pen;
    int int_pen;
    int per_pen;
    // Not translated (static string should not be translated)
    std::string msg;
    int msg_chance;
    // Note: NOT std::string because the pointer is stored so c_str() is not OK
    // Also not translated
    const char *miss_msg;

    temperature_effect( int sp, int dp, int ip, int pp, const std::string &ms, int mc,
                        const char *mm ) :
        str_pen( sp ), dex_pen( dp ), int_pen( ip ), per_pen( pp ), msg( ms ),
        msg_chance( mc ), miss_msg( mm ) {
    }

    void apply( player &u ) const {
        if( str_pen > 0 ) {
            u.mod_str_bonus( -str_pen );
        }
        if( dex_pen > 0 ) {
            u.mod_dex_bonus( -dex_pen );
            u.add_miss_reason( _( miss_msg ), dex_pen );
        }
        if( int_pen > 0 ) {
            u.mod_int_bonus( -int_pen );
        }
        if( per_pen > 0 ) {
            u.mod_per_bonus( -per_pen );
        }
        if( !msg.empty() && !u.has_effect( effect_sleep ) && one_in( msg_chance ) ) {
            u.add_msg_if_player( m_warning, "%s", _( msg ) );
        }
    }
};

static void eff_fun_cold( player &u, effect &it )
{
    // { body_part, intensity }, { str_pen, dex_pen, int_pen, per_pen, msg, msg_chance, miss_msg }
    static const std::map<std::pair<body_part, int>, temperature_effect> effs = {{
            { { bp_head, 3 }, { 0, 0, 3, 0, translate_marker( "Your thoughts are unclear." ), 2400, "" } },
            { { bp_head, 2 }, { 0, 0, 1, 0, "", 0, "" } },
            { { bp_mouth, 3 }, { 0, 0, 0, 3, translate_marker( "Your face is stiff from the cold." ), 2400, "" } },
            { { bp_mouth, 2 }, { 0, 0, 0, 1, "", 0, "" } },
            { { bp_torso, 3 }, { 0, 4, 0, 0, translate_marker( "Your torso is freezing cold.  You should put on a few more layers." ), 400, translate_marker( "You quiver from the cold." ) } },
            { { bp_torso, 2 }, { 0, 2, 0, 0, "", 0, translate_marker( "Your shivering makes you unsteady." ) } },
            { { bp_arm_l, 3 }, { 0, 2, 0, 0, translate_marker( "Your left arm is shivering." ), 4800, translate_marker( "Your left arm trembles from the cold." ) } },
            { { bp_arm_l, 2 }, { 0, 1, 0, 0, translate_marker( "Your left arm is shivering." ), 4800, translate_marker( "Your left arm trembles from the cold." ) } },
            { { bp_arm_r, 3 }, { 0, 2, 0, 0, translate_marker( "Your right arm is shivering." ), 4800, translate_marker( "Your right arm trembles from the cold." ) } },
            { { bp_arm_r, 2 }, { 0, 1, 0, 0, translate_marker( "Your right arm is shivering." ), 4800, translate_marker( "Your right arm trembles from the cold." ) } },
            { { bp_hand_l, 3 }, { 0, 2, 0, 0, translate_marker( "Your left hand feels like ice." ), 4800, translate_marker( "Your left hand quivers in the cold." ) } },
            { { bp_hand_l, 2 }, { 0, 1, 0, 0, translate_marker( "Your left hand feels like ice." ), 4800, translate_marker( "Your left hand quivers in the cold." ) } },
            { { bp_hand_r, 3 }, { 0, 2, 0, 0, translate_marker( "Your right hand feels like ice." ), 4800, translate_marker( "Your right hand quivers in the cold." ) } },
            { { bp_hand_r, 2 }, { 0, 1, 0, 0, translate_marker( "Your right hand feels like ice." ), 4800, translate_marker( "Your right hand quivers in the cold." ) } },
            { { bp_leg_l, 3 }, { 2, 2, 0, 0, translate_marker( "Your left leg trembles against the relentless cold." ), 4800, translate_marker( "Your legs uncontrollably shake from the cold." ) } },
            { { bp_leg_l, 2 }, { 1, 1, 0, 0, translate_marker( "Your left leg trembles against the relentless cold." ), 4800, translate_marker( "Your legs uncontrollably shake from the cold." ) } },
            { { bp_leg_r, 3 }, { 2, 2, 0, 0, translate_marker( "Your right leg trembles against the relentless cold." ), 4800, translate_marker( "Your legs uncontrollably shake from the cold." ) } },
            { { bp_leg_r, 2 }, { 1, 1, 0, 0, translate_marker( "Your right leg trembles against the relentless cold." ), 4800, translate_marker( "Your legs uncontrollably shake from the cold." ) } },
            { { bp_foot_l, 3 }, { 2, 2, 0, 0, translate_marker( "Your left foot feels frigid." ), 4800, translate_marker( "Your left foot is as nimble as a block of ice." ) } },
            { { bp_foot_l, 2 }, { 1, 1, 0, 0, translate_marker( "Your left foot feels frigid." ), 4800, translate_marker( "Your freezing left foot messes up your balance." ) } },
            { { bp_foot_r, 3 }, { 2, 2, 0, 0, translate_marker( "Your right foot feels frigid." ), 4800, translate_marker( "Your right foot is as nimble as a block of ice." ) } },
            { { bp_foot_r, 2 }, { 1, 1, 0, 0, translate_marker( "Your right foot feels frigid." ), 4800, translate_marker( "Your freezing right foot messes up your balance." ) } },
        }
    };
    const auto iter = effs.find( { it.get_bp()->token, it.get_intensity() } );
    if( iter != effs.end() ) {
        iter->second.apply( u );
    }
}

static void eff_fun_hot( player &u, effect &it )
{
    // { body_part, intensity }, { str_pen, dex_pen, int_pen, per_pen, msg, msg_chance, miss_msg }
    static const std::map<std::pair<body_part, int>, temperature_effect> effs = {{
            { { bp_head, 3 }, { 0, 0, 0, 0, translate_marker( "Your head is pounding from the heat." ), 2400, "" } },
            { { bp_head, 2 }, { 0, 0, 0, 0, "", 0, "" } },
            { { bp_torso, 3 }, { 2, 0, 0, 0, translate_marker( "You are sweating profusely." ), 2400, "" } },
            { { bp_torso, 2 }, { 1, 0, 0, 0, "", 0, "" } },
            { { bp_hand_l, 3 }, { 0, 2, 0, 0, "", 0, translate_marker( "Your left hand's too sweaty to grip well." ) } },
            { { bp_hand_l, 2 }, { 0, 1, 0, 0, "", 0, translate_marker( "Your left hand's too sweaty to grip well." ) } },
            { { bp_hand_r, 3 }, { 0, 2, 0, 0, "", 0, translate_marker( "Your right hand's too sweaty to grip well." ) } },
            { { bp_hand_r, 2 }, { 0, 1, 0, 0, "", 0, translate_marker( "Your right hand's too sweaty to grip well." ) } },
            { { bp_leg_l, 3 }, { 0, 0, 0, 0, translate_marker( "Your left leg is cramping up." ), 4800, "" } },
            { { bp_leg_l, 2 }, { 0, 0, 0, 0, "", 0, "" } },
            { { bp_leg_r, 3 }, { 0, 0, 0, 0, translate_marker( "Your right leg is cramping up." ), 4800, "" } },
            { { bp_leg_r, 2 }, { 0, 0, 0, 0, "", 0, "" } },
            { { bp_foot_l, 3 }, { 0, 0, 0, 0, translate_marker( "Your left foot is swelling in the heat." ), 4800, "" } },
            { { bp_foot_l, 2 }, { 0, 0, 0, 0, "", 0, "" } },
            { { bp_foot_r, 3 }, { 0, 0, 0, 0, translate_marker( "Your right foot is swelling in the heat." ), 4800, "" } },
            { { bp_foot_r, 2 }, { 0, 0, 0, 0, "", 0, "" } },
        }
    };

    const body_part bp = it.get_bp()->token;
    const int intense = it.get_intensity();
    const auto iter = effs.find( { it.get_bp()->token, it.get_intensity() } );
    if( iter != effs.end() ) {
        iter->second.apply( u );
    }
    // Hothead effects are a special snowflake
    if( bp == bp_head && intense >= 2 ) {
        auto iter = u.get_body().find( body_part_head );
        if( iter == u.get_body().end() ) {
            debugmsg( "%s has no head(?!)", u.disp_name() );
            return;
        }
        int temp_cur = iter->second.get_temp_cur();
        if( one_in( std::max( 25, std::min( 89500, 90000 - temp_cur ) ) ) ) {
            u.vomit();
        }
        if( !u.has_effect( effect_sleep ) && one_in( 2400 ) ) {
            u.add_msg_if_player( m_bad, _( "The heat is making you see things." ) );
        }
    }
}

static void eff_fun_frostbite( player &u, effect &it )
{
    // { body_part, intensity }, { str_pen, dex_pen, int_pen, per_pen, msg, msg_chance, miss_msg }
    static const std::map<std::pair<body_part, int>, temperature_effect> effs = {{
            { { bp_hand_l, 2 }, { 0, 2, 0, 0, "", 0, translate_marker( "You have trouble grasping with your numb fingers." ) } },
            { { bp_hand_r, 2 }, { 0, 2, 0, 0, "", 0, translate_marker( "You have trouble grasping with your numb fingers." ) } },
            { { bp_foot_l, 2 }, { 0, 0, 0, 0, translate_marker( "Your foot has gone numb." ), 4800, "" } },
            { { bp_foot_l, 1 }, { 0, 0, 0, 0, translate_marker( "Your foot has gone numb." ), 4800, "" } },
            { { bp_foot_r, 2 }, { 0, 0, 0, 0, translate_marker( "Your foot has gone numb." ), 4800, "" } },
            { { bp_foot_r, 1 }, { 0, 0, 0, 0, translate_marker( "Your foot has gone numb." ), 4800, "" } },
            { { bp_mouth, 2 }, { 0, 0, 0, 3, translate_marker( "Your face feels numb." ), 4800, "" } },
            { { bp_mouth, 1 }, { 0, 0, 0, 1, translate_marker( "Your face feels numb." ), 4800, "" } },
        }
    };
    const auto iter = effs.find( { it.get_bp()->token, it.get_intensity() } );
    if( iter != effs.end() ) {
        iter->second.apply( u );
    }
}

static void eff_fun_bloated( player &u, effect &it )
{
    if( u.get_stored_kcal() > u.max_stored_kcal() || u.get_thirst() < 0 ) {
        it.set_duration( 5_minutes );
    }
}

static void eff_fun_toxin_buildup( player &u, effect &it )
{
    if( it.get_intensity() > 3 ) {
        u.vitamin_set( vitamin_mutant_toxin, 0 );
        u.add_effect( effect_mutating, effect_mutating->get_max_duration() / 2 );
    }
}

static void eff_fun_mutating( player &u, effect &it )
{
    if( it.get_intensity() > 1 ) {
        // "Activate" toxins that would otherwise need to accumulate first
        // There is some loss
        const vitamin &tox = *vitamin_mutant_toxin;
        int vit_amt = u.vitamin_get( vitamin_mutant_toxin );
        time_duration extra_duration = it.get_max_duration() / 2 * vit_amt / tox.max();
        it.mod_duration( extra_duration );
        u.vitamin_set( vitamin_mutant_toxin, 0 );
    }

    // At intensity 1
    constexpr time_duration base_time_per_mut = 2_days;
    int mutation_mult = it.get_intensity() * it.get_intensity();
    float muts_per_second = mutation_mult / to_seconds<float>( base_time_per_mut );
    float mgen_per_mut = to_seconds<float>( effect_accumulated_mutagen->get_int_dur_factor() );
    float mgen_per_second = mgen_per_mut * muts_per_second;
    // How much accumulated mutagen effect do we add per second
    time_duration mgen_time_mult = 1_seconds * roll_remainder( mgen_per_second );
    u.add_effect( effect_accumulated_mutagen, mgen_time_mult );
    if( u.get_effect_int( effect_accumulated_mutagen ) > 1 ) {
        u.mutate();
    }
}

void Character::hardcoded_effects( effect &it )
{
    if( auto buff = ma_buff::from_effect( it ) ) {
        if( buff->is_valid_character( *this ) ) {
            buff->apply_character( *this );
        } else {
            it.set_duration( 0_turns ); // removes the effect
        }
        return;
    }
    using hc_effect_fun = std::function<void( player &, effect & )>;
    static const std::map<efftype_id, hc_effect_fun> hc_effect_map = {{
            { effect_onfire, eff_fun_onfire },
            { effect_spores, eff_fun_spores },
            { effect_fungus, eff_fun_fungus },
            { effect_rat, eff_fun_rat },
            { effect_bleed, eff_fun_bleed },
            { effect_hallu, eff_fun_hallu },
            { effect_cold, eff_fun_cold },
            { effect_hot, eff_fun_hot },
            { effect_frostbite, eff_fun_frostbite },
            { effect_bloated, eff_fun_bloated },
            { effect_toxin_buildup, eff_fun_toxin_buildup },
            { effect_mutating, eff_fun_mutating },
        }
    };
    const efftype_id &id = it.get_id();
    const auto &iter = hc_effect_map.find( id );
    if( iter != hc_effect_map.end() ) {
        iter->second( *as_player(), it );
        return;
    }

    const time_duration dur = it.get_duration();
    int intense = it.get_intensity();
    body_part bp = it.get_bp()->token;
    bool sleeping = has_effect( effect_sleep );
    if( id == effect_dermatik ) {
        bool triggered = false;
        int formication_chance = 3600;
        if( dur < 4_hours ) {
            formication_chance += 14400 - to_turns<int>( dur );
        }
        if( one_in( formication_chance ) ) {
            add_effect( effect_formication, 60_minutes, convert_bp( bp ) );
        }
        if( dur < 1_days && one_in( 14400 ) ) {
            vomit();
        }
        if( dur > 1_days ) {
            // Spawn some larvae!
            // Choose how many insects; more for large characters
            ///\EFFECT_STR_MAX increases number of insects hatched from dermatik infection
            int num_insects = rng( 1, std::min( 3, str_max / 3 ) );
            apply_damage( nullptr, convert_bp( bp ).id(), rng( 2, 4 ) * num_insects );
            // Figure out where they may be placed
            add_msg_player_or_npc( m_bad,
                                   _( "Your flesh crawls; insects tear through the flesh and begin to emerge!" ),
                                   _( "Insects begin to emerge from <npcname>'s skin!" ) );
            for( ; num_insects > 0; num_insects-- ) {
                if( monster *const grub = g->place_critter_around( mon_dermatik_larva, pos(), 1 ) ) {
                    if( one_in( 3 ) ) {
                        grub->friendly = -1;
                    }
                }
            }
            g->events().send<event_type::dermatik_eggs_hatch>( getID() );
            remove_effect( effect_formication, convert_bp( bp ) );
            moves -= 600;
            triggered = true;
        }
        if( triggered ) {
            // Set ourselves up for removal
            it.set_duration( 0_turns );
        } else {
            // Count duration up
            it.mod_duration( 1_turns );
        }
    } else if( id == effect_formication ) {
        ///\EFFECT_INT decreases occurrence of itching from formication effect
        if( x_in_y( intense, 600 + 300 * get_int() ) && !has_effect( effect_narcosis ) ) {
            if( !is_npc() ) {
                //~ %s is bodypart in accusative.
                add_msg( m_warning, _( "You start scratching your %s!" ), body_part_name_accusative( bp ) );
                g->u.cancel_activity();
            } else if( g->u.sees( pos() ) ) {
                //~ 1$s is NPC name, 2$s is bodypart in accusative.
                add_msg( _( "%1$s starts scratching their %2$s!" ), name, body_part_name_accusative( bp ) );
            }
            moves -= 150;
            apply_damage( nullptr, convert_bp( bp ).id(), 1 );
        }
    } else if( id == effect_evil ) {
        // Worn or wielded; diminished effects
        bool lesserEvil = primary_weapon().has_effect_when_wielded( AEP_EVIL ) ||
                          primary_weapon().has_effect_when_carried( AEP_EVIL );
        for( auto &w : worn ) {
            if( w->has_effect_when_worn( AEP_EVIL ) ) {
                lesserEvil = true;
                break;
            }
        }
        if( lesserEvil ) {
            // Only minor effects, some even good!
            mod_str_bonus( dur > 450_minutes ? 10.0 : dur / 45_minutes );
            if( dur < 1_hours ) {
                mod_dex_bonus( 1 );
            } else {
                int dex_mod = -( dur > 360_minutes ? 10.0 : ( dur - 1_hours ) / 30_minutes );
                mod_dex_bonus( dex_mod );
                add_miss_reason( _( "Why waste your time on that insignificant speck?" ), -dex_mod );
            }
            mod_int_bonus( -( dur > 300_minutes ? 10.0 : ( dur - 50_minutes ) / 25_minutes ) );
            mod_per_bonus( -( dur > 480_minutes ? 10.0 : ( dur - 80_minutes ) / 40_minutes ) );
        } else {
            // Major effects, all bad.
            mod_str_bonus( -( dur > 500_minutes ? 10.0 : dur / 50_minutes ) );
            int dex_mod = -( dur > 600_minutes ? 10.0 : dur / 60_minutes );
            mod_dex_bonus( dex_mod );
            add_miss_reason( _( "Why waste your time on that insignificant speck?" ), -dex_mod );
            mod_int_bonus( -( dur > 450_minutes ? 10.0 : dur / 45_minutes ) );
            mod_per_bonus( -( dur > 400_minutes ? 10.0 : dur / 40_minutes ) );
        }
    } else if( id == effect_attention ) {
        if( intense > 6 ) {
            if( one_in( 7200 - ( intense * 450 ) ) ) {
                add_msg_if_player( m_bad,
                                   _( "You feel something reaching out to you, before reality around you frays!" ) );
                if( has_psy_protection( *this, 10 ) ) {
                    // Transfers half of remaining duration of nether attention, tinfoil only sometimes helps
                    add_effect( effect_teleglow, ( dur / 2 ), bodypart_str_id::NULL_ID(), ( intense / 2 ) );
                } else {
                    // Transfers all remaining duration of nether attention to dimensional instability
                    add_effect( effect_teleglow, dur, bodypart_str_id::NULL_ID(), intense );
                }
                it.set_duration( 0_turns );
            }
            if( one_in( 8000 - ( intense * 500 ) ) && one_in( 2 ) ) {
                if( !is_npc() ) {
                    add_msg( m_bad, _( "You pass out from the strain of something bearing down on your mind." ) );
                }
                fall_asleep( 2_hours );
                if( one_in( 10 ) ) {
                    it.set_duration( 0_turns );
                }
                it.mod_duration( -20_minutes * intense );
                it.mod_intensity( -1 );
            }
        }
        if( intense > 4 ) {
            if( one_in( 6000 - ( intense * 375 ) ) ) {
                if( has_psy_protection( *this, 4 ) ) {
                    add_msg_if_player( m_bad, _( "You feel something probing your mind, but it is rebuffed!" ) );
                } else {
                    add_msg_if_player( m_bad, _( "A terrifying image in the back out your mind paralyzes you." ) );
                    add_effect( effect_fearparalyze, 5_turns );
                    moves -= 4 * get_speed();
                }
                it.mod_duration( -10_minutes * intense );
                if( one_in( 2 ) ) {
                    it.mod_intensity( -1 );
                }
            }
            if( one_turn_in( 1200_minutes - ( intense * 90_minutes ) ) ) {
                if( has_psy_protection( *this, 4 ) ) {
                    add_msg_if_player( m_bad, _( "You feel a buzzing in the back of your mind, but it passes." ) );
                } else {
                    add_msg_if_player( m_bad, _( "You feel something scream in the back of your mind!" ) );
                    add_effect( effect_dazed, rng( 1_minutes, 2_minutes ) );
                }
                it.mod_duration( -10_minutes * intense );
                if( one_in( 3 ) ) {
                    it.mod_intensity( -1 );
                }
            }
        }
        if( intense > 2 ) {
            if( one_turn_in( 1200_minutes - ( intense * 90_minutes ) ) ) {
                add_msg_if_player( m_bad, _( "Your vision is filled with bright lights…" ) );
                add_effect( effect_blind, rng( 1_minutes, 2_minutes ) );
                it.mod_duration( -10_minutes * intense );
                if( one_in( 4 ) ) {
                    it.mod_intensity( -1 );
                }
            }
            if( one_in( 5000 ) && !has_effect( effect_nausea ) ) {
                add_msg_if_player( m_bad, _( "A wave of nausea passes over you." ) );
                add_effect( effect_nausea, 5_minutes );
            }
        }
        if( one_in( 5000 ) && !has_effect( effect_hallu ) ) {
            add_msg_if_player( m_bad, _( "Shifting shapes dance on the edge of your vision." ) );
            add_effect( effect_hallu, 4_hours );
            it.mod_duration( -10_minutes * intense );
        }
        if( one_turn_in( 40_minutes ) ) {
            if( has_psy_protection( *this, 4 ) ) {
                add_msg_if_player( m_bad, _( "You feel weird for a moment, but it passes." ) );
            } else {
                // Less morale drop and faster decay than Psychosis negative messages, but more frequent
                const translation snip = SNIPPET.random_from_category( "nether_attention_watching" ).value_or(
                                             translation() );
                add_msg_if_player( m_warning, "%s", snip );
                add_morale( MORALE_FEELING_BAD, -10, -50, 60_minutes, 20_minutes, true );
            }
        }
    } else if( id == effect_teleglow ) {
        // Each teleportation increases intensity by 1, 2 intensities per tier of effect.
        // TODO: Include a chance to teleport to the nether realm.
        // TODO: This with regards to NPCS
        if( !is_player() ) {
            // NO, no teleporting around the player because an NPC has teleglow!
            return;
        }
        if( intense > 6 ) {
            if( one_in( 6000 - ( intense * 250 ) ) ) {
                if( !is_npc() ) {
                    add_msg( _( "Glowing lights surround you, and you teleport." ) );
                }
                teleport::teleport( *this );
                g->events().send<event_type::teleglow_teleports>( getID() );
                if( one_in( 10 ) ) {
                    // Set ourselves up for removal
                    it.set_duration( 0_turns );
                }
                // Since teleporting grants 1 intensity and 30 minutes duration,
                // if it doesn't remove it'll get more intense but shorter.
                it.mod_duration( -20_minutes * intense );
            }
            if( one_in( 7200 - ( intense * 250 ) ) ) {
                add_msg_if_player( m_bad, _( "You are beset with a vision of a prowling beast." ) );
                for( const tripoint &dest : g->m.points_in_radius( pos(), 6 ) ) {
                    if( g->m.is_cornerfloor( dest ) ) {
                        g->m.add_field( dest, fd_tindalos_rift, 3 );
                        add_msg_if_player( m_info, _( "Your surroundings are permeated with a foul scent." ) );
                        break;
                    }
                }
                if( one_in( 2 ) ) {
                    // Set ourselves up for removal
                    it.set_duration( 0_turns );
                }
                it.mod_intensity( -1 );
            }
        }
        if( intense > 4 ) {
            // Once every 4 hours baseline, once every 2 hours max
            if( one_turn_in( 14_hours - ( intense * 90_minutes ) ) ) {
                tripoint dest( 0, 0, posz() );
                int &x = dest.x;
                int &y = dest.y;
                int tries = 0;
                do {
                    x = posx() + rng( -4, 4 );
                    y = posy() + rng( -4, 4 );
                    tries++;
                    if( tries >= 10 ) {
                        break;
                    }
                } while( g->critter_at( dest ) );
                if( tries < 10 ) {
                    if( g->m.impassable( dest ) ) {
                        g->m.make_rubble( dest, f_rubble_rock );
                    }
                    MonsterGroupResult spawn_details = MonsterGroupManager::GetResultFromGroup(
                                                           GROUP_NETHER );
                    g->place_critter_at( spawn_details.name, dest );
                    if( g->u.sees( dest ) ) {
                        g->cancel_activity_or_ignore_query( distraction_type::hostile_spotted_far,
                                                            _( "A monster appears nearby!" ) );
                        add_msg( m_warning, _( "A portal opens nearby, and a monster crawls through!" ) );
                    }
                    it.mod_duration( -10_minutes * intense );
                    it.mod_intensity( -1 );
                }
            }
            if( one_in( 21000 - ( intense * 1125 ) ) ) {
                add_msg_if_player( m_bad, _( "You shudder suddenly." ) );
                mutate();
                it.mod_duration( -10_minutes * intense );
                if( one_in( 2 ) ) {
                    it.mod_intensity( -1 );
                }
            }
        }
        if( intense > 2 ) {
            if( one_in( 10000 ) ) {
                if( !has_trait( trait_M_IMMUNE ) ) {
                    add_effect( effect_fungus, 1_turns, bodypart_str_id::NULL_ID() );
                    add_msg_if_player( m_bad, _( "You smell mold, and your skin itches." ) );
                } else {
                    add_msg_if_player( m_info, _( "We have many colonists awaiting passage." ) );
                }
                // Set ourselves up for removal
                it.set_duration( 0_turns );
            }
            if( one_in( 5000 ) ) {
                // Like with the glow anomaly trap, but lower max and bypasses radsuits
                add_msg_if_player( m_bad, _( "A blue flash of radiation permeates your vision briefly!" ) );
                irradiate( rng( 10, 20 ), true );
                it.mod_duration( -10_minutes * intense );
                if( one_in( 4 ) ) {
                    it.mod_intensity( -1 );
                }
            }
        }
        if( one_in( 4000 ) ) {
            add_msg_if_player( m_bad, _( "You're suddenly covered in ectoplasm." ) );
            add_effect( effect_boomered, 10_minutes );
            it.mod_duration( -10_minutes * intense );
        }
        if( one_in( 5000 ) ) {
            add_msg_if_player( m_bad, _( "A strange sound reverberates around the edges of reality." ) );
            // Comparable to the humming anomaly trap, with a narrower range
            int volume = rng( 25, 150 );
            std::string sfx;
            if( volume <= 50 ) {
                sfx = _( "hrmmm" );
            } else if( volume <= 100 ) {
                sfx = _( "HRMMM" );
            } else {
                sfx = _( "VRMMMMMM" );
            }
            sounds::sound( pos(), volume, sounds::sound_t::activity, sfx, false, "humming", "machinery" );
        }
    } else if( id == effect_asthma ) {
        if( has_effect( effect_adrenaline ) || has_effect( effect_datura ) ) {
            add_msg_if_player( m_good, _( "Your asthma attack stops." ) );
            it.set_duration( 0_turns );
        } else if( dur > 2_hours ) {
            add_msg_if_player( m_bad, _( "Your asthma overcomes you.\nYou asphyxiate." ) );
            g->events().send<event_type::dies_from_asthma_attack>( getID() );
            hurtall( 500, nullptr );
        } else if( dur > 70_minutes ) {
            if( one_in( 120 ) ) {
                add_msg_if_player( m_bad, _( "You wheeze and gasp for air." ) );
            }
        }
    } else if( id == effect_brainworms ) {
        if( one_in( 1536 ) ) {
            add_msg_if_player( m_bad, _( "Your head aches faintly." ) );
        }
        if( one_in( 6144 ) ) {
            mod_healthy_mod( -10, -100 );
            apply_damage( nullptr, bodypart_id( "head" ), rng( 0, 1 ) );
            if( !has_effect( effect_visuals ) ) {
                add_msg_if_player( m_bad, _( "Your vision is getting fuzzy." ) );
                add_effect( effect_visuals, rng( 1_minutes, 60_minutes ) );
            }
        }
        if( one_in( 24576 ) ) {
            mod_healthy_mod( -10, -100 );
            apply_damage( nullptr, bodypart_id( "head" ), rng( 1, 2 ) );
            if( !is_blind() && !sleeping ) {
                add_msg_if_player( m_bad, _( "Your vision goes black!" ) );
                add_effect( effect_blind, rng( 5_turns, 20_turns ) );
            }
        }
    } else if( id == effect_tapeworm ) {
        if( one_in( 3072 ) ) {
            add_msg_if_player( m_bad, _( "Your bowels ache." ) );
        }
    } else if( id == effect_bloodworms ) {
        if( one_in( 3072 ) ) {
            add_msg_if_player( m_bad, _( "Your veins itch." ) );
        }
    } else if( id == effect_paincysts ) {
        if( one_in( 3072 ) ) {
            add_msg_if_player( m_bad, _( "Your muscles feel like they're knotted and tired." ) );
        }
    } else if( id == effect_datura ) {
        if( dur > 100_minutes && focus_pool >= 1 && one_in( 24 ) ) {
            focus_pool--;
        }
        if( dur > 200_minutes && one_in( 48 ) && get_stim() < 20 ) {
            mod_stim( 1 );
        }
        if( dur > 300_minutes && focus_pool >= 1 && one_in( 12 ) ) {
            focus_pool--;
        }
        if( dur > 400_minutes && one_in( 384 ) ) {
            mod_pain( rng( -1, -8 ) );
        }
        if( ( !has_effect( effect_hallu ) ) && ( dur > 500_minutes && one_in( 24 ) ) ) {
            add_effect( effect_hallu, 6_hours );
        }
        if( dur > 600_minutes && one_in( 768 ) ) {
            mod_pain( rng( -3, -24 ) );
            if( dur > 800_minutes && one_in( 16 ) ) {
                add_msg_if_player( m_bad,
                                   _( "You're experiencing loss of basic motor skills and blurred vision.  Your mind recoils in horror, unable to communicate with your spinal column." ) );
                add_msg_if_player( m_bad, _( "You stagger and fall!" ) );
                add_effect( effect_downed, rng( 1_turns, 4_turns ), bodypart_str_id::NULL_ID(), 0, true );
                if( one_in( 8 ) || x_in_y( character_effects::vomit_mod( *this ), 10 ) ) {
                    vomit();
                }
            }
        }
        if( dur > 700_minutes && focus_pool >= 1 ) {
            focus_pool--;
        }
        if( dur > 800_minutes && one_in( 1536 ) ) {
            add_effect( effect_visuals, rng( 4_minutes, 20_minutes ) );
            mod_pain( rng( -8, -40 ) );
        }
        if( dur > 1200_minutes && one_in( 1536 ) ) {
            add_msg_if_player( m_bad, _( "There's some kind of big machine in the sky." ) );
            add_effect( effect_visuals, rng( 8_minutes, 40_minutes ) );
            if( one_in( 32 ) ) {
                add_msg_if_player( m_bad, _( "It's some kind of electric snake, coming right at you!" ) );
                mod_pain( rng( 4, 40 ) );
                vomit();
            }
        }
        if( dur > 1400_minutes && one_in( 768 ) ) {
            add_msg_if_player( m_bad,
                               _( "Order us some golf shoes, otherwise we'll never get out of this place alive." ) );
            add_effect( effect_visuals, rng( 40_minutes, 200_minutes ) );
            if( one_in( 8 ) ) {
                add_msg_if_player( m_bad,
                                   _( "The possibility of physical and mental collapse is now very real." ) );
                if( one_in( 2 ) || x_in_y( character_effects::vomit_mod( *this ), 10 ) ) {
                    add_msg_if_player( m_bad, _( "No one should be asked to handle this trip." ) );
                    vomit();
                    mod_pain( rng( 8, 40 ) );
                }
            }
        }

        if( dur > 1800_minutes && one_in( 300 * 512 ) ) {
            if( !has_trait( trait_NOPAIN ) ) {
                add_msg_if_player( m_bad,
                                   _( "Your heart spasms painfully and stops, dragging you back to reality as you die." ) );
            } else {
                add_msg_if_player(
                    _( "You dissolve into beautiful paroxysms of energy.  Life fades from your nebulae and you are no more." ) );
            }
            g->events().send<event_type::dies_from_drug_overdose>( getID(), id );
            set_part_hp_cur( bodypart_id( "torso" ), 0 );
        }
    } else if( id == effect_grabbed ) {
        set_num_blocks_bonus( get_num_blocks_bonus() - 1 );
        int zed_number = 0;
        for( auto &dest : g->m.points_in_radius( pos(), 1, 0 ) ) {
            const monster *const mon = g->critter_at<monster>( dest );
            if( mon && mon->has_effect( effect_grabbing ) ) {
                zed_number += mon->get_grab_strength();
            }
        }
        if( zed_number > 0 ) {
            //If intensity isn't pass the cap, average it with # of zeds
            add_effect( effect_grabbed, 2_turns, body_part_torso, ( intense + zed_number ) / 2 );
        }
    } else if( id == effect_bite ) {
        bool recovered = false;
        /* Recovery chances, use binomial distributions if balancing here. Healing in the bite
         * stage provides additional benefits, so both the bite stage chance of healing and
         * the cumulative chances for spontaneous healing are both given.
         * Cumulative heal chances for the bite + infection stages:
         * -200 health - 38.6%
         *    0 health - 46.8%
         *  200 health - 53.7%
         *
         * Heal chances in the bite stage:
         * -200 health - 23.4%
         *    0 health - 28.3%
         *  200 health - 32.9%
         *
         * Cumulative heal chances the bite + infection stages with the resistant mutation:
         * -200 health - 82.6%
         *    0 health - 84.5%
         *  200 health - 86.1%
         *
         * Heal chances in the bite stage with the resistant mutation:
         * -200 health - 60.7%
         *    0 health - 63.2%
         *  200 health - 65.6%
         */
        if( dur % 10_turns == 0_turns )  {
            int recover_factor = 100;
            if( has_effect( effect_recover ) ) {
                recover_factor -= get_effect_dur( effect_recover ) / 1_hours;
            }
            if( has_trait( trait_INFRESIST ) ) {
                recover_factor += 200;
            }
            if( has_effect( effect_panacea ) ) {
                recover_factor = 648000; //panacea is a guaranteed cure
            } else if( has_effect( effect_strong_antibiotic ) ) {
                recover_factor += 400;
            } else if( has_effect( effect_antibiotic ) ) {
                recover_factor += 200;
            } else if( has_effect( effect_weak_antibiotic ) ) {
                recover_factor += 100;
            }
            recover_factor += get_healthy() / 10;

            if( x_in_y( recover_factor, 648000 ) ) {
                //~ %s is bodypart name.
                add_msg_if_player( m_good, _( "Your %s wound begins to feel better!" ),
                                   body_part_name( bp ) );
                // Set ourselves up for removal
                it.set_duration( 0_turns );
                recovered = true;
            }
        }
        if( !recovered ) {
            // Move up to infection
            // Infection resistance can keep us in bite phase arbitrarily long
            if( dur > 6_hours && !has_trait( trait_INFRESIST ) ) {
                add_effect( effect_infected, 1_turns, convert_bp( bp ) );
                // Set ourselves up for removal
                it.set_duration( 0_turns );
            } else if( has_effect( effect_strong_antibiotic ) ) {
                // Strong antibiotic reverses progress
                it.mod_duration( -1_turns );
            } else if( has_effect( effect_antibiotic ) ) {
                // Normal antibiotic prevents progression
            } else if( has_effect( effect_weak_antibiotic ) ) {
                if( calendar::once_every( 4_turns ) ) {
                    // Weak antibiotic slows down to a quarter
                    it.mod_duration( 1_turns );
                }
            } else {
                it.mod_duration( 1_turns );
            }
        }
    } else if( id == effect_infected ) {
        bool recovered = false;
        // Recovery chance, use binomial distributions if balancing here.
        // See "bite" for balancing notes on this.
        if( dur % 10_turns == 0_turns )  {
            int recover_factor = 100;
            if( has_effect( effect_recover ) ) {
                recover_factor -= get_effect_dur( effect_recover ) / 1_hours;
            }
            if( has_trait( trait_INFRESIST ) ) {
                recover_factor += 200;
            }
            if( has_effect( effect_panacea ) ) {
                recover_factor = 5184000;
            } else if( has_effect( effect_strong_antibiotic ) ) {
                recover_factor += 400;
            } else if( has_effect( effect_antibiotic ) ) {
                recover_factor += 200;
            } else if( has_effect( effect_weak_antibiotic ) ) {
                recover_factor += 100;
            }
            recover_factor += get_healthy() / 10;

            if( x_in_y( recover_factor, 5184000 ) ) {
                //~ %s is bodypart name.
                add_msg_if_player( m_good, _( "Your %s wound begins to feel better!" ),
                                   body_part_name( bp ) );
                add_effect( effect_recover, 4 * dur );
                // Set ourselves up for removal
                it.set_duration( 0_turns );
                recovered = true;
            }
        }
        if( !recovered ) {
            // Don't kill if the player is on antibiotics
            if( has_effect( effect_strong_antibiotic ) ) {
                it.mod_duration( -1_turns );
            } else if( has_effect( effect_antibiotic ) ) {
                // No progression
            } else if( has_effect( effect_weak_antibiotic ) ) {
                if( calendar::once_every( 4_turns ) ) {
                    it.mod_duration( 1_turns );
                }
            } else if( dur > 1_days ) {
                add_msg_if_player( m_bad, _( "You succumb to the infection." ) );
                g->events().send<event_type::dies_of_infection>( getID() );
                hurtall( 500, nullptr );
            } else {
                it.mod_duration( 1_turns );
            }
        }
    } else if( id == effect_lying_down ) {
        set_moves( 0 );
        if( character_funcs::roll_can_sleep( *this ) ) {
            fall_asleep();
            // Set ourselves up for removal
            it.set_duration( 0_turns );
        }
        if( dur == 1_turns && !sleeping ) {
            add_msg_if_player( _( "You try to sleep, but can't…" ) );
        }
    } else if( id == effect_sleep ) {
        set_moves( 0 );
        if( is_avatar() ) {
            inp_mngr.pump_events();
        }

        if( has_effect( effect_narcosis ) && get_fatigue() <= 25 ) {
            set_fatigue( 25 ); //Prevent us from waking up naturally while under anesthesia
        }

        if( get_fatigue() <= 10 && !has_effect( effect_narcosis ) && !is_hibernating() ) {
            // Mycus folks upgrade once per night of sleep.
            if( has_trait( trait_THRESH_MYCUS ) ) {
                mutate_category( mutation_category_id( "MYCUS" ) );
            }

            // Check mutation category strengths to see if we're mutated enough to get a dream
            mutation_category_id highcat = get_highest_category();
            int highest = mutation_category_level[highcat];

            // Determine the strength of effects or dreams based upon category strength
            int strength = 0; // Category too weak for any effect or dream
            if( crossed_threshold() ) {
                strength = 4; // Post-human.
            } else if( highest >= 20 && highest < 35 ) {
                strength = 1; // Low strength
            } else if( highest >= 35 && highest < 50 ) {
                strength = 2; // Medium strength
            } else if( highest >= 50 ) {
                strength = 3; // High strength
            }

            // Get a dream if category strength is high enough.
            if( strength != 0 ) {
                // Select a dream
                std::string dream = dreams::get_random_for_category( highcat, strength );
                if( !dream.empty() && x_in_y( strength + 5, 10 ) ) {
                    add_msg( dream );
                }
            }
            set_fatigue( 0 );
            if( get_sleep_deprivation() < sleep_deprivation_levels::harmless ) {
                add_msg_if_player( m_good, _( "You feel well rested." ) );
            } else {
                add_msg_if_player( m_warning,
                                   _( "You feel physically rested, but you haven't been able to catch up on your missed sleep yet." ) );
            }
            it.set_duration( 0_seconds );
        }

        // TODO: Move this to update_needs when NPCs can mutate
        if( calendar::once_every( 10_minutes ) && ( has_trait( trait_CHLOROMORPH ) ||
                has_trait( trait_M_SKIN3 ) || has_trait( trait_WATERSLEEP ) ) &&
            g->m.is_outside( pos() ) ) {
            if( has_trait( trait_CHLOROMORPH ) ) {
                // Hunger and thirst fall before your Chloromorphic physiology!
                if( g->natural_light_level( posz() ) >= 12 &&
                    get_weather().weather_id->sun_intensity >= sun_intensity_type::light ) {
                    if( get_stored_kcal() < max_stored_kcal() - 50 ) {
                        mod_stored_kcal( 50 );
                    }
                    if( get_thirst() >= thirst_levels::turgid ) {
                        mod_thirst( -5 );
                    }
                }
            }
            if( has_trait( trait_M_SKIN3 ) ) {
                // Spores happen!
                if( g->m.has_flag_ter_or_furn( "FUNGUS", pos() ) ) {
                    if( get_fatigue() >= 0 ) {
                        mod_fatigue( -5 ); // Local guides need less sleep on fungal soil
                    }
                    if( calendar::once_every( 1_hours ) ) {
                        spores(); // spawn some P O O F Y   B O I S
                    }
                }
            }
            if( has_trait( trait_WATERSLEEP ) ) {
                mod_fatigue( -3 ); // Fish sleep less in water
            }
        }

        bool woke_up = false;
        int tirednessVal = rng( 5, 200 ) + rng( 0, std::abs( get_fatigue() * 2 * 5 ) );
        if( !is_blind() && !has_effect( effect_narcosis ) ) {
            if( !has_trait(
                    trait_SEESLEEP ) ) { // People who can see while sleeping are acclimated to the light.
                if( has_trait( trait_HEAVYSLEEPER2 ) && !has_trait( trait_HIBERNATE ) ) {
                    // So you can too sleep through noon
                    if( ( tirednessVal * 1.25 ) < g->m.ambient_light_at( pos() ) && ( get_fatigue() < 10 ||
                            one_in( get_fatigue() / 2 ) ) ) {
                        add_msg_if_player( _( "It's too bright to sleep." ) );
                        // Set ourselves up for removal
                        it.set_duration( 0_turns );
                        woke_up = true;
                    }
                    // Ursine hibernators would likely do so indoors.  Plants, though, might be in the sun.
                } else if( has_trait( trait_HIBERNATE ) ) {
                    if( ( tirednessVal * 5 ) < g->m.ambient_light_at( pos() ) && ( get_fatigue() < 10 ||
                            one_in( get_fatigue() / 2 ) ) ) {
                        add_msg_if_player( _( "It's too bright to sleep." ) );
                        // Set ourselves up for removal
                        it.set_duration( 0_turns );
                        woke_up = true;
                    }
                } else if( tirednessVal < g->m.ambient_light_at( pos() ) && ( get_fatigue() < 10 ||
                           one_in( get_fatigue() / 2 ) ) ) {
                    add_msg_if_player( _( "It's too bright to sleep." ) );
                    // Set ourselves up for removal
                    it.set_duration( 0_turns );
                    woke_up = true;
                }
            } else if( has_active_mutation( trait_SEESLEEP ) ) {
                Creature *hostile_critter = g->is_hostile_very_close();
                if( hostile_critter != nullptr ) {
                    add_msg_if_player( _( "You see %s approaching!" ),
                                       hostile_critter->disp_name() );
                    it.set_duration( 0_turns );
                    woke_up = true;
                }
            }
        }

        // Have we already woken up?
        if( !woke_up && !has_effect( effect_narcosis ) ) {
            // Cold or heat may wake you up.
            // Player will sleep through cold or heat if fatigued enough
            for( const auto &pr : get_body() ) {
                int temp_cur = pr.second.get_temp_cur();
                if( temp_cur < BODYTEMP_VERY_COLD - get_fatigue() / 2 ) {
                    if( one_in( 30000 ) ) {
                        add_msg_if_player( _( "You toss and turn trying to keep warm." ) );
                    }
                    if( temp_cur < BODYTEMP_FREEZING - get_fatigue() / 2 ||
                        one_in( temp_cur * 6 + 30000 ) ) {
                        add_msg_if_player( m_bad, _( "It's too cold to sleep." ) );
                        // Set ourselves up for removal
                        it.set_duration( 0_turns );
                        woke_up = true;
                        break;
                    }
                } else if( temp_cur > BODYTEMP_VERY_HOT + get_fatigue() / 2 ) {
                    if( one_in( 30000 ) ) {
                        add_msg_if_player( _( "You toss and turn in the heat." ) );
                    }
                    if( temp_cur > BODYTEMP_SCORCHING + get_fatigue() / 2 ||
                        one_in( 90000 - temp_cur ) ) {
                        add_msg_if_player( m_bad, _( "It's too hot to sleep." ) );
                        // Set ourselves up for removal
                        it.set_duration( 0_turns );
                        woke_up = true;
                        break;
                    }
                }
            }
            if( ( ( has_trait( trait_SCHIZOPHRENIC ) || has_artifact_with( AEP_SCHIZO ) ) &&
                  one_in( 43200 ) && is_player() ) ) {
                if( one_in( 2 ) ) {
                    sound_hallu();
                } else {
                    int max_count = rng( 1, 3 );
                    int count = 0;
                    for( const tripoint &mp : g->m.points_in_radius( pos(), 1 ) ) {
                        if( mp == pos() ) {
                            continue;
                        }
                        if( g->m.has_flag( "FLAT", mp ) &&
                            g->m.pl_sees( mp, 2 ) ) {
                            g->spawn_hallucination( mp );
                            if( ++count > max_count ) {
                                break;
                            }
                        }
                    }
                }
                it.set_duration( 0_turns );
                woke_up = true;
            }
        }

        // A bit of a hack: check if we are about to wake up for any reason, including regular
        // timing out of sleep
        if( dur == 1_turns || woke_up ) {
            wake_up();
        }
    } else if( id == effect_alarm_clock ) {
        if( in_sleep_state() ) {
            const bool asleep = has_effect( effect_sleep );
            if( has_bionic( bio_infolink ) ) {
                if( dur == 1_turns ) {
                    if( !asleep ) {
                        add_msg_if_player( _( "Your internal chronometer went off and you haven't slept a wink." ) );
                        activity->set_to_null();
                    } else {
                        // Secure the flag before wake_up() clears the effect
                        bool slept_through = has_effect( effect_slept_through_alarm );
                        wake_up();
                        if( slept_through ) {
                            add_msg_if_player( _( "Your internal chronometer finally wakes you up." ) );
                        } else {
                            add_msg_if_player( _( "Your internal chronometer wakes you up." ) );
                        }
                    }
                }
            } else {
                if( asleep && dur == 1_turns ) {
                    if( !has_effect( effect_slept_through_alarm ) ) {
                        add_effect( effect_slept_through_alarm, 1_turns, bodypart_str_id::NULL_ID() );
                    }
                    // 10 minute automatic snooze
                    it.mod_duration( 10_minutes );
                } else if( dur == 2_turns ) {
                    // let the sound code handle the wake-up part
                    sounds::sound( pos(), 16, sounds::sound_t::alarm, _( "beep-beep-beep!" ), false, "tool",
                                   "alarm_clock" );
                }
            }
        } else {
            if( dur == 1_turns ) {
                if( is_avatar() && has_alarm_clock() ) {
                    sounds::sound( pos(), 16, sounds::sound_t::alarm, _( "beep-beep-beep!" ), false, "tool",
                                   "alarm_clock" );
                    const std::string alarm = _( "Your alarm is going off." );
                    g->cancel_activity_or_ignore_query( distraction_type::alert, alarm );
                    add_msg( _( "Your alarm went off." ) );
                }
            }
        }
    } else if( id == effect_disabled ) {
        if( get_part_hp_cur( convert_bp( bp ) ) >= get_part_hp_max( convert_bp( bp ) ) ) {
            remove_effect( effect_disabled );
        }
    } else if( id == effect_panacea ) {
        // restore health all body parts, dramatically reduce pain
        healall( 1 );
        mod_pain( -10 );
    }
}
