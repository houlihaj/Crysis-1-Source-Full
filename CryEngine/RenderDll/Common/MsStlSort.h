#pragma once

namespace mssort
{
  const int _ISORT_MAX = 32;

  struct input_iterator_tag
  {	// identifying tag for input iterators
  };

  struct forward_iterator_tag
    : public input_iterator_tag
  {	// identifying tag for forward iterators
  };

  struct bidirectional_iterator_tag
    : public forward_iterator_tag
  {	// identifying tag for bidirectional iterators
  }; 

  struct random_access_iterator_tag
    : public bidirectional_iterator_tag
  {	// identifying tag for random-access iterators
  };

  template<class _Ty1,
  class _Ty2> struct pair
  {	// store a pair of values
    typedef pair<_Ty1, _Ty2> _Myt;
    typedef _Ty1 first_type;
    typedef _Ty2 second_type;

    pair()
      : first(_Ty1()), second(_Ty2())
    {	// construct from defaults
    }

    pair(const _Ty1& _Val1, const _Ty2& _Val2)
      : first(_Val1), second(_Val2)
    {	// construct from specified values
    }

    template<class _Other1,
    class _Other2>
      pair(const pair<_Other1, _Other2>& _Right)
      : first(_Right.first), second(_Right.second)
    {	// construct from compatible pair
    }

    void swap(_Myt& _Right)
    {	// exchange contents with _Right
      std::swap(first, _Right.first);
      std::swap(second, _Right.second);
    }

    _Ty1 first;	// the first stored value
    _Ty2 second;	// the second stored value
  };


  template<class _FwdIt1,
  class _FwdIt2> inline
    void iter_swap(_FwdIt1 _Left, _FwdIt2 _Right)
  {	// swap *_Left and *_Right
    std::swap(*_Left, *_Right);
  }

  template<class _RanIt> inline
  void _Med3(_RanIt _First, _RanIt _Mid, _RanIt _Last)
  {	// sort median of three elements to middle
    if (*_Mid < *_First)
      std::iter_swap(_Mid, _First);
    if (*_Last < *_Mid)
      std::iter_swap(_Last, _Mid);
    if (*_Mid < *_First)
      std::iter_swap(_Mid, _First);
  }

  template<class _RanIt> inline
  void _Median(_RanIt _First, _RanIt _Mid, _RanIt _Last)
  {	// sort median element to middle
    if (40 < _Last - _First)
    {	// median of nine
      int _Step = (_Last - _First + 1) / 8;
      _Med3(_First, _First + _Step, _First + 2 * _Step);
      _Med3(_Mid - _Step, _Mid, _Mid + _Step);
      _Med3(_Last - 2 * _Step, _Last - _Step, _Last);
      _Med3(_First + _Step, _Mid, _Last - _Step);
    }
    else
      _Med3(_First, _Mid, _Last);
  }

  template<class _RanIt,
  class _Pr> inline
    void _Med3(_RanIt _First, _RanIt _Mid, _RanIt _Last, _Pr _Pred)
  {	// sort median of three elements to middle
    if (_Pred(*_Mid, *_First))
      std::iter_swap(_Mid, _First);
    if (_Pred(*_Last, *_Mid))
      std::iter_swap(_Last, _Mid);
    if (_Pred(*_Mid, *_First))
      std::iter_swap(_Mid, _First);
  }

  template<class _RanIt,
  class _Pr> inline
    void _Median(_RanIt _First, _RanIt _Mid, _RanIt _Last, _Pr _Pred)
  {	// sort median element to middle
    if (40 < _Last - _First)
    {	// median of nine
      int _Step = (_Last - _First + 1) / 8;
      _Med3(_First, _First + _Step, _First + 2 * _Step, _Pred);
      _Med3(_Mid - _Step, _Mid, _Mid + _Step, _Pred);
      _Med3(_Last - 2 * _Step, _Last - _Step, _Last, _Pred);
      _Med3(_First + _Step, _Mid, _Last - _Step, _Pred);
    }
    else
      _Med3(_First, _Mid, _Last, _Pred);
  }

  template<class _RanIt,
  class _Diff,
  class _Ty> inline
    void _Push_heap(_RanIt _First, _Diff _Hole,
    _Diff _Top, _Ty _Val)
  {	// percolate _Hole to _Top or where _Val belongs, using operator<
    for (_Diff _Idx = (_Hole - 1) / 2;
      _Top < _Hole && *(_First + _Idx) < _Val;
      _Idx = (_Hole - 1) / 2)
    {	// move _Hole up to parent
      *(_First + _Hole) = *(_First + _Idx);
      _Hole = _Idx;
    }

    *(_First + _Hole) = _Val;	// drop _Val into final hole
  }

