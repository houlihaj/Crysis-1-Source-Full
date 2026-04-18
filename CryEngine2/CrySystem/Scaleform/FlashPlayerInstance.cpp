#include "StdAfx.h"

#include "FlashPlayerInstance.h"
#include "../System.h"
#include <IConsole.h>

// flash player implementation via Scaleform's GFx
#ifndef EXCLUDE_SCALEFORM_SDK

#include "GImageInfoXRender.h"
#include "GTextureXRender.h"
#include "GAllocatorCryMem.h"
#include "SharedStates.h"

//////////////////////////////////////////////////////////////////////////
// IFlashPlayer instance profiler helpers

struct SFlashPeak
{
	SFlashPeak(float timeRecorded, const char* pDisplayInfo)
	: m_timeRecorded(timeRecorded)
	, m_displayInfo(pDisplayInfo)
	{
	}

	float m_timeRecorded;
	string m_displayInfo;
};


class CFlashPeakHistory
{
public:
	typedef std::vector<SFlashPeak> FlashPeakHistory;

public:
	CFlashPeakHistory(int maxPeakEntries, float timePeakExpires)
	: m_writeProtected(false)
	, mc_maxPeakEntries(maxPeakEntries)
	, mc_timePeakExpires(timePeakExpires)
	, m_history()
	{
		m_history.reserve(mc_maxPeakEntries);
	}

	void Add(const SFlashPeak& peak)
	{
		if (!m_writeProtected)
		{
			if (m_history.size() >= mc_maxPeakEntries)
				m_history.pop_back();

			m_history.insert(m_history.begin(), peak);
		}
	}

	void ToggleWriteProtection()
	{
		m_writeProtected = !m_writeProtected;
	}

	void UpdateHistory(float curTime)
	{
		FlashPeakHistory::iterator it(m_history.begin());
		for (; it != m_history.end();)
		{
			const SFlashPeak& peak(*it);
			if (curTime - peak.m_timeRecorded > mc_timePeakExpires && !m_writeProtected)
				it = m_history.erase(it);
			else
				++it;
		}
	}

	float GetTimePeakExpires() const
	{
		return mc_timePeakExpires;
	}

	const FlashPeakHistory& GetHistory() const
	{
		return m_history;
	}

private:
	bool m_writeProtected;
	const int mc_maxPeakEntries;
	const float mc_timePeakExpires;
	FlashPeakHistory m_history;
};

static CFlashPeakHistory s_flashPeakHistory(10, 5.0f);


enum EFlashFunctionID
{
	eFncAdvance,
	eFncDisplay,
	eFncSetVar,
	eFncGetVar,
	eFncIsAvailable,
	eFncInvoke,

	eNumFncIDs
};


inline static const char* GetFlashFunctionName(EFlashFunctionID functionID)
{
	switch(functionID)
	{
	case eFncAdvance:
		return "Advance";
	case eFncDisplay:
		return "Display";
	case eFncSetVar:
		return "SetVar";
	case eFncGetVar:
		return "GetVar";
	case eFncIsAvailable:
		return "IsAvailable";
	case eFncInvoke:
		return "Invoke";
	default:
		return "Unknown";
	}
}


template <typename T, int HistogramSize>
struct SFlashProfilerHistogram
{
	SFlashProfilerHistogram()
	: m_curIdx(0)
	{
		for (int i(0); i<HistogramSize; ++i)
			m_hist[i] = 0;
	}

	void Advance()
	{
		m_curIdx = (m_curIdx + 1) % HistogramSize;
		m_hist[m_curIdx] = 0;
	}

	void Add(T amount)
	{
		m_hist[m_curIdx] += amount;
	}

	T GetAvg() const
	{
		T sum(0);
		for (int i(0); i<HistogramSize; ++i)
			sum += m_hist[i];

		return sum / (T) HistogramSize;
	}

	T GetMin() const
	{
		T min(m_hist[0]);
		for (int i(1); i<HistogramSize; ++i)
			min = m_hist[i] < min ? m_hist[i] : min;

		return min;
	}

	T GetMax() const
	{
		T max(m_hist[0]);
		for (int i(1); i<HistogramSize; ++i)
			max = m_hist[i] > max ? m_hist[i] : max;

		return max;
	}

	T GetCur() const
	{
		return m_hist[m_curIdx];
	}

	T GetAt(int idx) const
	{
		return m_hist[idx];
	}

	int GetCurIdx() const
	{
		return m_curIdx;
	}

private:
	int m_curIdx;
	T m_hist[HistogramSize];
};


struct SFlashProfilerData
{
	enum
	{
		HistogramSize = 64
	};

	typedef SFlashProfilerHistogram<int, HistogramSize> TotalCallHistogram;
	typedef SFlashProfilerHistogram<float, HistogramSize> TotalTimeHistogram;

	SFlashProfilerData()
	: m_viewExpanded(false)
	, m_preventFunctionExectution(false)
	, m_totalCalls()
	, m_totalTime()
	, m_totalTimeAllFuncs()
	{
		for (int i(0); i<eNumFncIDs+1; ++i)
		{
			m_funcColorValue[i] = 0;
			m_funcColorValueVariance[i] = 0;
		}
	}

	void Advance()
	{
		if (!ms_histWriteProtected)
		{
			for (int i(0); i<eNumFncIDs; ++i)
			{
				m_totalCalls[i].Advance();
				m_totalTime[i].Advance();
			}
			m_totalTimeAllFuncs.Advance();
		}
	}

	static void AdvanceAllInsts()
	{
		if (!ms_histWriteProtected)
			ms_totalTimeAllInsts.Advance();
	}

	void AddTotalCalls(EFlashFunctionID functionID, int amount)
	{
		if (!ms_histWriteProtected)
			m_totalCalls[functionID].Add(amount);
	}

	void AddTotalTime(EFlashFunctionID functionID, float amount)
	{
		if (!ms_histWriteProtected)
		{
			m_totalTime[functionID].Add(amount);
			m_totalTimeAllFuncs.Add(amount);
			ms_totalTimeAllInsts.Add(amount);
		}
	}

	const TotalCallHistogram& GetTotalCallsHisto(EFlashFunctionID functionID) const
	{
		return m_totalCalls[functionID];
	}

	const TotalTimeHistogram& GetTotalTimeHisto(EFlashFunctionID functionID) const
	{
		return m_totalTime[functionID];
	}

	const TotalTimeHistogram& GetTotalTimeHistoAllFuncs() const
	{
		return m_totalTimeAllFuncs;
	}

	static const TotalTimeHistogram& GetTotalTimeHistoAllInsts()
	{
		return ms_totalTimeAllInsts;
	}

	void SetViewExpanded(bool expanded)
	{
		m_viewExpanded = expanded;
	}

	bool IsViewExpanded() const
	{
		return m_viewExpanded;
	}

	void TogglePreventFunctionExecution()
	{
		m_preventFunctionExectution = !m_preventFunctionExectution;
	}

	bool PreventFunctionExectution() const
	{
		return m_preventFunctionExectution;
	}

	void SetColorValue(EFlashFunctionID functionID, float value)
	{
		if (!ms_histWriteProtected)
			m_funcColorValue[functionID] = value;
	}

	float GetColorValue(EFlashFunctionID functionID) const
	{
		return m_funcColorValue[functionID];
	}

	void SetColorValueVariance(EFlashFunctionID functionID, float value)
	{
		if (!ms_histWriteProtected)
			m_funcColorValueVariance[functionID] = value;
	}

	float GetColorValueVariance(EFlashFunctionID functionID) const
	{
		return m_funcColorValueVariance[functionID];
	}

	static void ToggleHistogramWriteProtection()
	{
		ms_histWriteProtected = !ms_histWriteProtected;
	}

	static bool HistogramWriteProtected()
	{
		return ms_histWriteProtected;
	}

private:
	static bool ms_histWriteProtected;
	static TotalTimeHistogram ms_totalTimeAllInsts;

private:
	bool m_viewExpanded;
	bool m_preventFunctionExectution;
	float m_funcColorValue[eNumFncIDs+1];
	float m_funcColorValueVariance[eNumFncIDs+1];
	TotalCallHistogram m_totalCalls[eNumFncIDs];
	TotalTimeHistogram m_totalTime[eNumFncIDs];
	TotalTimeHistogram m_totalTimeAllFuncs;
};

bool SFlashProfilerData::ms_histWriteProtected(false);
SFlashProfilerData::TotalTimeHistogram SFlashProfilerData::ms_totalTimeAllInsts;


struct SPODVariant
{
	struct SFlashVarValueList
	{
		const SFlashVarValue* pArgs;
		unsigned int numArgs;
	};

	union Data
	{
		bool b;

		int i;
		unsigned int ui;

		double d;
		float f;

		const char* pStr;
		const wchar_t* pWstr;
		const void*	pVoid;

		EFlashVariableArrayType fvat;

		const SFlashVarValue* pfvv;
		SFlashVarValueList fvvl;
	};

	enum Type
	{
		eBool,

		eInt,
		eUInt,

		eDouble,
		eFloat,

