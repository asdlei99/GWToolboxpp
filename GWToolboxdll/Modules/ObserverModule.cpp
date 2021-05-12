#include "stdafx.h"

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Npc.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWToolbox.h>
#include <GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Modules/ObserverModule.h>

#include <Logger.h>

#define INI_FILENAME L"observerlog.ini"
#define IniSection "observer"


namespace ObserverLabel {
    const char* Profession = "Prf";
    const char* Name = "Name";
    const char* PlayerGuildTag = "Tag";
    const char* PlayerGuildRating = "Rtg";
    const char* PlayerGuildRank = "Rnk";
    const char* Kills = "K";
    const char* Deaths = "D";
    const char* KDR = "KDR";
    const char* Cancels = "C";
    const char* Interrupts = "I";
    const char* Knockdowns = "Kd";
    const char* Finishes = "F";
    const char* AttacksReceivedFromOtherParties = "A-";
    const char* AttacksDealtToOtherParties = "A+";
    const char* CritsReceivedFromOtherParties = "Cr-";
    const char* CritsDealToOtherParties = "Cr+";
    const char* SkillsReceivedFromOtherParties = "Sk-";
    const char* SkillsUsedOnOtherParties = "Sk+";
}; // namespace ObserverLabels


// Destructor
ObserverModule::~ObserverModule() {
    Reset();
}

void ObserverModule::Initialize()
{
    ToolboxModule::Initialize();

    is_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    is_observer = GW::Map::GetIsObserving();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &InstanceLoadInfo_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet) -> void {
            HandleInstanceLoadInfo(status, packet);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(
        &AgentState_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::AgentState* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }
            HandleAgentState(packet->agent_id, packet->state);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(
        &AgentAdd_Entry,
            [this](GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }
            HandleAgentAdd(packet->agent_id);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentProjectileLaunched>(
        &AgentProjectileLaunched_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::AgentProjectileLaunched* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }
            HandleAgentProjectileLaunched(packet->agent_id, (packet->is_attack == 0) ? false : true);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(
        &GenericModifier_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericModifier* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            const uint32_t value_id = packet->type;
            const uint32_t caster_id = packet->cause_id;
            const uint32_t target_id = packet->target_id;
            const float value = packet->value;
            const bool no_target = false;
            HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
        }
    );

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t value = packet->value;
            const bool no_target = false;
            HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValue_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t target_id = NO_AGENT;
            const uint32_t value = packet->value;
            const float f_value = 0;
            const bool no_target = true;
            HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericFloat>(
        &GenericFloat_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericFloat* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            const uint32_t value_id = packet->type;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t target_id = NO_AGENT;
            const uint32_t i_value = 0;
            const float value = packet->value;
            const bool no_target = true;
            HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
        }
    );

    if (IsActive() && !observer_session_initialized) {
        InitializeObserverSession();
    }
}

void ObserverModule::Terminate() {
    Reset();
    ToolboxModule::Terminate();
}


// Is the Module actively tracking agents?
const bool ObserverModule::IsActive() {
    // an observer match is considered an explorable area
    return is_enabled && is_explorable && (enable_in_explorable_areas || is_observer);
}


// Handle InstanceLoadInfo Packet
void ObserverModule::HandleInstanceLoadInfo(GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo *packet) {
    UNREFERENCED_PARAMETER(status);
    UNREFERENCED_PARAMETER(packet);

    const bool was_active = IsActive();

    is_explorable = packet->is_explorable;
    is_observer = packet->is_observer;

    const bool is_active = IsActive();

    if (is_active) {
        Reset();
        InitializeObserverSession();
    } else {
        // mark false so we initialize next time we load a session
        observer_session_initialized = false;
    }
}


// Handle a GenericPacket of type float
void ObserverModule::HandleGenericPacket(const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const float value, const bool no_target) {
    UNREFERENCED_PARAMETER(no_target);

    switch (value_id) {
        case GW::Packet::StoC::GenericValueID::damage:
            HandleDamageDone(caster_id, target_id, value, false);
            break;

        case GW::Packet::StoC::GenericValueID::critical:
            HandleDamageDone(caster_id, target_id, value, true);
            break;

        case GW::Packet::StoC::GenericValueID::armorignoring:
            HandleDamageDone(caster_id, target_id, value, false);
            break;

        case GW::Packet::StoC::GenericValueID::knocked_down:
            HandleKnockedDown(caster_id, value);
            break;
    };
}

