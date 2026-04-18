#include "stdafx.h"
#include "PhysTestbed.h"

#include <stdio.h>
#include "platform.h"
#include "ILog.h"
#include "IPhysics.h"

#define va_call(func,param) \
	va_list args; va_start(args, param); \
	func (param, args);	\
	va_end(args) 

struct CSimpleLog : ILog {
	virtual void Release() {}
	virtual void	SetFileName(const char *command = NULL) {}
	virtual const char*	GetFileName() { return 0; }
	virtual void	Log(const char *szCommand,...) {
		//
	}
	virtual void	LogV (const ELogType nType, const char* szFormat, va_list args) { Log(szFormat,args); }
	virtual void	LogWarning(const char *szCommand,...) {	va_call(Log,szCommand); }
	virtual void	LogError(const char *szCommand,...) { va_call(Log,szCommand); }
	virtual void	LogPlus(const char *command,...) { va_call(Log,command); }
	virtual void	LogToFile(const char *command,...) { va_call(Log,command); }
	virtual void	LogToFilePlus(const char *command,...) { va_call(Log,command); }
	virtual void	LogToConsole(const char *command,...) { va_call(Log,command); }
	virtual void	LogToConsolePlus(const char *command,...) { va_call(Log,command); }
	virtual void	UpdateLoadingScreen(const char *command,...) {}
	virtual void	UpdateLoadingScreenPlus(const char *command,...) {}
	virtual void	EnableVerbosity( bool bEnable ) {}
	virtual void	SetVerbosity( int verbosity ) { m_iVerbosity=verbosity; }
	virtual int		GetVerbosityLevel() { return m_iVerbosity; }
	virtual void  AddCallback(ILogCallback *pCallback) {}
	virtual void  RemoveCallback(ILogCallback *pCallback) {}
	virtual void RegisterConsoleVariables() {}
	virtual void UnregisterConsoleVariables() {}
	int m_iVerbosity;
};
CSimpleLog g_Log;

struct CSimpleSystem : ISystem {
	virtual int GetCPUFlags() { return CPUF_SSE|CPUF_SSE2|CPUF_MMX; }
	virtual struct ILog *GetILog() { return &g_Log; }
};
CSimpleSystem g_System;

struct SRayRec {
	Vec3 origin;
	Vec3 dir;
	float time;
};
struct ColorB {
	ColorB() {}
	ColorB(unsigned char _r,unsigned char _g,unsigned char _b,unsigned char _a) { r=_r;g=_g;b=_b;a=_a; }
	unsigned char r,g,b,a;
};

class CPhysRenderer : public IPhysRenderer {
public:
	CPhysRenderer();
	~CPhysRenderer();
	void Init();
	void DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, const ColorB &clr);
	void DrawBuffers(float dt);

	virtual void DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, int idxColor=0, int bSlowFadein=0);
	virtual void DrawLine(const Vec3& pt0, const Vec3& pt1, int idxColor=0, int bSlowFadein=0);
	virtual void DrawText(const Vec3 &pt, const char *txt, int idxColor, float saturation=0) {}
	virtual const char *GetForeignName(void *pForeignData,int iForeignData,int iForeignFlags) { return ""; };

	float m_wireframeDist;
	float m_timeRayFadein;

protected:
	SRayRec *m_rayBuf;
	int m_szRayBuf,m_iFirstRay,m_iLastRay,m_nRays;
	IGeometry *m_pRayGeom;
	primitives::ray *m_pRay;
	static ColorB g_colorTab[8];
};
CPhysRenderer g_PhysRenderer;


IPhysicalWorld *g_pWorld;
ProfilerData *g_pPhysProfilerData;
Vec3 g_campos,g_camdir;
float g_viewdist = 100;
float g_movespeed = 3;
int g_idSelected = -1;
HANDLE g_hThread,g_hThreadActive;
__int64 g_freq,g_profreq=1000000000;
__int64 g_stepTime=0,g_moveTime=0;
extern int g_bFast,g_bShowColl,g_bShowProfiler;

struct ProfilerVisInfo {
	unsigned __int64 iCode;
	float timeout;
	char bExpanded;
};
ProfilerVisInfo g_ProfVisInfos[1024];
int g_nProfVisInfos = 0;
unsigned __int64 g_iActiveCode = 0;

