#include "Glow.h"
#include "../hook.h"
#include "../definitions.h"
#include "../../l4d2Simple2/xorstr.h"
#include <cstring>

CGlow* g_pGlow = nullptr;

// Static pointer to the trampoline for calling the original function
static void* s_pOriginalCaller = nullptr;

// Offsets from client.dll base (from Storm-L4D2)
static constexpr unsigned int GLOW_COLOR_FUNC_OFFSET = 2455600;
static constexpr unsigned int GLOW_ENABLE_BYTE_OFFSET = 3244715;
static constexpr unsigned int LOCAL_PLAYER_PTR_OFFSET = 7498712;

CGlow::CGlow() : CBaseFeatures::CBaseFeatures()
{
}

CGlow::~CGlow()
{
	CBaseFeatures::~CBaseFeatures();
}

void CGlow::Install()
{
	if (m_bInstalled)
		return;

	HMODULE hClientModule = GetModuleHandleW(L"client.dll");
	if (hClientModule == nullptr)
		return;

	uintptr_t clientBase = reinterpret_cast<uintptr_t>(hClientModule);
	uintptr_t glowFuncAddr = clientBase + GLOW_COLOR_FUNC_OFFSET;
	uintptr_t enableByteAddr = clientBase + GLOW_ENABLE_BYTE_OFFSET;

	// Save original bytes
	memcpy(m_OriginalBytes, reinterpret_cast<void*>(glowFuncAddr), 7);
	m_OriginalEnableByte = *reinterpret_cast<unsigned char*>(enableByteAddr);
	m_bBytesSaved = true;

	// Create trampoline (13 bytes: 7 original + 6 for PUSH addr; RET)
	m_pTrampoline = VirtualAlloc(nullptr, 13, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (m_pTrampoline == nullptr)
		return;

	unsigned char* tramp = static_cast<unsigned char*>(m_pTrampoline);

	// Copy first 7 bytes from original function
	memcpy(tramp, reinterpret_cast<void*>(glowFuncAddr), 7);

	// Write PUSH <original+7>; RET at trampoline[7]
	tramp[7] = 0x68; // PUSH imm32
	uintptr_t continueAddr = glowFuncAddr + 7;
	memcpy(&tramp[8], &continueAddr, 4);
	tramp[12] = 0xC3; // RET

	s_pOriginalCaller = m_pTrampoline;

	// Patch original function: PUSH <Redirected_Get_Glow_Color>; RET
	DWORD oldProtect = 0;
	VirtualProtect(reinterpret_cast<void*>(glowFuncAddr), 6, PAGE_EXECUTE_READWRITE, &oldProtect);

	unsigned char* original = static_cast<unsigned char*>(reinterpret_cast<void*>(glowFuncAddr));
	original[0] = 0x68; // PUSH imm32
	uintptr_t redirectedAddr = reinterpret_cast<uintptr_t>(&Redirected_Get_Glow_Color);
	memcpy(&original[1], &redirectedAddr, 4);
	original[5] = 0xC3; // RET

	VirtualProtect(reinterpret_cast<void*>(glowFuncAddr), 6, oldProtect, &oldProtect);

	// Patch enable byte to 0x31 (49) to enable glow
	DWORD oldProtect2 = 0;
	VirtualProtect(reinterpret_cast<void*>(enableByteAddr), 1, PAGE_EXECUTE_READWRITE, &oldProtect2);
	*reinterpret_cast<unsigned char*>(enableByteAddr) = 0x31;
	VirtualProtect(reinterpret_cast<void*>(enableByteAddr), 1, oldProtect2, &oldProtect2);

	m_bInstalled = true;
}

void __fastcall Redirected_Get_Glow_Color(void* Entity, void* /*edx*/, float* a2, float* a3, float* a4, float* a5)
{
	// Call original function first
	if (s_pOriginalCaller != nullptr)
	{
		typedef void(__fastcall *OriginalFn)(void*, void*, float*, float*, float*, float*);
		reinterpret_cast<OriginalFn>(s_pOriginalCaller)(Entity, nullptr, a2, a3, a4, a5);
	}

	if (g_pGlow == nullptr || !g_pGlow->IsInstalled())
		return;

	if (!g_pGlow->m_bActive)
		return;

	if (Entity == nullptr)
		return;

	// Access glow settings via g_pGlow
	// We use a simple approach: check the entity's class ID and team
	CBaseEntity* pEntity = reinterpret_cast<CBaseEntity*>(Entity);
	if (pEntity == nullptr)
		return;

	int classID = 0;
	try
	{
		classID = pEntity->GetClassID();
	}
	catch (...)
	{
		return;
	}

	// Valid entity types for glow (from Storm-L4D2 Get_Identifier)
	static const int validTypes[] = { 0, 13, 99, 232, 263, 264, 265, 270, 272, 275, 276, 277 };
	bool isValid = false;
	for (int type : validTypes)
	{
		if (classID == type)
		{
			isValid = true;
			break;
		}
	}

	if (!isValid)
	{
		*a2 = *a3 = *a4 = *a5 = 0.0f;
		return;
	}

	// Check if local player should be skipped
	if (g_pGlow->m_bIgnoreLocal)
	{
		HMODULE hClientModule = GetModuleHandleW(L"client.dll");
		if (hClientModule != nullptr)
		{
			uintptr_t clientBase = reinterpret_cast<uintptr_t>(hClientModule);
			void* localPlayer = *reinterpret_cast<void**>(clientBase + LOCAL_PLAYER_PTR_OFFSET);
			if (localPlayer == Entity)
			{
				*a2 = *a3 = *a4 = *a5 = 0.0f;
				return;
			}
		}
	}

	// Get team and ghost status
	CBasePlayer* pPlayer = reinterpret_cast<CBasePlayer*>(Entity);
	int team = pPlayer->GetTeam();
	bool isGhost = pPlayer->IsGhost();

	float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
	bool shouldApply = false;

	// These accesses go through g_pGlow which is a CGlow* 
	// We need to access the private members - use friend approach or make them accessible
	// For simplicity, we'll use the same pattern as Storm-L4D2 and access via offset

	DWORD color = 0;

	if (isGhost)
	{
		color = g_pGlow->m_dwGhostColor;
		shouldApply = g_pGlow->m_bGhost;
	}
	else if (team == TEAM_SURVIVORS)
	{
		color = g_pGlow->m_dwSurvivorColor;
		shouldApply = g_pGlow->m_bSurvivor;
	}
	else if (team == TEAM_INFECTED)
	{
		color = g_pGlow->m_dwInfectedColor;
		shouldApply = g_pGlow->m_bInfected;
	}

	if (!shouldApply)
	{
		*a2 = *a3 = *a4 = *a5 = 0.0f;
		return;
	}

	r = ((color >> 16) & 0xFF) / 255.0f;
	g = ((color >> 8) & 0xFF) / 255.0f;
	b = (color & 0xFF) / 255.0f;
	a = ((color >> 24) & 0xFF) / 255.0f;

	*a2 = r;
	*a3 = g;
	*a4 = b;
	*a5 = a;
}

void CGlow::OnShutdown()
{
	if (!m_bInstalled)
		return;

	HMODULE hClientModule = GetModuleHandleW(L"client.dll");
	if (hClientModule == nullptr)
	{
		m_bInstalled = false;
		return;
	}

	uintptr_t clientBase = reinterpret_cast<uintptr_t>(hClientModule);
	uintptr_t glowFuncAddr = clientBase + GLOW_COLOR_FUNC_OFFSET;
	uintptr_t enableByteAddr = clientBase + GLOW_ENABLE_BYTE_OFFSET;

	// Restore original function bytes
	if (m_bBytesSaved)
	{
		DWORD oldProtect = 0;
		VirtualProtect(reinterpret_cast<void*>(glowFuncAddr), 7, PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy(reinterpret_cast<void*>(glowFuncAddr), m_OriginalBytes, 7);
		VirtualProtect(reinterpret_cast<void*>(glowFuncAddr), 7, oldProtect, &oldProtect);

		DWORD oldProtect2 = 0;
		VirtualProtect(reinterpret_cast<void*>(enableByteAddr), 1, PAGE_EXECUTE_READWRITE, &oldProtect2);
		*reinterpret_cast<unsigned char*>(enableByteAddr) = m_OriginalEnableByte;
		VirtualProtect(reinterpret_cast<void*>(enableByteAddr), 1, oldProtect2, &oldProtect2);
	}

	// Free trampoline
	if (m_pTrampoline != nullptr)
	{
		VirtualFree(m_pTrampoline, 0, MEM_RELEASE);
		m_pTrampoline = nullptr;
		s_pOriginalCaller = nullptr;
	}

	m_bInstalled = false;
}

void CGlow::OnMenuDrawing()
{
	if (!ImGui::TreeNode(XorStr("Glow")))
		return;

	ImGui::Checkbox(XorStr("Glow Active"), &m_bActive);
	IMGUI_TIPS("Glow master switch");

	ImGui::Checkbox(XorStr("Survivor Glow"), &m_bSurvivor);
	ImGui::Checkbox(XorStr("Infected Glow"), &m_bInfected);
	ImGui::Checkbox(XorStr("Ghost Glow"), &m_bGhost);
	ImGui::Checkbox(XorStr("Ignore Local"), &m_bIgnoreLocal);
	IMGUI_TIPS("Don't apply glow to local player");

	ImGui::Separator();

	float survivorColor[4] = {
		((m_dwSurvivorColor >> 16) & 0xFF) / 255.0f,
		((m_dwSurvivorColor >> 8) & 0xFF) / 255.0f,
		(m_dwSurvivorColor & 0xFF) / 255.0f,
		((m_dwSurvivorColor >> 24) & 0xFF) / 255.0f
	};
	if (ImGui::ColorEdit4(XorStr("Survivor Glow Color"), survivorColor))
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
	if (ImGui::ColorEdit4(XorStr("Infected Glow Color"), infectedColor))
	{
		m_dwInfectedColor = D3DCOLOR_ARGB(
			static_cast<int>(infectedColor[3] * 255),
			static_cast<int>(infectedColor[0] * 255),
			static_cast<int>(infectedColor[1] * 255),
			static_cast<int>(infectedColor[2] * 255));
	}

	float ghostColor[4] = {
		((m_dwGhostColor >> 16) & 0xFF) / 255.0f,
		((m_dwGhostColor >> 8) & 0xFF) / 255.0f,
		(m_dwGhostColor & 0xFF) / 255.0f,
		((m_dwGhostColor >> 24) & 0xFF) / 255.0f
	};
	if (ImGui::ColorEdit4(XorStr("Ghost Glow Color"), ghostColor))
	{
		m_dwGhostColor = D3DCOLOR_ARGB(
			static_cast<int>(ghostColor[3] * 255),
			static_cast<int>(ghostColor[0] * 255),
			static_cast<int>(ghostColor[1] * 255),
			static_cast<int>(ghostColor[2] * 255));
	}

	ImGui::TreePop();
}

void CGlow::OnConfigLoading(CProfile& cfg)
{
	const std::string mainKeys = XorStr("Glow");

	m_bActive = cfg.GetBoolean(mainKeys, XorStr("glow_active"), m_bActive);
	m_bSurvivor = cfg.GetBoolean(mainKeys, XorStr("glow_survivor"), m_bSurvivor);
	m_bInfected = cfg.GetBoolean(mainKeys, XorStr("glow_infected"), m_bInfected);
	m_bGhost = cfg.GetBoolean(mainKeys, XorStr("glow_ghost"), m_bGhost);
	m_bIgnoreLocal = cfg.GetBoolean(mainKeys, XorStr("glow_ignore_local"), m_bIgnoreLocal);
	m_dwSurvivorColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("glow_survivor_color"), static_cast<int>(m_dwSurvivorColor)));
	m_dwInfectedColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("glow_infected_color"), static_cast<int>(m_dwInfectedColor)));
	m_dwGhostColor = static_cast<DWORD>(cfg.GetInteger(mainKeys, XorStr("glow_ghost_color"), static_cast<int>(m_dwGhostColor)));
}

void CGlow::OnConfigSave(CProfile& cfg)
{
	const std::string mainKeys = XorStr("Glow");

	cfg.SetValue(mainKeys, XorStr("glow_active"), m_bActive);
	cfg.SetValue(mainKeys, XorStr("glow_survivor"), m_bSurvivor);
	cfg.SetValue(mainKeys, XorStr("glow_infected"), m_bInfected);
	cfg.SetValue(mainKeys, XorStr("glow_ghost"), m_bGhost);
	cfg.SetValue(mainKeys, XorStr("glow_ignore_local"), m_bIgnoreLocal);
	cfg.SetValue(mainKeys, XorStr("glow_survivor_color"), static_cast<int>(m_dwSurvivorColor));
	cfg.SetValue(mainKeys, XorStr("glow_infected_color"), static_cast<int>(m_dwInfectedColor));
	cfg.SetValue(mainKeys, XorStr("glow_ghost_color"), static_cast<int>(m_dwGhostColor));
}