// Handle a Generic Packet of type uint32_t
void ObserverModule::HandleGenericPacket(const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const uint32_t value, const bool no_target) {

    switch (value_id) {
        case GW::Packet::StoC::GenericValueID::melee_attack_finished:
            HandleAttackFinished(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::attack_stopped:
            HandleAttackStopped(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::attack_started: {
            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id;     // 0
            } else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            // handle
            HandleAttackStarted(_caster_id, _target_id);
            break;
        }

        case GW::Packet::StoC::GenericValueID::interrupted:
            HandleInterrupted(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::attack_skill_finished:
            HandleAttackSkillFinished(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::instant_skill_activated:
            HandleInstantSkillActivated(caster_id, target_id, value);
            break;

        case GW::Packet::StoC::GenericValueID::attack_skill_stopped:
            HandleAttackSkillStopped(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::attack_skill_activated: {
            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id;     // 0
            } else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            // handle
            HandleAttackSkillStarted(_caster_id, _target_id, value);
            break;
        }

        case GW::Packet::StoC::GenericValueID::skill_finished:
            HandleSkillFinished(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::skill_stopped:
            HandleSkillStopped(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::skill_activated: {
            // TODO: do location effecs cause entry here?
            // if so, Isle of the Dead, Burning Isle, Isle of Meditation,
            // Frozen Isle, Isle of Weeping Stone, etc... might slow down
            // our application by coming in here 10,000 times
            // TODO: verify whether we need to check for NO_AGENT on caster,
            // or for no living agent...

            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id;     // 0
            } else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            HandleSkillActivated(_caster_id, _target_id, value);
            break;
        }
    };
}



// Handle AgentState Packet
// Fired when the server notifies us of an agent state change
// Can tell us if the agent has just died
void ObserverModule::HandleAgentState(const uint32_t agent_id, const uint32_t state) {
    ObservableAgent* observable_agent = GetObservableAgentById(agent_id);

    if (!observable_agent) return;

    // 16 = dead
    if (state != 16) return;

    // notify the player
    observable_agent->stats.HandleDeath();

    // notify the party
    if (observable_agent->party_id != NO_PARTY) {
        ObservableParty* party = GetObservablePartyById(observable_agent->party_id);
        if (party) {
            party->stats.HandleDeath();

            // credit the kill to the last-hitter and their party,
            // only if the dead guy belongs to a party
            if (observable_agent->last_hit_by != NO_AGENT) {
                ObservableAgent* killer = GetObservableAgentById(observable_agent->last_hit_by);
                if (killer) {
                    killer->stats.HandleKill();
                    uint32_t killer_party_id = killer->party_id;
                    if (killer_party_id != NO_PARTY) {
                        ObservableParty* killer_party = GetObservablePartyById(killer_party_id);
                        if (killer_party) killer_party->stats.HandleKill();
                    }
                }
            }
        }
    }
}


// Handle DamageDone (GenericModifier float Packet)
void ObserverModule::HandleDamageDone(const uint32_t caster_id, const uint32_t target_id, const float amount_pc, const bool is_crit) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    ObservableAgent* target = GetObservableAgentById(target_id);

    // get last hit to credit the kill
    if (target && caster->party_id != NO_PARTY && (amount_pc < 0)) {
        target->last_hit_by = caster->agent_id;
    }

    if (is_crit) {
        ObservableParty* caster_party = nullptr;
        ObservableParty* target_party = nullptr;
        if (caster) caster_party = GetObservablePartyById(caster->party_id);
        if (target) target_party = GetObservablePartyById(target->party_id);

        // notify the caster
        if (caster) {
            caster->stats.total_crits_dealt += 1;
            if (target_party) caster->stats.total_party_crits_dealt += 1;
        }

        // notify the caster_party
        if (caster_party) {
            caster_party->stats.total_crits_dealt += 1;
            if (target_party) caster_party->stats.total_party_crits_dealt += 1;
        }

        // notify the target
        if (target) {
            target->stats.total_crits_received += 1;
            if (caster_party) target->stats.total_party_crits_received += 1;
        }

        // notify the target_party
        if (target_party) {
            target_party->stats.total_crits_received += 1;
            if (caster_party) target_party->stats.total_party_crits_received += 1;
        }
    }
}


// Handle AgentAdd Packet
// Fired when an Agent is to be loaded into memory
void ObserverModule::HandleAgentAdd(const uint32_t agent_id) {
    // queue update parties
    UNREFERENCED_PARAMETER(agent_id);
    party_sync_timer = TIMER_INIT();
}


// Handle AgentProjectileLaunched Packet
// If is_attack, signals that an attack has stopped
void ObserverModule::HandleAgentProjectileLaunched(const uint32_t agent_id, const bool is_attack) {
    if (!is_attack) return;
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster) return;
    if (!caster->current_target_action) return;
    ReduceAction(caster, *caster->current_target_action, ActionStage::Finished);
}


// Handle AttackFinished Packet
void ObserverModule::HandleAttackFinished(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster) return;
    if (!caster->current_target_action) return;
    ReduceAction(caster, *caster->current_target_action, ActionStage::Finished);
}

// Handle AttackStopped Packet
void ObserverModule::HandleAttackStopped(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster) return;
    if (!caster->current_target_action) return;
    ReduceAction(caster, *caster->current_target_action, ActionStage::Stopped);
}


// Handle AttackStarted Packet
void ObserverModule::HandleAttackStarted(const uint32_t caster_id, const uint32_t target_id) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    if (!caster) return;
    const TargetAction* action = new TargetAction(caster_id, target_id, true, false, NO_SKILL);
    ReduceAction(caster, *action, ActionStage::Started);
}


// Handle Interrupted Packet
void ObserverModule::HandleInterrupted(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
    ReduceAction(caster, *caster->current_target_action, ActionStage::Interrupted);
}

// Handle Attack SkillFinished Packet
void ObserverModule::HandleAttackSkillFinished(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
    ReduceAction(caster, *caster->current_target_action, ActionStage::Finished);
}


// Handle Attack SkillStopped Packet
void ObserverModule::HandleAttackSkillStopped(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
    ReduceAction(caster, *caster->current_target_action, ActionStage::Stopped);
}



// Handle InstantSkillActivated Packet
void ObserverModule::HandleInstantSkillActivated(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    // assuming there are no instant attack skills...
    const TargetAction* action = new TargetAction(caster_id, target_id, false, true, skill_id);
    ReduceAction(caster, *action, ActionStage::Instant);
    // TODO: move this cleanup better place... (to the ObservableAgent)
    // We can only delete here because we know ReduceAction won't own the action since it's instant...
    delete action;
}


// Handle AttackSkillActivated Packet
void ObserverModule::HandleAttackSkillStarted(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    const TargetAction* action = new TargetAction(caster_id, target_id, true, true, skill_id);
    ReduceAction(caster, *action, ActionStage::Started);
}


// Handle AttackSkillFinished Packet
void ObserverModule::HandleSkillFinished(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
    ReduceAction(caster, *caster->current_target_action, ActionStage::Finished);
}


// Handle SkillFinished Packet
void ObserverModule::HandleSkillStopped(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
    ReduceAction(caster, *caster->current_target_action, ActionStage::Stopped);
}


// Handle SkillActivated Packet
void ObserverModule::HandleSkillActivated(const uint32_t caster_id, const uint32_t target_id, uint32_t skill_id) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    const TargetAction* action = new TargetAction(caster_id, target_id, false, true, skill_id);
    ReduceAction(caster, *action, ActionStage::Started);
}


// Handle KnockedDown Packet
void ObserverModule::HandleKnockedDown(const uint32_t agent_id, const float duration) {
    // notify the agent
    ObservableAgent* agent = GetObservableAgentById(agent_id);
    if (!agent) return;
    agent->stats.knocked_down_count += 1;
    agent->stats.knocked_down_duration += duration;

    // notify the agents party
    ObservableParty* party = GetObservablePartyById(agent->party_id);
    if (!party) return;
    party->stats.knocked_down_count += 1;
    party->stats.knocked_down_duration += 1;
}