int getProfVisInfo(unsigned __int64 iCode, int bCreate) 
{
	for(int i=0; i<g_nProfVisInfos; i++) if (g_ProfVisInfos[i].iCode==iCode) {
		g_ProfVisInfos[i].timeout = 5.0f;
		return i;
	}
	if (!bCreate || g_nProfVisInfos==sizeof(g_ProfVisInfos)/sizeof(g_ProfVisInfos[0]))
		return 0;
	g_ProfVisInfos[g_nProfVisInfos].iCode = iCode;
	g_ProfVisInfos[g_nProfVisInfos].bExpanded = g_ProfVisInfos[0].bExpanded;
	g_ProfVisInfos[g_nProfVisInfos].timeout = 5.0f;
	return g_nProfVisInfos++;
}

int getProfParent(ProfilerData *pd, int iParent, int iSlot)
{
	int iChild;
	if (iParent<0)
		return -1;
	for(iChild=pd->TimeSamples[iParent].iChild; iChild>=0; iChild=pd->TimeSamples[iChild].iNext) if (iChild==iSlot)
		return iParent;
	for(iChild=pd->TimeSamples[iParent].iChild; iChild>=0; iChild=pd->TimeSamples[iChild].iNext) if ((iParent=getProfParent(pd,iChild,iSlot))>0)
		return iParent;
	return -1;
}

int getActiveSlot(ProfilerData *pd) 
{
	int iSlot;
	for(iSlot=pd->iLastTimeSample; iSlot>0 && pd->TimeSamples[iSlot].iCode!=g_iActiveCode; iSlot--);
	return iSlot;
}

void SelectNextProfVisInfo()
{
	ProfilerData *pd = g_pPhysProfilerData;
	int iSlot = getActiveSlot(g_pPhysProfilerData);
	if (!iSlot) { g_iActiveCode = pd->TimeSamples[1].iCode; return; }

	if (g_ProfVisInfos[getProfVisInfo(pd->TimeSamples[iSlot].iCode,0)].bExpanded && pd->TimeSamples[iSlot].iChild>=0)
			g_iActiveCode = pd->TimeSamples[pd->TimeSamples[iSlot].iChild].iCode;
	else {
		do {
			if (pd->TimeSamples[iSlot].iNext>=0) {
				g_iActiveCode = pd->TimeSamples[pd->TimeSamples[iSlot].iNext].iCode; return;
			} else
				iSlot = getProfParent(pd,0,iSlot);
		} while(iSlot);
		g_iActiveCode = 0;
	}
}

void SelectPrevProfVisInfo()
{
	ProfilerData *pd = g_pPhysProfilerData;
	int i,iSlot = getActiveSlot(g_pPhysProfilerData);
	if (!iSlot) { g_iActiveCode = pd->TimeSamples[1].iCode; return; }

	for(i=pd->iLastTimeSample; i>0 && pd->TimeSamples[i].iNext!=iSlot && pd->TimeSamples[i].iChild!=iSlot; i--);
	g_iActiveCode = pd->TimeSamples[i].iCode;

	if (pd->TimeSamples[i].iChild!=iSlot) 
		while (g_ProfVisInfos[getProfVisInfo(g_iActiveCode,0)].bExpanded && pd->TimeSamples[i].iChild>=0) {
			for(i=pd->TimeSamples[i].iChild; pd->TimeSamples[i].iNext>=0; i=pd->TimeSamples[i].iNext);
			g_iActiveCode = pd->TimeSamples[i].iCode;
		}
}

void ExpandProfVisInfo(int bExpand)
{
	g_ProfVisInfos[getProfVisInfo(g_iActiveCode,1)].bExpanded = bExpand;
}

DWORD WINAPI PhysProc(void *pParam)
{
	__int64 curTime; 

	while(1) {
		WaitForSingleObject(g_hThreadActive,INFINITE);
		QueryPerformanceCounter((LARGE_INTEGER*)&curTime);
		ResetProfiler(g_pPhysProfilerData);
		float dt = (float)(((curTime-g_stepTime)*10000)/g_freq)*0.0001f;
		if (dt<0 || dt>1)
			dt = 0.01f;
		if (dt<0.01f) {
			ReleaseMutex(g_hThreadActive);
			Sleep((int)((0.01f-dt)*1000));
			continue;
		}
		g_pWorld->TimeStep(dt);
		g_stepTime = curTime;
		ReleaseMutex(g_hThreadActive);
	}

	return 0;
}