		eConstStrPtr,
		eConstWstrPtr,
		eConstVoidPtr,

		eFlashVariableArrayType,

		eFlashVarValuePtr,
		eFlashVarValueList
	};

	SPODVariant(bool val)
		: type(eBool)
	{
		data.b = val;
	}
	SPODVariant(int val)
	: type(eInt)
	{
		data.i = val;
	}
	SPODVariant(unsigned int val)
		: type(eUInt)
	{
		data.ui = val;
	}
	SPODVariant(double val)
	: type(eDouble)
	{
		data.d = val;
	}
	SPODVariant(float val)
		: type(eFloat)
	{
		data.f = val;
	}
	SPODVariant(const char* val)
	: type(eConstStrPtr)
	{
		data.pStr = val;
	}
	SPODVariant(const wchar_t* val)
	: type(eConstWstrPtr)
	{
		data.pWstr = val;
	}
	SPODVariant(const void* val)
	: type(eConstVoidPtr)
	{
		data.pVoid = val;
	}
	SPODVariant(const EFlashVariableArrayType& val)
	: type(eFlashVariableArrayType)
	{
		data.fvat = val;
	}
	SPODVariant(const SFlashVarValue* val)
		: type(eFlashVarValuePtr)
	{
		data.pfvv = val;
	}
	SPODVariant(const SFlashVarValueList& val)
		: type(eFlashVarValueList)
	{
		data.fvvl = val;
	}

	Type type;
	Data data;
};


class CFlashFunctionProfilerLog
{
public:
	static CFlashFunctionProfilerLog& GetAccess()
	{
		static CFlashFunctionProfilerLog s_instance;
		return s_instance;
	}

	void Enable(bool enabled, int curFrame)
	{
		if (!enabled && m_file)
		{
			fclose(m_file);
			m_file = 0;
		}
		else if (enabled && !m_file)
		{
			m_file = fopen("flash.log", "wb");
		}
		m_enabled = enabled;

		if (m_curFrame != curFrame && m_file)
		{
			char frameMsg[512]; frameMsg[511] = '\0';
			_snprintf(frameMsg, sizeof(frameMsg) - 1, ">>>>>>>> Frame: %.10d <<<<<<<<", curFrame);
			fwrite("\n", 1, 1, m_file);
			fwrite(frameMsg, 1, strlen(frameMsg), m_file);
			fwrite("\n\n", 1, 2, m_file);
		}
		m_curFrame = curFrame;
	}

	bool IsEnabled() const
	{
		return m_enabled;
	}

	void Log(const char* pMsg)
	{
		if (m_file && pMsg)
		{
			fwrite(pMsg, 1, strlen(pMsg), m_file);
			fwrite("\n", 1, 1, m_file);
		}
	}

private:
	CFlashFunctionProfilerLog()
		: m_enabled(false)
		, m_curFrame(0)
		, m_file(0)
	{
	}

	~CFlashFunctionProfilerLog()
	{
	}

	bool m_enabled;
	unsigned int m_curFrame;
	FILE* m_file;
};


class CFlashFunctionProfiler
{
public:
	CFlashFunctionProfiler(float peakTolerance, EFlashFunctionID functionID, int numFuncArgs, const SPODVariant* pFuncArgs, 
		const char* pCustomFuncName, SFlashProfilerData* pProfilerData, const char* pFlashFilePath)
	: m_funcTime()
	, m_peakTolerance(peakTolerance)
	, m_functionID(functionID)
	, m_numFuncArgs(numFuncArgs)
	, m_pFuncArgs(pFuncArgs)
	, m_pCustomFuncName(pCustomFuncName)
	, m_pProfilerData(pProfilerData)
	, m_pFlashFilePath(pFlashFilePath)
	{
	}

	void ProfileEnter()
	{
		m_funcTime = gEnv->pTimer->GetAsyncTime();
	}

	void ProfilerLeave()
	{
		m_funcTime = gEnv->pTimer->GetAsyncTime() - m_funcTime;

		float prevTotalTimeMs(m_pProfilerData->GetTotalTimeHisto(m_functionID).GetCur());
		float curTotalTimeMs(m_funcTime.GetMilliSeconds());

		m_pProfilerData->AddTotalCalls(m_functionID, 1);
		m_pProfilerData->AddTotalTime(m_functionID, curTotalTimeMs);

		bool isPeak(curTotalTimeMs - prevTotalTimeMs > m_peakTolerance && (ms_peakFuncExcludeMask & (1 << m_functionID)) == 0);
		bool reqLog(CFlashFunctionProfilerLog::GetAccess().IsEnabled());
		if (isPeak || reqLog)
		{
			char msg[2048];
			FormatMsg(curTotalTimeMs, msg, sizeof(msg));

			if (isPeak)
				s_flashPeakHistory.Add(SFlashPeak(gEnv->pTimer->GetAsyncCurTime(), msg));

			if (reqLog)
				CFlashFunctionProfilerLog::GetAccess().Log(msg);
		}
	}

private:
	void FormatFlashVarValue(const SFlashVarValue* pArg, char* pBuf, size_t sizeBuf) const
	{
		switch (pArg->GetType())
		{
		case SFlashVarValue::eBool:
			_snprintf(pBuf, sizeBuf, "%s", pArg->GetBool() ? "true" : "false");
			break;
		case SFlashVarValue::eInt:
			_snprintf(pBuf, sizeBuf, "%d", pArg->GetInt());
			break;
		case SFlashVarValue::eUInt:
			_snprintf(pBuf, sizeBuf, "%u", pArg->GetUInt());
			break;
		case SFlashVarValue::eDouble:
			_snprintf(pBuf, sizeBuf, "%lf", pArg->GetDouble());
			break;
		case SFlashVarValue::eFloat:
			_snprintf(pBuf, sizeBuf, "%f", pArg->GetFloat());
			break;
		case SFlashVarValue::eConstStrPtr:
			_snprintf(pBuf, sizeBuf, "\"%s\"", pArg->GetConstStrPtr());
			break;
		case SFlashVarValue::eConstWstrPtr:
			_snprintf(pBuf, sizeBuf, "\"%ls\"", pArg->GetConstWstrPtr());
			break;
		default:
			assert(0);
		case SFlashVarValue::eUndefined:
			_snprintf(pBuf, sizeBuf, "???");
			break;
		}
	}
	void FormatMsg(float funcTimeMs, char* msgBuf, int sizeMsgBuf) const
	{
		char funcArgList[1024]; funcArgList[0] = '\0';
		{
			size_t pos(0);
			for (int i(0); i<m_numFuncArgs; ++i)
			{
				char curArg[512]; curArg[511] = '\0';
				switch(m_pFuncArgs[i].type)
				{
				case SPODVariant::eBool:
					{
						_snprintf(curArg, sizeof(curArg) - 1, "%s", m_pFuncArgs[i].data.b ? "true" : "false");
						break;
					}
				case SPODVariant::eInt:
					{
						_snprintf(curArg, sizeof(curArg) - 1, "%d", m_pFuncArgs[i].data.i);
						break;
					}
				case SPODVariant::eUInt:
					{
						_snprintf(curArg, sizeof(curArg) - 1, "%u", m_pFuncArgs[i].data.ui);
						break;
					}
				case SPODVariant::eDouble:
					{
						_snprintf(curArg, sizeof(curArg) - 1, "%lf", m_pFuncArgs[i].data.d);
						break;
					}
				case SPODVariant::eFloat:
					{
						_snprintf(curArg, sizeof(curArg) - 1, "%f", m_pFuncArgs[i].data.f);
						break;
					}
				case SPODVariant::eConstStrPtr:
					{
						_snprintf(curArg, sizeof(curArg) - 1, "\"%s\"", m_pFuncArgs[i].data.pStr);
						break;
					}
				case SPODVariant::eConstWstrPtr:
					{
						_snprintf(curArg, sizeof(curArg) - 1, "\"%ls\"", m_pFuncArgs[i].data.pWstr);
						break;
					}
				case SPODVariant::eConstVoidPtr:
					{
						_snprintf(curArg, sizeof(curArg) - 1, "0x%p", m_pFuncArgs[i].data.pVoid);
						break;
					}
				case SPODVariant::eFlashVariableArrayType:
					{
						const char* pFlashVarTypeName("FVAT_???");
						switch(m_pFuncArgs[i].data.fvat)
						{
						case FVAT_Int:
							pFlashVarTypeName = "FVAT_Int";
							break;
						case FVAT_Double:
							pFlashVarTypeName = "FVAT_Double";
							break;
						case FVAT_Float:
							pFlashVarTypeName = "FVAT_Float";
							break;
						case FVAT_ConstStrPtr:
							pFlashVarTypeName = "FVAT_ConstStrPtr";
							break;
						case FVAT_ConstWstrPtr:
							pFlashVarTypeName = "FVAT_ConstWstrPtr";
							break;
						default:
							assert(0);
							break;
						}
						_snprintf(curArg, sizeof(curArg) - 1, "%s", pFlashVarTypeName);
						break;
					}
				case SPODVariant::eFlashVarValuePtr:
					{
						FormatFlashVarValue(m_pFuncArgs[i].data.pfvv, curArg, sizeof(curArg));
						break;
					}
				case SPODVariant::eFlashVarValueList:
					{						
						curArg[0] = '\0';
						size_t curArgPos(0);

						const SFlashVarValue* pArgs(m_pFuncArgs[i].data.fvvl.pArgs);
						unsigned int numArgs(m_pFuncArgs[i].data.fvvl.numArgs);
						for (unsigned int i(0); i<numArgs; ++i)
						{
							char tmpArg[512]; tmpArg[511] = '\0';
							FormatFlashVarValue(&pArgs[i], tmpArg, sizeof(tmpArg) - 1);

							strncpy(&curArg[curArgPos], tmpArg, sizeof(curArg) - curArgPos - 1);
							curArgPos += strlen(&curArg[curArgPos]);

							if ((i < numArgs - 1) && curArgPos < sizeof(curArg) - 2)
							{
								curArg[curArgPos]   = ',';
								curArg[curArgPos+1] = ' ';
								curArgPos += 2;
							}
						}					
						break;
					}
				default:
					{
						curArg[0] = '\0';
						break;
					}
				}

				strncpy(&funcArgList[pos], curArg, sizeof(funcArgList) - pos - 1);
				pos += strlen(&funcArgList[pos]);
				
				if ((i < m_numFuncArgs - 1) && pos < sizeof(funcArgList) - 2)
				{
					funcArgList[pos]   = ',';
					funcArgList[pos+1] = ' ';
					pos += 2;
				}
			}
		}

		msgBuf[sizeMsgBuf - 1] = '\0';
		_snprintf(msgBuf, sizeMsgBuf - 1, "%.2f %s(%s) - %s", funcTimeMs, m_pCustomFuncName, funcArgList, m_pFlashFilePath);
	}

public:
	static void ResetPeakFuncExcludeMask()
	{
		ms_peakFuncExcludeMask = 0;
	}
	static void AddToPeakFuncExcludeMask(EFlashFunctionID functionID)
	{
		ms_peakFuncExcludeMask |= 1 << functionID;
	}

private:
	static unsigned int ms_peakFuncExcludeMask;

private:
	CTimeValue m_funcTime;
	float m_peakTolerance;
	EFlashFunctionID m_functionID;
	int m_numFuncArgs;
	const SPODVariant* m_pFuncArgs;
	const char* m_pCustomFuncName;
	SFlashProfilerData* m_pProfilerData;
	const char* m_pFlashFilePath;
};

