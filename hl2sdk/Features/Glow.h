#pragma once
#include "BaseFeatures.h"

class CGlow : public CBaseFeatures
{
public:
	CGlow();
	~CGlow();

	virtual void OnMenuDrawing() override;
	virtual void OnConfigLoading(CProfile& cfg) override;
	virtual void OnConfigSave(CProfile& cfg) override;
	virtual void OnShutdown() override;

	void Install();
	bool IsInstalled() const { return m_bInstalled; }

	friend void __fastcall Redirected_Get_Glow_Color(void* Entity, void* /*edx*/, float* a2, float* a3, float* a4, float* a5);

private:
	bool m_bActive = true;
	bool m_bSurvivor = true;
	bool m_bInfected = true;
	bool m_bGhost = true;
	bool m_bIgnoreLocal = true;

	DWORD m_dwSurvivorColor = D3DCOLOR_ARGB(255, 0, 255, 0);
	DWORD m_dwInfectedColor = D3DCOLOR_ARGB(255, 255, 0, 0);
	DWORD m_dwGhostColor = D3DCOLOR_ARGB(255, 255, 255, 0);

	bool m_bInstalled = false;
	void* m_pTrampoline = nullptr;
	unsigned char m_OriginalBytes[7] = { 0 };
	unsigned char m_OriginalEnableByte = 0;
	bool m_bBytesSaved = false;
};

extern CGlow* g_pGlow;

// Redirected function called by the game
void __fastcall Redirected_Get_Glow_Color(void* Entity, void* /*edx*/, float* a2, float* a3, float* a4, float* a5);
