#pragma once
#include "BaseFeatures.h"

class CChams : public CBaseFeatures
{
public:
	CChams();
	~CChams();

	virtual void OnDrawModel(DrawModelState_t& state, ModelRenderInfo_t& pInfo, matrix3x4_t* pCustomBoneToWorld) override;
	virtual void OnMenuDrawing() override;
	virtual void OnConfigLoading(CProfile& cfg) override;
	virtual void OnConfigSave(CProfile& cfg) override;

private:
	void InitMaterial();

	bool m_bActive = true;
	bool m_bPlayers = true;
	bool m_bInfected = true;
	bool m_bIgnoreZ = true;

	DWORD m_dwSurvivorColor = D3DCOLOR_ARGB(255, 0, 255, 0);
	DWORD m_dwInfectedColor = D3DCOLOR_ARGB(255, 255, 0, 0);
	DWORD m_dwCommonColor = D3DCOLOR_ARGB(255, 200, 100, 0);

	IMaterial* m_pMaterial = nullptr;
	bool m_bMaterialInit = false;
};

extern CChams* g_pChams;