// Reduce Action
// Update the state of the module based on an Action & Stage
void ObserverModule::ReduceAction(ObservableAgent* caster, const TargetAction& action, ActionStage stage) {
    if (!caster) return;
    ObserverModule::ObservableAgent* target = GetObservableAgentById(action.target_id);

    // If starting a new action, delete the last stored action & store this new action
    if (stage == ActionStage::Started) {
        // destroy previous action
        if (caster->current_target_action) delete caster->current_target_action;
        // store next action
        caster->current_target_action = &action;
    }

    ObservableParty* caster_party = GetObservablePartyById(caster->party_id);
    ObservableParty* target_party = nullptr;
    if (target) target_party = GetObservablePartyById(target->party_id);

    bool same_team = caster && target && (caster->team_id != NO_TEAM) && (caster->team_id == target->team_id);
    bool same_party = target_party && caster_party && (target_party->party_id == caster_party->party_id);

    // notify caster & caster_party of interrupt
    // @note: the interrupt packet comes after a cancelled packet.
    // if we get interrupted, we've falsely assumed a cancel and must remove it
    // @note: knockdowns's on actions also count as "cancelled", but I don't
    // have a way to track that properly at the moment
    if (stage == ActionStage::Interrupted) {
        if (caster) {
            if (caster->stats.cancelled_count > 0) caster->stats.cancelled_count -= 1;
            caster->stats.interrupted_count += 1;
        }
        if (caster_party) {
            if (caster_party->stats.cancelled_count > 0) caster_party->stats.cancelled_count -= 1;
            caster_party->stats.interrupted_count += 1;
        }
        if (action.is_skill) {
            if (caster) {
                if (caster->stats.cancelled_skills_count > 0) caster->stats.cancelled_skills_count -= 1;
                caster->stats.interrupted_skills_count += 1;
            }
            if (caster_party) {
                if (caster_party->stats.cancelled_skills_count > 0) caster_party->stats.cancelled_skills_count -= 1;
                caster_party->stats.interrupted_skills_count += 1;
            }
        }
    }

    // notify & caster_party caster of cancel
    if (stage == ActionStage::Stopped) {
        if (caster) caster->stats.cancelled_count += 1;
        if (caster_party) caster_party->stats.cancelled_count += 1;
        if (action.is_skill) {
            if (caster) caster->stats.cancelled_skills_count += 1;
            if (caster_party) caster_party->stats.cancelled_skills_count += 1;
        }
    }

    // handle attack
    if (action.is_attack) {
        // update the caster
        if (caster) {
            caster->stats.total_attacks_dealt.Reduce(stage);
            if (target) caster->stats.LazyGetAttacksDealedAgainst(target->agent_id).Reduce(stage);
            // if the target belonged to a party, the caster just attacked that other party
            if (target_party) caster->stats.total_attacks_dealt_to_other_party.Reduce(stage);

        }

        // update the casters party
        if (caster_party) {
            caster_party->stats.total_attacks_dealt.Reduce(stage);
            // if the target belonged to a party, the casters party just attacked that other party
            if (target_party) caster_party->stats.total_attacks_dealt_to_other_party.Reduce(stage);
        }

        // update the target
        if (target) {
            target->stats.total_attacks_received.Reduce(stage);
            if (caster) target->stats.LazyGetAttacksReceivedFrom(caster->agent_id).Reduce(stage);
            // if the caster belonged to a party, the target was just attacked by that other party
            if (caster_party) target->stats.total_attacks_received_from_other_party.Reduce(stage);
        }

        // update the targets party
        if (target_party) {
            target_party->stats.total_attacks_received.Reduce(stage);
            // if the caster belonged to a party, the target_party was just attacked by that other party
            if (caster_party) target_party->stats.total_attacks_received_from_other_party.Reduce(stage);
        }
    }

    // handle skill
    if (action.is_skill) {
        // update skill
        ObservableSkill* skill = GetObservableSkillById(action.skill_id);
        if (!skill) {
            // no skill??! skill needs to be added to GWCA!
            Log::Error((std::string("Unknown skill_id: (")
                + std::to_string(action.skill_id)
                + "). Please contact a developer or create a GitHub issue https://github.com/HasKha/GWToolboxpp/issues/ with this message." ).c_str());
            return;
        }

        // Modify the effective `target` and `target_party`, based on the
        // targetting type of the skill to make stats more intuitive.

        // For example if using Heal Burst on yourself, the packet only
        // includes a caster and no target.
        // here we effectively set the
        // target to the caster.
        uint32_t target_type = skill->gw_skill.target;
        switch (target_type) {
            case (uint32_t) TargetType::no_target: {
                // For stats purposes, consider as "used on self"
                // e.g. Sprint, Whirlwind
                if (action.target_id == NO_AGENT) {
                    target = caster;
                    target_party = caster_party;
                }
                break;
            }
            case (uint32_t) TargetType::anyone: {
                // Ensure we interpret the target correctly
                // e.g. Mirror of Ice, Stone Sheath
                if (action.target_id == NO_AGENT) {
                    target = caster;
                    target_party = caster_party;
                }
                break;
            }
            case (uint32_t) TargetType::ally: {
                // Ensure we interpret the target correctly
                // e.g. Healing Burst
                if (action.target_id == NO_AGENT) {
                    target = caster;
                    target_party = caster_party;
                }
                break;
            }
            case (uint32_t) TargetType::other_ally: {
                // should always have a target
                break;
            }
            case (uint32_t) TargetType::enemy: {
                // should always have a target
                break;
            }
        }
        same_team = caster && target && (caster->team_id != NO_TEAM) && (caster->team_id == target->team_id);
        same_party = target_party && caster_party && (target_party->party_id == caster_party->party_id);

        // notify the skill
        skill->stats.total_usages.Reduce(stage);
        if (target) {
            // target usages
            if (caster == target) skill->stats.total_self_usages.Reduce(stage);
            else skill->stats.total_other_usages.Reduce(stage);

            // team usages
            if (same_team) skill->stats.total_own_team_usages.Reduce(stage);
            else skill->stats.total_own_team_usages.Reduce(stage);
        }
        if (target_party) {
            // party usages
            if (caster_party == target_party) skill->stats.total_own_party_usages.Reduce(stage);
            else skill->stats.total_other_party_usages.Reduce(stage);
        }

        // notify the caster
        if (caster) {
            caster->stats.total_skills_used.Reduce(stage);
            caster->stats.LazyGetSkillUsed(action.skill_id).Reduce(stage);

            // used against a target?
            if (target) {
                // use against agent
                caster->stats.LazyGetSkillUsedOn(target->agent_id, action.skill_id).Reduce(stage);

                // team:
                // same team
                if (same_team) caster->stats.total_skills_used_on_own_team.Reduce(stage);
                // diff team
                else caster->stats.total_skills_used_on_other_teams.Reduce(stage);
            }

            // party:
            if (target_party) {
                // same party
                if (same_party) caster->stats.total_skills_used_on_own_party.Reduce(stage);
                // diff party
                else caster->stats.total_skills_used_on_other_parties.Reduce(stage);
            }
        }

        // notify the caster_party
        if (caster_party) {
            caster_party->stats.total_skills_used.Reduce(stage);

            // team
            // same team
            if (same_team) caster_party->stats.total_skills_used_on_own_team.Reduce(stage);
            // diff team
            else caster_party->stats.total_skills_used_on_other_teams.Reduce(stage);

            // party:
            if (target_party) {
                // same party
                if (same_party) caster_party->stats.total_skills_used_on_own_party.Reduce(stage);
                // diff party
                else caster_party->stats.total_skills_used_on_other_parties.Reduce(stage);
            }
        }

        // notify the target
        if (target) {
            target->stats.total_skills_received.Reduce(stage);
            target->stats.LazyGetSkillReceived(action.skill_id).Reduce(stage);
            // used from a living caster? (redundant)
            if (caster) {
                // use against agent
                target->stats.LazyGetSkillReceivedFrom(caster->agent_id, action.skill_id).Reduce(stage);

                // team
                // same team
                if (same_team) target->stats.total_skills_received_from_own_team.Reduce(stage);
                // diff team
                else target->stats.total_skills_received_from_other_teams.Reduce(stage);
            }

            // party:
            if (caster_party) {
                // same party
                if (same_party) target->stats.total_skills_received_from_own_party.Reduce(stage);
                // diff party
                else target->stats.total_skills_received_from_other_parties.Reduce(stage);
            }
        }

        // notify the target_party
        if (target_party) {
            target_party->stats.total_skills_received.Reduce(stage);

            // team:
            // same team
            if (same_team) target_party->stats.total_skills_received_from_own_team.Reduce(stage);
            // diff team
            else target_party->stats.total_skills_received_from_other_teams.Reduce(stage);

            // party:
            if (caster_party) {
                // same party
                if (same_party) target_party->stats.total_skills_received_from_own_party.Reduce(stage);
                // diff party
                else target_party->stats.total_skills_received_from_other_parties.Reduce(stage);
            }
        }
    }
}