void InitPhysics()
{
	g_pWorld = CreatePhysicalWorld(&g_System);
	g_pPhysProfilerData = GetProfileData();
	QueryPerformanceFrequency((LARGE_INTEGER*)&g_freq);
	g_ProfVisInfos[0].iCode = 0;
	g_ProfVisInfos[0].timeout = 10.0f;
	g_ProfVisInfos[0].bExpanded = 0;

	HKEY hKey;
	DWORD dwSize = sizeof(g_profreq);
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0,KEY_QUERY_VALUE, &hKey)==ERROR_SUCCESS && 
			RegQueryValueEx(hKey, "~MHz", 0,0, (LPBYTE)&g_profreq, &dwSize)==ERROR_SUCCESS)
		g_profreq *= 1000000;

	g_hThreadActive = CreateMutex(0,TRUE,0);
	g_hThread = CreateThread(0,0,PhysProc,0,0,0);
	//SetThreadPriority(g_hThread, THREAD_PRIORITY_LOWEST);
	g_PhysRenderer.Init();
}

void ShutdownPhysics()
{
	WaitForSingleObject(g_hThreadActive,INFINITE);
	TerminateThread(g_hThread,0);
	CloseHandle(g_hThreadActive);
	g_pWorld->Shutdown();
}

void ReloadWorld(const char *fworld)
{
	WaitForSingleObject(g_hThreadActive,INFINITE);
	g_pWorld->ClearLoggedEvents();
	g_pWorld->DestroyDynamicEntities();
	g_pWorld->Shutdown(0);
	g_pWorld->SerializeWorld(fworld,0);
	ReleaseMutex(g_hThreadActive);
}

void SaveWorld(const char *fworld)
{
	WaitForSingleObject(g_hThreadActive,INFINITE);
	g_pWorld->SerializeWorld(fworld,1);
	ReleaseMutex(g_hThreadActive);
}

void ReloadWorldAndGeometries(const char *fworld, const char *fgeoms)
{
	WaitForSingleObject(g_hThreadActive,INFINITE);
	g_pWorld->Shutdown();
	g_pWorld->Init();
	g_pWorld->SerializeGeometries(fgeoms,0);
	g_pWorld->SerializeWorld(fworld,0);

	pe_status_pos sp;
	IPhysicalEntity **ppEnts;
	Vec3 pos(ZERO), bbox[2]={ Vec3(-1E10f,-1E10f,-1E10f), Vec3(1E10f,1E10f,1E10f) };
	int i,nEnts;

	ResetProfiler(g_pPhysProfilerData);
	if (!(nEnts = g_pWorld->GetEntitiesInBox(bbox[0],bbox[1],ppEnts,ent_sleeping_rigid|ent_rigid)))
		nEnts = g_pWorld->GetEntitiesInBox(bbox[0],bbox[1],ppEnts,ent_independent);
	for(i=0; i<nEnts; i++) {
		ppEnts[i]->GetStatus(&sp);
		pos += sp.pos;
	}
	if (i>0)
		pos /= (float)i;

	if (g_pWorld->GetEntitiesInBox(bbox[0],bbox[1],ppEnts,ent_living)) {
		ppEnts[0]->GetStatus(&sp);
		g_campos = sp.pos;
	}	else
		(g_campos = pos).z += 10;
	g_camdir = (pos-g_campos).normalized();
	ReleaseMutex(g_hThreadActive);

	GLfloat zero[] = {0,0,0,0};
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 0);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
}


void OnCameraMove(float dx, float dy)
{
	__int64 curTime; QueryPerformanceCounter((LARGE_INTEGER*)&curTime);
	float dist = (float)(((curTime-g_moveTime)*10000)/g_freq)*0.0001f;
	g_moveTime = curTime;
	if (dist<0 || dist>0.1f)
		dist = 0.01f;
	dist *= g_movespeed*(1+g_bFast*3);
	g_campos += g_camdir*(dist*dy) + 
		(sqr(g_camdir.z)>g_camdir.len2()*0.99f ? g_camdir.GetOrthogonal() : g_camdir^Vec3(0,0,1)).normalized()*(dist*dx);
}

void OnCameraRotate(float dx, float dy)
{
	Vec3 dirx,diry;
	dirx = (sqr(g_camdir.z)>g_camdir.len2()*0.99f ? g_camdir.GetOrthogonal() : g_camdir^Vec3(0,0,1)).normalized();
	diry = g_camdir^dirx;
	(g_camdir += dirx*dx+diry*dy).normalize();
}