unsigned int CFlashFunctionProfiler::ms_peakFuncExcludeMask(0);


class CFlashFunctionProfilerProxy
{
public:
	CFlashFunctionProfilerProxy(CFlashFunctionProfiler* pProfiler)
	: m_pProfiler(pProfiler)
	{
		if (m_pProfiler)
			m_pProfiler->ProfileEnter();
	}

	~CFlashFunctionProfilerProxy()
	{
		if (m_pProfiler)
			m_pProfiler->ProfilerLeave();
	}

private:
	CFlashFunctionProfiler* m_pProfiler;
};


#define FLASH_PROFILE_FUNC_PRECOND(defRet) \
	if (!IsFlashEnabled() || ms_sys_flash_info && m_pProfilerData && m_pProfilerData->PreventFunctionExectution()) \
		return defRet; 

#define FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(numArgs) \
	CFlashFunctionProfiler* pProfiler(0); \
	char memFlashProfiler[sizeof(CFlashFunctionProfiler)]; \
	char memFuncArgs[(numArgs > 0 ? numArgs : 1) * sizeof(SPODVariant)]; \
	if (ms_sys_flash_info) \
	{ \
		int numArgsInit(0); \
		SPODVariant* pArg((SPODVariant*)memFuncArgs); \

#define FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg) \
		new (pArg) SPODVariant(arg); \
		++pArg; \
		++numArgsInit; \

#define FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_VALIST(vargList, numArgs) \
		if (numArgs > 0) \
		{ \
			SPODVariant::SFlashVarValueList val; \
			val.pArgs = vargList; \
			val.numArgs = numArgs; \
			new (pArg) SPODVariant(val); \
			++pArg; \
			++numArgsInit; \
		} \

#define FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName) \
		if (!m_pProfilerData) \
			/*(SFlashProfilerData*)*/ m_pProfilerData = new SFlashProfilerData; \
		pProfiler = new (memFlashProfiler) CFlashFunctionProfiler(ms_sys_flash_info_peak_tolerance, funcID, numArgsInit, \
			numArgsInit > 0 ? (SPODVariant*)memFuncArgs : 0, funcCustomName, m_pProfilerData, m_filePath.c_str()); \
	} \
	CFlashFunctionProfilerProxy proxy(pProfiler);


#define VOID_RETURN ((void) 0)

#define FLASH_PROFILE_FUNC(funcID, defRet, funcCustomName) \
	FLASH_PROFILE_FUNC_PRECOND(defRet) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(0) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

#define FLASH_PROFILE_FUNC_1ARG(funcID, defRet, funcCustomName, arg0) \
	FLASH_PROFILE_FUNC_PRECOND(defRet) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(1) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

#define FLASH_PROFILE_FUNC_1ARG_VALIST(funcID, defRet, funcCustomName, arg0, vargList, numArgs) \
	FLASH_PROFILE_FUNC_PRECOND(defRet) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(2) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_VALIST(vargList, numArgs) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

#define FLASH_PROFILE_FUNC_2ARG(funcID, defRet, funcCustomName, arg0, arg1) \
	FLASH_PROFILE_FUNC_PRECOND(defRet) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(2) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg1) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

#define FLASH_PROFILE_FUNC_3ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2) \
	FLASH_PROFILE_FUNC_PRECOND(defRet) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(3) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg1) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg2) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

#define FLASH_PROFILE_FUNC_4ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2, arg3) \
	FLASH_PROFILE_FUNC_PRECOND(defRet) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(4) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg1) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg2) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg3) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

#define FLASH_PROFILE_FUNC_5ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2, arg3, arg4) \
	FLASH_PROFILE_FUNC_PRECOND(defRet) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(5) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg1) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg2) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg3) \
		FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg4) \
	FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)


//////////////////////////////////////////////////////////////////////////
// flash log context

class CFlashLogContext
{
public:
	CFlashLogContext(CryGFxLog* pLog, const char* pFlashContext)
	: m_pLog(pLog)
	, m_pPrevLogContext(0)
	{
		m_pPrevLogContext = pLog->GetContext();
		pLog->SetContext(pFlashContext);
	}

	~CFlashLogContext()
	{
		m_pLog->SetContext(m_pPrevLogContext);
	}

private:
	CryGFxLog* m_pLog;
	const char* m_pPrevLogContext;
};

#define SET_LOG_CONTEXT CFlashLogContext logContext(&CryGFxLog::GetAccess(), m_filePath.c_str());


//////////////////////////////////////////////////////////////////////////
// GFxValue <-> SFlashVarValue translation

static inline GFxValue ConvertValue(const SFlashVarValue& src)
{
	//FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	switch (src.GetType())
	{
	case SFlashVarValue::eBool:		
		return GFxValue(src.GetBool());

	case SFlashVarValue::eInt:
		return GFxValue((double)src.GetInt());

	case SFlashVarValue::eUInt:
		return GFxValue((double)src.GetUInt());

	case SFlashVarValue::eDouble:
		return GFxValue(src.GetDouble());

	case SFlashVarValue::eFloat:
		return GFxValue((double)src.GetFloat());

	case SFlashVarValue::eConstStrPtr:
		return GFxValue(src.GetConstStrPtr());

	case SFlashVarValue::eConstWstrPtr:
		return GFxValue(src.GetConstWstrPtr());

	case SFlashVarValue::eUndefined:
	default:
		return GFxValue();
	}
}

static inline SFlashVarValue ConvertValue(const GFxValue& src)
{
	//FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	switch (src.GetType())
	{
	case GFxValue::VT_Boolean:
		return SFlashVarValue(src.GetBool());

	case GFxValue::VT_Number:
		return SFlashVarValue(src.GetNumber());

	case GFxValue::VT_String:
		return SFlashVarValue(src.GetString());

	case GFxValue::VT_StringW:
		return SFlashVarValue(src.GetStringW());	

	case GFxValue::VT_Undefined:
	case GFxValue::VT_Null:
	default:
		return SFlashVarValue::CreateUndefined();
	}
}

