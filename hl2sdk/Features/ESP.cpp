#include "ESP.h"
#include "../hook.h"
#include "../definitions.h"
#include "../../l4d2Simple2/drawing.h"
#include "../../l4d2Simple2/xorstr.h"
#include <cstdio>
#include <algorithm>

CESP* g_pESP = nullptr;

CESP::CESP() : CBaseFeatures::CBaseFeatures()
{
}

CESP::~CESP()
{
	CBaseFeatures::~CBaseFeatures();
}

const char* CESP::GetZombieName(int classID)
{
	switch (classID)
	{
	case ET_BOOMER:    return "Boomer";
	case ET_SMOKER:    return "Smoker";
	case ET_HUNTER:    return "Hunter";
	case ET_JOCKEY:    return "Jockey";
	case ET_SPITTER:   return "Spitter";
	case ET_CHARGER:   return "Charger";
	case ET_TANK:      return "Tank";
	case ET_WITCH:     return "Witch";
	default:           return "Infected";
	}
}

bool CESP::GetEntityBounds(CBaseEntity* pEntity, int& x, int& y, int& w, int& h)
{
	if (pEntity == nullptr)
		return false;

	IClientRenderable* pRenderable = pEntity->GetRenderable();
	if (pRenderable == nullptr)
		return false;

	Vector mins, maxs;
	pRenderable->GetRenderBoundsWorldspace(mins, maxs);

	Vector vPoints[8];
	vPoints[0] = Vector(mins.x, mins.y, mins.z);
	vPoints[1] = Vector(maxs.x, mins.y, mins.z);
	vPoints[2] = Vector(mins.x, maxs.y, mins.z);
	vPoints[3] = Vector(maxs.x, maxs.y, mins.z);
	vPoints[4] = Vector(mins.x, mins.y, maxs.z);
	vPoints[5] = Vector(maxs.x, mins.y, maxs.z);
	vPoints[6] = Vector(mins.x, maxs.y, maxs.z);
	vPoints[7] = Vector(maxs.x, maxs.y, maxs.z);

	// Use engine DebugOverlay->ScreenPosition instead of D3D W2S
	// D3D transforms are invalid during PAINT_UIPANELS (UI ortho projection active)
	Vector vScreen[8];
	for (int i = 0; i < 8; i++)
	{
		if (g_pInterface->DebugOverlay->ScreenPosition(vPoints[i], vScreen[i]) != 0)
			return false;
	}

	float left = vScreen[0].x;
	float top = vScreen[0].y;
	float right = vScreen[0].x;
	float bottom = vScreen[0].y;

	for (int i = 1; i < 8; i++)
	{
		if (vScreen[i].x < left)   left = vScreen[i].x;
		if (vScreen[i].x > right)  right = vScreen[i].x;
		if (vScreen[i].y < top)    top = vScreen[i].y;
		if (vScreen[i].y > bottom) bottom = vScreen[i].y;
	}

	x = static_cast<int>(left);
	y = static_cast<int>(top);
	w = static_cast<int>(right - left);
	h = static_cast<int>(bottom - top);

	if (w < 2 || h < 2)
		return false;

	return true;
}

