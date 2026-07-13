#pragma once
#include "BaseFeatures.h"

class CESP : public CBaseFeatures
{
public:
	CESP();
	~CESP();

	virtual void OnEnginePaint(PaintMode_t mode) override;
	virtual void OnMenuDrawing() override;
	virtual void OnConfigLoading(CProfile& cfg) override;
	virtual void OnConfigSave(CProfile& cfg) override;

private:
	bool GetEntityBounds(CBaseEntity* pEntity, int& x, int& y, int& w, int& h);
	const char* GetZombieName(int classID);

	bool m_bActive = true;
	bool m_bBox = true;
	bool m_bName = true;
	bool m_bHealthBar = true;
	bool m_bDistance = true;
	bool m_bEnemyOnly = false;

	DWORD m_dwSurvivorColor = D3DCOLOR_ARGB(255, 0, 255, 0);
	DWORD m_dwInfectedColor = D3DCOLOR_ARGB(255, 255, 0, 0);
	DWORD m_dwCommonColor = D3DCOLOR_ARGB(255, 200, 100, 0);
};

extern CESP* g_pESP;