static inline bool GetArrayType(EFlashVariableArrayType type, GFxMovie::SetArrayType& translatedType)
{
	//FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	switch (type)
	{
	case FVAT_Int:
		translatedType = GFxMovie::SA_Int;
		return true;
	case FVAT_Double:
		translatedType = GFxMovie::SA_Double;
		return true;
	case FVAT_Float:
		translatedType = GFxMovie::SA_Float;
		return true;
	case FVAT_ConstStrPtr:
		translatedType = GFxMovie::SA_String;
		return true;
	case FVAT_ConstWstrPtr:
		translatedType = GFxMovie::SA_StringW;
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
// implementation of IFlashPlayer

ICVar* CFlashPlayer::CV_sys_flash(0);
ICVar* CFlashPlayer::CV_sys_flash_edgeaa(0);
ICVar* CFlashPlayer::CV_sys_flash_info(0);
ICVar* CFlashPlayer::CV_sys_flash_info_peak_tolerance(0);
ICVar* CFlashPlayer::CV_sys_flash_info_peak_exclude(0);
ICVar* CFlashPlayer::CV_sys_flash_info_histo_scale(0);
ICVar* CFlashPlayer::CV_sys_flash_log_options(0);
ICVar* CFlashPlayer::CV_sys_flash_curve_tess_error(0);
ICVar* CFlashPlayer::CV_sys_flash_warning_level(0);

int CFlashPlayer::ms_sys_flash(1);
int CFlashPlayer::ms_sys_flash_edgeaa(1);
int CFlashPlayer::ms_sys_flash_info(0);
float CFlashPlayer::ms_sys_flash_info_peak_tolerance(5.0f);
float CFlashPlayer::ms_sys_flash_info_histo_scale(1.0f);
int CFlashPlayer::ms_sys_flash_log_options(0);
float CFlashPlayer::ms_sys_flash_curve_tess_error(1.0f);
int CFlashPlayer::ms_sys_flash_warning_level(1);

SLinkNode<CFlashPlayer> CFlashPlayer::ms_rootNode;
IFlashLoadMovieHandler* CFlashPlayer::ms_pLoadMovieHandler(0);


CFlashPlayer::CFlashPlayer()
: m_rgEnabled(false)
, m_allowEgdeAA(false)
, m_depth(0)
, m_renderConfig()
, m_asVerbosity()
, m_pFSCmdHandler(0)
, m_pEIHandler(0)
, m_pMovieDef(0)
, m_pMovieView(0)
, m_pLoader(0)
, m_pRenderer(0)
, m_filePath()
, m_node()
, m_pProfilerData(0)
{
	Link();
}


CFlashPlayer::~CFlashPlayer()
{
	Unlink();

	// release ref counts of all shared resources
	{
		SET_LOG_CONTEXT;
		
		m_pMovieView = 0;
		m_pMovieDef = 0;

		m_pLoader = 0;
		m_pRenderer = 0;
	}

	SAFE_DELETE(m_pProfilerData);
}


void CFlashPlayer::Release()
{
	if (m_rgEnabled)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "!Trying to release flash object \"%s\" in a guarded code section (callback, etc). Crashes are likely to occur!", m_filePath.c_str());

	delete this;
}


static void OnSysFlashInfoLogOptionsChanged(ICVar*)
{
	GFxLoader2* pLoader(CSharedFlashPlayerResources::GetAccess().GetLoader(true));
	if (pLoader)
		pLoader->UpdateParserVerbosity();
}


void CFlashPlayer::InitCVars()
{
	assert(!CV_sys_flash);

	CV_sys_flash = gEnv->pConsole->Register("sys_flash", &ms_sys_flash, 1, 
		VF_CHEAT, "Enables/disables execution of flash files.");

	CV_sys_flash_edgeaa = gEnv->pConsole->Register("sys_flash_edgeaa", &ms_sys_flash_edgeaa, 1, 
		0, "Enables/disables edge anti-aliased rendering of flash files.");

	CV_sys_flash_info = gEnv->pConsole->Register("sys_flash_info", &ms_sys_flash_info, 0, 
		0, "Enables flash profiling (1). Additionally sorts the list of flash files lexicographically (2, automatically resets to 1).");

	CV_sys_flash_info_peak_tolerance = gEnv->pConsole->Register("sys_flash_info_peak_tolerance", &ms_sys_flash_info_peak_tolerance, 5.0f, 
		0, "Defines tolerance value for peaks (in ms) inside the flash profiler.");

	CV_sys_flash_info_peak_exclude = gEnv->pConsole->RegisterString("sys_flash_info_peak_exclude", "", 
		0, "Comma separated list of flash functions to excluded from peak history.");

	CV_sys_flash_info_histo_scale = gEnv->pConsole->Register("sys_flash_info_histo_scale", &ms_sys_flash_info_histo_scale, 1.0f, 
		0, "Defines scaling of function histogram inside the flash profiler.");

	CV_sys_flash_log_options = gEnv->pConsole->Register("sys_flash_log_options", &ms_sys_flash_log_options, 0, 
		0, "Enables logging of several flash related aspects (add them to combine logging)...\n"
		"1) Flash loading                                                       : 1\n"
		"2) Flash actions script execution                                      : 2\n"
		"3) Flash related high-level calls inspected by the profiler into a file: 4\n"
		"   Please note that for (3) the following cvars apply:\n"
		"   * sys_flash_info\n"
		"   * sys_flash_info_peak_exclude", OnSysFlashInfoLogOptionsChanged);

	CV_sys_flash_curve_tess_error = gEnv->pConsole->Register("sys_flash_curve_tess_error", &ms_sys_flash_curve_tess_error, 1.0f, 
		0, "Controls curve tessellation. Larger values result in coarser, more angular curves.");

	CV_sys_flash_warning_level = gEnv->pConsole->Register("sys_flash_warning_level", &ms_sys_flash_warning_level, 1, 
		0, "Sets verbosity level for CryEngine related warnings...\n"
		"0) Omit warning\n"
		"1) Log warning\n"
		"2) Log warning and display message box");
}


int CFlashPlayer::GetWarningLevel()
{
	return ms_sys_flash_warning_level;
}


bool CFlashPlayer::Load(const char* pFilePath, unsigned int options)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	m_pLoader = *CSharedFlashPlayerResources::GetAccess().GetLoader();
	m_pRenderer = *CSharedFlashPlayerResources::GetAccess().GetRenderer();

	// copy file path
	m_filePath = pFilePath;

	SET_LOG_CONTEXT;

	// get movie info
	GFxMovieInfo movieInfo;
	if (!m_pLoader->GetMovieInfo(pFilePath, &movieInfo))
		return false;

	int warningLevel(GetWarningLevel());
	if (warningLevel && !movieInfo.IsStripped())
	{
		switch (warningLevel)
		{
		case 1:
			CryGFxLog::GetAccess().Log("Trying to load non-stripped flash movie! Use gfxexport to strip and compress embedded images.");
			break;
		case 2:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "!Trying to load non-stripped flash movie! "
				"Use gfxexport to strip and compress embedded images. [%s]", pFilePath);
			break;
		default:
			break;
		}
	}

	GPtr<GFxMovieDef> pMovieDef(0);
	GPtr<GFxMovieView> pMovieView(0);

	// create movie
	uint32 loadFlags(GFxLoader::LoadAll);
	pMovieDef = *m_pLoader->CreateMovie(pFilePath, loadFlags);
	if (!pMovieDef)
		return false;

	// register translator
	pMovieDef->SetTranslator(&CryGFxTranslator::GetAccess());

	// create movie view
	pMovieView = *pMovieDef->CreateInstance((options & INIT_FIRST_FRAME) ? 1 : 0);
	if (!pMovieView)
		return false;

	// set movie definition and movie view
	m_pMovieDef = pMovieDef;
	m_pMovieView = pMovieView;

	// set action script verbosity
	UpdateASVerbosity();
	m_pMovieView->SetActionControl(&m_asVerbosity);
	
	// register custom renderer and set renderer flags	
	unsigned int renderFlags(m_renderConfig.GetRenderFlags());
	renderFlags &= ~GFxRenderConfig::RF_StrokeMask;
	renderFlags |= GFxRenderConfig::RF_StrokeNormal;
	m_allowEgdeAA = (options & RENDER_EDGE_AA) != 0;
	if (m_allowEgdeAA)
		renderFlags |= GFxRenderConfig::RF_EdgeAA;
	m_renderConfig.SetRenderFlags(renderFlags);
	m_renderConfig.SetRenderer(m_pRenderer);
	m_pMovieView->SetRenderConfig(&m_renderConfig);

	// register action script (fs & external interface) and user event command handler, 
	// please note that client has to implement IFSCommandHandler interface 
	// and set it via IFlashPlayer::SetFSCommandHandler() to provide hook
	m_pMovieView->SetUserData(this);
	m_pMovieView->SetFSCommandHandler(&CryGFxFSCommandHandler::GetAccess());
	m_pMovieView->SetExternalInterface(&CryGFxExternalInterface::GetAccess());
	m_pMovieView->SetUserEventHandler(&CryGFxUserEventHandler::GetAccess());

	// enable mouse support
	m_pMovieView->EnableMouseSupport((options & ENABLE_MOUSE_SUPPORT) ? true: false);

	// done
	return true;
}


void CFlashPlayer::SetBackgroundColor(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		GColor color;
		color.SetRGBA(red, green, blue, alpha);
		m_pMovieView->SetBackgroundColor(color);
	}
}


void CFlashPlayer::SetBackgroundAlpha(float alpha)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
		m_pMovieView->SetBackgroundAlpha(alpha);
}