// Module: Reset the Modules state
void ObserverModule::Reset() {
    if (map) {
        delete map;
        map = nullptr;
    }

    // clear guild info
    observable_guild_ids.clear();
    for (const auto& [_, guild] : observable_guilds) if (guild) delete guild;
    observable_guilds.clear();

    // clear skill info
    observable_skill_ids.clear();
    for (const auto& [_, skill] : observable_skills) if (skill) delete skill;
    observable_skills.clear();

    // clear agent info
    observable_agent_ids.clear();
    for (const auto& [_, agent] : observable_agents) if (agent) delete agent;
    observable_agents.clear();

    // clear party info
    observable_party_ids.clear();
    for (const auto& [_, party] : observable_parties) if (party) delete party;
    observable_parties.clear();
}


// Load all known ObservableAgent's on the current map
// Returns false if failed to initialise. True if successful
bool ObserverModule::InitializeObserverSession() {
    // load area
    GW::AreaInfo* map_info = GW::Map::GetCurrentMapInfo();
    if (!map_info) return false;

    // load parties
    if (!SynchroniseParties()) return false;

    // load all other agents
    const GW::AgentArray agents = GW::Agents::GetAgentArray();
    if (!agents.valid()) return false;

    for (GW::Agent* agent : agents) {
        // not found (maybe hasn't loaded in yet)?
        if (!agent) continue;
        // trigger lazy load
        GetObservableAgentById(agent->agent_id);
    }

    // initialise the map
    if (map) delete map;
    map = new ObservableMap(*map_info);

    observer_session_initialized = true;
    return true;
}


// Synchronise known parties in the area
bool ObserverModule::SynchroniseParties() {
    GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (!party_ctx) return false;

    GW::Array<GW::PartyInfo*> parties = party_ctx->parties;
    if (!parties.valid()) return false;

    for (const GW::PartyInfo* party_info : parties) {
        if (!party_info) continue;
        // load and synchronize the party
        ObserverModule::ObservableParty* observable_party = GetObservablePartyByPartyInfo(*party_info);
        if (observable_party) {
            bool party_synchronised = observable_party->SynchroniseParty();
            if (!party_synchronised) return false;
        }
    }

    // success
    return true;
}


// Load settings
void ObserverModule::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
    is_enabled                  = ini->GetBoolValue(Name(), VAR_NAME(is_enabled), true);
    trim_hench_names            = ini->GetBoolValue(Name(), VAR_NAME(trim_hench_names), false);
    enable_in_explorable_areas  = ini->GetBoolValue(Name(), VAR_NAME(enable_in_explorable_areas), false);
}


// Save settings
void ObserverModule::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(is_enabled), is_enabled);
    ini->SetBoolValue(Name(), VAR_NAME(trim_hench_names), trim_hench_names);
    ini->SetBoolValue(Name(), VAR_NAME(enable_in_explorable_areas), enable_in_explorable_areas);
    
}


// Draw internal settings
void ObserverModule::DrawSettingInternal() {
    ImGui::Text("Enable data collection in Observer Mode.");
    ImGui::Text("Disable if not using this feature to avoid using extra CPU and memory in Observer Mode.");
    ImGui::Checkbox("Enabled", &is_enabled);
    ImGui::Checkbox("Trim henchman names", &trim_hench_names);
    ImGui::Checkbox("Enable in all Explorable Areas", &enable_in_explorable_areas);
}


void ObserverModule::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (party_sync_timer == 0) return;
    if (!IsActive()) {
        party_sync_timer = 0;
        return;
    };
    if (TIMER_DIFF(party_sync_timer) > 1000) {
        SynchroniseParties();
        party_sync_timer = 0;
    }
}

// Lazy load an ObservableGuild using a guild_id
ObserverModule::ObservableGuild* ObserverModule::GetObservableGuildById(const uint32_t guild_id) {
    // shortcircuit for agent_id = 0
    if (guild_id == NO_GUILD) return nullptr;

    // lazy load
    auto it = observable_guilds.find(guild_id);

    // found
    if (it != observable_guilds.end()) return it->second;

    // create if active
    if (!IsActive()) return nullptr;
    const GW::GuildArray guilds = GW::GuildMgr::GetGuildArray();
    if (!guilds.valid() && guild_id >= guilds.size()) return nullptr;
    GW::Guild* guild = guilds[guild_id];
    if (!guild) return nullptr;

    ObserverModule::ObservableGuild* observable_guild = CreateObservableGuild(*guild);
    return observable_guild;
}


// Create an ObservableGuild from a GW::Guild and cache it
// Do NOT call this if the Agent already exists, it will cause a memory leak
ObserverModule::ObservableGuild* ObserverModule::CreateObservableGuild(const GW::Guild& guild) {
    // create
    ObserverModule::ObservableGuild* observable_guild = new ObserverModule::ObservableGuild(*this, guild);
    // cache
    observable_guilds.insert({ observable_guild->guild_id, observable_guild });
    observable_guild_ids.push_back(observable_guild->guild_id);
    std::sort(observable_guild_ids.begin(), observable_guild_ids.end());
    return observable_guild;
}



