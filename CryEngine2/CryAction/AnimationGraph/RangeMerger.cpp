#include "StdAfx.h"
#include "RangeMerger.h"

ILINE void CRangeMerger::Dump(const char * msg, const Range * pRange) const
{
/*
	CryLogAlways("***********************************************");
	if (pRange)
		CryLogAlways("Range merger : %s(%d -> %d)", msg, pRange->minimum, pRange->maximum);
	else
		CryLogAlways("Range merger : %s", msg);
	for (WorkingSet::const_iterator iter = m_working.begin(); iter != m_working.end(); ++iter)
		CryLogAlways("  %d -> %d", iter->minimum, iter->maximum);
	CryLogAlways("***********************************************");
*/
}

void CRangeMerger::AddRange( Range range, bool goBack )
{
	Dump("Enter_AddRange", &range);

	WorkingSet::iterator lbound = m_working.lower_bound(range);
	if (lbound == m_working.end()) // possibly safe to insert
	{
		InsertRange(range, goBack);
	}
	else if (lbound->minimum == range.minimum)
	{
		// overlap start
		if (lbound->maximum == range.maximum)
			; // duplicate
		else
		{
			Range insertRange( min(lbound->maximum, range.maximum)+1, max(lbound->maximum, range.maximum) );
			const_cast<Range&>(*lbound).maximum = min(lbound->maximum, range.maximum);
			InsertRange(insertRange, goBack);
		}
	}
	else
	{
		InsertRange(range, goBack);
	}

	Dump("Exit_AddRange");

	assert(!goBack || Verify());
}

void CRangeMerger::InsertRange( Range range, bool goBack )
{
	Dump("Enter_InsertRange", &range);

	WorkingSet::iterator iter = m_working.insert(range).first;
	SolveProblems(iter);
	if (goBack && iter != m_working.begin())
	{
		--iter;
		SolveProblems(iter);
	}

	Dump("Exit_InsertRange");
}

void CRangeMerger::SolveProblems( WorkingSet::iterator iter )
{
	Dump("Enter_SolveProblems", &*iter);

	WorkingSet::iterator next = iter;
	++next;
	if (next == m_working.end())
		return;
	if (next->minimum > iter->maximum)
		return;
	if (next->maximum < iter->maximum)
	{
		Range insertRange( next->maximum+1, iter->maximum );
		const_cast<Range&>(*iter).maximum = next->minimum-1;
		AddRange(insertRange, false);
	}
	else if (next->maximum == iter->maximum)
	{
		const_cast<Range&>(*iter).maximum = next->minimum-1;
	}
	else if (next->maximum > iter->maximum)
	{
		Range insertRange( iter->maximum+1, next->maximum );
		const_cast<Range&>(*iter).maximum = next->minimum-1;
		const_cast<Range&>(*next).maximum = insertRange.minimum-1;
		AddRange(insertRange, false);
	}

	Dump("Exit_SolveProblems", &*iter);
}

bool CRangeMerger::Verify()
{
	if (m_working.empty())
		return true;

	WorkingSet::iterator iter = m_working.begin();
	Range current = *iter;
	if (current.maximum < current.minimum)
		return false;
	++iter;
	while (iter != m_working.end())
	{
		if (current.maximum >= iter->minimum)
			return false;
		current = *iter;
		if (current.maximum < current.minimum)
			return false;
    
		++iter;
	}
	return true;
}

void CRangeMerger::ClipToRange( Range range )
{
	Dump("Enter_ClipToRange", &range);

	std::vector<WorkingSet::iterator> discardIters;
	std::vector<Range> insertRanges;
	for (WorkingSet::iterator iter = m_working.begin(); iter != m_working.end(); ++iter)
	{
		if (iter->maximum < range.minimum)
			discardIters.push_back(iter);
		else if (iter->minimum < range.minimum && iter->maximum >= range.minimum)
		{
			discardIters.push_back(iter);
			Range insertRange( range.minimum, iter->maximum );
			insertRanges.push_back(insertRange);
		}
		else if (iter->minimum <= range.maximum && iter->maximum > range.maximum)
		{
			discardIters.push_back(iter);
			Range insertRange( iter->minimum, range.maximum );
			insertRanges.push_back(insertRange);
		}
		else if (iter->minimum > range.maximum)
			discardIters.push_back(iter);
	}
	while (!discardIters.empty())
	{
		m_working.erase(discardIters.back());
		discardIters.pop_back();
	}
	while (!insertRanges.empty())
	{
		m_working.insert(insertRanges.back());
		insertRanges.pop_back();
	}
	assert(Verify());

	Dump("Exit_ClipToRange");
}

void CRangeMerger::FillGaps( Range range )
{
	if (m_working.empty())
		return;

	Dump("Enter_FillGaps");

	WorkingSet::iterator iter = m_working.begin();
	for (; iter != m_working.end(); ++iter)
	{
		WorkingSet::iterator next = iter; 
		++next;
		if (next == m_working.end())
			continue;

		if (next->minimum != iter->maximum+1)
		{
			int midpoint = (next->minimum + iter->maximum) / 2;
			const_cast<Range&>(*iter).maximum = midpoint;
			Range newNext = *next;
			newNext.minimum = midpoint+1;
			m_working.erase(next);
			m_working.insert(newNext);
		}
	}
	assert(Verify());

	Dump("Exit_FillGaps");
}