Vec3 g_raypos[2];
void SelectObject(float dx,float dy)
{
	Vec3 dirx,diry;
	dirx = (sqr(g_camdir.z)>g_camdir.len2()*0.99f ? g_camdir.GetOrthogonal() : g_camdir^Vec3(0,0,1)).normalized();
	diry = g_camdir^dirx;

	ray_hit hit;
	g_idSelected = -1;
	g_raypos[0]=g_campos; g_raypos[1]=g_raypos[0]+(g_camdir+dirx*dx+diry*dy)*g_viewdist;
	if (g_pWorld->RayWorldIntersection(g_campos,(g_camdir+dirx*dx+diry*dy)*g_viewdist, ent_all&~ent_terrain,rwi_stop_at_pierceable,&hit,1))
		g_idSelected = g_pWorld->GetPhysicalEntityId(hit.pCollider);
}


void EvolveWorld()
{
	__int64 curTime; QueryPerformanceCounter((LARGE_INTEGER*)&curTime);
	ResetProfiler(g_pPhysProfilerData);
	float dt = (float)(((curTime-g_stepTime)*10000)/g_freq)*0.0001f;
	if (dt<0 || dt>1)
		dt = 0.01f;
	g_pWorld->TimeStep(dt);
	g_pWorld->PumpLoggedEvents();
	g_stepTime = curTime;

	int i,j,bSelectionRemoved=0;
	for(i=j=1; i<g_nProfVisInfos; i++) {
		if (i!=j)
			g_ProfVisInfos[j] = g_ProfVisInfos[i];
		if (g_ProfVisInfos[i].iCode==g_iActiveCode && g_ProfVisInfos[i].timeout<5.0f)
			bSelectionRemoved = 1;		
		if ((g_ProfVisInfos[i].timeout-=dt)>0)
			j++;
	}
	g_nProfVisInfos = j;
	if (bSelectionRemoved)
		SelectPrevProfVisInfo();
}

void DrawLine(float *p1,float *p2)
{
	glVertex3fv(p1);
	glVertex3fv(p2);
}

int DrawProfileNode(HDC hDC, int ix,int iy, int szy, ProfilerData *pd,int iNode)
{
	char str[1024];
	char bExpanded=0;
	float time;

	for(int iChild=pd->TimeSamples[iNode].iChild; iChild>=0; iChild=pd->TimeSamples[iChild].iNext) {
		bExpanded = g_ProfVisInfos[getProfVisInfo(pd->TimeSamples[iChild].iCode,0)].bExpanded;
		if (pd->TimeSamples[iChild].iCode==g_iActiveCode)
			SetTextColor(hDC, RGB(255,255,255));
		time = (float)(((bExpanded ? pd->TimeSamples[iChild].iSelfTime : pd->TimeSamples[iChild].iTotalTime)*(__int64)1000000)/g_profreq)*0.001f;
		TextOut(hDC, ix,iy,	str, sprintf(str, "%s %s %.3fms / %d", pd->TimeSamples[iChild].iChild>=0 ? (bExpanded ? "-":"+"):"", 
			pd->TimeSamples[iChild].pProfiler->m_name, time, pd->TimeSamples[iChild].iCount));
		if (pd->TimeSamples[iChild].iCode==g_iActiveCode)
			SetTextColor(hDC, RGB(176,225,111));
		iy += szy;
		if (bExpanded && pd->TimeSamples[iChild].iChild>=0)
			iy = DrawProfileNode(hDC, ix+10,iy,szy, pd,iChild);
	}
	return iy;
}

void RenderWorld(HWND hWnd, HDC hDC)
{
	RECT rect;
	GetClientRect(hWnd, &rect);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, (double)rect.right/rect.bottom, 0.05f,g_viewdist*4);
	glViewport(0,0,rect.right,rect.bottom);

	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0f);
	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(g_campos.x,g_campos.y,g_campos.z, g_campos.x+g_camdir.x,g_campos.y+g_camdir.y,g_campos.z+g_camdir.z, 0,0,1);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_LIGHTING);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	g_pWorld->GetPhysVars()->iDrawHelpers = 8114+g_bShowColl;
	g_pWorld->DrawPhysicsHelperInformation(&g_PhysRenderer);

	glFlush();glFinish();
	SwapBuffers(hDC);

	if (g_bShowProfiler) {
		WaitForSingleObject(g_hThreadActive,INFINITE);
		SetBkMode(hDC, TRANSPARENT);
		SetTextColor(hDC, RGB(176,225,111));
		SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
		TEXTMETRIC tm;
		GetTextMetrics(hDC, &tm);
		char str[128]; 
		TextOut(hDC,20,20,str,sprintf(str,"Selected Entity Id: %d", g_idSelected));
		DrawProfileNode(hDC, 20,20+tm.tmHeight,tm.tmHeight, g_pPhysProfilerData,0);
		ReleaseMutex(g_hThreadActive);
	}
}