// Lazy load an ObservableAgent using an agent_id
ObserverModule::ObservableAgent* ObserverModule::GetObservableAgentById(const uint32_t agent_id) {
    // shortcircuit for agent_id = 0
    if (agent_id == NO_AGENT) return nullptr;

    // lazy load
    auto it = observable_agents.find(agent_id);

    // found
    if (it != observable_agents.end()) return it->second;

    // create if active
    if (!IsActive()) return nullptr;
    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    if (!agent) return nullptr;

    const GW::AgentLiving* agent_living = agent->GetAsAgentLiving();
    if (!agent_living) return nullptr;

    ObserverModule::ObservableAgent* observable_agent = CreateObservableAgent(*agent_living);
    return observable_agent;
}


// Create an ObservableAgent from a GW::AgentLiving and cache it
// Do NOT call this if the Agent already exists, it will cause a memory leak
ObserverModule::ObservableAgent* ObserverModule::CreateObservableAgent(const GW::AgentLiving& agent_living) {
    // create
    // ensure the guild is loaded...
    GetObservableGuildById(static_cast<uint32_t>(agent_living.tags->guild_id));
    ObserverModule::ObservableAgent* observable_agent = new ObserverModule::ObservableAgent(*this, agent_living);
    // cache
    observable_agents.insert({ observable_agent->agent_id, observable_agent });
    observable_agent_ids.push_back(observable_agent->agent_id);
    std::sort(observable_agent_ids.begin(), observable_agent_ids.end());
    return observable_agent;
}


// Lazy load an ObservableSkill using a skill_id
ObserverModule::ObservableSkill* ObserverModule::GetObservableSkillById(uint32_t skill_id) {
    // short circuit for skill_id = 0
    if (skill_id == NO_SKILL) return nullptr;

    // find
    auto it_existing = observable_skills.find(skill_id);

    // found
    if (it_existing != observable_skills.end()) return it_existing->second;

    // create if active
    if (!IsActive()) return nullptr;
    const GW::Skill& gw_skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    if (gw_skill.skill_id == 0) return nullptr;
    ObservableSkill* skill = CreateObservableSkill(gw_skill);
    return skill;
}



// Create an ObservableSkill from a GW::Skill and cache it
// Do NOT call this is if the Skill already exists, It will cause a memory leak
ObserverModule::ObservableSkill* ObserverModule::CreateObservableSkill(const GW::Skill& gw_skill) {
    // create
    ObservableSkill* observable_skill = new ObservableSkill(*this, gw_skill);
    // cache
    observable_skills.insert({ gw_skill.skill_id, observable_skill });
    observable_skill_ids.push_back(observable_skill->skill_id);
    std::sort(observable_skill_ids.begin(), observable_skill_ids.end());
    return observable_skill;
}


// Lazy load an ObservableParty using a PartyInfo
ObserverModule::ObservableParty* ObserverModule::GetObservablePartyByPartyInfo(const GW::PartyInfo& party_info) {
    // lazy load
    auto it_observable_party = observable_parties.find(party_info.party_id);
    // found
    if (it_observable_party != observable_parties.end()) return it_observable_party->second;

    // create if active
    if (!IsActive()) return nullptr;
    ObserverModule::ObservableParty* observable_party = this->CreateObservableParty(party_info);
    return observable_party;
}


// Lazy load an ObservableParty using a party_id
ObserverModule::ObservableParty* ObserverModule::GetObservablePartyById(const uint32_t party_id) {
    // shortcircuit for party_id = 0
    if (party_id == NO_PARTY) return nullptr;

    // try to find
    const auto it_party = observable_parties.find(party_id);

    // found
    if (it_party != observable_parties.end()) return it_party->second;

    // create if active
    if (!IsActive()) return nullptr;
    const GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (!party_ctx) return nullptr;

    // get all parties
    const GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) return nullptr;

    // check index
    if (party_id >= parties.size()) return nullptr;
    GW::PartyInfo* party_info = parties[party_id];
    if (!party_info) return nullptr;

    // create
    ObservableParty* observable_party = CreateObservableParty(*party_info);

    return observable_party;
}


// Create an ObservableParty and cache it
// Do NOT call this if the party already exists, will cause memory leak
ObserverModule::ObservableParty* ObserverModule::CreateObservableParty(const GW::PartyInfo& party_info) {
    // create
    ObservableParty* observable_party = new ObservableParty(*this, party_info);
    // cache
    observable_parties.insert({ observable_party->party_id, observable_party });
    observable_party_ids.push_back(observable_party->party_id);
    std::sort(observable_party_ids.begin(), observable_party_ids.end());
    return observable_party;
}



// change state based on an actions stage
void ObserverModule::ObservedAction::Reduce(const ObserverModule::ActionStage stage) {
    switch (stage) {
        case ActionStage::Instant:
            started += 1;
            finished += 1;
            break;
        case ActionStage::Started:
            started += 1;
            break;
        case ActionStage::Stopped:
            stopped += 1;
            break;
        case ActionStage::Interrupted:
            stopped -= 1;
            interrupted += 1;
            break;
        case ActionStage::Finished:
            finished += 1;
            break;
    }
}


// fired when the Agent dies
void ObserverModule::SharedStats::HandleDeath() {
    deaths += 1;
    // recalculate kdr
    kdr_pc = (float) kills / deaths;
    // get kdr string
    std::stringstream str;
    str << std::fixed << std::setprecision(2) << kdr_pc;
    kdr_str = str.str();
}


// fired when the agent scores a kill
void ObserverModule::SharedStats::HandleKill() {
    kills += 1;
    // recalculate kdr
    if (deaths < 1)
        kdr_pc = static_cast<float>(kills);
    else
        kdr_pc = static_cast<float>(kills) / deaths;
    // get kdr string
    std::stringstream str;
    str << std::fixed << std::setprecision(2) << kdr_pc;
    kdr_str = str.str();
}


ObserverModule::ObservableAgentStats::~ObservableAgentStats() {
    // attacks received (by agent)
    for (const auto& [_, o_atk] : attacks_received_from_agent) if (o_atk) delete o_atk;
    attacks_received_from_agent.clear();

    // attacks dealed (by agent)
    for (const auto& [_, o_atk] : attacks_dealt_to_agent) if (o_atk) delete o_atk;
    attacks_dealt_to_agent.clear();

    // skills used
    skill_ids_used.clear();
    for (const auto& [_, o_skill] : skills_used) if (o_skill) delete o_skill;
    skills_used.clear();

    // skills received
    skill_ids_received.clear();
    for (const auto& [_, o_skill] : skills_received) if (o_skill) delete o_skill;
    skills_received.clear();

    // skill received (by agent)
    for (auto& [_, skill_ids] : skills_ids_received_by_agent) skill_ids.clear();
    skills_ids_received_by_agent.clear();
    for (auto& [_, agent_skills] : skills_received_from_agent) {
        for (const auto [__, o_skill] : agent_skills) if (o_skill) delete o_skill;
        agent_skills.clear();
    }
    skills_received_from_agent.clear();

    // skill used (by agent)
    for (auto& [_, skill_ids] : skills_ids_used_on_agent) skill_ids.clear();
    for (auto& [_, agent_skills] : skills_used_on_agent) {
        for (const auto [__, o_skill] : agent_skills) if (o_skill) delete o_skill;
        agent_skills.clear();
    }
    skills_ids_used_on_agent.clear();
}


