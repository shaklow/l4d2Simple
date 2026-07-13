#include "Chams.h"
#include "../hook.h"
#include "../definitions.h"
#include "../../l4d2Simple2/xorstr.h"

CChams* g_pChams = nullptr;

CChams::CChams() : CBaseFeatures::CBaseFeatures()
{
}

CChams::~CChams()
{
	CBaseFeatures::~CBaseFeatures();
}

void CChams::InitMaterial()
{
	if (m_bMaterialInit)
		return;

	if (g_pInterface == nullptr || g_pInterface->MaterialSystem == nullptr)
		return;

	m_pMaterial = g_pInterface->MaterialSystem->FindMaterial(XorStr("debug/debugambientcube"), TEXTURE_GROUP_OTHER, false);
	if (m_pMaterial != nullptr)
	{
		m_pMaterial->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, m_bIgnoreZ);
		m_pMaterial->SetMaterialVarFlag(MATERIAL_VAR_NOFOG, true);
		m_pMaterial->SetMaterialVarFlag(MATERIAL_VAR_FLAT, true);
	}

	m_bMaterialInit = true;
}

void CChams::OnDrawModel(DrawModelState_t& state, ModelRenderInfo_t& pInfo, matrix3x4_t* pCustomBoneToWorld)
{
	if (!m_bActive || m_bMenuOpen)
		return;

	if (!g_pInterface->Engine->IsInGame())
		return;

	InitMaterial();
	if (m_pMaterial == nullptr)
		return;

	// Don't use pInfo.pRenderable directly - it points to IClientRenderable (entity+0x4),
	// not CBaseEntity (entity+0x0). Using it causes vtable offset errors and crashes.
	if (pInfo.entity_index <= 0)
		return;

	int localIndex = g_pInterface->Engine->GetLocalPlayer();
	if (pInfo.entity_index == localIndex)
		return;

	CBaseEntity* pEntity = reinterpret_cast<CBaseEntity*>(g_pInterface->EntList->GetClientEntity(pInfo.entity_index));
	if (pEntity == nullptr || pEntity->IsDormant())
		return;

	// Only special infected:
	// AI specials have specific classID, human-controlled specials use ET_CTERRORPLAYER(232)
	int classID = pEntity->GetClassID();
	CBasePlayer* pPlayer = reinterpret_cast<CBasePlayer*>(pEntity);
	bool isSI = pPlayer->IsSpecialInfected() ||
		(classID == ET_CTERRORPLAYER && pPlayer->GetTeam() == TEAM_INFECTED);
	if (!isSI)
		return;

	if (!pPlayer->IsAlive())
		return;

	DWORD color = m_dwInfectedColor;

	float r = ((color >> 16) & 0xFF) / 255.0f;
	float g = ((color >> 8) & 0xFF) / 255.0f;
	float b = (color & 0xFF) / 255.0f;
	float a = ((color >> 24) & 0xFF) / 255.0f;

	m_pMaterial->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, m_bIgnoreZ);
	m_pMaterial->ColorModulate(r, g, b);
	m_pMaterial->AlphaModulate(a);

	g_pInterface->ModelRender->ForcedMaterialOverride(m_pMaterial);
}

