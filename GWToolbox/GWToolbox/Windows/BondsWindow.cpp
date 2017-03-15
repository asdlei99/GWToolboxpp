#include "BondsWindow.h"

#include <sstream>
#include <string>
#include <d3dx9tex.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\GWStructures.h>

#include "GuiUtils.h"
#include "OtherModules\ToolboxSettings.h"
#include <OtherModules\Resources.h>

DWORD BondsWindow::buff_id[MAX_PLAYERS][MAX_BONDS] = { 0 };

void BondsWindow::Initialize() {
	ToolboxWidget::Initialize();
	for (int i = 0; i < MAX_BONDS; ++i) textures[i] = nullptr;
	Resources::Instance().LoadTextureAsync(&textures[BalthazarSpirit], "Balthazar's_Spirit.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[EssenceBond], "Essence_Bond.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[HolyVeil], "Holy_Veil.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[LifeAttunement], "Life_Attunement.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[LifeBarrier], "Life_Barrier.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[LifeBond], "Life_Bond.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[LiveVicariously], "Live_Vicariously.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[Mending], "Mending.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[ProtectiveBond], "Protective_Bond.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[PurifyingVeil], "Purifying_Veil.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[Retribution], "Retribution.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[StrengthOfHonor], "Strength_of_Honor.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[Succor], "Succor.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[VitalBlessing], "Vital_Blessing.jpg", "img");
	Resources::Instance().LoadTextureAsync(&textures[WatchfulSpirit], "Watchful_Spirit.jpg", "img");
}

void BondsWindow::Terminate() {
	for (int i = 0; i < MAX_BONDS; ++i) {
		if (textures[i]) {
			textures[i]->Release();
			textures[i] = nullptr;
		}
	}
}

void BondsWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) return;
	
	int img_size = row_height > 0 ? row_height : GuiUtils::GetPartyHealthbarHeight();
	int party_size = GW::Partymgr().GetPartySize();

	int size = GW::Partymgr().GetPartySize();
	if (size > MAX_PLAYERS) size = MAX_PLAYERS;
	
	UpdateSkillbarBonds();

	memset(buff_id, 0, sizeof(DWORD) * MAX_PLAYERS * n_bonds);

	GW::AgentEffectsArray effects = GW::Effects().GetPartyEffectArray();
	if (effects.valid()) {
		GW::BuffArray buffs = effects[0].Buffs;
		GW::AgentArray agents = GW::Agents().GetAgentArray();

		if (buffs.valid() && agents.valid() && effects.valid()) {
			for (size_t i = 0; i < buffs.size(); ++i) {
				int player = -1;
				DWORD target_id = buffs[i].TargetAgentId;
				if (target_id < agents.size() && agents[target_id]) {
					player = agents[target_id]->PlayerNumber;
					if (player == 0 || player > MAX_PLAYERS) continue;
					--player;	// player numbers are from 1 to partysize in party list
				}
				if (player == -1) continue;

				int bond = -1;
				for (int j = 0; j < n_bonds; ++j) {
					if (buffs[i].SkillId == skillbar_bond_skillid[j]) {
						bond = j;
					}
				}
				if (bond == -1) continue;

				buff_id[player][bond] = buffs[i].BuffId;
			}
		}
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(background));
	ImGui::SetNextWindowSize(ImVec2((float)(n_bonds * img_size), (float)(party_size * img_size)));
	if (ImGui::Begin(Name(), &visible, GetWinFlags(0, !click_to_use))) {
		float x = ImGui::GetWindowPos().x;
		float y = ImGui::GetWindowPos().y;
		for (int player = 0; player < party_size; ++player) {
			for (int bond = 0; bond < n_bonds; ++bond) {
				ImVec2 tl(x + (bond + 0) * img_size, y + (player + 0) * img_size);
				ImVec2 br(x + (bond + 1) * img_size, y + (player + 1) * img_size);
				if (buff_id[player][bond] > 0) {
					ImGui::GetWindowDrawList()->AddImage((ImTextureID)textures[skillbar_bond_idx[bond]],
						ImVec2(tl.x, tl.y),
						ImVec2(br.x, br.y));
				}
				if (click_to_use) {
					if (ImGui::IsMouseHoveringRect(tl, br)) {
						ImGui::GetWindowDrawList()->AddRect(tl, br, IM_COL32(255, 255, 255, 255));
						if (ImGui::IsMouseReleased(0)) {
							if (buff_id[player][bond] > 0) {
								GW::Effects().DropBuff(buff_id[player][bond]);
							} else {
								UseBuff(player, bond);
							}
						}
					}
				}
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleColor(); // window bg
	ImGui::PopStyleVar(2);
}

void BondsWindow::UseBuff(int player, int bond) {
	printf("casting %d on %d\n", bond, player);
	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Explorable) return;

	DWORD target = GW::Agents().GetAgentIdByLoginNumber(player + 1);
	if (target == 0) return;

	int slot = skillbar_bond_slot[bond];
	GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
	if (!skillbar.IsValid()) return;
	if (skillbar.Skills[slot].Recharge != 0) return;

	GW::Skillbarmgr().UseSkill(slot, target);
}

void BondsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	background = Colors::Load(ini, Name(), "background", Colors::ARGB(76, 0, 0, 0));
	click_to_use = ini->GetBoolValue(Name(), "click_to_use", true);
	row_height = ini->GetLongValue(Name(), "row_height", 0);
}

void BondsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	Colors::Save(ini, Name(), "background", background);
	ini->SetBoolValue(Name(), "click_to_use", click_to_use);
	ini->SetLongValue(Name(), "row_height", row_height);
}

void BondsWindow::DrawSettingInternal() {
	Colors::DrawSetting("Background", &background);
	ImGui::Checkbox("Click to Drop/Use", &click_to_use);
	ImGui::InputInt("Row Height", &row_height);
	ImGui::ShowHelp("Height of each row, leave 0 for default");
}

bool BondsWindow::UpdateSkillbarBonds() {
	n_bonds = 0;
	skillbar_bond_idx.clear();
	skillbar_bond_slot.clear();
	skillbar_bond_skillid.clear();

	GW::Skillbar bar = GW::Skillbar::GetPlayerSkillbar();
	if (!bar.IsValid()) return false;
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < MAX_BONDS; ++j) {
			const Bond bond = static_cast<Bond>(j);
			if (bar.Skills[i].SkillId == (DWORD)GetSkillID(bond)) {
				++n_bonds;
				skillbar_bond_idx.push_back(bond);
				skillbar_bond_slot.push_back(i);
				skillbar_bond_skillid.push_back(bar.Skills[i].SkillId);
			}
		}
	}
	return true;
}

GW::Constants::SkillID BondsWindow::GetSkillID(Bond bond) const {
	using namespace GW::Constants;
	switch (bond) {
	case BondsWindow::BalthazarSpirit: return SkillID::Balthazars_Spirit;
	case BondsWindow::EssenceBond: return SkillID::Essence_Bond;
	case BondsWindow::HolyVeil: return SkillID::Holy_Veil;
	case BondsWindow::LifeAttunement: return SkillID::Life_Attunement;
	case BondsWindow::LifeBarrier: return SkillID::Life_Barrier;
	case BondsWindow::LifeBond: return SkillID::Life_Bond;
	case BondsWindow::LiveVicariously: return SkillID::Live_Vicariously;
	case BondsWindow::Mending: return SkillID::Mending;
	case BondsWindow::ProtectiveBond: return SkillID::Protective_Bond;
	case BondsWindow::PurifyingVeil: return SkillID::Purifying_Veil;
	case BondsWindow::Retribution: return SkillID::Retribution;
	case BondsWindow::StrengthOfHonor: return SkillID::Strength_of_Honor;
	case BondsWindow::Succor: return SkillID::Succor;
	case BondsWindow::VitalBlessing: return SkillID::Vital_Blessing;
	case BondsWindow::WatchfulSpirit: return SkillID::Watchful_Spirit;
	default: return SkillID::No_Skill;
	}
}