// Get attacks dealed against this agent, by a caster_agent_id
// Lazy initialises the caster_agent_id
ObserverModule::ObservedAction& ObserverModule::ObservableAgentStats::LazyGetAttacksDealedAgainst(const uint32_t target_agent_id) {
    auto it = attacks_dealt_to_agent.find(target_agent_id);
    if (it == attacks_dealt_to_agent.end()) {
        // receiver not registered
        ObservedAction* observed_action = new ObservedAction();
        attacks_dealt_to_agent.insert({target_agent_id, observed_action});
        return *observed_action;
    } else {
        // receiver is already reigstered
        return *it->second;
    }
}


// Get attacks dealed against this agent, by a caster_agent_id
// Lazy initialises the caster_agent_id
ObserverModule::ObservedAction& ObserverModule::ObservableAgentStats::LazyGetAttacksReceivedFrom(const uint32_t caster_agent_id) {
    auto it = attacks_received_from_agent.find(caster_agent_id);
    if (it == attacks_received_from_agent.end()) {
        // attacker not registered
        ObservedAction* observed_action = new ObservedAction();
        attacks_received_from_agent.insert({caster_agent_id, observed_action});
        return *observed_action;
    } else {
        // attacker is already reigstered
        return *it->second;
    }
}


// Get skills used by this agent
// Lazy initialises the skill_id
ObserverModule::ObservedAction& ObserverModule::ObservableAgentStats::LazyGetSkillUsed(const uint32_t skill_id) {
    auto it_skill = skills_used.find(skill_id);
    if (it_skill == skills_used.end()) {
        // skill not registered
        skill_ids_used.push_back(skill_id);
        // re-sort skills
        std::sort(skill_ids_used.begin(), skill_ids_used.end());
        ObservedSkill* observed_skill = new ObservedSkill(skill_id);
        skills_used.insert({skill_id, observed_skill});
        return *observed_skill;
    } else {
        // skill already registered
        return *it_skill->second;
    }
}


// Get skills used received by this agent
// Lazy initialises the skill_id
ObserverModule::ObservedAction& ObserverModule::ObservableAgentStats::LazyGetSkillReceived(const uint32_t skill_id) {
    auto it_skill = skills_received.find(skill_id);
    if (it_skill == skills_received.end()) {
        // skill not registered
        skill_ids_received.push_back(skill_id);
        // re-sort skills
        std::sort(skill_ids_received.begin(), skill_ids_received.end());
        ObservedSkill* observed_skill = new ObservedSkill(skill_id);
        skills_received.insert({skill_id, observed_skill});
        return *observed_skill;
    } else {
        // skill already registered
        return *it_skill->second;
    }
}



// Get a skill received by this agent, from another agent
// Lazy initialises the skill_id and caster_agent_id
ObserverModule::ObservedSkill& ObserverModule::ObservableAgentStats::LazyGetSkillReceivedFrom(const uint32_t caster_agent_id, const uint32_t skill_id) {
    auto it_caster = skills_received_from_agent.find(caster_agent_id);
    if (it_caster == skills_received_from_agent.end()) {
        // receiver and his skills are not registered with this agent
        std::vector<uint32_t> received_skill_ids = {skill_id};
        skills_ids_received_by_agent.insert({ caster_agent_id, received_skill_ids });
        ObservedSkill* observed_skill = new ObservedSkill(skill_id);
        std::unordered_map<uint32_t, ObservedSkill*> received_skills = {{skill_id, observed_skill}};
        skills_received_from_agent.insert({caster_agent_id, received_skills});
        return *observed_skill;
    } else {
        // receiver is registered with this agent
        std::unordered_map<uint32_t, ObservedSkill*>& used_by_caster = it_caster->second;
        auto it_observed_skill = used_by_caster.find(skill_id);
        // does receiver have the skill registered from/against us?
        if (it_observed_skill == used_by_caster.end()) {
            // caster hasn't registered this skill with this agent
            // add & re-sort skill_ids by the caster
            std::vector<uint32_t>& skills_ids_received_by_agent_vec = skills_ids_received_by_agent.find(caster_agent_id)->second;
            skills_ids_received_by_agent_vec.push_back(skill_id);
            // re-sort
            std::sort(skills_ids_received_by_agent_vec.begin(), skills_ids_received_by_agent_vec.end());
            // add the observed skill for the caster
            ObservedSkill* observed_skill = new ObservedSkill(skill_id);
            used_by_caster.insert({skill_id, observed_skill});
            return *observed_skill;
        } else {
            // receivers already registered this skill
            return *(it_observed_skill->second);
        }
    }
}


// Get a skill received by this agent, from another agent
// Lazy initialises the skill_id and caster_agent_id
ObserverModule::ObservedSkill& ObserverModule::ObservableAgentStats::LazyGetSkillUsedOn(const uint32_t target_agent_id, const uint32_t skill_id) {
    auto it_target = skills_used_on_agent.find(target_agent_id);
    if (it_target == skills_used_on_agent.end()) {
        // receiver and his skills are not registered with this agent
        std::vector<uint32_t> used_skill_ids = {skill_id};
        skills_ids_used_on_agent.insert({ target_agent_id, used_skill_ids });
        ObservedSkill* observed_skill = new ObservedSkill(skill_id);
        std::unordered_map<uint32_t, ObservedSkill*> used_skills = {{skill_id, observed_skill}};
        skills_used_on_agent.insert({target_agent_id, used_skills});
        return *observed_skill;
    } else {
        std::unordered_map<uint32_t, ObservedSkill*>& used_on_target = it_target->second;
        // receiver is registered with this agent
        auto it_observed_skill = used_on_target.find(skill_id);
        // does receiver have the skill registered from/against us?
        if (it_observed_skill == used_on_target.end()) {
            // target hasn't registered this skill with this agent
            // add & re-sort skill_ids by the caster
            std::vector<uint32_t>& skills_ids_used_on_agent_vec = skills_ids_used_on_agent.find(target_agent_id)->second;
            skills_ids_used_on_agent_vec.push_back(skill_id);
            // re-sort
            std::sort(skills_ids_used_on_agent_vec.begin(), skills_ids_used_on_agent_vec.end());
            // add the observed skill for the caster
            ObservedSkill* observed_skill = new ObservedSkill(skill_id);
            used_on_target.insert({skill_id, observed_skill});
            return *observed_skill;
        } else {
            // receivers already registered this skill
            return *(it_observed_skill->second);
        }
    }
}