ColorB CPhysRenderer::g_colorTab[8] = { 
	ColorB(136,141,162,255), ColorB(212,208,200,255), ColorB(255,255,255,255), ColorB(214,222,154,128), 
	ColorB(231,192,188,255), ColorB(164,0,0,80), ColorB(164,0,0,255), ColorB(168,224,251,255) 
};


CPhysRenderer::CPhysRenderer()
{
	m_iFirstRay = 0; m_iLastRay = -1; m_nRays = 0;
	m_rayBuf = 0; m_pRayGeom = 0; m_pRay = 0;
	m_timeRayFadein = 0.2f;
	m_wireframeDist = 5.0f;
}

void CPhysRenderer::Init()
{
	primitives::ray aray; 
	aray.dir.Set(0,0,1); aray.origin.zero();
	m_pRayGeom = g_pWorld->GetGeomManager()->CreatePrimitive(primitives::ray::type, &aray);
	m_pRay = (primitives::ray*)m_pRayGeom->GetData();
	m_rayBuf = new SRayRec[m_szRayBuf = 64];
	m_iFirstRay = 0; m_iLastRay = -1; m_nRays = 0;
}

CPhysRenderer::~CPhysRenderer()
{
	if (m_pRayGeom) m_pRayGeom->Release();
	if (m_rayBuf) delete[] m_rayBuf;
}


void CPhysRenderer::DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, int idxColor, int bSlowFadein)
{
	if (!bSlowFadein)	{
		ColorB clr = g_colorTab[idxColor & 7];
		clr.a >>= idxColor>>8;
		DrawGeometry(pGeom,pgwd, clr);
	} else {
		if (pGeom->GetType()==GEOM_RAY) {
			primitives::ray *pray = (primitives::ray*)pGeom->GetData();
			if (m_nRays==m_szRayBuf) {
				int i;
				SRayRec *prevbuf = m_rayBuf; m_rayBuf = new SRayRec[m_szRayBuf+=64];
				memcpy(m_rayBuf+m_iFirstRay, prevbuf+m_iFirstRay, (m_nRays-m_iFirstRay)*sizeof(SRayRec));
				if (m_iFirstRay > m_iLastRay)	{
					memcpy(m_rayBuf+m_nRays, prevbuf, (i = min(m_szRayBuf-m_nRays,m_iLastRay+1))*sizeof(SRayRec));
					memcpy(m_rayBuf, prevbuf+i, (m_iLastRay+1-i)*sizeof(SRayRec));
					m_iLastRay -= i - (m_nRays+i & (m_iLastRay-i)>>31);
				}
			}
			m_iLastRay += 1-(m_szRayBuf & (m_szRayBuf-2-m_iLastRay)>>31); m_nRays++;
			if (!pgwd) {
				m_rayBuf[m_iLastRay].origin = pray->origin;
				m_rayBuf[m_iLastRay].dir = pray->dir;
			} else {
				m_rayBuf[m_iLastRay].origin = pgwd->R*pray->origin*pgwd->scale + pgwd->offset;
				m_rayBuf[m_iLastRay].dir = pgwd->R*pray->dir;
			}
			m_rayBuf[m_iLastRay].time = m_timeRayFadein;
		}
	}
}

void CPhysRenderer::DrawLine(const Vec3& pt0, const Vec3& pt1, int idxColor, int bSlowFadein)
{
	m_pRay->origin = pt0;
	m_pRay->dir = pt1-pt0;
	DrawGeometry(m_pRayGeom,0, idxColor, bSlowFadein);
}