void CFlashPlayer::SetViewport(int x0, int y0, int width, int height, float aspectRatio)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		GViewport viewport(width, height, x0, y0, width, height, 0); // invalidates scissor rect!
		viewport.AspectRatio = aspectRatio > 0 ? aspectRatio : 1;
		m_pMovieView->SetViewport(viewport);
	}
}


void CFlashPlayer::GetViewport(int& x0, int& y0, int& width, int& height, float& aspectRatio)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		x0 = viewport.Left;
		y0 = viewport.Top;
		width = viewport.Width;
		height = viewport.Height;
		aspectRatio = viewport.AspectRatio;
	}
	else
	{
		x0 = y0 = width = height = 0;
		aspectRatio = 1;
	}
}


void CFlashPlayer::SetScissorRect(int x0, int y0, int width, int height)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		viewport.SetScissorRect(x0, y0, width, height);
		m_pMovieView->SetViewport(viewport);
	}
}


void CFlashPlayer::GetScissorRect(int& x0, int& y0, int& width, int& height)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;	
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		if (viewport.Flags & GViewport::View_UseScissorRect)
		{
			x0 = viewport.ScissorLeft;
			y0 = viewport.ScissorTop;
			width = viewport.ScissorWidth;
			height = viewport.ScissorHeight;
			return;
		}
	}
	x0 = y0 = width = height = 0;
}


void CFlashPlayer::Advance(float deltaTime)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	FLASH_PROFILE_FUNC_1ARG(eFncAdvance, VOID_RETURN, "Advance", deltaTime);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		// UI should run at full speed even if time scaling is used -- MarcoK
		// TODO: revise! 
		//   Either  caller should adjust the time or each flash instance should have 
		//   the means to decide whether to adjust for bullet time or not -- CarstenW
		float timeScale = gEnv->pTimer->GetTimeScale();
		if (timeScale < 1.0f)
		{
			timeScale = max(0.0001f, timeScale);
			deltaTime = deltaTime / timeScale;
		}

		UpdateASVerbosity();
		m_pMovieView->Advance(deltaTime);
	}
}


void CFlashPlayer::Render()
{
	if (ms_sys_flash_info)
		gEnv->pRenderer->SF_Flush();

	{
		FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
		FLASH_PROFILE_FUNC(eFncDisplay, VOID_RETURN, "Display");
		SET_LOG_CONTEXT;
    int* isLost((int*) m_pRenderer->GetXRender()->EF_Query(EFQ_DeviceLost, 0));
		if (m_pMovieView && !*isLost)
		{
			UpdateRenderFlags();
			m_pRenderer->SetCompositingDepth(m_depth);
			m_pMovieView->Display();
		}

		if (ms_sys_flash_info)
			gEnv->pRenderer->SF_Flush();
	}
}


void CFlashPlayer::SetCompositingDepth(float depth)
{
	m_depth = depth;
}


void CFlashPlayer::SetFSCommandHandler(IFSCommandHandler* pHandler)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	m_pFSCmdHandler = pHandler;
}


void CFlashPlayer::SetExternalInterfaceHandler(IExternalInterfaceHandler* pHandler)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	m_pEIHandler = pHandler;
}


void CFlashPlayer::SendCursorEvent(const SFlashCursorEvent& cursorEvent)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		switch (cursorEvent.m_state)
		{
		case SFlashCursorEvent::eCursorMoved:
			{
				GFxMouseEvent event(GFxEvent::MouseMove, 0, (SInt16) cursorEvent.m_cursorX, (SInt16) cursorEvent.m_cursorY);
				m_pMovieView->HandleEvent(event);
				break;
			}
		case SFlashCursorEvent::eCursorPressed:
			{
				GFxMouseEvent event(GFxEvent::MouseDown, 0, (SInt16) cursorEvent.m_cursorX, (SInt16) cursorEvent.m_cursorY);
				m_pMovieView->HandleEvent(event);
				break;
			}
		case SFlashCursorEvent::eCursorReleased:
			{
				GFxMouseEvent event(GFxEvent::MouseUp, 0, (SInt16) cursorEvent.m_cursorX, (SInt16) cursorEvent.m_cursorY);
				m_pMovieView->HandleEvent(event);
				break;
			}
		}
	}
}


void CFlashPlayer::SendKeyEvent(const SFlashKeyEvent& keyEvent)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		GFxKeyEvent event(keyEvent.m_state == SFlashKeyEvent::eKeyDown ? GFxEvent::KeyDown : GFxEvent::KeyUp, 
			(GFxKey::Code) keyEvent.m_keyCode, keyEvent.m_asciiCode, keyEvent.m_wcharCode);

		event.SpecialKeysState.SetShiftPressed((keyEvent.m_specialKeyState & SFlashKeyEvent::eShiftPressed) != 0);
		event.SpecialKeysState.SetCtrlPressed((keyEvent.m_specialKeyState & SFlashKeyEvent::eCtrlPressed) != 0);
		event.SpecialKeysState.SetAltPressed((keyEvent.m_specialKeyState & SFlashKeyEvent::eAltPressed) != 0);
		event.SpecialKeysState.SetCapsToggled((keyEvent.m_specialKeyState & SFlashKeyEvent::eCapsToggled) != 0);
		event.SpecialKeysState.SetNumToggled((keyEvent.m_specialKeyState & SFlashKeyEvent::eNumToggled) != 0);
		event.SpecialKeysState.SetScrollToggled((keyEvent.m_specialKeyState & SFlashKeyEvent::eScrollToggled) != 0);

		m_pMovieView->HandleEvent(event);
	}
}


void CFlashPlayer::SetVisible(bool visible)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
		m_pMovieView->SetVisible(visible);
}


bool CFlashPlayer::GetVisible() const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	bool res(false);
	if (m_pMovieView)
		res = m_pMovieView->GetVisible();	
	return res;
}


bool CFlashPlayer::SetVariable(const char* pPathToVar, const SFlashVarValue& value)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	FLASH_PROFILE_FUNC_2ARG(eFncSetVar, false, "SetVariable", pPathToVar, &value);
	SET_LOG_CONTEXT;
	bool res(false);
	if (m_pMovieView)
		res = m_pMovieView->SetVariable(pPathToVar, ConvertValue(value));
	return res;
}


bool CFlashPlayer::GetVariable(const char* pPathToVar, SFlashVarValue* pValue) const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	FLASH_PROFILE_FUNC_2ARG(eFncGetVar, false, "GetVariable", pPathToVar, (void*) pValue);
	SET_LOG_CONTEXT;
	bool res(false);
	if (m_pMovieView && pValue)
	{
		GFxValue retVal;
		res = m_pMovieView->GetVariable(&retVal, pPathToVar);
		*pValue = ConvertValue(retVal);
	}
	return res;
}


bool CFlashPlayer::IsAvailable(const char* pPathToVar) const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	FLASH_PROFILE_FUNC_1ARG(eFncIsAvailable, false, "IsAvailable", pPathToVar);
	SET_LOG_CONTEXT;
	bool res(false);
	if (m_pMovieView)
		res = m_pMovieView->IsAvailable(pPathToVar);
	return res;
}


bool CFlashPlayer::SetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, const void* pData, unsigned int count) const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	FLASH_PROFILE_FUNC_5ARG(eFncSetVar, false, "SetVariableArray", type, pPathToVar, index, pData, count);
	SET_LOG_CONTEXT;
	GFxMovie::SetArrayType translatedType;
	bool res(false);
	if (m_pMovieView && GetArrayType(type, translatedType))
		res = m_pMovieView->SetVariableArray(translatedType, pPathToVar, index, pData, count);
	return res;
}


unsigned int CFlashPlayer::GetVariableArraySize(const char* pPathToVar) const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	FLASH_PROFILE_FUNC_1ARG(eFncGetVar, 0, "GetVariableArraySize", pPathToVar);
	SET_LOG_CONTEXT;
	int res(0);
	if (m_pMovieView)
		res = m_pMovieView->GetVariableArraySize(pPathToVar);
	return res;
}


bool CFlashPlayer::GetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, void* pData, unsigned int count)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	FLASH_PROFILE_FUNC_5ARG(eFncGetVar, false, "GetVariableArray", type, pPathToVar, index, pData, count);
	SET_LOG_CONTEXT;
	bool res(false);
	if (m_pMovieView && pData)
	{
		GFxMovie::SetArrayType translatedType;
		if (GetArrayType(type, translatedType))
			res = m_pMovieView->GetVariableArray(translatedType, pPathToVar, index, pData, count);
	}
	return res;
}


