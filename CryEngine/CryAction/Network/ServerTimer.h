#ifndef __SERVERTIMER_H__
#define __SERVERTIMER_H__

#pragma once

class CServerTimer : public ITimer
{
public:
	virtual void ResetTimer();	
	virtual void UpdateOnFrameStart();
	virtual float GetCurrTime(ETimer which = ETIMER_GAME) const;
	virtual CTimeValue GetFrameStartTime(ETimer which = ETIMER_GAME) const;
	virtual CTimeValue GetAsyncTime() const;
	virtual float GetAsyncCurTime();
	virtual float GetFrameTime(ETimer which = ETIMER_GAME) const;
	virtual float GetRealFrameTime(const bool ignoreGamePause = true) const;
	virtual float GetTimeScale() const;
	virtual void SetTimeScale(float scale);
	virtual void EnableTimer( const bool bEnable );
	virtual bool IsTimerEnabled() const;
	virtual float	GetFrameRate();
	virtual float GetProfileFrameBlending( float* pfBlendTime = 0, int* piBlendMode = 0 ) { return 1.f; }
	virtual void Serialize( TSerialize ser );
	virtual bool PauseTimer(ETimer which, bool bPause);
	virtual bool IsTimerPaused(ETimer which);
	virtual bool SetTimer(ETimer which, float timeInSeconds);
	virtual void SecondsToDateUTC(time_t time, struct tm& outDateUTC);
	virtual time_t DateToSecondsUTC(struct tm& timePtr);

	static ITimer * Get()
	{
		return &m_this;
	}

private:
	CServerTimer();
	
	CTimeValue m_remoteFrameStartTime;
	float m_frameTime;

	static CServerTimer m_this;
};

#endif
