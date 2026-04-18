#ifndef __GAMEOBJECTCAMERASOURCE_H__
#define __GAMEOBJECTCAMERASOURCE_H__

#pragma once

#include "IGameObject.h"

class CCameraSource : public CGameObjectExtensionHelper<CCameraSource, IGameObjectExtension>, public IGameObjectView
{
public:
	CCameraSource();
	virtual ~CCameraSource();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId) {};
	virtual void PostInit(IGameObject *pGameObject);
	virtual void PostInitClient(int channelId) {};
	virtual void Release();
	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void PostSerialize() {}
	virtual void Update( SEntityUpdateContext &ctx, int updateSlot);
	virtual void PostUpdate(float frameTime ) {};
	virtual void HandleEvent( const SGameObjectEvent &);
	virtual void ProcessEvent(SEntityEvent &);
	virtual void SetChannelId(uint16 id) {}
	virtual void SetAuthority(bool auth);
	//~IGameObjectExtension

	// IGameObjectView
	virtual void UpdateView(SViewParams& params);
	//~IGameObjectView
private:
};

#endif // __GAMEOBJECTCAMERASOURCE_H__