  template<class _RanIt,
  class _Diff,
  class _Ty> inline
    void _Adjust_heap(_RanIt _First, _Diff _Hole, _Diff _Bottom, _Ty _Val)
  {	// percolate _Hole to _Bottom, then push _Val, using operator<
    _Diff _Top = _Hole;
    _Diff _Idx = 2 * _Hole + 2;

    for (; _Idx < _Bottom; _Idx = 2 * _Idx + 2)
    {	// move _Hole down to larger child
      if (*(_First + _Idx) < *(_First + (_Idx - 1)))
        --_Idx;
      *(_First + _Hole) = *(_First + _Idx), _Hole = _Idx;
    }

    if (_Idx == _Bottom)
    {	// only child at bottom, move _Hole down to it
      *(_First + _Hole) = *(_First + (_Bottom - 1));
      _Hole = _Bottom - 1;
    }
    _Push_heap(_First, _Hole, _Top, _Val);
  }

  template<class _RanIt,
  class _Diff,
  class _Ty> inline
    void _Pop_heap(_RanIt _First, _RanIt _Last, _RanIt _Dest,
    _Ty _Val, _Diff *)
  {	// pop *_First to *_Dest and reheap, using operator<
    *_Dest = *_First;
    _Adjust_heap(_First, _Diff(0), _Diff(_Last - _First), _Val);
  }

  template<class _RanIt,
  class _Diff,
  class _Ty> inline
    void _Make_heap(_RanIt _First, _RanIt _Last, _Diff *, _Ty *)
  {	// make nontrivial [_First, _Last) into a heap, using operator<
    _Diff _Bottom = _Last - _First;

    for (_Diff _Hole = _Bottom / 2; 0 < _Hole; )
    {	// reheap top half, bottom to top
      --_Hole;
      _Adjust_heap(_First, _Hole, _Bottom, _Ty(*(_First + _Hole)));
    }
  }

  template<class _RanIt> inline
  void sort_heap(_RanIt _First, _RanIt _Last)
  {	// order heap by repeatedly popping, using operator<
    for (; 1 < _Last - _First; --_Last)
      std::pop_heap(_First, _Last);
  }

  template<class _RanIt,
  class _Pr> inline
    void sort_heap(_RanIt _First, _RanIt _Last, _Pr _Pred)
  {	// order heap by repeatedly popping, using _Pred
    for (; 1 < _Last - _First; --_Last)
      std::pop_heap(_First, _Last, _Pred);
  }

  template<class _RanIt> inline
  pair<_RanIt, _RanIt> _Unguarded_partition(_RanIt _First, _RanIt _Last)
  {	// partition [_First, _Last), using operator<
    _RanIt _Mid = _First + (_Last - _First) / 2;	// sort median to _Mid
    _Median(_First, _Mid, _Last - 1);
    _RanIt _Pfirst = _Mid;
    _RanIt _Plast = _Pfirst + 1;

    while (_First < _Pfirst
      && !(*(_Pfirst - 1) < *_Pfirst)
      && !(*_Pfirst < *(_Pfirst - 1)))
      --_Pfirst;
    while (_Plast < _Last
      && !(*_Plast < *_Pfirst)
      && !(*_Pfirst < *_Plast))
      ++_Plast;

    _RanIt _Gfirst = _Plast;
    _RanIt _Glast = _Pfirst;

    for (; ; )
    {	// partition
      for (; _Gfirst < _Last; ++_Gfirst)
        if (*_Pfirst < *_Gfirst)
          ;
        else if (*_Gfirst < *_Pfirst)
          break;
        else
          std::iter_swap(_Plast++, _Gfirst);
        for (; _First < _Glast; --_Glast)
          if (*(_Glast - 1) < *_Pfirst)
            ;
          else if (*_Pfirst < *(_Glast - 1))
            break;
          else
            std::iter_swap(--_Pfirst, _Glast - 1);
          if (_Glast == _First && _Gfirst == _Last)
            return (pair<_RanIt, _RanIt>(_Pfirst, _Plast));

          if (_Glast == _First)
          {	// no room at bottom, rotate pivot upward
            if (_Plast != _Gfirst)
              std::iter_swap(_Pfirst, _Plast);
            ++_Plast;
            std::iter_swap(_Pfirst++, _Gfirst++);
          }
          else if (_Gfirst == _Last)
          {	// no room at top, rotate pivot downward
            if (--_Glast != --_Pfirst)
              std::iter_swap(_Glast, _Pfirst);
            std::iter_swap(_Pfirst, --_Plast);
          }
          else
            std::iter_swap(_Gfirst++, --_Glast);
    }
  }