void CChams::OnMenuDrawing()
{
	if (!ImGui::TreeNode(XorStr("Chams")))
		return;

	ImGui::Checkbox(XorStr("Chams Active"), &m_bActive);
	IMGUI_TIPS("Chams master switch");

	ImGui::Checkbox(XorStr("Players"), &m_bPlayers);
	IMGUI_TIPS("Apply chams to players");

	ImGui::Checkbox(XorStr("Infected"), &m_bInfected);
	IMGUI_TIPS("Apply chams to infected");

	ImGui::Checkbox(XorStr("Ignore Z"), &m_bIgnoreZ);
	IMGUI_TIPS("Visible through walls");

	ImGui::Separator();

	float survivorColor[4] = {
		((m_dwSurvivorColor >> 16) & 0xFF) / 255.0f,
		((m_dwSurvivorColor >> 8) & 0xFF) / 255.0f,
		(m_dwSurvivorColor & 0xFF) / 255.0f,
		((m_dwSurvivorColor >> 24) & 0xFF) / 255.0f
	};
	if (ImGui::ColorEdit4(XorStr("Survivor Color"), survivorColor))
	{
		m_dwSurvivorColor = D3DCOLOR_ARGB(
			static_cast<int>(survivorColor[3] * 255),
			static_cast<int>(survivorColor[0] * 255),
			static_cast<int>(survivorColor[1] * 255),
			static_cast<int>(survivorColor[2] * 255));
	}

	float infectedColor[4] = {
		((m_dwInfectedColor >> 16) & 0xFF) / 255.0f,
		((m_dwInfectedColor >> 8) & 0xFF) / 255.0f,
		(m_dwInfectedColor & 0xFF) / 255.0f,
		((m_dwInfectedColor >> 24) & 0xFF) / 255.0f
	};
	if (ImGui::ColorEdit4(XorStr("Infected Color"), infectedColor))
	{
		m_dwInfectedColor = D3DCOLOR_ARGB(
			static_cast<int>(infectedColor[3] * 255),
			static_cast<int>(infectedColor[0] * 255),
			static_cast<int>(infectedColor[1] * 255),
			static_cast<int>(infectedColor[2] * 255));
	}

	float commonColor[4] = {
		((m_dwCommonColor >> 16) & 0xFF) / 255.0f,
		((m_dwCommonColor >> 8) & 0xFF) / 255.0f,
		(m_dwCommonColor & 0xFF) / 255.0f,
		((m_dwCommonColor >> 24) & 0xFF) / 255.0f
	};
	if (ImGui::ColorEdit4(XorStr("Common Infected Color"), commonColor))
	{
		m_dwCommonColor = D3DCOLOR_ARGB(
			static_cast<int>(commonColor[3] * 255),
			static_cast<int>(commonColor[0] * 255),
			static_cast<int>(commonColor[1] * 255),
			static_cast<int>(commonColor[2] * 255));
	}

	ImGui::TreePop();
}

void CChams::OnConfigLoading(CProfile& cfg)
{
	const std::string mainKeys = XorStr("Chams");

	m_bActive = cfg.GetBoolean(mainKeys, XorStr("chams_active"), m_bActive);
	m_bPlayers = cfg.GetBoolean(mainKeys, XorStr("chams_players"), m_bPlayers);
	m_bInfected = cfg.GetBoolean(mainKeys, XorStr("chams_infected"), m_bInfected);
	m_bIgnoreZ = cfg.GetBoolean(mainKeys, XorStr("chams_ignorez"), m_bIgnoreZ);
	m_dwSurvivorColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("chams_survivor_color"), static_cast<int>(m_dwSurvivorColor)));
	m_dwInfectedColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("chams_infected_color"), static_cast<int>(m_dwInfectedColor)));
	m_dwCommonColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("chams_common_color"), static_cast<int>(m_dwCommonColor)));
}

void CChams::OnConfigSave(CProfile& cfg)
{
	const std::string mainKeys = XorStr("Chams");

	cfg.SetValue(mainKeys, XorStr("chams_active"), m_bActive);
	cfg.SetValue(mainKeys, XorStr("chams_players"), m_bPlayers);
	cfg.SetValue(mainKeys, XorStr("chams_infected"), m_bInfected);
	cfg.SetValue(mainKeys, XorStr("chams_ignorez"), m_bIgnoreZ);
	cfg.SetValue(mainKeys, XorStr("chams_survivor_color"), static_cast<int>(m_dwSurvivorColor));
	cfg.SetValue(mainKeys, XorStr("chams_infected_color"), static_cast<int>(m_dwInfectedColor));
	cfg.SetValue(mainKeys, XorStr("chams_common_color"), static_cast<int>(m_dwCommonColor));
}
