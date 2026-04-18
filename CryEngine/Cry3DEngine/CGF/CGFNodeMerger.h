#ifndef __CGFNODEMERGER_H__
#define __CGFNODEMERGER_H__

class CContentCGF;
class CMesh;
struct CMaterialCGF;

class CCGFNodeMerger
{
public:
	bool MergeNodes(CContentCGF* pCGF, std::vector<int>& usedMaterialIds, string& errorMessage);
	bool SetupMesh(CMesh &mesh,CMaterialCGF *pMaterialCGF, std::vector<int>& usedMaterialIds);
};

#endif //__CGFNODEMERGER_H__