  template<class _RanIt,
  class _Pr> inline
    pair<_RanIt, _RanIt> _Unguarded_partition(_RanIt _First, _RanIt _Last,
    _Pr _Pred)
  {	// partition [_First, _Last), using _Pred
    _RanIt _Mid = _First + (_Last - _First) / 2;
    _Median(_First, _Mid, _Last - 1, _Pred);
    _RanIt _Pfirst = _Mid;
    _RanIt _Plast = _Pfirst + 1;

    while (_First < _Pfirst
      && !_Pred(*(_Pfirst - 1), *_Pfirst)
      && !_Pred(*_Pfirst, *(_Pfirst - 1)))
      --_Pfirst;
    while (_Plast < _Last
      && !_Pred(*_Plast, *_Pfirst)
      && !_Pred(*_Pfirst, *_Plast))
      ++_Plast;

    _RanIt _Gfirst = _Plast;
    _RanIt _Glast = _Pfirst;

    for (; ; )
    {	// partition
      for (; _Gfirst < _Last; ++_Gfirst)
        if (_Pred(*_Pfirst, *_Gfirst))
          ;
        else if (_Pred(*_Gfirst, *_Pfirst))
          break;
        else
          std::iter_swap(_Plast++, _Gfirst);
        for (; _First < _Glast; --_Glast)
          if (_Pred(*(_Glast - 1), *_Pfirst))
            ;
          else if (_Pred(*_Pfirst, *(_Glast - 1)))
            break;
          else
            std::iter_swap(--_Pfirst, _Glast - 1);
          if (_Glast == _First && _Gfirst == _Last)
            return (pair<_RanIt, _RanIt>(_Pfirst, _Plast));

          if (_Glast == _First)
          {	// no room at bottom, rotate pivot upward
            if (_Plast != _Gfirst)
              std::iter_swap(_Pfirst, _Plast);
            ++_Plast;
            std::iter_swap(_Pfirst++, _Gfirst++);
          }
          else if (_Gfirst == _Last)
          {	// no room at top, rotate pivot downward
            if (--_Glast != --_Pfirst)
              std::iter_swap(_Glast, _Pfirst);
            std::iter_swap(_Pfirst, --_Plast);
          }
          else
            std::iter_swap(_Gfirst++, --_Glast);
    }
  }

  template<class _FwdIt> inline
  void _Rotate(_FwdIt _First, _FwdIt _Mid, _FwdIt _Last,
              forward_iterator_tag)
  {	// rotate [_First, _Last), forward iterators
    for (_FwdIt _Next = _Mid; ; )
    {	// swap [_First, ...) into place
      std::iter_swap(_First, _Next);
      if (++_First == _Mid)
        if (++_Next == _Last)
          break;	// done, quit
        else
          _Mid = _Next;	// mark end of next interval
      else if (++_Next == _Last)
        _Next = _Mid;	// wrap to last end
    }
  }

  template<class _BidIt> inline
  void _Rotate(_BidIt _First, _BidIt _Mid, _BidIt _Last,
              bidirectional_iterator_tag)
  {	// rotate [_First, _Last), bidirectional iterators
    std::reverse(_First, _Mid);
    std::reverse(_Mid, _Last);
    std::reverse(_First, _Last);
  }

  template<class _RanIt,
  class _Diff,
  class _Ty> inline
    void _Rotate(_RanIt _First, _RanIt _Mid, _RanIt _Last, _Diff *, _Ty *)
  {	// rotate [_First, _Last), random-access iterators
    _Diff _Shift = _Mid - _First;
    _Diff _Count = _Last - _First;

    for (_Diff _Factor = _Shift; _Factor != 0; )
    {	// find subcycle count as GCD of shift count and length
      _Diff _Tmp = _Count % _Factor;
      _Count = _Factor, _Factor = _Tmp;
    }

    if (_Count < _Last - _First)
      for (; 0 < _Count; --_Count)
      {	// rotate each subcycle
        _RanIt _Hole = _First + _Count;
        _RanIt _Next = _Hole;
        _Ty _Holeval = *_Hole;
        _RanIt _Next1 = _Next + _Shift == _Last ? _First : _Next + _Shift;
        while (_Next1 != _Hole)
        {	// percolate elements back around subcycle
          *_Next = *_Next1;
          _Next = _Next1;
          _Next1 = _Shift < _Last - _Next1 ? _Next1 + _Shift
            : _First + (_Shift - (_Last - _Next1));
        }
        *_Next = _Holeval;
      }
  }

  template<class _RanIt> inline
  void _Rotate(_RanIt _First, _RanIt _Mid, _RanIt _Last,
              random_access_iterator_tag)
  {	// rotate [_First, _Last), random-access iterators
    _Rotate(_First, _Mid, _Last, _Dist_type(_First), _Val_type(_First));
  }

