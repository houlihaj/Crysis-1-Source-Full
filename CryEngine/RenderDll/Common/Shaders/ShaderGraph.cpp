/*=============================================================================
  ShaderGraph.cpp : implementation of the Shaders graph parser part of shaders manager.
  Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"

#if defined(WIN32) || defined(WIN64)
#include <direct.h>
#include <io.h>
#elif defined(LINUX)
#endif

SShaderGraphNode::~SShaderGraphNode()
{
  SAFE_DELETE(m_pFunction);
}

SShaderGraphBlock::~SShaderGraphBlock()
{
  FXShaderGraphNodeItor it;

  for (it=m_Nodes.begin(); it!=m_Nodes.end(); it++)
  {
    SShaderGraphNode *pNode = *it;
    SAFE_DELETE(pNode);
  }
  m_Nodes.clear();
}

void CShaderGraph::ShutDown(void)
{
  FXShaderGraphBlocksItor it;

  for (it=m_Blocks.begin(); it!=m_Blocks.end(); it++)
  {
    SShaderGraphBlock *pBL = *it;
    SAFE_DELETE(pBL);
  }
  m_Blocks.clear();
}

void CShaderGraph::Init(void)
{
  InitNodes();
  //CShader *ef = gRenDev->m_cEF.mfForName("Composer::Example", 0);
}

SShaderGraphNode *CShaderGraph::GetGraphNode(string& classBlock, string& classNode)
{
  int j, n;
  SShaderGraphNode *pND = NULL;

  for (j=0; j<m_Blocks.size(); j++)
  {
    SShaderGraphBlock *pGBL = m_Blocks[j];
    if (classBlock == pGBL->m_ClassName)
    {
      for (n=0; n<pGBL->m_Nodes.size(); n++)
      {
        pND = pGBL->m_Nodes[n];
        if (pND->m_Name == classNode)
          break;
      }
      if (n == pGBL->m_Nodes.size())
        pND = NULL;
      break;
    }
  }
  return pND;
}

bool CShaderGraph::ParseNode_Properties(char *buf, SShaderGraphNode *pND)
{
  char szValue[128];
  char szName[32];
  char szType[32];

  if (!pND || !buf)
    return false;

  SkipCharacters(&buf, kWhiteSpace);
  while (*buf)
  {
    szName[0] = 0;
    szValue[0] = 0;
    szType[0] = 0;
    shFill(&buf, szType, 32);
    SkipCharacters(&buf, kWhiteSpace);
    shFill(&buf, szName, 32);
    SkipCharacters(&buf, kWhiteSpace);
    assert (*buf == '=');
    if (*buf != '=')
      return false;
    buf++;
    SkipCharacters(&buf, kWhiteSpace);
    fxFill(&buf, szValue, 128);
    assert (szName[0] && szValue[0] && szType[0]);
    SShaderParam pr;
    strncpy(pr.m_Name, szName, 32);
    strlwr(pr.m_Name);
    if (!stricmp(szType, "float"))
    {
      pr.m_Type = eType_FLOAT;
      pr.m_Value.m_Float = atof(szValue);
    }
    else
    {
      assert(0);
      iLog->Log("Error: Unknown property '%s' type '%s' for node '%s'", szName, szType, pND->m_Name.c_str());
    }
    pND->m_Properties.push_back(pr);
    SkipCharacters(&buf, kWhiteSpace);
  }
  return true;
}

bool CShaderGraph::ParseNode(char *szCode, SShaderGraphNode *pND)
{
  char* name;
  long cmd;
  char *params;
  char *data;

  enum {eType=1, eFormat, eEditable, eIOSemantic, eFunction, eProperties};
  static STokenDesc commands[] =
  {
    {eType, "Type"},
    {eFormat, "Format"},
    {eEditable, "Editable"},
    {eIOSemantic, "IOSemantic"},
    {eFunction, "Function"},
    {eProperties, "Properties"},
    {0,0}
  };

  if (!pND)
    return false;

  bool bOk = true;
  while ((cmd = shGetObject (&szCode, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;
    switch (cmd)
    {
      case eType:
        if (!data || !data[0])
        {
          Warning("Warning: missing Type parameter for Node in Shader graph (Skipping)");
          break;
        }
        if (!strnicmp(data, "Out", 3))
          pND->m_eType = eGrNode_Output;
        else
        if (!strnicmp(data, "In", 2))
          pND->m_eType = eGrNode_Input;
        else
          Warning("Warning: unknown Type parameter '%d' for Node in Shader graph '%s'", (int)cmd,data);
        break;

      case eFormat:
        if (!data || !data[0])
        {
          Warning("Warning: missing Format parameter for Node in Shader graph (Skipping)");
          break;
        }
        if (!stricmp(data, "Float"))
          pND->m_eFormat = eGrNodeFormat_Float;
        else
        if (!stricmp(data, "Vector"))
          pND->m_eFormat = eGrNodeFormat_Vector;
        else
        if (!stricmp(data, "Matrix"))
          pND->m_eFormat = eGrNodeFormat_Matrix;
        else
        if (!stricmp(data, "Int"))
          pND->m_eFormat = eGrNodeFormat_Int;
        else
        if (!stricmp(data, "Bool"))
          pND->m_eFormat = eGrNodeFormat_Bool;
        else
        if (!stricmp(data, "Texture2D"))
          pND->m_eFormat = eGrNodeFormat_Texture2D;
        else
        if (!stricmp(data, "Texture3D"))
          pND->m_eFormat = eGrNodeFormat_Texture3D;
        else
        if (!stricmp(data, "TextureCUBE"))
          pND->m_eFormat = eGrNodeFormat_TextureCUBE;
        else
        {
          assert(0);
          Warning("Warning: unknown Format parameter '%d' for Node in Shader graph '%s'", (int)cmd, data);
        }
        break;

      case eIOSemantic:
        if (!data || !data[0])
        {
          Warning("Warning: missing IOSemantic parameter for Node in Shader graph (Skipping)");
          break;
        }
        if (!strnicmp(data, "VertexPos", 9))
          pND->m_eSemantic = eGrNodeIOSemantic_VPos;
        else
        if (!stricmp(data, "Color0"))
          pND->m_eSemantic = eGrNodeIOSemantic_Color0;
        else
        if (!stricmp(data, "Color1"))
          pND->m_eSemantic = eGrNodeIOSemantic_Color1;
        else
        if (!stricmp(data, "Color2"))
          pND->m_eSemantic = eGrNodeIOSemantic_Color2;
        else
        if (!stricmp(data, "Color3"))
          pND->m_eSemantic = eGrNodeIOSemantic_Color3;
        else
        if (!stricmp(data, "VertexNormal"))
          pND->m_eSemantic = eGrNodeIOSemantic_Normal;
        else
        if (!stricmp(data, "VertexTexCoord0"))
          pND->m_eSemantic = eGrNodeIOSemantic_TexCoord0;
        else
        if (!stricmp(data, "VertexTexCoord1"))
          pND->m_eSemantic = eGrNodeIOSemantic_TexCoord1;
        else
        if (!stricmp(data, "VertexTexCoord2"))
          pND->m_eSemantic = eGrNodeIOSemantic_TexCoord2;
        else
        if (!stricmp(data, "VertexTexCoord3"))
          pND->m_eSemantic = eGrNodeIOSemantic_TexCoord3;
        else
        if (!stricmp(data, "VertexTexCoord4"))
          pND->m_eSemantic = eGrNodeIOSemantic_TexCoord4;
        else
        if (!stricmp(data, "VertexTexCoord5"))
          pND->m_eSemantic = eGrNodeIOSemantic_TexCoord5;
        else
        if (!stricmp(data, "VertexTexCoord6"))
          pND->m_eSemantic = eGrNodeIOSemantic_TexCoord6;
        else
        if (!stricmp(data, "VertexTexCoord7"))
          pND->m_eSemantic = eGrNodeIOSemantic_TexCoord7;
        else
        if (!stricmp(data, "VertexBinormal"))
          pND->m_eSemantic = eGrNodeIOSemantic_Binormal;
        else
        if (!stricmp(data, "VertexTangent"))
          pND->m_eSemantic = eGrNodeIOSemantic_Tangent;
        else
        {
//          assert(0);
          pND->m_eSemantic = eGrNodeIOSemantic_Custom;
          pND->m_CustomSemantics = data;
        }
        break;

      case eEditable:
        pND->m_bEditable = shGetBool(data);
        break;

      case eFunction:
        {
          SShaderGraphFunction *pFunc = new SShaderGraphFunction;
          pFunc->m_Data = data;
          pND->m_pFunction = pFunc;

          char strF[1024], strFunc[256], stIType[16], stType[32], st[128];
          const char *c = strchr(data, ')');
          int nLen = c-data+1;
          strncpy(strF, data, nLen);
          strF[nLen] = 0;
          char *s = strF;
          shFill(&s, strFunc, 1024);
          if (strcmp(strFunc, "void"))
          {
            assert(0);
          }
          else
          {
            fxFillPr(&s, strFunc);
            pFunc->m_Name = strFunc;
            while(*s != '(') {s++;}
            s++;
            while(true)
            {
              char *ss = fxFillPr(&s, stIType);
              if (!*ss)
                break;
              fxFillPr(&s, stType);
              if (strncmp(stType, "float", 5) && strncmp(stType, "sampler", 7))
              {
                assert (0);
              }
              fxFillPr(&s, st);
              if (!strcmp(stIType, "in"))
              {
                pFunc->szInTypes.push_back(stType);
                pFunc->inParams.push_back(st);
              }
              else
              if (!strcmp(stIType, "out"))
              {
                pFunc->szOutTypes.push_back(stType);
                pFunc->outParams.push_back(st);
              }
              else
              {
                assert(0);
              }
            }
          }
        }
        break;

      case eProperties:
        bOk &= ParseNode_Properties(params, pND);
        break;
    }
  }

  return bOk;
}

bool CShaderGraph::ParseBlock(char *szCode, SShaderGraphBlock *pBL)
{
  char* name;
  long cmd;
  char *params;
  char *data;

  enum {eType=1, eNode};
  static STokenDesc commands[] =
  {
    {eNode, "Node"},
    {eType, "Type"},
    {0,0}
  };

  if (!pBL)
    return false;

  bool bOk = true;
  while ((cmd = shGetObject (&szCode, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;
    switch (cmd)
    {
      case eNode:
        {
          SShaderGraphNode *pND = new SShaderGraphNode;
          pND->m_Name = name;
          bool bRes = ParseNode(params, pND);
          if (bRes)
            pBL->m_Nodes.push_back(pND);
          else
          {
            delete pND;
            bOk = false;
          }
        }
        break;

      case eType:
        if (!data || !data[0])
        {
          Warning("Warning: missing Type parameter for Block in Shader graph (Skipping)");
          break;
        }
        if (!stricmp(data, "VertexInput"))
          pBL->m_eType = eGrBlock_VertexInput;
        else
        if (!stricmp(data, "VertexOutput"))
          pBL->m_eType = eGrBlock_VertexOutput;
        else
        if (!stricmp(data, "PixelInput"))
          pBL->m_eType = eGrBlock_PixelInput;
        else
        if (!stricmp(data, "PixelOutput"))
          pBL->m_eType = eGrBlock_PixelOutput;
        else
        if (!stricmp(data, "Texture"))
          pBL->m_eType = eGrBlock_Texture;
        else
        if (!stricmp(data, "Sampler"))
          pBL->m_eType = eGrBlock_Sampler;
        else
        if (!stricmp(data, "Function"))
          pBL->m_eType = eGrBlock_Function;
        else
        if (!stricmp(data, "Constant"))
          pBL->m_eType = eGrBlock_Constant;
        else
          Warning("Warning: unknown Type parameter '%d' for Block in Shader graph '%s'", (int)cmd, data);
        pBL->m_ClassName = data;
        break;
    }
  }

  return bOk;
}

bool CShaderGraph::ParseNodes(char *szCode)
{
  char* name;
  long cmd;
  char *params;

  enum {eBlock=1};
  static STokenDesc commands[] =
  {
    {eBlock, "Block"},
    {0,0}
  };

  bool bOk = true;
  while ((cmd = shGetObject (&szCode, commands, &name, &params)) > 0)
  {
    switch (cmd)
    {
      case eBlock:
        {
          SShaderGraphBlock *bl = new SShaderGraphBlock;
          bool bRes = ParseBlock(params, bl);
          if (bRes)
            m_Blocks.push_back(bl);
          else
          {
            bOk = false;
            delete bl;
          }
        }
        break;
    }
  }

  return bOk;
}

bool CShaderGraph::ParseNodesForFile(const char *szName)
{
  FILE *fp = iSystem->GetIPak()->FOpen(szName, "r");
  if (!fp)
    return false;
  iSystem->GetIPak()->FSeek(fp, 0, SEEK_END);
  int len = iSystem->GetIPak()->FTell(fp);
  char *buf = new char [len+1];
  iSystem->GetIPak()->FSeek(fp, 0, SEEK_SET);
  len = iSystem->GetIPak()->FRead(buf, len, fp);
  iSystem->GetIPak()->FClose(fp);
  buf[len] = 0;
  RemoveCR(buf);
  ParseNodes(buf);
  SAFE_DELETE_ARRAY(buf);

  return true;
}

void CShaderGraph::InitNodesForPath(const char *szPath)
{
  struct _finddata_t fileinfo;
  intptr_t handle;
  char nmf[256];
  char dirn[256];

  strcpy(dirn, szPath);
  strcat(dirn, "*.cnd");

  handle = iSystem->GetIPak()->FindFirst (dirn, &fileinfo);
  if (handle == -1)
    return;
  do
  {
    if (fileinfo.name[0] == '.')
      continue;
    if (fileinfo.attrib & _A_SUBDIR)
    {
      sprintf(nmf, "%s%s/", szPath, fileinfo.name);
      InitNodesForPath(nmf);
      continue;
    }
    strcpy(nmf, szPath);
    strcat(nmf, fileinfo.name);
    ParseNodesForFile(nmf);
  } while (iSystem->GetIPak()->FindNext(handle, &fileinfo) != -1);

  iSystem->GetIPak()->FindClose (handle);
}

void CShaderGraph::InitNodes(void)
{
  char path[256];
  m_Blocks.clear();

  sprintf(path, "%sComposer/Nodes/", gRenDev->m_cEF.m_ShadersPath);
  InitNodesForPath(path);
}

bool CShaderGraph::ParseShBlock_ConnectionProperties(char *buf, CShader *ef, SShaderShBlockConnection *pCN, SShaderShBlock *pBL)
{
  char value[128];
  char name[32];
  int i;

  if (!pCN || !buf || !pBL)
    return false;

  SkipCharacters(&buf, kWhiteSpace);
  while (*buf)
  {
    name[0] = 0;
    value[0] = 0;
    shFill(&buf, name, 32);
    SkipCharacters(&buf, kWhiteSpace);
    assert (*buf == '=');
    if (*buf != '=')
      return false;
    buf++;
    SkipCharacters(&buf, kWhiteSpace);
    fxFill(&buf, value, 128);
    assert (name[0] && value[0]);
    SShaderGraphNode *pND = GetGraphNode(pBL->m_ClassName, pCN->m_ClassName);
    assert(pND);
    if (pND)
    {
      for (i=0; i<pND->m_Properties.size(); i++)
      {
        SShaderParam *pr = &pND->m_Properties[i];
        if (!stricmp(pr->m_Name, name))
          break;
      }
      if (i == pND->m_Properties.size())
      {
        assert(0);
        iLog->Log("Error: Couldn't find connection property declaration in node '%s' for block '%s' (Shader '%s')", pCN->m_ClassName.c_str(), pBL->m_ClassName.c_str(), ef->GetName());
      }
      SShaderParam *pr = &pND->m_Properties[i];
      pCN->m_Properties.push_back(pND->m_Properties[i]);
      UParamVal prv;
      switch (pr->m_Type)
      {
        case eType_FLOAT:
          prv.m_Float = atof(value);
          break;
        default:
          assert(0);
      }
      bool bRes = SShaderParam::SetParam(name, &pCN->m_Properties, prv);
      assert (bRes);
    }
    SkipCharacters(&buf, kWhiteSpace);
  }
  return true;
}

bool CShaderGraph::ParseShBlock_Connections(char *buf, CShader *ef, SShaderShBlock *pBL, SShaderShBlockConnections *pCN)
{
  bool bOk = true;

  /*long cmd;
  char *data;
  char *value;
  char *assign;
  char *annot;
  char *name;
  std::vector<SFXStruct> Structs;

  if (!pBL || !pCN)
    return false;

  enum {eConnection=1};
  static SFXTokenDesc commands[] =
  {
    {eConnection, "Connection"},
    {0,0}
  };

  while ((cmd = fxGetObject (&buf, commands, &name, &value, &data, &assign, &annot, Structs)) > 0)
  {
    switch (cmd)
    {
      case eConnection:
        {
          SShaderShBlockConnection CN;
          CN.m_Name = name;
          CN.m_ClassName = assign;
          if (data)
            bOk &= ParseShBlock_ConnectionProperties(data, ef, &CN, pBL);
          pCN->m_Connections.push_back(CN);
        }
        break;
    }
  }*/

  return bOk;
}