bool CFlashPlayer::Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	FLASH_PROFILE_FUNC_1ARG_VALIST(eFncInvoke, false, "Invoke", pMethodName, pArgs, numArgs);
	SET_LOG_CONTEXT;
	bool res(false);
	if (m_pMovieView && (pArgs || !numArgs))
	{
		assert(!pArgs || numArgs);
		GFxValue* pTranslatedArgs(0);
		if (pArgs && numArgs)
		{
			pTranslatedArgs = (GFxValue*) alloca(numArgs * sizeof(GFxValue));
			if (pTranslatedArgs)
			{
				for (unsigned int i(0); i<numArgs; ++i)
					pTranslatedArgs[i] = ConvertValue(pArgs[i]);
			}
		}

		if (pTranslatedArgs || !numArgs)
		{
			GFxValue retVal;
			res = m_pMovieView->Invoke(pMethodName, &retVal, pTranslatedArgs, numArgs);
			if (pResult)
				*pResult = ConvertValue(retVal);
		}
	}
	return res;
}


int CFlashPlayer::GetWidth() const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	int res(0);
	if (m_pMovieDef)
		res = (int) m_pMovieDef->GetWidth();
	return res;
}


int CFlashPlayer::GetHeight() const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	int res(0);
	if (m_pMovieDef)
		res = (int) m_pMovieDef->GetHeight();
	return res;
}


size_t CFlashPlayer::GetMetadata(char* pBuff, unsigned int buffSize) const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	size_t res(0);
	if (m_pMovieDef)
		res = m_pMovieDef->GetMetadata(pBuff, buffSize);
	return res;
}


const char* CFlashPlayer::GetFilePath() const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	return m_filePath.c_str();
}


void CFlashPlayer::ScreenToClient(int& x, int& y) const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		x -= viewport.Left;
		y -= viewport.Top;
	}
	else
	{
		x = y = 0;
	}
}


void CFlashPlayer::ClientToScreen(int& x, int& y) const
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	SET_LOG_CONTEXT;
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		x += viewport.Left;
		y += viewport.Top;
	}
}


void CFlashPlayer::DelegateFSCommandCallback(const char* pCommand, const char* pArgs)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	// delegate action script command to client
	if (m_pFSCmdHandler)
	{
		EnableReleaseGuard(true);
		m_pFSCmdHandler->HandleFSCommand(pCommand, pArgs);
		EnableReleaseGuard(false);
	}
}


void CFlashPlayer::DelegateExternalInterfaceCallback(const char* pMethodName, const GFxValue* pArgs, UInt numArgs)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	if (m_pMovieView)
	{
		// delegate action script command to client
		GFxValue retVal(GFxValue::VT_Undefined);
		if (m_pEIHandler)
		{
			EnableReleaseGuard(true);

			assert(!pArgs || numArgs);
			SFlashVarValue* pTranslatedArgs(0);
			if (pArgs && numArgs)
			{
				pTranslatedArgs = (SFlashVarValue*) alloca(numArgs * sizeof(SFlashVarValue));
				if (pTranslatedArgs)
				{
					for (unsigned int i(0); i<numArgs; ++i)
						pTranslatedArgs[i] = ConvertValue(pArgs[i]);
				}
			}

			if (pTranslatedArgs || !numArgs)
			{
				SFlashVarValue ret(SFlashVarValue::eUndefined);
				m_pEIHandler->HandleExternalInterfaceCall(pMethodName, pTranslatedArgs, numArgs, &ret);
				retVal = ConvertValue(ret);
			}

			EnableReleaseGuard(false);
		}
		m_pMovieView->SetExternalInterfaceRetVal(retVal);
	}
}


bool CFlashPlayer::IsFlashEnabled()
{
	return ms_sys_flash != 0;
}


bool CFlashPlayer::IsEdgeAaAllowed() const
{
	return m_allowEgdeAA && (ms_sys_flash_edgeaa != 0);
}


void CFlashPlayer::UpdateRenderFlags()
{
	const unsigned int renderFlags(m_renderConfig.GetRenderFlags());
	
	unsigned int newRenderFlags(renderFlags);
	newRenderFlags &= ~GFxRenderConfig::RF_EdgeAA;
	if (IsEdgeAaAllowed())
		newRenderFlags |= GFxRenderConfig::RF_EdgeAA;

	if (newRenderFlags != renderFlags)
		m_renderConfig.SetRenderFlags(newRenderFlags);

	float curMaxCurveError(m_renderConfig.GetMaxCurvePixelError());
	if (ms_sys_flash_curve_tess_error != curMaxCurveError)
	{
		if (ms_sys_flash_curve_tess_error < 1.0f)
			ms_sys_flash_curve_tess_error = 1.0f;
		m_renderConfig.SetMaxCurvePixelError(ms_sys_flash_curve_tess_error);
	}
}


void CFlashPlayer::UpdateASVerbosity()
{
	m_asVerbosity.SetActionFlags((ms_sys_flash_log_options & CFlashPlayer::LO_ACTIONSCRIPT) ? GFxActionControl::Action_Verbose : 0);
}


void CFlashPlayer::Link()
{
	m_node.m_pHandle = this;
	m_node.Link(&ms_rootNode);
}


void CFlashPlayer::Unlink()
{
	m_node.Unlink();
	m_node.m_pHandle = 0;
}


void CFlashPlayer::EnableReleaseGuard(bool enabled)
{
	m_rgEnabled = enabled;
}


static void DrawText(float x, float y, float* pColor, const char* pFormat, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, pFormat);
	vsnprintf(buffer, sizeof(buffer), pFormat, args);
	va_end(args);

	gEnv->pRenderer->Draw2dLabel(x, y, 1.4f, pColor, false, "%s", buffer);
}


static void DrawTextNoFormat(float x, float y, float* pColor, const char* pText)
{
	gEnv->pRenderer->Draw2dLabel(x, y, 1.4f, pColor, false, "%s", pText);
}


static inline float CalculateFlashVarianceFactor(float value, float variance)
{
	const float VAL_EPSILON(0.000001f);
	const float VAR_MULTIPLIER(2.0f);

	//variance = fabs(variance - value*value);
	float difference((float)sqrt_tpl(variance));

	value = (float) fabs(value);
	if (value < VAL_EPSILON)
		return 0;

	float factor(0);
	if (value > 0.01f)
		factor = (difference / value) * VAR_MULTIPLIER;

	return factor;
}


static void CalculateColor(float factor, float* pOutColor)
{
	float ColdColor[4] = {0.15f, 0.9f, 0.15f, 1};
	float HotColor[4] = {1, 1, 1, 1};

	for (int k=0; k<3; ++k)
		pOutColor[k] = HotColor[k] * factor + ColdColor[k] * (1.0f - factor);
	pOutColor[3] = 1;
}


static void CalculateColor(float value, float variance, float* pOutColor)
{
	CalculateColor(clamp_tpl(CalculateFlashVarianceFactor(value, variance), 0.0f, 1.0f), pOutColor);
}


static float CalculateVariance(float value, float avg)
{
	return (value - avg) * (value - avg);
}