  template<class _FwdIt> inline
  void rotate(_FwdIt _First, _FwdIt _Mid, _FwdIt _Last)
  {	// rotate [_First, _Last)
    if (_First != _Mid && _Mid != _Last)
      _Rotate(_First, _Mid, _Last, _Iter_cat(_First));
  }

  template<class _BidIt> inline
  void _Insertion_sort(_BidIt _First, _BidIt _Last)
  {	// insertion sort [_First, _Last), using operator<
    if (_First != _Last)
      for (_BidIt _Next = _First; ++_Next != _Last; )
        if (*_Next < *_First)
        {	// found new earliest element, rotate to front
          _BidIt _Next1 = _Next;
          std::rotate(_First, _Next, ++_Next1);
        }
        else
        {	// look for insertion point after first
          _BidIt _Dest = _Next;
          for (_BidIt _Dest0 = _Dest; *_Next < *--_Dest0; )
            _Dest = _Dest0;
          if (_Dest != _Next)
          {	// rotate into place
            _BidIt _Next1 = _Next;
            std::rotate(_Dest, _Next, ++_Next1);
          }
        }
  }

  template<class _BidIt,
  class _Pr> inline
    void _Insertion_sort(_BidIt _First, _BidIt _Last, _Pr _Pred)
  {	// insertion sort [_First, _Last), using _Pred
    if (_First != _Last)
      for (_BidIt _Next = _First; ++_Next != _Last; )
        if (_Pred(*_Next, *_First))
        {	// found new earliest element, rotate to front
          _BidIt _Next1 = _Next;
          std::rotate(_First, _Next, ++_Next1);
        }
        else
        {	// look for insertion point after first
          _BidIt _Dest = _Next;
          for (_BidIt _Dest0 = _Dest; _Pred(*_Next, *--_Dest0); )
            _Dest = _Dest0;
          if (_Dest != _Next)
          {	// rotate into place
            _BidIt _Next1 = _Next;
            std::rotate(_Dest, _Next, ++_Next1);
          }
        }
  }

  template<class _RanIt,
  class _Diff> inline
    void _Sort(_RanIt _First, _RanIt _Last, _Diff _Ideal)
  {	// order [_First, _Last), using operator<
    _Diff _Count;
    for (; _ISORT_MAX < (_Count = _Last - _First) && 0 < _Ideal; )
    {	// divide and conquer by quicksort
      pair<_RanIt, _RanIt> _Mid = _Unguarded_partition(_First, _Last);
      _Ideal /= 2, _Ideal += _Ideal / 2;	// allow 1.5 log2(N) divisions

      if (_Mid.first - _First < _Last - _Mid.second)	// loop on larger half
        _Sort(_First, _Mid.first, _Ideal), _First = _Mid.second;
      else
        _Sort(_Mid.second, _Last, _Ideal), _Last = _Mid.first;
    }

    if (_ISORT_MAX < _Count)
    {	// heap sort if too many divisions
      std::make_heap(_First, _Last);
      std::sort_heap(_First, _Last);
    }
    else if (1 < _Count)
      _Insertion_sort(_First, _Last);	// small, insertion sort
  }

  template<class _RanIt,
  class _Diff,
  class _Pr> inline
    void _Sort(_RanIt _First, _RanIt _Last, _Diff _Ideal, _Pr _Pred)
  {	// order [_First, _Last), using _Pred
    _Diff _Count;
    for (; _ISORT_MAX < (_Count = _Last - _First) && 0 < _Ideal; )
    {	// divide and conquer by quicksort
      pair<_RanIt, _RanIt> _Mid =
        _Unguarded_partition(_First, _Last, _Pred);
      _Ideal /= 2, _Ideal += _Ideal / 2;	// allow 1.5 log2(N) divisions

      if (_Mid.first - _First < _Last - _Mid.second)	// loop on larger half
        _Sort(_First, _Mid.first, _Ideal, _Pred), _First = _Mid.second;
      else
        _Sort(_Mid.second, _Last, _Ideal, _Pred), _Last = _Mid.first;
    }

    if (_ISORT_MAX < _Count)
    {	// heap sort if too many divisions
      std::make_heap(_First, _Last, _Pred);
      std::sort_heap(_First, _Last, _Pred);
    }
    else if (1 < _Count)
      _Insertion_sort(_First, _Last, _Pred);	// small, insertion sort
  }

  template<class _RanIt> inline
  void sort(_RanIt _First, _RanIt _Last)
  {	// order [_First, _Last), using operator<
    _Sort(_First, _Last, _Last - _First);
  }

  template<class _RanIt,
  class _Pr> inline
    void sort(_RanIt _First, _RanIt _Last, _Pr _Pred)
  {	// order [_First, _Last), using _Pred
    _Sort(_First, _Last, _Last - _First, _Pred);
  }

}