void CESP::OnEnginePaint(PaintMode_t mode)
{
	static int debugCounter = 0;
	bool debugLog = false;
	if (++debugCounter >= 300) { debugCounter = 0; debugLog = true; }

	if (debugLog)
	{
		FILE* f = nullptr;
		if (fopen_s(&f, "D:\\Tools_a\\AAA\\HAC\\CPC\\esp_debug.log", "a") == 0 && f)
		{
			fprintf(f, "=== OnEnginePaint mode=%d active=%d inGame=%d ===\n",
				(int)mode, m_bActive ? 1 : 0, g_pInterface->Engine->IsInGame() ? 1 : 0);
			fclose(f);
		}
	}

	if (!m_bActive || m_bMenuOpen)
		return;

	if (!g_pInterface->Engine->IsInGame())
		return;

	CBasePlayer* pLocal = g_pClientPrediction->GetLocalPlayer();
	if (pLocal == nullptr)
		return;

	int nLocalIndex = g_pInterface->Engine->GetLocalPlayer();
	int nMaxEntities = g_pInterface->EntList->GetHighestEntityIndex();

	for (int n = 1; n <= nMaxEntities; n++)
	{
		if (n == nLocalIndex)
			continue;

		CBaseEntity* pEntity = reinterpret_cast<CBaseEntity*>(g_pInterface->EntList->GetClientEntity(n));
		if (pEntity == nullptr || pEntity->IsDormant())
			continue;

		int classID = pEntity->GetClassID();

		// Only special infected:
		// AI specials have specific classID, human-controlled specials use ET_CTERRORPLAYER(232)
		CBasePlayer* pPlayer = reinterpret_cast<CBasePlayer*>(pEntity);
		bool isSI = pPlayer->IsSpecialInfected() ||
			(classID == ET_CTERRORPLAYER && pPlayer->GetTeam() == TEAM_INFECTED);
		if (!isSI)
			continue;

		if (!pPlayer->IsAlive())
			continue;

		int x, y, w, h;
		if (!GetEntityBounds(pEntity, x, y, w, h))
		{
			if (debugLog)
			{
				FILE* f = nullptr;
				if (fopen_s(&f, "D:\\Tools_a\\AAA\\HAC\\CPC\\esp_debug.log", "a") == 0 && f)
				{
					fprintf(f, "  bounds failed ent=%d classID=%d\n", n, classID);
					fclose(f);
				}
			}
			continue;
		}

		if (debugLog)
		{
			FILE* f = nullptr;
			if (fopen_s(&f, "D:\\Tools_a\\AAA\\HAC\\CPC\\esp_debug.log", "a") == 0 && f)
			{
				fprintf(f, "  drawing ent=%d classID=%d x=%d y=%d w=%d h=%d\n", n, classID, x, y, w, h);
				fclose(f);
			}
		}

		DWORD color = m_dwInfectedColor;

		int health = pPlayer->GetHealth();
		if (health < 0) health = 0;
		if (health > 100) health = 100;

		// Box
		if (m_bBox)
		{
			g_pDrawing->DrawRect(x - 1, y - 1, w + 2, h + 2, CDrawing::BLACK);
			g_pDrawing->DrawRect(x, y, w, h, color);
			g_pDrawing->DrawRect(x + 1, y + 1, w - 2, h - 2, CDrawing::BLACK);
		}

		// Name
		if (m_bName)
		{
			const char* zombieName = GetZombieName(classID);
			g_pDrawing->DrawText(x + w / 2, y - 16, color, true, zombieName);
		}

		// Distance
		if (m_bDistance)
		{
			Vector localOrigin = pLocal->GetAbsOrigin();
			Vector entityOrigin = pEntity->GetAbsOrigin();
			float dist = (localOrigin - entityOrigin).Length();
			int distM = static_cast<int>(dist * 0.01905f);

			char distText[32];
			sprintf_s(distText, "[%dM]", distM);
			g_pDrawing->DrawText(x + w / 2, y + h + 2, CDrawing::WHITE, true, distText);
		}

		// Health bar
		if (m_bHealthBar)
		{
			int barX = x - 6;
			int barY = y;
			int barW = 3;
			int barH = h;

			g_pDrawing->DrawRectFilled(barX - 1, barY - 1, barW + 2, barH + 2, CDrawing::BLACK);

			float ratio = health / 100.0f;
			int fillH = static_cast<int>(barH * ratio);
			DWORD healthColor = D3DCOLOR_ARGB(255,
				static_cast<int>(255 * (1.0f - ratio)),
				static_cast<int>(255 * ratio),
				0);
			g_pDrawing->DrawRectFilled(barX, barY + barH - fillH, barW, fillH, healthColor);
		}
	}
}

void CESP::OnMenuDrawing()
{
	if (!ImGui::TreeNode(XorStr("ESP")))
		return;

	ImGui::Checkbox(XorStr("ESP Active"), &m_bActive);
	IMGUI_TIPS("ESP master switch");

	ImGui::Checkbox(XorStr("Box"), &m_bBox);
	ImGui::Checkbox(XorStr("Name"), &m_bName);
	ImGui::Checkbox(XorStr("Health Bar"), &m_bHealthBar);
	ImGui::Checkbox(XorStr("Distance"), &m_bDistance);
	ImGui::Checkbox(XorStr("Enemy Only"), &m_bEnemyOnly);
	IMGUI_TIPS("Show enemies only");

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

void CESP::OnConfigLoading(CProfile& cfg)
{
	const std::string mainKeys = XorStr("ESP");

	m_bActive = cfg.GetBoolean(mainKeys, XorStr("esp_active"), m_bActive);
	m_bBox = cfg.GetBoolean(mainKeys, XorStr("esp_box"), m_bBox);
	m_bName = cfg.GetBoolean(mainKeys, XorStr("esp_name"), m_bName);
	m_bHealthBar = cfg.GetBoolean(mainKeys, XorStr("esp_healthbar"), m_bHealthBar);
	m_bDistance = cfg.GetBoolean(mainKeys, XorStr("esp_distance"), m_bDistance);
	m_bEnemyOnly = cfg.GetBoolean(mainKeys, XorStr("esp_enemyonly"), m_bEnemyOnly);
	m_dwSurvivorColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("esp_survivor_color"), static_cast<int>(m_dwSurvivorColor)));
	m_dwInfectedColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("esp_infected_color"), static_cast<int>(m_dwInfectedColor)));
	m_dwCommonColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("esp_common_color"), static_cast<int>(m_dwCommonColor)));
}

void CESP::OnConfigSave(CProfile& cfg)
{
	const std::string mainKeys = XorStr("ESP");

	cfg.SetValue(mainKeys, XorStr("esp_active"), m_bActive);
	cfg.SetValue(mainKeys, XorStr("esp_box"), m_bBox);
	cfg.SetValue(mainKeys, XorStr("esp_name"), m_bName);
	cfg.SetValue(mainKeys, XorStr("esp_healthbar"), m_bHealthBar);
	cfg.SetValue(mainKeys, XorStr("esp_distance"), m_bDistance);
	cfg.SetValue(mainKeys, XorStr("esp_enemyonly"), m_bEnemyOnly);
	cfg.SetValue(mainKeys, XorStr("esp_survivor_color"), static_cast<int>(m_dwSurvivorColor));
	cfg.SetValue(mainKeys, XorStr("esp_infected_color"), static_cast<int>(m_dwInfectedColor));
	cfg.SetValue(mainKeys, XorStr("esp_common_color"), static_cast<int>(m_dwCommonColor));
}