// Constructor
ObserverModule::ObservableParty::ObservableParty(ObserverModule& parent, const GW::PartyInfo& info)
    : parent(parent)
    , party_id(info.party_id) {}



// Destructor
ObserverModule::ObservableParty::~ObservableParty() {
    agent_ids.clear();
}


// Synchronise the ObservableParty with its agents/members
// Does not load Party Allies (others) (Pets, Guild Lord, Bodyguard, ...)
bool ObserverModule::ObservableParty::SynchroniseParty() {
    GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (!party_ctx) return false;

    const GW::PlayerArray& players = GW::Agents::GetPlayerArray();
    if (!players.valid()) return false;

    const GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) return false;

    const GW::PartyInfo* party_info = party_ctx->parties[party_id];
    if (!party_info) return false;
    if (!party_info->players.valid()) return false;

    // load party members:
    // 1. players
    //  1.1 player heroes
    // 2. henchmen

    size_t party_index = 0;

    // ensure agent_ids size
    size_t party_size = party_info->players.size() + party_info->heroes.size() + party_info->henchmen.size();
    if (party_size > agent_ids.size()) {
        // agent_ids is too small
        // add empty agent_ids
        agent_ids.resize(party_size, NO_AGENT);
    } else if (party_size < agent_ids.size()) {
        // agent_ids is too large
        // clear stale agent_ids
        for (size_t i = party_size; i < agent_ids.size(); i += 1) {
            // clear old party member
            ObservableAgent* observable_agent_prev = parent.GetObservableAgentById(agent_ids[i]);
            if (observable_agent_prev) {
                observable_agent_prev->party_id = NO_PARTY;
                observable_agent_prev->party_index = 0;
            }
        }
        agent_ids.resize(party_size, NO_AGENT);
    }

    // fill agent_ids and notify the agents
    for (const GW::PlayerPartyMember& party_player : party_info->players) {
        // notify the player of their party & position
        const GW::Player& player = players[party_player.login_number];
        if (player.agent_id != 0) {
            // if agent_id is 0, the agent either hasn't loaded or has disconnected
            // if the agent has simply disconnected we keep them from agent_ids
            // by avoiding this code block
            if (agent_ids[party_index] != player.agent_id) {
                // clear old party member
                ObservableAgent* observable_player_prev = parent.GetObservableAgentById(agent_ids[party_index]);
                if (observable_player_prev) {
                    observable_player_prev->party_id = NO_PARTY;
                    observable_player_prev->party_index = 0;
                }
            }
            // add new party member
            agent_ids[party_index] = player.agent_id;
            ObservableAgent* observable_player = parent.GetObservableAgentById(player.agent_id);
            if (observable_player) {
                // notify the player of their party & position
                observable_player->party_id = party_id;
                observable_player->party_index = party_index;
            }
        }
        party_index += 1;

        // No heroes in PvP but keeping this for consistency in all explorable areas...
        for (const GW::HeroPartyMember& hero : party_info->heroes) {
            if (hero.owner_player_id == party_player.login_number) {
                if (hero.agent_id != 0) {
                    // out of scope/compass for some reason if agent_id = 0;
                    // just leave the previous entry in our party
                    if (agent_ids[party_index] != hero.agent_id) {
                        // clear old party member
                        ObserverModule::ObservableAgent* observable_hero_prev = parent.GetObservableAgentById(agent_ids[party_index]);
                        if (observable_hero_prev) {
                            observable_hero_prev->party_id = NO_PARTY;
                            observable_hero_prev->party_index = 0;
                        }
                    }
                    // add new party member
                    agent_ids[party_index] = hero.agent_id;
                    ObserverModule::ObservableAgent* observable_hero = parent.GetObservableAgentById(hero.agent_id);
                    if (observable_hero) {
                        // notify the hero of their party & position
                        observable_hero->party_id = party_id;
                        observable_hero->party_index = party_index;
                    }
                }
            }
            party_index += 1;
        }
    }

    for (const GW::HenchmanPartyMember& hench : party_info->henchmen) {
        if (hench.agent_id != 0) {
            // out of scope/compass for some reason if agent_id = 0;
            // just leave the previous entry in our party
            if (agent_ids[party_index] != hench.agent_id) {
                // clear old party member
                ObserverModule::ObservableAgent* observable_hench_prev = parent.GetObservableAgentById(agent_ids[party_index]);
                if (observable_hench_prev) {
                    observable_hench_prev->party_id = NO_PARTY;
                    observable_hench_prev->party_index = 0;
                }
            }
            // add new party member
            agent_ids[party_index] = hench.agent_id;
            ObserverModule::ObservableAgent* observable_hench = parent.GetObservableAgentById(hench.agent_id);
            if (observable_hench) {
                // notify the henchman of their party & position
                observable_hench->party_id = party_id;
                observable_hench->party_index = party_index;
            }
        }
        party_index += 1;
    }

    // infer teams name from first players guild
    // TODO: retrieve this information from memory instead of inferring it
    // note: this won't be accurate in HA where the teams name isn't simply
    // player 0's guild
    guild_id = NO_GUILD;
    name = "";
    display_name = "";
    rank = NO_RANK;
    rank_str = "";
    rating = NO_RATING;
    if (agent_ids.size() > 0) {
        ObservableAgent* agent0 = parent.GetObservableAgentById(agent_ids[0]);
        if (agent0) {
            ObservableGuild* guild = parent.GetObservableGuildById(agent0->guild_id);
            if (guild) {
                guild_id = guild->guild_id;
                name = guild->name;
                rank = guild->rank;
                rank_str = (guild->rank == NO_RANK) ? "N/A" : std::to_string(guild->rank);
                rating = guild->rating;
                display_name = guild->name + " [" + guild->tag + "]";
            } else {
                name = agent0->SanitizedName() + "'s team";
                display_name = agent0->SanitizedName() + "'s team";
            }
        }
    }

    // success
    return true;
}