void CPhysRenderer::DrawBuffers(float dt)
{
	if (m_nRays>0) {
		int i=m_iFirstRay,iprev;
		float rtime=1/m_timeRayFadein;
		ColorB clr;
		do {
			clr = g_colorTab[7]; clr.a = FtoI(clr.a*m_rayBuf[i].time*rtime);
			m_pRay->origin = m_rayBuf[i].origin;
			m_pRay->dir = m_rayBuf[i].dir;
			DrawGeometry(m_pRayGeom, 0, clr);
			iprev = i; i += 1-(m_szRayBuf & (m_szRayBuf-2-i)>>31);
			if ((m_rayBuf[iprev].time -= dt)<0)
				m_iFirstRay=i,m_nRays--;
		} while(iprev!=m_iLastRay);
	}
}


inline float getheight(primitives::heightfield *phf, int ix,int iy) { 
	return phf->fpGetHeightCallback ? 
		phf->getheight(ix,iy) : 
		((float*)phf->fpGetSurfTypeCallback)[vector2di(ix,iy)*phf->stride]*phf->heightscale;
}

#define _clr(c) glColor4ubv((GLubyte*)&(c))
#define _vtx(v) glVertex3fv((const GLfloat*)&(v))

void CPhysRenderer::DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, const ColorB& clr)
{
	Matrix33 R = Matrix33::CreateIdentity();
	Vec3 pos(ZERO),center,sz,ldir0,ldir1(0,0,1),n,pt[5],campos;
	float scale=1.0f,t,l,sx,dist;
	primitives::box bbox;
	ColorB clrlit[4];
	int i,j,itype=pGeom->GetType();
#define FIAT_LUX(color) \
	t = (n*ldir0)*-0.5f; l = t+fabs_tpl(t); t = (n*ldir1)*-0.1f; l += t+fabs_tpl(t); l = min(1.0f,l+0.05f); \
	color = ColorB(FtoI(l*clr.r), FtoI(l*clr.g), FtoI(l*clr.b), clr.a)

	if (pgwd) {
		R = pgwd->R; pos = pgwd->offset; scale = pgwd->scale;
	}
	pGeom->GetBBox(&bbox);
	sz = (bbox.size*(bbox.Basis *= R.T()).Fabs())*scale;
	center = pos + R*bbox.center*scale;
	campos = g_campos*3;
	(ldir0 = g_camdir).z = 0; (ldir0 = ldir0.normalized()*0.5f).z = (float)sqrt3*-0.5f;
	glDepthMask(clr.a==255 ? GL_TRUE:GL_FALSE);

	switch (itype) 
	{
  	case GEOM_TRIMESH: case GEOM_VOXELGRID:	{
			mesh_data *pmesh = (mesh_data*)pGeom->GetData();
			glBegin(GL_TRIANGLES);
			for(i=0;i<pmesh->nTris;i++) {
				for(j=0;j<3;j++) pt[j] = R*pmesh->pVertices[pmesh->pIndices[i*3+j]]*scale+pos;
				n = R*pmesh->pNormals[i]; FIAT_LUX(clrlit[0]);
				 _clr(clrlit[0].r);	_vtx(pt[0]); _vtx(pt[1]); _vtx(pt[2]); 
			}	glEnd(); 
			glBegin(GL_LINES);
			for(i=0;i<pmesh->nTris;i++) {
				for(j=0;j<3;j++) pt[j] = R*pmesh->pVertices[pmesh->pIndices[i*3+j]]*scale+pos;
				if ((pt[0]+pt[1]+pt[2]-campos).len2()<sqr(m_wireframeDist)*9)	{
					 _clr(clr);	_vtx(pt[0]);_vtx(pt[1]); _vtx(pt[1]);_vtx(pt[2]); _vtx(pt[2]);_vtx(pt[0]);
				}
			}	glEnd(); 
			break; }

		case GEOM_HEIGHTFIELD: {
			primitives::heightfield *phf = (primitives::heightfield*)pGeom->GetData();
			center = phf->Basis*(((g_campos-pos)*R)/scale-phf->origin);
			vector2df c(center.x*phf->stepr.x, center.y*phf->stepr.y);
			ColorB clrhf[2];
			dist = 100.0f/scale;
			clrhf[0] = ColorB(FtoI(clr.r*0.8f), FtoI(clr.g*0.8f), FtoI(clr.b*0.8f), clr.a);
			clrhf[1] = ColorB(FtoI(clr.r*0.4f), FtoI(clr.g*0.4f), FtoI(clr.b*0.4f), clr.a);
			R *= phf->Basis.T();
			pos += R*phf->origin*scale;
			glBegin(GL_TRIANGLES);
			for(j=max(0,FtoI(c.y-dist*phf->stepr.y-0.5f)); j<=min(phf->size.y-1,FtoI(c.y+dist*phf->stepr.y-0.5f)); j++) {
				sx = sqrt_tpl(max(0.0f, sqr(dist*phf->stepr.y)-sqr(j+0.5f-c.y)));
				i = max(0,FtoI(c.x-sx-0.5f));
				pt[0] = R*Vec3(i*phf->step.x, j*phf->step.y, getheight(phf,i,j))*scale + pos;
				pt[1] = R*Vec3(i*phf->step.x, (j+1)*phf->step.y, getheight(phf,i,j+1))*scale + pos;
				for(; i<=min(phf->size.x-1,FtoI(c.x+sx-0.5f)); i++)	{
					clrlit[0] = clrhf[(i^j)&1];
					pt[2] = R*Vec3((i+1)*phf->step.x, j*phf->step.y, getheight(phf,i+1,j))*scale + pos;
					pt[3] = R*Vec3((i+1)*phf->step.x, (j+1)*phf->step.y, getheight(phf,i+1,j+1))*scale + pos;
					_clr(clrlit[0]);
					_vtx(pt[0]); _vtx(pt[2]); _vtx(pt[1]);
					_vtx(pt[1]); _vtx(pt[2]); _vtx(pt[3]);
					pt[0] = pt[2]; pt[1] = pt[3];
				}
			}	glEnd();
			break; }

		case GEOM_BOX: {
			primitives::box *pbox = (primitives::box*)pGeom->GetData();
			bbox.Basis = pbox->Basis*R.T();
			bbox.center = pos+R*pbox->center*scale;
			bbox.size = pbox->size*scale;
			glBegin(GL_TRIANGLES);
			for(i=0;i<6;i++) {
				n = bbox.Basis.GetRow(i>>1)*float((i*2&2)-1); FIAT_LUX(clrlit[0]);
				pt[4] = bbox.center + n*bbox.size[i>>1];
				for(j=0;j<4;j++)
					pt[j] = pt[4] + bbox.Basis.GetRow(incm3(i>>1))*bbox.size[incm3(i>>1)]*float(((j^i)*2&2)-1) +
													bbox.Basis.GetRow(decm3(i>>1))*bbox.size[decm3(i>>1)]*float((j&2)-1);
				_clr(clrlit[0]);
				_vtx(pt[0]); _vtx(pt[2]); _vtx(pt[3]);
				_vtx(pt[0]); _vtx(pt[3]); _vtx(pt[1]);
			} glEnd();
			break; }

		case GEOM_CYLINDER: {
			primitives::cylinder *pcyl = (primitives::cylinder*)pGeom->GetData();
			const float cos15=0.96592582f,sin15=0.25881904f;
			float x,y;
			Vec3 axes[3],center;
			axes[2] = R*pcyl->axis;
			axes[0] = axes[2].GetOrthogonal().normalized();
			axes[1] = axes[2]^axes[0];
			center = R*pcyl->center*scale + pos;
			pt[0] = pt[2] = center+axes[0]*(pcyl->r*scale);
			n = axes[0]; FIAT_LUX(clrlit[0]);
			n = axes[2]; FIAT_LUX(clrlit[2]);
			n = -axes[2]; FIAT_LUX(clrlit[3]);
			glBegin(GL_TRIANGLES);
			axes[2] *= pcyl->hh*scale;
			for(i=0,x=cos15,y=sin15; i<24; i++,pt[0]=pt[1],clrlit[0]=clrlit[1]) {
				n = axes[0]*x + axes[1]*y; FIAT_LUX(clrlit[1]);
				pt[1] = center + n*(pcyl->r*scale);
				_clr(clrlit[0]);_vtx(pt[0]-axes[2]); _clr(clrlit[1]);_vtx(pt[1]-axes[2]); _clr(clrlit[0]);_vtx(pt[0]+axes[2]);
				_clr(clrlit[1]);_vtx(pt[1]+axes[2]); _clr(clrlit[0]);_vtx(pt[0]+axes[2]); _clr(clrlit[1]);_vtx(pt[1]-axes[2]);
				_clr(clrlit[2]); _vtx(pt[2]+axes[2]);_vtx(pt[0]+axes[2]);_vtx(pt[1]+axes[2]);
				_clr(clrlit[3]); _vtx(pt[2]-axes[2]);_vtx(pt[1]-axes[2]);_vtx(pt[0]-axes[2]);
				t = x; x = x*cos15-y*sin15; y = y*cos15+t*sin15;
			}	glEnd();
			break; }

		case GEOM_CAPSULE: case GEOM_SPHERE: {
			primitives::cylinder cyl,*pcyl;
			primitives::sphere *psph;
			if (itype==GEOM_CAPSULE)
				pcyl = (primitives::cylinder*)pGeom->GetData();
			else {
				psph = (primitives::sphere*)pGeom->GetData();	pcyl = &cyl;
				cyl.axis.Set(0,0,1); cyl.center=psph->center; cyl.hh=0; cyl.r=psph->r; 
			}
			const float cos15=0.96592582f,sin15=0.25881904f;
			float x,y,cost,sint,costup,sintup;
			Vec3 axes[3],center,haxis,nxy;
			int icap;
			axes[2] = R*pcyl->axis;
			axes[0] = axes[2].GetOrthogonal().normalized();
			axes[1] = axes[2]^axes[0];
			center = R*pcyl->center*scale + pos;
			pt[0] = pt[2] = center+axes[0]*(pcyl->r*scale);
			haxis = axes[2]*(pcyl->hh*scale);
			n = axes[0]; FIAT_LUX(clrlit[0]);
			glBegin(GL_TRIANGLES);
			for(i=0,x=cos15,y=sin15; i<24; i++,pt[0]=pt[1],clrlit[0]=clrlit[1]) {
				n = axes[0]*x + axes[1]*y; FIAT_LUX(clrlit[1]);
				pt[1] = center + n*(pcyl->r*scale);
				_clr(clrlit[0]);_vtx(pt[0]-haxis); _clr(clrlit[1]);_vtx(pt[1]-haxis); _clr(clrlit[0]);_vtx(pt[0]+haxis);
				_clr(clrlit[1]);_vtx(pt[1]+haxis); _clr(clrlit[0]);_vtx(pt[0]+haxis); _clr(clrlit[1]);_vtx(pt[1]-haxis);
				t = x; x = x*cos15-y*sin15; y = y*cos15+t*sin15;
			}
			for(icap=0;icap<2;icap++,haxis.Flip(),axes[2].Flip()) for(j=0,cost=1,sint=0,costup=cos15,sintup=sin15; j<6; j++) {
				n = axes[0]*cost+axes[2]*sint; FIAT_LUX(clrlit[0]); 
				pt[0] = center + haxis + n*(pcyl->r*scale);
				n = axes[0]*costup+axes[2]*sintup; FIAT_LUX(clrlit[2]);
				pt[2] = center + haxis + n*(pcyl->r*scale);
				for(i=0,x=cos15,y=sin15; i<24; i++,pt[0]=pt[1],pt[2]=pt[3],clrlit[0]=clrlit[1],clrlit[2]=clrlit[3]) {
					nxy = axes[0]*x + axes[1]*y; 
					n = nxy*costup+axes[2]*sintup; FIAT_LUX(clrlit[3]);
					pt[3] = center + haxis + n*(pcyl->r*scale);
					n = nxy*cost+axes[2]*sint; FIAT_LUX(clrlit[1]); 
					pt[1] = center + haxis + n*(pcyl->r*scale);
					_clr(clrlit[0]);_vtx(pt[0]); _clr(clrlit[1+icap]);_vtx(pt[1+icap]); _clr(clrlit[2-icap]);_vtx(pt[2-icap]);
					_clr(clrlit[1]);_vtx(pt[1]); _clr(clrlit[3-icap]);_vtx(pt[3-icap]); _clr(clrlit[2+icap]);_vtx(pt[2+icap]);
					t = x; x = x*cos15-y*sin15; y = y*cos15+t*sin15;
				}
				cost = costup; sint = sintup;
				costup = cost*cos15-sint*sin15; sintup = sint*cos15+cost*sin15;
			}
			glEnd();
			break; }

		case GEOM_RAY: {
			primitives::ray *pray = (primitives::ray*)pGeom->GetData();
			pt[0] = pos + R*pray->origin*scale;
			pt[1] = pt[0]+R*pray->dir*scale;
			glBegin(GL_LINES); _clr(clr);_vtx(pt[0]);_vtx(pt[1]); glEnd();
			break; }
	}
}