bool CShaderGraph::ParseShBlock(char *buf, CShader *ef, SShaderShBlock *pBL)
{
  char* name;
  long cmd;
  char *params, *data;

  if (!pBL)
    return false;

  enum {eClass=1, eSubClass, eId, eInput, eOutput};
  static STokenDesc commands[] =
  {
    {eClass, "Class"},
    {eSubClass, "SubClass"},
    {eId, "Id"},
    {eInput, "Input"},
    {eOutput, "Output"},
    {0,0}
  };

  bool bOk = true;
  while ((cmd = shGetObject (&buf, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;
    switch (cmd)
    {
      case eClass:
        pBL->m_ClassName = data;
        break;

      case eSubClass:
        pBL->m_SubClassName = data;
        break;

      case eId:
        pBL->m_Id = shGetInt(data);
        break;

      case eInput:
        bOk &= ParseShBlock_Connections(params, ef, pBL, &pBL->m_InputConnections);
        break;

      case eOutput:
        bOk &= ParseShBlock_Connections(params, ef, pBL, &pBL->m_OutputConnections);
        break;
    }
  }

  return bOk;
}

bool CShaderGraph::ParseShEdge(char *buf, CShader *ef, SShaderShEdge *pEdge)
{
  char* name;
  long cmd;
  char *params, *data;

  if (!pEdge)
    return false;

  enum {eOutId=1, eInId, ePortOut, ePortIn};
  static STokenDesc commands[] =
  {
    {eOutId, "OutId"},
    {eInId, "InId"},
    {ePortOut, "PortOut"},
    {ePortIn, "PortIn"},
    {0,0}
  };

  int nMask = 0;
  while ((cmd = shGetObject (&buf, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;
    switch (cmd)
    {
      case eOutId:
        pEdge->m_Id[1] = shGetInt(data);
        nMask |= 1;
        break;

      case eInId:
        pEdge->m_Id[0] = shGetInt(data);
        nMask |= 2;
        break;

      case ePortIn:
        pEdge->m_Port[0] = data;
        nMask |= 4;
        break;

      case ePortOut:
        pEdge->m_Port[1] = data;
        nMask |= 8;
        break;
    }
  }
  if (nMask == 0xf)
    return true;
  return false;
}

bool CShaderGraph::ParseShEdges(char *buf, CShader *ef, TArray<SShaderShEdge *>& ShEdges)
{
  char* name;
  long cmd;
  char *params;

  enum {eEdge=1};
  static STokenDesc commands[] =
  {
    {eEdge, "Edge"},
    {0,0}
  };

  bool bOk = true;
  while ((cmd = shGetObject (&buf, commands, &name, &params)) > 0)
  {
    switch (cmd)
    {
      case eEdge:
        {
          SShaderShEdge *pEdge = new SShaderShEdge;
          bool bRes = ParseShEdge(params, ef, pEdge);
          if (bRes)
            ShEdges.AddElem(pEdge);
          else
          {
            delete pEdge;
            bOk = false;
          }
        }
        break;
    }
  }

  return bOk;
}

int CShaderGraph::ValidateGraphShader(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges)
{
  int i, j;

  if (!ShEdges.Num())
    return -1;

  for (i=0; i<ShEdges.Num(); i++)
  {
    SShaderShEdge *pEdge = ShEdges[i];
    int nBlockIn = -1;
    int nBlockOut = -1;
    for (j=0; j<ShBlocks.Num(); j++)
    {
      SShaderShBlock *pBL = ShBlocks[j];
      if (pEdge->m_Id[0] == pBL->m_Id)
        break;
    }
    if (j == ShBlocks.Num())
      return 1;
    nBlockIn = j;

    for (j=0; j<ShBlocks.Num(); j++)
    {
      SShaderShBlock *pBL = ShBlocks[j];
      if (pEdge->m_Id[1] == pBL->m_Id)
        break;
    }
    if (j == ShBlocks.Num())
      return 1;
    nBlockOut = j;

    if (nBlockIn == nBlockOut)
      return 5;

    SShaderShBlock *pBL = ShBlocks[nBlockIn];
    for (j=0; j<pBL->m_InputConnections.m_Connections.size(); j++)
    {
      SShaderShBlockConnection *pCN = &pBL->m_InputConnections.m_Connections[j];
      if (pEdge->m_Port[0] == pCN->m_Name)
        break;
    }
    if (j == pBL->m_InputConnections.m_Connections.size())
      return 2;

    pBL = ShBlocks[nBlockOut];
    for (j=0; j<pBL->m_OutputConnections.m_Connections.size(); j++)
    {
      SShaderShBlockConnection *pCN = &pBL->m_OutputConnections.m_Connections[j];
      if (pEdge->m_Port[1] == pCN->m_Name)
        break;
    }
    if (j == pBL->m_OutputConnections.m_Connections.size())
      return 2;
  }

  for (i=0; i<ShBlocks.Num(); i++)
  {
    SShaderShBlock *pBL = ShBlocks[i];
    string szClass;
    string szNode;
    szClass = pBL->m_ClassName;
    szNode = pBL->m_SubClassName;
    for (j=0; j<m_Blocks.size(); j++)
    {
      SShaderGraphBlock *pGBL = m_Blocks[j];
      if (szClass == pGBL->m_ClassName)
      {
        if (!szNode.empty())
        {
          int n;
          for (n=0; n<pGBL->m_Nodes.size(); n++)
          {
            SShaderGraphNode *pND = pGBL->m_Nodes[n];
            if (pND->m_Name == szNode)
              break;
          }
          if (n == pGBL->m_Nodes.size())
            return 4;

        }
        pBL->m_pGraphBlock = pGBL;
        break;
      }
    }
    if (j == m_Blocks.size())
      return 3;
  }

  return 0;
}

bool CShaderGraph::GenerateGraphShaderStruct(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, string& shStruct, bool bOutput)
{
  int i, j, n;
  std::vector<string> shStructElems;

  for (i=0; i<ShBlocks.Num(); i++)
  {
    SShaderShBlock *pBL = ShBlocks[i];
    SShaderGraphBlock *pGB = pBL->m_pGraphBlock;
    if ((!bPixel && !bOutput && pGB->m_eType == eGrBlock_VertexInput) ||
        (!bPixel &&  bOutput && pGB->m_eType == eGrBlock_VertexOutput) ||
        ( bPixel && !bOutput && pGB->m_eType == eGrBlock_PixelInput) ||
        ( bPixel &&  bOutput && pGB->m_eType == eGrBlock_PixelOutput))
    {
      SShaderShBlockConnections *pCons = bOutput ? &pBL->m_InputConnections : &pBL->m_OutputConnections;
      for (j=0; j<pCons->m_Connections.size(); j++)
      {
        SShaderShBlockConnection *pCN = &pCons->m_Connections[j];
        SShaderGraphNode *pND = NULL;
        for (n=0; n<pGB->m_Nodes.size(); n++)
        {
          pND = pGB->m_Nodes[n];
          if (pND->m_Name == pCN->m_ClassName)
            break;
        }
        if (n == pGB->m_Nodes.size())
          return false;
        string elem;
        switch (pND->m_eFormat)
        {
          case eGrNodeFormat_Float:
            elem += "float ";
            break;
          case eGrNodeFormat_Vector:
            elem += "float4 ";
            break;
          default:
            assert(0);
        }
        elem += pND->m_Name;
        elem += " : ";
        switch (pND->m_eSemantic)
        {
          case eGrNodeIOSemantic_VPos:
            elem += "POSITION";
            break;
          case eGrNodeIOSemantic_Color0:
            elem += "COLOR0";
            break;
          case eGrNodeIOSemantic_Color1:
            elem += "COLOR1";
            break;
          case eGrNodeIOSemantic_Normal:
            elem += "NORMAL";
            break;
          case eGrNodeIOSemantic_Tangent:
            elem += "TANGENT";
            break;
          case eGrNodeIOSemantic_Binormal:
            elem += "BINORMAL";
            break;
          case eGrNodeIOSemantic_TexCoord0:
            elem += "TEXCOORD0";
            break;
          case eGrNodeIOSemantic_TexCoord1:
            elem += "TEXCOORD1";
            break;
          case eGrNodeIOSemantic_TexCoord2:
            elem += "TEXCOORD2";
            break;
          case eGrNodeIOSemantic_TexCoord3:
            elem += "TEXCOORD3";
            break;
          case eGrNodeIOSemantic_TexCoord4:
            elem += "TEXCOORD4";
            break;
          case eGrNodeIOSemantic_TexCoord5:
            elem += "TEXCOORD5";
            break;
          case eGrNodeIOSemantic_TexCoord6:
            elem += "TEXCOORD6";
            break;
          case eGrNodeIOSemantic_TexCoord7:
            elem += "TEXCOORD7";
            break;
          default:
            assert (0);
        }
        elem += ';';
        shStructElems.push_back(elem);
      }
    }
  }
  if (shStructElems.size())
  {
    if (!bPixel)
    {
      if (!bOutput)
        shStruct = "struct inputVS\n{\n";
      else
        shStruct = "struct outputVS\n{\n";
    }
    else
    {
      if (!bOutput)
        shStruct = "struct inputPS\n{\n";
      else
        shStruct = "struct outputPS\n{\n";
    }
    for (i=0; i<shStructElems.size(); i++)
    {
      shStruct += "  ";
      shStruct += shStructElems[i];
      shStruct += "\n";
    }
    shStruct += "};\n";
  }
  else
    return false;

  return true;
}

void CShaderGraph::ReferencedBlock_r(CShader *ef, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges, SShaderShBlock *pBL, int& nBlocks)
{
  if (pBL->m_bReferenced == 1)
    return;
  nBlocks++;
  pBL->m_bReferenced = 1;
  int i, j;
  int nId = -1;
  for (i=0; i<pBL->m_InputConnections.m_Connections.size(); i++)
  {
    SShaderShBlockConnection *pCN = &pBL->m_InputConnections.m_Connections[i];
    SShaderShEdge *pEdge = NULL;
    for (j=0; j<ShEdges.Num(); j++)
    {
      pEdge = ShEdges[j];
      nId = -1;
      if (pEdge->m_Id[0] == pBL->m_Id)
        nId = 0;
      else
      if (pEdge->m_Id[1] == pBL->m_Id)
        nId = 1;
      if (nId == -1)
        continue;
      if (pEdge->m_Port[nId] == pCN->m_Name)
        break;
    }
    if (j != ShEdges.Num())
    {
      pCN->m_bUsed = true;
      int nId1 = pEdge->m_Id[1-nId];
      SShaderShBlock *pBL1 = NULL;
      for (j=0; j<ShBlocks.Num(); j++)
      {
        pBL1 = ShBlocks[j];
        if (pBL1->m_Id == nId1)
          break;
      }
      if (j == ShBlocks.Num())
        continue;
      if (!pBL1->m_bReferenced)
        ReferencedBlock_r(ef, ShBlocks, ShEdges, pBL1, nBlocks);
    }
  }

  for (i=0; i<pBL->m_OutputConnections.m_Connections.size(); i++)
  {
    SShaderShBlockConnection *pCN = &pBL->m_OutputConnections.m_Connections[i];
    SShaderShEdge *pEdge = NULL;
    int nId = -1;
    for (j=0; j<ShEdges.Num(); j++)
    {
      pEdge = ShEdges[j];
      nId = -1;
      if (pEdge->m_Id[0] == pBL->m_Id)
        nId = 0;
      else
        if (pEdge->m_Id[1] == pBL->m_Id)
          nId = 1;
      if (nId == -1)
        continue;
      if (pEdge->m_Port[nId] == pCN->m_Name)
        break;
    }
    if (j != ShEdges.Num())
    {
      pCN->m_bUsed = true;
      int nId1 = pEdge->m_Id[1-nId];
      SShaderShBlock *pBL1 = NULL;
      for (j=0; j<ShBlocks.Num(); j++)
      {
        pBL1 = ShBlocks[j];
        if (pBL1->m_Id == nId1)
          break;
      }
      if (j == ShBlocks.Num())
        continue;
      if (!pBL1->m_bReferenced)
        ReferencedBlock_r(ef, ShBlocks, ShEdges, pBL1, nBlocks);
    }
  }
}

bool CShaderGraph::DeclareConstant(SShaderShBlock *pBS, string& ClassName, string& Name, std::vector<SShaderShConstant>& shGlobalVars)
{
  int n;
  SShaderGraphNode *pN;
  for (n=0; n<pBS->m_pGraphBlock->m_Nodes.size(); n++)
  {
    pN = pBS->m_pGraphBlock->m_Nodes[n];
    if (pN->m_Name == ClassName.c_str())
      break;
  }
  if (n == pBS->m_pGraphBlock->m_Nodes.size())
  {
    assert(0);
    return false;
  }
  SShaderShConstant Const;
  Const.m_eFormat = pN->m_eFormat;
  Const.m_Name = Name;
  Const.m_Semantics = pN->m_CustomSemantics;
  Const.m_pGraphNode = pN;
  Const.m_pGraphBlock = pBS->m_pGraphBlock;
  if (pBS->m_OutputConnections.m_Connections.size())
    Const.m_Properties = pBS->m_OutputConnections.m_Connections[0].m_Properties;
  shGlobalVars.push_back(Const);

  return true;
}

int CShaderGraph::ApplyCode(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges, SShaderShBlock *pBL, TArray<SShaderConnect>& fnParams, std::vector<string>& codeLines, std::vector<string>& shFunctions, std::vector<SShaderShConstant>& shGlobalVars, std::vector<SShaderShTexture>& shSamplers)
{
  int i, j;
  char strF[1024];
  if (pBL && pBL->m_pGraphBlock && (pBL->m_pGraphBlock->m_eType == eGrBlock_VertexOutput || pBL->m_pGraphBlock->m_eType == eGrBlock_PixelOutput))
  {
    for (i=0; i<fnParams.Num(); i++)
    {
      SShaderConnect *pCN = &fnParams[i];
      if (pCN->m_pOutBlock->m_pGraphBlock->m_eType == eGrBlock_Constant)
      {
        bool bRes = DeclareConstant(pCN->m_pOutBlock, pCN->m_pOutConnection->m_ClassName, pCN->m_pOutConnection->m_Name, shGlobalVars);
        assert(bRes);
        sprintf(strF, "OUT.%s = %s", pCN->m_pInConnection->m_ClassName.c_str(), pCN->m_pOutConnection->m_Name.c_str());
      }
      else
        sprintf(strF, "OUT.%s = %s_ID%d", pCN->m_pInConnection->m_ClassName.c_str(), pCN->m_pOutConnection->m_Name.c_str(), pCN->m_pOutBlock->m_Id);
      codeLines.push_back(strF);
    }
    return 0;
  }

  assert(pBL && pBL->m_pGraphBlock && (pBL->m_pGraphBlock->m_eType == eGrBlock_Function || pBL->m_pGraphBlock->m_eType == eGrBlock_Sampler));
  if (!pBL || !pBL->m_pGraphBlock || (pBL->m_pGraphBlock->m_eType != eGrBlock_Function && pBL->m_pGraphBlock->m_eType != eGrBlock_Sampler))
    return 1;

  SShaderGraphNode *pND;
  for (i=0; i<pBL->m_pGraphBlock->m_Nodes.size(); i++)
  {
    pND = pBL->m_pGraphBlock->m_Nodes[i];
    if (pND->m_Name == pBL->m_SubClassName)
      break;
  }
  if (i == pBL->m_pGraphBlock->m_Nodes.size())
  {
    assert(0);
    return 2;
  }
  if (!pND->m_pFunction)
  {
    assert(0);
    return 3;
  }
  SShaderGraphFunction *pFunc = pND->m_pFunction;
  if (!pND->m_bWasAdded)
  {
    pND->m_bWasAdded = true;
    shFunctions.push_back(pFunc->m_Data.c_str());
  }

  // Output parameters declarations
  SShaderConnect *pC = NULL;
  std::vector<SShaderShBlockConnection *> outNames;
  for (i=0; i<pFunc->szOutTypes.size(); i++)
  {
    for (j=0; i<pBL->m_OutputConnections.m_Connections.size(); j++)
    {
      SShaderShBlockConnection *pCN = &pBL->m_OutputConnections.m_Connections[j];
      if (pCN->m_ClassName == pFunc->outParams[i])
        break;
    }
    assert (j < pBL->m_OutputConnections.m_Connections.size());
    if (j >= pBL->m_OutputConnections.m_Connections.size())
      return 1;
    SShaderShBlockConnection *pCN = &pBL->m_OutputConnections.m_Connections[j];
    outNames.push_back(pCN);
    sprintf(strF, "%s %s_ID%d", pFunc->szOutTypes[i].c_str(), pCN->m_Name.c_str(), pBL->m_Id);
    codeLines.push_back(strF);
  }

  // Function call
  sprintf(strF, "%s (", pFunc->m_Name.c_str());
  assert (pFunc->szInTypes.size() == fnParams.Num());
  for (i=0; i<pFunc->szInTypes.size(); i++)
  {
    for (j=0; j<fnParams.Num(); j++)
    {
      pC = &fnParams[j];
      if (pC->m_pInConnection->m_ClassName == pFunc->inParams[i].c_str())
        break;
    }
    assert (j < fnParams.Num());
    if (j >= fnParams.Num())
      return 1;
    bool bNoID = false;
    if (fnParams[j].m_pOutBlock->m_pGraphBlock)
    {
      SShaderGraphBlock *pB = fnParams[j].m_pOutBlock->m_pGraphBlock;
      if (pB->m_eType == eGrBlock_Constant)
      {
        SShaderShBlock *pBS = fnParams[j].m_pOutBlock;
        bool bRes = DeclareConstant(pBS, fnParams[j].m_pOutConnection->m_ClassName, fnParams[j].m_pOutConnection->m_Name, shGlobalVars);
        if (!bRes)
          return 4;
        bNoID = true;
      }
      else
      if (pB->m_eType == eGrBlock_Texture)
      {
        SShaderShBlock *pBL1 = fnParams[j].m_pOutBlock;
        SShaderShTexture shTex;
        assert(pBL1->m_OutputConnections.m_Connections.size() == 1);
        if (pBL1->m_OutputConnections.m_Connections.size() == 1)
        {
          shTex.m_Name = pBL1->m_OutputConnections.m_Connections[0].m_Name;
          shTex.m_pGraphBlock = pBL1->m_pGraphBlock;
          int n;
          SShaderGraphNode *pN = NULL;
          for (n=0; n<pBL1->m_pGraphBlock->m_Nodes.size(); n++)
          {
            pN = pBL1->m_pGraphBlock->m_Nodes[n];
            if (pN->m_Name == fnParams[j].m_pOutConnection->m_ClassName.c_str())
              break;
          }
          if (n == pBL1->m_pGraphBlock->m_Nodes.size())
            return 4;
          shTex.m_pGraphNode = pN;
          shSamplers.push_back(shTex);
          bNoID = true;
        }
      }
    }

    int nS = strlen(strF);
    if (i)
    {
      strF[nS++] =',';
      strF[nS++] = 0x20;
    }

    if (bNoID)
      sprintf(&strF[nS], "%s", fnParams[j].m_pOutConnection->m_Name.c_str());
    else
      sprintf(&strF[nS], "%s_ID%d", fnParams[j].m_pOutConnection->m_Name.c_str(), fnParams[j].m_pOutConnection->m_IdVar);
  }
  for (i=0; i<pFunc->szOutTypes.size(); i++)
  {
    int nS = strlen(strF);
    if (i || pFunc->szInTypes.size())
    {
      strF[nS++] =',';
      strF[nS++] = 0x20;
    }
    sprintf(&strF[nS], "%s_ID%d", outNames[i]->m_Name.c_str(), pBL->m_Id);
  }
  strcat(strF, ")");
  codeLines.push_back(strF);

  return 0;
}

void CShaderGraph::GenerateCode_r(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges, SShaderShBlock *pBL, std::vector<string>& shCodeLines, std::vector<string>& shFunctions, std::vector<SShaderShConstant>& shGlobalVars, std::vector<SShaderShTexture>& shSamplers)
{
  if (pBL->m_bReferenced == 2)
    return;
  pBL->m_bReferenced = 2;
  int i, j;
  char str[1024];
  TArray<SShaderConnect> ShConns;
  for (i=0; i<pBL->m_InputConnections.m_Connections.size(); i++)
  {
    SShaderShBlockConnection *pCNIn = &pBL->m_InputConnections.m_Connections[i];
    SShaderShEdge *pEdge;
    for (j=0; j<ShEdges.Num(); j++)
    {
      pEdge = ShEdges[j];
      if (pEdge->m_Id[0] == pBL->m_Id && pEdge->m_Port[0] == pCNIn->m_Name)
        break;
    }
    assert(j < ShEdges.Num());

    SShaderShBlock *pBL1;
    for (j=0; j<ShBlocks.Num(); j++)
    {
      pBL1 = ShBlocks[j];
      if (pBL1->m_Id == pEdge->m_Id[1])
        break;
    }
    assert(j<ShBlocks.Num());

    SShaderShBlockConnection *pCNOut;
    for (j=0; j<pBL1->m_OutputConnections.m_Connections.size(); j++)
    {
      pCNOut = &pBL1->m_OutputConnections.m_Connections[j];
      if (pCNOut->m_Name == pEdge->m_Port[1])
        break;
    }
    assert(j<pBL1->m_OutputConnections.m_Connections.size());
    if (pCNOut->m_IdVar < 0)
      GenerateCode_r(ef, bPixel, ShBlocks, ShEdges, pBL1, shCodeLines, shFunctions, shGlobalVars, shSamplers);
    assert(pCNOut->m_IdVar >= 0);
    SShaderConnect cn;
    cn.m_pInConnection = pCNIn;
    cn.m_pInBlock = pBL;
    cn.m_pOutConnection = pCNOut;
    cn.m_pOutBlock = pBL1;
    ShConns.AddElem(cn);
  }
  if (ShConns.Num())
  {
    int nResult = ApplyCode(ef, bPixel, ShBlocks, ShEdges, pBL, ShConns, shCodeLines, shFunctions, shGlobalVars, shSamplers);
    if (nResult)
      return;
  }

  for (i=0; i<pBL->m_OutputConnections.m_Connections.size(); i++)
  {
    SShaderShBlockConnection *pCN = &pBL->m_OutputConnections.m_Connections[i];
    pCN->m_IdVar = pBL->m_Id;
    if (pBL->m_pGraphBlock && (pBL->m_pGraphBlock->m_eType == eGrBlock_VertexInput || pBL->m_pGraphBlock->m_eType == eGrBlock_PixelInput))
    {
      sprintf(str, "float4 %s_ID%d = IN.%s", pCN->m_Name.c_str(), pBL->m_Id, pCN->m_ClassName.c_str());
      shCodeLines.push_back(str);
    }
  }
  for (i=0; i<pBL->m_OutputConnections.m_Connections.size(); i++)
  {
    SShaderShBlockConnection *pCN = &pBL->m_OutputConnections.m_Connections[i];
    SShaderShEdge *pEdge;
    for (j=0; j<ShEdges.Num(); j++)
    {
      pEdge = ShEdges[j];
      if (pEdge->m_Id[1] == pBL->m_Id && pEdge->m_Port[1] == pCN->m_Name)
        break;
    }
    assert(j < ShEdges.Num());

    SShaderShBlock *pBL1;
    for (j=0; j<ShBlocks.Num(); j++)
    {
      pBL1 = ShBlocks[j];
      if (pBL1->m_Id == pEdge->m_Id[0])
        break;
    }
    assert(j<ShBlocks.Num());
    GenerateCode_r(ef, bPixel, ShBlocks, ShEdges, pBL1, shCodeLines, shFunctions, shGlobalVars, shSamplers);
  }
}

bool CShaderGraph::GenerateGraphShader(CShader *ef, bool bPixel, TArray<SShaderShBlock *>& ShBlocks, TArray<SShaderShEdge *>& ShEdges, std::vector<string>& shCodeLines, string& shInputStruct, string& shOutputStruct, std::vector<string>& shFunctions, std::vector<SShaderShConstant>& shGlobalVars, std::vector<SShaderShTexture>& shSamplers)
{
  int nRes = ValidateGraphShader(ef, bPixel, ShBlocks, ShEdges);
  if (nRes)
  {
    assert(0);
    return false;
  }
  string shBody;
  string shCode;
  int i;
  bool bRes = true;

  // Generate shader input struct
  bRes = GenerateGraphShaderStruct(ef, bPixel, ShBlocks, shInputStruct, false);

  // Generate shader output struct
  bRes &= GenerateGraphShaderStruct(ef, bPixel, ShBlocks, shOutputStruct, true);
  if (!bRes)
    return false;

  // Optimise graph starting from output structure
  // Remove non-referenced blocks
  SShaderShBlock *pBL = NULL;
  for (i=0; i<ShBlocks.Num(); i++)
  {
    pBL = ShBlocks[i];
    if ((bPixel && pBL->m_pGraphBlock->m_eType == eGrBlock_PixelOutput) || (!bPixel && pBL->m_pGraphBlock->m_eType == eGrBlock_VertexOutput))
      break;
  }
  if (!pBL)
    return false;
  int nBlocks = 0;
  ReferencedBlock_r(ef, ShBlocks, ShEdges, pBL, nBlocks);

  // Generate shader body starting from input structure
  for (i=0; i<ShBlocks.Num(); i++)
  {
    pBL = ShBlocks[i];
    if ((bPixel && pBL->m_pGraphBlock->m_eType == eGrBlock_PixelInput) || (!bPixel && pBL->m_pGraphBlock->m_eType == eGrBlock_VertexInput))
      break;
  }
  if (!pBL)
    return false;
  m_VarCounter = 0;
  GenerateCode_r(ef, bPixel, ShBlocks, ShEdges, pBL, shCodeLines, shFunctions, shGlobalVars, shSamplers);

  return true;
}

static bool sConstantString(SShaderShConstant *pConst, char *str)
{
  char st[1024];

  if (!pConst || !str)
    return false;
  switch (pConst->m_eFormat)
  {
    case eGrNodeFormat_Float:
      strcpy(str, "float ");
      break;
    case eGrNodeFormat_Vector:
      strcpy(str, "float4 ");
      break;
    case eGrNodeFormat_Matrix:
      strcpy(str, "float4x4 ");
      break;
    default:
      assert(0);
  }
  strcat(str, pConst->m_Name.c_str());
  if (!pConst->m_Semantics.empty())
  {
    strcat(str, " : ");
    strcat(str, pConst->m_Semantics.c_str());
  }
  else
  if (pConst->m_eFormat == eGrNodeFormat_Float && pConst->m_pGraphNode)
  {
    float vDefault[4], vMin[4], vMax[4], vStep[4];
    bool bResDefault = pConst->GetValue("default", vDefault);
    bool bResMin = pConst->GetValue("Min", vMin);
    bool bResMax = pConst->GetValue("Max", vMax);
    bool bResStep = pConst->GetValue("Step", vStep);
    if (bResMin || bResMax || bResStep)
    {
      strcat(str, "\n<\n");
      sprintf(st, "string UIWidget = \"slider\";\n");
      strcat(str, st);
 
      sprintf(st, "string UIName = \"%s\";\n", pConst->m_Name.c_str());
      strcat(str, st);

      if (bResMin)
      {
        sprintf(st, "float UIMin = %f;\n", vMin[0]);
        strcat(str, st);
      }
      if (bResMax)
      {
        sprintf(st, "float UIMax = %f;\n", vMax[0]);
        strcat(str, st);
      }
      if (bResStep)
      {
        sprintf(st, "float UIStep = %f;\n", vStep[0]);
        strcat(str, st);
      }
      strcat(str, ">");
    }
    if (bResDefault)
    {
      sprintf(st, " = %f", vDefault[0]);
      strcat(str, st);
    }
  }

  return true;
}

char *CShaderGraph::GenerateFXGraphShader(CShader *ef, std::vector<string>& shVSCodeLines, string& shVSInputStruct, string& shVSOutputStruct, std::vector<string>& shVSFunctions, std::vector<SShaderShConstant>& shVSGlobalVars, std::vector<SShaderShTexture>& shVSSamplers, std::vector<string>& shPSCodeLines, string& shPSInputStruct, string& shPSOutputStruct, std::vector<string>& shPSFunctions, std::vector<SShaderShConstant>& shPSGlobalVars, std::vector<SShaderShTexture>& shPSSamplers)
{
  string finalCode;
  int i;
  char str[1024];

  // Global variables
  if (shVSGlobalVars.size())
  {
    finalCode.append("//==============================================================\n");
    finalCode.append("//=== VertexShader Global variables ============================\n");
    finalCode.append("//==============================================================\n");
    for (i=0; i<shVSGlobalVars.size(); i++)
    {
      sConstantString(&shVSGlobalVars[i], str);
      finalCode.append(str);
      finalCode.append(";\n");
    }
    finalCode.append("\n");
  }

  // Global variables
  if (shPSGlobalVars.size())
  {
    finalCode.append("//==============================================================\n");
    finalCode.append("//=== PixelShader Global variables =============================\n");
    finalCode.append("//==============================================================\n");
    for (i=0; i<shPSGlobalVars.size(); i++)
    {
      sConstantString(&shPSGlobalVars[i], str);
      finalCode.append(str);
      finalCode.append(";\n");
    }
    finalCode.append("\n");
  }

  // Functions
  if (shVSFunctions.size())
  {
    finalCode.append("//==============================================================\n");
    finalCode.append("//=== VertexShader Functions ===================================\n");
    finalCode.append("//==============================================================\n");
    for (i=0; i<shVSFunctions.size(); i++)
    {
      finalCode.append(shVSFunctions[i]);
      finalCode.append("\n\n");
    }
    finalCode.append("\n");
  }
  if (shPSFunctions.size())
  {
    finalCode.append("//==============================================================\n");
    finalCode.append("//=== PixelShader Functions ====================================\n");
    finalCode.append("//==============================================================\n");
    for (i=0; i<shPSFunctions.size(); i++)
    {
      finalCode.append(shPSFunctions[i]);
      finalCode.append("\n\n");
    }
    finalCode.append("\n");
  }


  // Structures
  finalCode.append("//=== VertexShader Input structure ===============================\n");
  finalCode.append(shVSInputStruct);
  finalCode.append("\n");

  finalCode.append("//=== VertexShader Output structure ==============================\n");
  finalCode.append(shVSOutputStruct);
  finalCode.append("\n");

  finalCode.append("//=== PixelShader Input structure ===============================\n");
  finalCode.append(shPSInputStruct);
  finalCode.append("\n");

  finalCode.append("//=== PixelShader Output structure ==============================\n");
  finalCode.append(shPSOutputStruct);
  finalCode.append("\n");

  finalCode.append("//=== PixelShader samplers =====================================\n");
  for (i=0; i<shPSSamplers.size(); i++)
  {
    SShaderShTexture *pTex = &shPSSamplers[i];
    if (pTex->m_pGraphNode->m_eFormat == eGrNodeFormat_Texture2D)
      finalCode.append("sampler2D ");
    else
    if (pTex->m_pGraphNode->m_eFormat == eGrNodeFormat_TextureCUBE)
      finalCode.append("samplerCUBE ");
    else
    if (pTex->m_pGraphNode->m_eFormat == eGrNodeFormat_Texture3D)
      finalCode.append("sampler3D ");
    finalCode.append(pTex->m_Name.c_str());
    finalCode.append(" = sampler_state\n{  Texture = ");
    finalCode.append(pTex->m_pGraphNode->m_CustomSemantics);
    finalCode.append(";  };\n");
  }
  finalCode.append("\n");

  // code
  finalCode.append("//=== VertexShader code =========================================\n");
  if (!strnicmp(ef->GetName(), "Composer::", 10))
    sprintf(str, "outputVS %s%s(inputVS IN)\n{\n  outputVS OUT;\n\n", &ef->GetName()[10], "VS");
  else
    sprintf(str, "outputVS %s%s(inputVS IN)\n{\n  outputVS OUT;\n\n", ef->GetName(), "VS");
  finalCode.append(str);
  for (i=0; i<shVSCodeLines.size(); i++)
  {
    finalCode.append("  ");
    finalCode.append(shVSCodeLines[i]);
    finalCode.append(";\n");
  }
  sprintf(str, "  return OUT;\n}\n\n");
  finalCode.append(str);

  finalCode.append("//=== PixelShader code =========================================\n");
  if (!strnicmp(ef->GetName(), "Composer::", 10))
    sprintf(str, "outputPS %s%s(inputPS IN)\n{\n  outputPS OUT;\n\n", &ef->GetName()[10], "PS");
  else
    sprintf(str, "outputPS %s%s(inputPS IN)\n{\n  outputPS OUT;\n\n", ef->GetName(), "PS");
  finalCode.append(str);
  for (i=0; i<shPSCodeLines.size(); i++)
  {
    finalCode.append("  ");
    finalCode.append(shPSCodeLines[i]);
    finalCode.append(";\n");
  }
  sprintf(str, "  return OUT;\n}\n\n");
  finalCode.append(str);

  //============================================================================================
  // Generate FX techniques

  finalCode.append("//=== Techniques =========================================\n\n");
  finalCode.append("technique General\n<\n");
  finalCode.append("  string Script = \n");
  finalCode.append("                \"TechniqueZ = ZPass\";\n");
  finalCode.append(">\n");
  finalCode.append("{\n");
  finalCode.append("  pass p0\n  {\n");
  if (!strnicmp(ef->GetName(), "Composer::", 10))
    sprintf(str, "    VertexShader = compile vs_Auto %s%s();\n", &ef->GetName()[10], "VS");
  else
    sprintf(str, "    VertexShader = compile vs_Auto %s%s();\n", ef->GetName(), "VS");
  finalCode.append(str);
  if (!strnicmp(ef->GetName(), "Composer::", 10))
    sprintf(str, "    PixelShader = compile ps_Auto %s%s();\n", &ef->GetName()[10], "PS");
  else
    sprintf(str, "    PixelShader = compile ps_Auto %s%s();\n", ef->GetName(), "PS");
  finalCode.append(str);

  sprintf(str, "    ZEnable = true;\n");
  finalCode.append(str);
  sprintf(str, "    ZWriteEnable = true;\n");
  finalCode.append(str);

  finalCode.append("  }\n");
  finalCode.append("}\n");

  //============================================================================================
  // Common FX techniques
  // todo: common passes: zPass, shadowPass, detailPass, ...

  char *pBuf = new char[finalCode.size()+1];
  memcpy(pBuf, finalCode.c_str(), finalCode.size());
  pBuf[finalCode.size()] = 0;

  {
    FILE *fp = fopen("grResult.cg", "w");
    if (fp)
    {
      fwrite(pBuf, 1, finalCode.size(), fp);
      fclose(fp);
    }
  }

  return pBuf;
}

bool CShaderGraph::ParseGraphShader(char *buf, CShader *ef, std::vector<string>& shCodeLines, string& shInputStruct, string& shOutputStruct, std::vector<string>& shFunctions, std::vector<SShaderShConstant>& shGlobalVars, std::vector<SShaderShTexture>& shSamplers, bool bPixel)
{
  char* name;
  long cmd;
  char *params;

  enum {eBlock=1, eEdges};
  static STokenDesc commands[] =
  {
    {eBlock, "Block"},
    {eEdges, "Edges"},
    {0,0}
  };

  bool bOk = true;
  TArray<SShaderShBlock *> ShBlocks;
  TArray<SShaderShEdge *> ShEdges;
  while ((cmd = shGetObject (&buf, commands, &name, &params)) > 0)
  {
    switch (cmd)
    {
      case eBlock:
        {
          SShaderShBlock *pBL = new SShaderShBlock;
          pBL->m_Name = name;
          bool bRes = ParseShBlock(params, ef, pBL);
          if (bRes)
            ShBlocks.AddElem(pBL);
          else
          {
            assert(0);
            delete pBL;
          }
        }
        break;

      case eEdges:
        ParseShEdges(params, ef, ShEdges);
        break;
    }
  }
  bool bRes = GenerateGraphShader(ef, bPixel, ShBlocks, ShEdges, shCodeLines, shInputStruct, shOutputStruct, shFunctions, shGlobalVars, shSamplers);

  int i;
  for (i=0; i<ShEdges.Num(); i++)
  {
    delete ShEdges[i];
  }
  for (i=0; i<ShBlocks.Num(); i++)
  {
    delete ShBlocks[i];
  }

  return bRes;
}

CShader *CShaderGraph::ParseGraph(char *buf, CShader *ef, CShader *efGen, uint nMaskGen)
{
  DynArray<SShaderTechParseParams> techParams;
  CCryName techStart[2];

  char* name;
  long cmd;
  char *params;

  enum {eVertexStart=1, ePixelStart};
  static STokenDesc commands[] =
  {
    {eVertexStart, "VertexStart"},
    {ePixelStart,  "PixelStart"},
    {0,0}
  };

  bool bOk = true;
  bool bVS = false;
  bool bPS = false;
  string shVSInputStruct, shPSInputStruct;
  string shVSOutputStruct, shPSOutputStruct;
  std::vector<string> shVSCodeLines, shPSCodeLines;
  std::vector<string> shVSFunctions, shPSFunctions;
  std::vector<SShaderShConstant> shVSGlobalVars, shPSGlobalVars;
  std::vector<SShaderShTexture> shVSSamplers, shPSSamplers;
  while ((cmd = shGetObject (&buf, commands, &name, &params)) > 0)
  {
    switch (cmd)
    {
      case eVertexStart:
        bVS = ParseGraphShader(params, ef, shVSCodeLines, shVSInputStruct, shVSOutputStruct, shVSFunctions, shVSGlobalVars, shVSSamplers, false);
        break;

      case ePixelStart:
        bPS = ParseGraphShader(params, ef, shPSCodeLines, shPSInputStruct, shPSOutputStruct, shPSFunctions, shPSGlobalVars, shPSSamplers, true);
        break;
      }
  }

  if (bPS && bVS)
  {
    char *pFX = GenerateFXGraphShader(ef, shVSCodeLines, shVSInputStruct, shVSOutputStruct, shVSFunctions, shVSGlobalVars, shVSSamplers, shPSCodeLines, shPSInputStruct, shPSOutputStruct, shPSFunctions, shPSGlobalVars, shPSSamplers);
    gRenDev->m_cEF.mfPostLoadFX(ef, techParams, techStart);
  }

  return ef;
}