void CFlashPlayer::RenderFlashInfo()
{
	CFlashFunctionProfilerLog::GetAccess().Enable(ms_sys_flash_info != 0 && (ms_sys_flash_log_options & LO_PEAKS) != 0, gEnv->pRenderer->GetFrameID(false));

	GRendererXRender* pFlashRenderer(CSharedFlashPlayerResources::GetAccess().GetRenderer(true));

	if (ms_sys_flash_info)
	{
		// define color
		const float inactiveColChannelMult(0.7f);
		float color[4] = {1, 1, 1, 1};
		float colorList[4] = {0.3f, 0.8f, 1, 1};
		float colorListInactive[4] = {0.7f, 0.7f, 0.7f, 1};
		float colorListSelected[4] = {1, 0, 0, 1};
		float colorListSelectedInactive[4] = {0.7f, 0, 0, 1};

		// display number of primitives rendered
		GRenderer::Stats stats;
		if (pFlashRenderer)
			pFlashRenderer->GetRenderStats(&stats, true);
		DrawText(10.0f, 10.0f, color, "#Tris : %d", stats.Triangles);
		DrawText(10.0f, 22.0f, color, "#Lines : %d", stats.Lines);
		DrawText(10.0f, 34.0f, color, "#DrawCalls : %d", stats.Primitives);

		// display memory statistics
		const GAllocator* pBlockAllocator(GMemory::GetBlockAllocator());
		const GAllocator* pAllocator(GMemory::GetAllocator());
		const GAllocatorStats* pBlockAllocatorStats(pBlockAllocator ? pBlockAllocator->GetStats() : 0);
		const GAllocatorStats* pAllocatorStats(pAllocator ? pAllocator->GetStats() : 0);

		if (pBlockAllocatorStats && pAllocatorStats)
		{
			DrawText(10.0f, 55.0f, color, "#Alloc() : %d", pBlockAllocatorStats->AllocCount + pAllocatorStats->AllocCount);
			DrawText(10.0f, 67.0f, color, "#Realloc() : %d", pBlockAllocatorStats->ReallocCount + pAllocatorStats->ReallocCount);
			DrawText(10.0f, 79.0f, color, "#Free() : %d", pBlockAllocatorStats->FreeCount + pAllocatorStats->FreeCount);
			DrawText(10.0f, 96.0f, color, "MemPoolSize : %dk", (pBlockAllocatorStats->GetSizeAllocated() + 1023) >> 10);
			DrawText(10.0f,108.0f, color, "Memory used : %dk", (pAllocatorStats->GetSizeAllocated() + 1023) >> 10);
			DrawText(10.0f,120.0f, color, "TexMemory used : %dk", (GTextureXRender::GetTextureMemoryUsed() + 1023) >> 10);
		}

		// sort lexicographically if requested
		if (ms_sys_flash_info == 2)
		{
			bool needSorting(true);
			while (needSorting)
			{
				needSorting = false;
				SLinkNode<CFlashPlayer>* pNode(ms_rootNode.m_pNext);
				SLinkNode<CFlashPlayer>* pNext(pNode->m_pNext);
				while (pNext != &ms_rootNode)
				{					
					if (pNode->m_pHandle->m_filePath.compareNoCase(pNext->m_pHandle->m_filePath) > 0)
					{
						needSorting = true;

						SLinkNode<CFlashPlayer>* pNodePrev(pNode->m_pPrev);
						SLinkNode<CFlashPlayer>* pNextNext(pNext->m_pNext);

						pNodePrev->m_pNext = pNext;
						pNextNext->m_pPrev = pNode;

						pNode->m_pPrev = pNext;
						pNode->m_pNext = pNextNext;

						pNext->m_pPrev = pNodePrev;
						pNext->m_pNext = pNode;
					}
					else
						pNode = pNode->m_pNext;

					pNext = pNode->m_pNext;
				}
			}

			ms_sys_flash_info = 1;
		}

		// toggle write protection to pause profiling
		// (should be done before display & gathering of profile data in next frame)		
		if (CryGetAsyncKeyState(VK_SCROLL) & 1)
		{
			SFlashProfilerData::ToggleHistogramWriteProtection();
			s_flashPeakHistory.ToggleWriteProtection();
		}
	
		// ypos = 142.0f; placeholder for number of (active) flash files 
		int numFlashFiles(0), numActiveFlashFiles(0);

		// ypos = 154.0f; placeholder for overall cost of flash processing / rendering
		// display flash instance profiling results
		static SLinkNode<CFlashPlayer>* s_pLastSelectedLinkNode(ms_rootNode.m_pNext);
		bool lastSelectedLinkNodeLost(true);

		const float deltaFrameTime(gEnv->pTimer->GetFrameTime());
		const float blendFactor(exp(-deltaFrameTime / 0.35f));
		float ypos(178.0f);
		SLinkNode<CFlashPlayer>* pNode(ms_rootNode.m_pNext);
		while (pNode != &ms_rootNode)
		{
			if (s_pLastSelectedLinkNode == pNode)
				lastSelectedLinkNodeLost = false;
		
			CFlashPlayer* pFlashPlayer(pNode->m_pHandle);
			SFlashProfilerData* pFlashProfilerData(pFlashPlayer ? pFlashPlayer->m_pProfilerData : 0);

			float funcCostAvg[eNumFncIDs], funcCostMin[eNumFncIDs], funcCostMax[eNumFncIDs], funcCostCur[eNumFncIDs];
			float totalCostAvg(0), totalCostMin(0), totalCostMax(0), totalCostCur(0);
			if (pFlashProfilerData)
			{
				for (int i(0); i<eNumFncIDs; ++i)
				{
					funcCostAvg[i] = pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetAvg();
					funcCostMin[i] = pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetMin();
					funcCostMax[i] = pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetMax();
					funcCostCur[i] = pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetCur();
				}

				totalCostAvg = pFlashProfilerData->GetTotalTimeHistoAllFuncs().GetAvg();
				totalCostMin = pFlashProfilerData->GetTotalTimeHistoAllFuncs().GetMin();
				totalCostMax = pFlashProfilerData->GetTotalTimeHistoAllFuncs().GetMax();
				totalCostCur = pFlashProfilerData->GetTotalTimeHistoAllFuncs().GetCur();
			}
			else
			{
				for (int i(0); i<eNumFncIDs; ++i)
				{
					funcCostAvg[i] = 0;
					funcCostMin[i] = 0;
					funcCostMax[i] = 0;
					funcCostCur[i] = 0;
				}
			}

			const char* pVisExpChar("");
			if (pFlashProfilerData)
				pVisExpChar = pFlashProfilerData->IsViewExpanded() ? "-" : "+";
			
			const char* pInstanceInfo("");
			if (pFlashPlayer && (!pFlashPlayer->m_pMovieDef || !pFlashPlayer->m_pMovieView))
				pInstanceInfo = "!!! Instanciated but Load() failed !!!";

			const char* pDisabled("");
			if (pFlashProfilerData && pFlashProfilerData->PreventFunctionExectution())
				pDisabled = "[disabled]";

			bool selected(s_pLastSelectedLinkNode == pNode); 
			bool inactive(!pFlashProfilerData || totalCostAvg < 1e-3f);

			float* col(0);
			if (inactive)
				col = (!selected) ? colorListInactive : colorListSelectedInactive;
			else
				col = (!selected) ? colorList : colorListSelected;
			
			DrawText(10.0f, ypos, col, "%.2f", totalCostAvg);
			DrawText(50.0f, ypos, col, "%s %s%s (pMovieDef = 0x%p, GetVisible() = %d) %s", pDisabled, pVisExpChar, 
				pFlashPlayer->m_filePath.c_str(), pFlashPlayer->m_pMovieDef.GetPtr(), !inactive && pFlashPlayer->GetVisible() ? 1 : 0, pInstanceInfo);
			ypos += 12.0f;

			if (pFlashProfilerData && pFlashProfilerData->IsViewExpanded())
			{
				float colChannelMult(!inactive ? 1 : inactiveColChannelMult);
				float colorDyn[4] = {0, 0, 0, 0};
				{
					float newColorValue(totalCostCur * (1 - blendFactor) + pFlashProfilerData->GetColorValue(eNumFncIDs) * blendFactor);
					float variance(CalculateVariance(totalCostCur, totalCostAvg));
					float newColorValueVariance(variance * (1 - blendFactor) + pFlashProfilerData->GetColorValueVariance(eNumFncIDs) * blendFactor);

					pFlashProfilerData->SetColorValue(eNumFncIDs, newColorValue);
					pFlashProfilerData->SetColorValueVariance(eNumFncIDs, newColorValueVariance);

					CalculateColor(newColorValue, newColorValueVariance, colorDyn);
					colorDyn[0] *= colChannelMult;
					colorDyn[1] *= colChannelMult;
					colorDyn[2] *= colChannelMult;

					ypos += 4.0f;
					DrawText( 50.0f, ypos, colorDyn, "Total:");
					DrawText(150.0f, ypos, colorDyn, "avg %.2f", totalCostAvg);
					DrawText(215.0f, ypos, colorDyn, "/ min %.2f", totalCostMin);
					DrawText(292.0f, ypos, colorDyn, "/ max %.2f", totalCostMax);
					ypos += 20.0f;
				}

				if (ms_sys_flash_info_histo_scale < 1e-2f)
					ms_sys_flash_info_histo_scale = 1e-2f;
				float histoScale(ms_sys_flash_info_histo_scale * 50.0f);

				for (int i(0); i<eNumFncIDs; ++i)
				{
					float newColorValue(funcCostCur[i] * (1 - blendFactor) + pFlashProfilerData->GetColorValue((EFlashFunctionID)i) * blendFactor);
					float variance(CalculateVariance(funcCostCur[i], funcCostAvg[i]));
					float newColorValueVariance(variance * (1 - blendFactor) + pFlashProfilerData->GetColorValueVariance((EFlashFunctionID)i) * blendFactor);

					pFlashProfilerData->SetColorValue((EFlashFunctionID)i, newColorValue);
					pFlashProfilerData->SetColorValueVariance((EFlashFunctionID)i, newColorValueVariance);

					CalculateColor(newColorValue, newColorValueVariance, colorDyn);
					colorDyn[0] *= colChannelMult;
					colorDyn[1] *= colChannelMult;
					colorDyn[2] *= colChannelMult;

					int numAvgCalls(pFlashProfilerData->GetTotalCallsHisto((EFlashFunctionID)i).GetAvg());
					DrawText( 25.0f, ypos, colorDyn, "%d/", numAvgCalls);
					DrawText( 50.0f, ypos, colorDyn, "%s()", GetFlashFunctionName((EFlashFunctionID)i));
					DrawText(150.0f, ypos, colorDyn, "avg %.2f", funcCostAvg[i]);
					DrawText(215.0f, ypos, colorDyn, "/ min %.2f", funcCostMin[i]);
					DrawText(292.0f, ypos, colorDyn, "/ max %.2f", funcCostMax[i]);

					unsigned char histogram[SFlashProfilerData::HistogramSize];
					for (int j(0); j<SFlashProfilerData::HistogramSize; ++j)
						histogram[j] = 255 - (unsigned int) clamp_tpl(pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetAt(j) * histoScale, 0.0f, 255.0f);

					ColorF histogramCol(0,1,0,1);
					gEnv->pRenderer->Graph(histogram, 400, (int) ypos, SFlashProfilerData::HistogramSize, 14, pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetCurIdx(), 2, 0, histogramCol, 1);

					ypos += 16.0f;
				}
			}

			if (pFlashProfilerData)
				pFlashProfilerData->Advance();

			++numFlashFiles;
			if (!inactive)
				++numActiveFlashFiles;

			pNode = pNode->m_pNext;	
		}

		// fill placeholders
		DrawText( 10.0f, 142.0f, color, "#FlashObj : %d", numFlashFiles);
		DrawText(110.0f, 142.0f, colorList, "/ #Active : %d", numActiveFlashFiles);
		DrawText(210.0f, 142.0f, colorListInactive, "/ #Inactive : %d", numFlashFiles - numActiveFlashFiles);

		DrawText( 10.0f, 160.0f, color, "Total:");
		DrawText( 50.0f, 160.0f, color, "avg %.2f", SFlashProfilerData::GetTotalTimeHistoAllInsts().GetAvg());
		DrawText(115.0f, 160.0f, color, "/ min %.2f", SFlashProfilerData::GetTotalTimeHistoAllInsts().GetMin());
		DrawText(192.0f, 160.0f, color, "/ max %.2f", SFlashProfilerData::GetTotalTimeHistoAllInsts().GetMax());

		SFlashProfilerData::AdvanceAllInsts();

		const char* pProfilerPaused("");
		if (SFlashProfilerData::HistogramWriteProtected())
			pProfilerPaused = "[paused]";		
		DrawText(280.0f, 160.0f, color, "%s", pProfilerPaused);

		// display flash peak history
		ypos += 4.0f;		
		float displayTime(gEnv->pTimer->GetAsyncCurTime());
		DrawText(10.0f, ypos, color, "Latest flash peaks (tolerance = %.2f ms)...", ms_sys_flash_info_peak_tolerance);
		ypos += 16.0f;

		s_flashPeakHistory.UpdateHistory(displayTime);
		const float maxFlashPeakDisplayTime(s_flashPeakHistory.GetTimePeakExpires());

		const CFlashPeakHistory::FlashPeakHistory& peakHistory(s_flashPeakHistory.GetHistory());
		CFlashPeakHistory::FlashPeakHistory::const_iterator it(peakHistory.begin());
		CFlashPeakHistory::FlashPeakHistory::const_iterator itEnd(peakHistory.end());
		for (; it != itEnd;)
		{
			const SFlashPeak& peak(*it);
			float colorDyn[4] = {0, 0, 0, 0};
			CalculateColor(clamp_tpl(1.0f - (displayTime - peak.m_timeRecorded) / maxFlashPeakDisplayTime, 0.0f, 1.0f), colorDyn);
			DrawTextNoFormat(10.0f, ypos, colorDyn, peak.m_displayInfo.c_str());
			ypos += 12.0f;
			++it;
		}

		// update peak history function exclude mask
		CFlashFunctionProfiler::ResetPeakFuncExcludeMask();
		const char* pExludeMaskStr(CV_sys_flash_info_peak_exclude ? CV_sys_flash_info_peak_exclude->GetString() : 0);
		if (pExludeMaskStr)
		{
			for (int i(0); i<eNumFncIDs; ++i)
			{
				EFlashFunctionID funcID((EFlashFunctionID) i);
				const char* pFlashFuncName(GetFlashFunctionName(funcID));				
				if (strstr(pExludeMaskStr, pFlashFuncName))
					CFlashFunctionProfiler::AddToPeakFuncExcludeMask(funcID);
			}
		}

		// allow user navigation for detailed profiling statistics
		if (lastSelectedLinkNodeLost)
			s_pLastSelectedLinkNode = ms_rootNode.m_pNext;

		if (CryGetAsyncKeyState(VK_UP) & 1)
		{
			if (CryGetAsyncKeyState(VK_CONTROL) & 0x8000)
			{
				SLinkNode<CFlashPlayer>* pNode(&ms_rootNode);
				SLinkNode<CFlashPlayer>* pNext(ms_rootNode.m_pNext);

				SLinkNode<CFlashPlayer>* pNodePrev(pNode->m_pPrev);
				SLinkNode<CFlashPlayer>* pNextNext(pNext->m_pNext);

				pNodePrev->m_pNext = pNext;
				pNextNext->m_pPrev = pNode;

				pNode->m_pPrev = pNext;
				pNode->m_pNext = pNextNext;

				pNext->m_pPrev = pNodePrev;
				pNext->m_pNext = pNode;
			}
			else
			{
				s_pLastSelectedLinkNode = s_pLastSelectedLinkNode->m_pPrev;
				if (s_pLastSelectedLinkNode == &ms_rootNode)
					s_pLastSelectedLinkNode = s_pLastSelectedLinkNode->m_pPrev;
			}
		}

		if (CryGetAsyncKeyState(VK_DOWN) & 1)
		{
			if (CryGetAsyncKeyState(VK_CONTROL) & 0x8000)
			{
				SLinkNode<CFlashPlayer>* pNode(&ms_rootNode);
				SLinkNode<CFlashPlayer>* pPrev(ms_rootNode.m_pPrev);

				SLinkNode<CFlashPlayer>* pNodeNext(pNode->m_pNext);
				SLinkNode<CFlashPlayer>* pPrevPrev(pPrev->m_pPrev);

				pNodeNext->m_pPrev = pPrev;
				pPrevPrev->m_pNext = pNode;

				pNode->m_pNext = pPrev;
				pNode->m_pPrev = pPrevPrev;

				pPrev->m_pNext = pNodeNext;
				pPrev->m_pPrev = pNode;
			}
			else
			{
				s_pLastSelectedLinkNode = s_pLastSelectedLinkNode->m_pNext;
				if (s_pLastSelectedLinkNode == &ms_rootNode)
					s_pLastSelectedLinkNode = s_pLastSelectedLinkNode->m_pNext;
			}
		}

		if (CryGetAsyncKeyState(VK_RIGHT) & 1)
		{
			if (s_pLastSelectedLinkNode != &ms_rootNode)
			{
				CFlashPlayer* pFlashPlayer(s_pLastSelectedLinkNode->m_pHandle);
				SFlashProfilerData* pFlashProfilerData(pFlashPlayer ? pFlashPlayer->m_pProfilerData : 0);
				if (pFlashProfilerData)
					pFlashProfilerData->SetViewExpanded(true);
			}
		}

		if (CryGetAsyncKeyState(VK_LEFT) & 1)
		{
			if (s_pLastSelectedLinkNode != &ms_rootNode)
			{
				CFlashPlayer* pFlashPlayer(s_pLastSelectedLinkNode->m_pHandle);
				SFlashProfilerData* pFlashProfilerData(pFlashPlayer ? pFlashPlayer->m_pProfilerData : 0);
				if (pFlashProfilerData)
					pFlashProfilerData->SetViewExpanded(false);
			}
		}

		if (CryGetAsyncKeyState(VK_SPACE) & 1)
		{
			if (s_pLastSelectedLinkNode != &ms_rootNode)
			{
				CFlashPlayer* pFlashPlayer(s_pLastSelectedLinkNode->m_pHandle);
				SFlashProfilerData* pFlashProfilerData(pFlashPlayer ? pFlashPlayer->m_pProfilerData : 0);
				if (pFlashProfilerData)
					pFlashProfilerData->TogglePreventFunctionExecution();
			}
		}
	}
	else
	{
		if (pFlashRenderer)
			pFlashRenderer->GetRenderStats(0, true);
	}
}


