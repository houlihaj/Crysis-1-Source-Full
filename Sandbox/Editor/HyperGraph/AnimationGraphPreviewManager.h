#ifndef __ANIMATIONGRAPHPREVIEWMANAGER_H__
#define __ANIMATIONGRAPHPREVIEWMANAGER_H__

#include "AnimationGraph.h"
#include "CharacterEditor\ModelViewportCE.h"

class CAnimationGraphPreviewManager
{
public:
	CAnimationGraphPreviewManager();
	void SetViewport(CModelViewportCE* pViewport);
	ICharacterInstance* GetCharacter() const;
	void SetState(CAGStatePtr pState);
	void EnablePreview(bool enablePreview);
	void SetParameter(const char* name, const char* value);
	
private:
	void StartPreview();
	void StopPreview();
	void ReplaceParameters( string& name ) const;

	CModelViewportCE* m_pViewport;
	CAGStatePtr m_pState;
	bool m_enablePreview;

	TParameterizationId m_mapLastParamValues;
};

#endif //__ANIMATIONGRAPHPREVIEWMANAGER_H__
