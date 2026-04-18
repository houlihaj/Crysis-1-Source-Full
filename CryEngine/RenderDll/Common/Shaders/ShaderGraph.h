/*=============================================================================
  ShaderGraph.h : FX Shaders Graph declarations.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#ifndef __SHADERGRAPH_H__
#define __SHADERGRAPH_H__


//============================================================================

struct SShaderShBlockConnection
{
  string m_Name;
  string m_ClassName;
  DynArray<SShaderParam> m_Properties;
  int m_IdBlock;
  short m_IdVar;
  bool m_bUsed;
  SShaderShBlockConnection()
  {
    m_IdBlock = -1;
    m_IdVar = -1;
    m_bUsed = false;
  }
};

struct SShaderShBlockConnections
{
  std::vector<SShaderShBlockConnection> m_Connections;
};

struct SShaderShBlock
{
  byte m_bReferenced;
  bool m_bUsed;
  string m_Name;
  string m_ClassName;
  string m_SubClassName;
  int m_Id;
  SShaderShBlockConnections m_InputConnections;
  SShaderShBlockConnections m_OutputConnections;

  SShaderGraphBlock *m_pGraphBlock;
  SShaderShBlock()
  {
    m_pGraphBlock = NULL;
    m_Id = 0;
    m_bUsed = false;
    m_bReferenced = 0;
  }
};

struct SShaderShEdge
{
  int m_Id[2]; // In, Out
  string m_Port[2]; // in, Out
};

struct SShaderShTexture
{
  string m_Name;
  SShaderGraphBlock *m_pGraphBlock;
  SShaderGraphNode *m_pGraphNode;
};

struct SShaderShConstant
{
  string m_Name;
  string m_Semantics;
  EGrNodeFormat m_eFormat;
  SShaderGraphBlock *m_pGraphBlock;
  SShaderGraphNode *m_pGraphNode;
  DynArray<SShaderParam> m_Properties;
  float m_fMin;
  float m_fMax;
  SShaderShConstant()
  {
    m_eFormat = eGrNodeFormat_Unknown;
    m_pGraphBlock = NULL;
    m_pGraphNode = NULL;
    m_fMin = -1;
    m_fMax = -1;
  }
  bool GetValue(const char *szName, float *v)
  {
    bool bRes = SShaderParam::GetValue(szName, &m_Properties, v, 0);
    if (!bRes)
      bRes = SShaderParam::GetValue(szName, &m_pGraphNode->m_Properties, v, 0);
    return bRes;
  }
};

struct SShaderConnect
{
  SShaderShBlock *m_pInBlock;
  SShaderShBlockConnection *m_pInConnection;
  SShaderShBlock *m_pOutBlock;
  SShaderShBlockConnection *m_pOutConnection;
};

class CShaderGraph
{
public:
  FXShaderGraphBlocks m_Blocks;

private:
  int m_VarCounter;

  void InitNodes();
  void InitNodesForPath(const char *szPath);
  bool ParseNodesForFile(const char *szName);
  bool ParseNodes(char *szCode);
  bool ParseBlock(char *szCode, SShaderGraphBlock *pBL);
  bool ParseNode(char *szCode, SShaderGraphNode *pND);
  bool ParseNode_Properties(char *szCode, SShaderGraphNode *pND);

  SShaderGraphNode *GetGraphNode(string& classBlock, string& classNode);

  bool DeclareConstant(SShaderShBlock *pBS, string& ClassName, string& Name, std::vector<SShaderShConstant>& shGlobalVars);
  bool ParseGraphShader(char *buf, CShader *ef, std::vector<string>& shCodeLines, string& shInputStruct, string& shOutputStruct, std::vector<string>& shFunctions, std::vector<SShaderShConstant>& shGlobalVars, std::vector<SShaderShTexture>& shSamplers, bool bPixel);
  bool ParseShBlock(char *buf, CShader *ef, SShaderShBlock *pBL);
  bool ParseShBlock_ConnectionProperties(char *buf, CShader *ef, SShaderShBlockConnection *pCN, SShaderShBlock *pBL);
  bool ParseShBlock_Connections(char *buf, CShader *ef, SShaderShBlock *pBL, SShaderShBlockConnections *pCN);
  bool ParseShEdges(char *buf, CShader *ef, TArray<SShaderShEdge *>& ShEdges);
  bool ParseShEdge(char *buf, CShader *ef, SShaderShEdge *pEdge);
  bool GenerateGraphShader(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges, std::vector<string>& shCodeLines, string& shInputStruct, string& shOutputStruct, std::vector<string>& shFunctions, std::vector<SShaderShConstant>& shGlobalVars, std::vector<SShaderShTexture>& shSamplers);
  char *GenerateFXGraphShader(CShader *ef, std::vector<string>& shVSCodeLines, string& shVSInputStruct, string& shVSOutputStruct, std::vector<string>& shVSFunctions, std::vector<SShaderShConstant>& shVSGlobalVars, std::vector<SShaderShTexture>& shVSSamplers, std::vector<string>& shPSCodeLines, string& shPSInputStruct, string& shPSOutputStruct, std::vector<string>& shPSFunctions, std::vector<SShaderShConstant>& shPSGlobalVars, std::vector<SShaderShTexture>& shPSSamplers);
  bool GenerateGraphShaderStruct(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, string& shStruct, bool bOutput);
  int  ValidateGraphShader(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges);
  void ReferencedBlock_r(CShader *ef, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges, SShaderShBlock *pBL, int& nBlocks);
  void GenerateCode_r(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges, SShaderShBlock *pBL, std::vector<string>& codeLines, std::vector<string>& shFunctions, std::vector<SShaderShConstant>& shGlobalVars, std::vector<SShaderShTexture>& shSamplers);
  int  ApplyCode(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges, SShaderShBlock *pBL, TArray<SShaderConnect>& fnParams, std::vector<string>& codeLines, std::vector<string>& shFunctions, std::vector<SShaderShConstant>& shGlobalVars, std::vector<SShaderShTexture>& shSamplers);

public:
  void Init();
  void ShutDown();

  CShader *ParseGraph(char *buf, CShader *ef, CShader *efGen, uint nMaskGen);
};

#endif