void CFlashPlayer::SetFlashLoadMovieHandler(IFlashLoadMovieHandler* pHandler)
{
	ms_pLoadMovieHandler = pHandler;
}


IFlashLoadMovieHandler* CFlashPlayer::GetFlashLoadMovieHandler()
{
	return ms_pLoadMovieHandler;
}


unsigned int CFlashPlayer::GetLogOptions()
{
	return ms_sys_flash_log_options;
}


#endif // #ifndef EXCLUDE_SCALEFORM_SDK


// factory for IFlashPlayer interface
IFlashPlayer* CSystem::CreateFlashPlayerInstance() const
{
#ifndef EXCLUDE_SCALEFORM_SDK
	return new CFlashPlayer;
#else
	return 0;
#endif
}


void CSystem::RenderFlashInfo()
{
#ifndef EXCLUDE_SCALEFORM_SDK
	CFlashPlayer::RenderFlashInfo();
#endif
}


void CSystem::GetFlashMemoryUsage(ICrySizer* pSizer)
{
#ifndef EXCLUDE_SCALEFORM_SDK
	const GAllocator* pBlockAllocator(GMemory::GetBlockAllocator());
	if (pBlockAllocator && pBlockAllocator->GetStats())
		pSizer->AddObject(pBlockAllocator, pBlockAllocator->GetStats()->GetSizeAllocated());
#endif
}


void CSystem::SetFlashLoadMovieHandler(IFlashLoadMovieHandler* pHandler) const
{
#ifndef EXCLUDE_SCALEFORM_SDK
	CFlashPlayer::SetFlashLoadMovieHandler(pHandler);
#endif
}