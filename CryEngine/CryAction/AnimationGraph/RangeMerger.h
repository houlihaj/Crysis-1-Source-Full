#ifndef __RANGEMERGER_H__
#define __RANGEMERGER_H__

#pragma once

#include <set>

// this class assists in turning ranges of numbers that overlap into ranges of
// numbers that don't overlap... eg [0,5], [3,10] --> [0,2], [3,5], [6,10]
// it also performs some utility tasks on the final list
class CRangeMerger
{
public:
	struct Range
	{
		Range( int a, int b ) : minimum(a), maximum(b) {}
		Range() {}
		int minimum;
		int maximum;
	};

private:
	struct CompareMinimum
	{
		bool operator()( const Range& a, const Range& b ) const
		{
			return a.minimum < b.minimum;
		}
	};

	typedef std::set<Range, CompareMinimum> WorkingSet;

public:
	// add a range to the set (making sure that it gets seperated appropriately
	void AddRange( Range range, bool goBack = true );
	// clip the entire set of ranges to another range (discard lower, higher regions)
	void ClipToRange( Range range );
	// make sure that from min to max any gaps are filled - ie [0,1], [4,5] --> [0,2], [3,5]
	void FillGaps( Range range );

	void AddRange( int minimum, int maximum )
	{
		AddRange( Range(minimum, maximum) );
	}

	typedef WorkingSet::const_iterator const_iterator;
	const_iterator begin() const { return m_working.begin(); }
	const_iterator end() const { return m_working.end(); }
	bool empty() const { return m_working.empty(); }

private:
	WorkingSet m_working;

	void InsertRange( Range range, bool goBack );
	void SolveProblems( WorkingSet::iterator iter );
	bool Verify();

	void Dump(const char * msg, const Range * optRange = 0) const;
};

#endif