// Constructor
ObserverModule::ObservableSkill::ObservableSkill(ObserverModule& parent, const GW::Skill& _gw_skill)
    : parent(parent)
    , gw_skill(_gw_skill)
{
    skill_id = _gw_skill.skill_id;
    // initialize the name asynchronously here
    if (!name_enc[0] && GW::UI::UInt32ToEncStr(gw_skill.name, name_enc, 16))
        GW::UI::AsyncDecodeStr(name_enc, name_dec, 256);
}


// Name of the skill
const std::string ObserverModule::ObservableSkill::Name() {
    // cached?
    if (_name.length() > 0) return _name;
    std::string name = GuiUtils::WStringToString(DecName());
    // not ready to cache
    if (name.length() == 0) return name;
    // ready to cache
    _name = name;
    return _name;
}


// Name + skill_id of the Skill
const std::string ObserverModule::ObservableSkill::DebugName() {
    return std::string("(") + std::to_string(skill_id) + ") \"" + GuiUtils::WStringToString(DecName()) + "\"";
}


// Constructor
ObserverModule::ObservableGuild::ObservableGuild(ObserverModule& parent, const GW::Guild& guild)
    : parent(parent)
    , guild_id(guild.index)
    , key(guild.key)
    , name(GuiUtils::WStringToString(guild.name))
    , tag(GuiUtils::WStringToString(guild.tag))
    , wrapped_tag("[" + tag + "]")
    , rank(guild.rank)
    , rating(guild.rating)
    , faction(guild.faction)
    , faction_point(guild.faction_point)
    , qualifier_point(guild.qualifier_point)
    , cape_trim(guild.cape_trim)
{
    //
}


// Constructor
ObserverModule::ObservableAgent::ObservableAgent(ObserverModule& parent, const GW::AgentLiving& agent_living)
    : parent(parent)
    , state(agent_living.model_state)
    , guild_id(static_cast<uint32_t>(agent_living.tags->guild_id))
    , agent_id(agent_living.agent_id)
    , team_id(agent_living.team_id)
    , primary((GW::Constants::Profession) agent_living.primary)
    , secondary((GW::Constants::Profession) agent_living.secondary)
    , is_npc(agent_living.IsNPC())
    , is_player(agent_living.IsPlayer())
    , login_number(agent_living.login_number)
{
    // async initialise the agents name now because we probably want it later
    GW::Agents::AsyncGetAgentName(&agent_living, _raw_name_w);

    if (primary != GW::Constants::Profession::None) {
        std::string prof = GW::Constants::GetProfessionAcronym(primary);
        if (secondary != GW::Constants::Profession::None) {
            std::string s_prof = GW::Constants::GetProfessionAcronym(secondary);
            prof = prof + "/" + s_prof;
        }
        profession = prof;
    }
};


// Destructor
ObserverModule::ObservableAgent::~ObservableAgent() {
    delete current_target_action;
}


// Name of the Agent to display on HUD
std::string ObserverModule::ObservableAgent::DisplayName() {
    bool is_initialised = _display_name.length() > 0;
    // additional name modification settings can go here...
    bool cache_busted = parent.trim_hench_names != trim_hench_name;
    if (is_initialised && !cache_busted) {
        return _display_name;
    }

    // generate and cache display_name
    std::string next_display_name = RawName();

    // remove hench name
    if (parent.trim_hench_names) {
        size_t begin = next_display_name.find("[");
        size_t end = next_display_name.find_first_of("]");
        if (std::string::npos != begin && std::string::npos != end && begin <= end) {
            next_display_name.erase(begin, end-begin + 1);
        }
    }

    // trim whitespace
    size_t w_first = next_display_name.find_first_not_of(' ');
    size_t w_last = next_display_name.find_last_not_of(' ');
    if (w_first != std::string::npos) {
        next_display_name = next_display_name.substr(w_first, w_last + 1);
    }

    trim_hench_name = parent.trim_hench_names;
    _display_name = next_display_name;
    return _display_name;
}


// Sanitized Name of the Agent (as std::string)
std::string ObserverModule::ObservableAgent::SanitizedName() {
    // has been cached
    if (_sanitized_name.length() > 0) return _sanitized_name;
    std::wstring sanitized_name_w = SanitizedNameW();
    // can't be cached yet
    if (sanitized_name_w.length() == 0) return "";
    // can now be cached
    _sanitized_name = GuiUtils::WStringToString(sanitized_name_w);
    return _sanitized_name;
}


// Sanitized Name of the Agent (as std::wstring)
std::wstring ObserverModule::ObservableAgent::SanitizedNameW() {
    // has been cached
    if (_sanitized_name_w.length() > 0) return _sanitized_name_w;
    std::wstring raw_name_w = RawNameW();
    // can't be cached yet
    if (raw_name_w.length() == 0) return L"";
    // can now be cached
    _sanitized_name_w = GuiUtils::SanitizePlayerName(raw_name_w);
    return _sanitized_name_w;
}


// Name of the Agent (as std::string)
std::string ObserverModule::ObservableAgent::RawName() {
    // has been cached
    if (_raw_name.length() > 0) return _raw_name;
    std::wstring raw_name_w = RawNameW();
    // can't be cached yet
    if (raw_name_w.length() == 0) return "";
    // can now be cached
    _raw_name = GuiUtils::WStringToString(raw_name_w);
    return _raw_name;
}


// Name of the Agent (un-edited as wstring)
std::wstring ObserverModule::ObservableAgent::RawNameW() {
    // rely on the constructor initialising the name...
    return _raw_name_w;
}


// Name + agent_id of the Agent
std::string ObserverModule::ObservableAgent::DebugName() {
    std::string debug_name = "(" + std::to_string(agent_id) + ") " + "\"" + RawName() + "\"";
    return debug_name;
}


// Constructor
ObserverModule::ObservableMap::ObservableMap(const GW::AreaInfo& area_info)
    : campaign(area_info.campaign)
    , continent(area_info.continent)
    , region(area_info.region)
    , type(area_info.type)
    , flags(area_info.flags)
    , name_id(area_info.name_id)
    , description_id(area_info.description_id)
{
    // async initialise the name
    if (GW::UI::UInt32ToEncStr(area_info.name_id, name_enc, 8)) {
        GW::UI::AsyncDecodeStr(name_enc, &name_w);
    }

    // async initialise the description
    if (GW::UI::UInt32ToEncStr(area_info.description_id, description_enc, 8)) {
        GW::UI::AsyncDecodeStr(description_enc, &description_w);
    }
}

// Cache & return name
std::string ObserverModule::ObservableMap::Name() {
    if (name.length() > 0) return name;
    name = GuiUtils::WStringToString(name_w);
    return name;
}

// Cache & return description
std::string ObserverModule::ObservableMap::Description() {
    if (description.length() > 0) return description;
    description = GuiUtils::WStringToString(description_w);
    return description;
}