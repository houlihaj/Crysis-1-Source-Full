/*=============================================================================
	MultiDimMap.h :
	Copyright 2004 Crytek Studios. All Rights Reserved.

	Revision history:
		* Created by Nick Kasyan

=============================================================================*/
#ifndef __MULTIDIMMAP_H__
#define __MULTIDIMMAP_H__

#if _MSC_VER > 1000
# pragma once
#endif

//dep
#include <cry_math.h>
#include <cry_geo.h>

#include "Ray.h"
#include "Photon.h"

#include <algorithm>


template <class T>
void TRefSwap (T& item1,T& item2)
{

	T	tmp	=	item1;
	item1		=	item2;
	item2		=	tmp;
}


//===================================================================================
// Template				:	CMultiDimMap
template <class T> class	CMultiDimMap
{
public:

	//===================================================================================
	// Method			:	CMultiDimMap 
	// Description:	constructor
	CMultiDimMap()
	{
			m_numPhotons	=	0;
			m_MaxPhotons	=	0;
			m_pPhotons		=	NULL;
			m_nStepSize	=	10000;
			m_BBox.Reset();

	}

	//===================================================================================
	// Method			:	CMultiDimMap 
	// Description:	destructor
	virtual	~CMultiDimMap()
	{
			if (m_pPhotons != NULL)	delete [] m_pPhotons;
	}


	//===================================================================================
	//fix change file format
	// Method				:	Read
	// Description			:	Read the photonmap from a file
	void	Read(FILE *in)
	{
		fread(&m_NnumPhotons,1,sizeof(int),in);
		fread(&m_MaxPhotons,1,sizeof(int),in);
		photons		=	new T[m_MaxPhotons+1];
		fread(m_pPhotons,m_NumPhotons+1,sizeof(T),in);
		fread(BBox,sizeof(AABB),1,in);

		//fix
		numPhotonsh			=	numPhotons >> 1;
	}

	//===================================================================================
	//fix change file format
	// Method				:	Write
	// Description			:	Write the photon map into a file
	void	Write(FILE *out)
	{
		fwrite(&m_NumPhotons,1,sizeof(int),out);
		fwrite(&m_MaxPhotons,1,sizeof(int),out);
		fwrite(m_pPhotons,m_NumPhotons+1,sizeof(T),out);
		fwrite(BBox,sizeof(AABB),1,out);
	}

	//===================================================================================
	// Method					:	reset
	// Description		:	Reset the photonmap
	void	Reset() 
	{
		if (m_pPhotons != NULL)	delete m_pPhotons;

		m_numPhotons	=	0;
		m_MaxPhotons	=	0;
		m_pPhotons		=	NULL;
		m_BBox.Reset();
	}

	//===================================================================================
	// Method				:	store
	// Description			:	Store a photon
	T* Store(const Vec3& P,const Vec3& N) 
	{
		if (m_numPhotons < m_MaxPhotons) 
		{
			T	*pPhoton	=	&m_pPhotons[++m_numPhotons];

			pPhoton->P = P;
			pPhoton->N = N;
			pPhoton->flags	=	0;

			m_BBox.Add( P );

			return	pPhoton;

		} else {

			//fix:: rewrite memory management
			T	*pNewPhotons;

			m_MaxPhotons	+=	m_nStepSize;
			m_nStepSize	*=	1.5; //fix to 2.0
			pNewPhotons	=	new T[m_MaxPhotons+1];
			if (m_numPhotons > 0)		{
				memcpy(pNewPhotons,m_pPhotons,(m_numPhotons+1)*sizeof(T));
				delete [] m_pPhotons;
			}
			m_pPhotons		=	pNewPhotons;

			T	*pPhoton	=	&m_pPhotons[++m_numPhotons];

			pPhoton->P = P;
			pPhoton->N = N;
			pPhoton->flags	=	0;

			m_BBox.Add( P );

			return	pPhoton;
		}
	}


	//===================================================================================
	// Method				:	store
	// Description			:	Store a photon item
	/*T* Store(const T& item)
	{
		if (m_numPhotons < m_MaxPhotons) 
		{
			T* pPhoton	=	&photons[++m_numPhotons];

			*pPhoton	=	item;
			Bbox.Add(item.P);

			return	pPhoton;
		} else {
			T	*pNewPhotons;

			m_MaxPhotons	+=	m_nStepSize;
			m_nStepSize	*=	2;
			pNewPhotons	=	new T[m_MaxPhotons+1];
			if (m_numPhotons > 0)		
			{
				memcpy(pNewPhotons,photons,(numPhotons+1)*sizeof(T));
				delete [] m_pPhotons;
			}

			pNewPhotons	=	newPhotons;

			T* pPhoton	=	&photons[++numPhotons];

			*pPhoton	=	item;
			Bbox.Add(item.P);

			return	pPhoton;
		}
	}*/

	//===================================================================================
	// Method				:	balance
	// Description			:	Balance the map
	virtual	void	Balance()
	{
		if (m_numPhotons == 0)	return;

		T	**ar1	=	new T*[m_numPhotons+1];
		T	**ar2	=	new T*[m_numPhotons+1];
		T	*pFinalPhotons;

		for (int i=0;i<=m_numPhotons;i++) 
		{
			ar2[i]	=	&m_pPhotons[i];
		}

		Balance(ar1,ar2,1,1,m_numPhotons);

		delete [] ar2;

		pFinalPhotons		=	new T[m_numPhotons+1];
		for (int i=1;i<=m_numPhotons;i++) {
			pFinalPhotons[i]	=	ar1[i][0];
		}

		delete [] ar1;
		delete [] m_pPhotons;
		m_MaxPhotons			=	m_numPhotons;
		m_pPhotons				=	pFinalPhotons;
		m_numPhotonsh			=	m_numPhotons >> 1;
	}


protected:

	class	CLookup 
	{
		public:
			int			nMaxFound;
			int			nNumFound;
			bool		bGotHeap;
			Vec3	P,N;
			float* pfDistances;
			const T	**indices;
	};

	//===================================================================================
	// Method				:	balance
	// Return Value			:	Internally used by balance
	void	Balance(T **ar1, T **ar2, int index, int start, int end) 
	{
		int	median	=	1;

		while((4*median) <= (end-start+1))	median	+=	median;

		if ((3*median) <= (end-start+1)) 
		{
			median	+=	median;
			median	+=	start-1;
		} else
		median	=	end-median+1;

		int	axis	=	2;
		if ( ((m_BBox.max.x - m_BBox.min.x) > (m_BBox.max.y - m_BBox.min.y))  
			&& ((m_BBox.max.x - m_BBox.min.x) > (m_BBox.max.z - m_BBox.min.z)) )
			axis	=	0;
		else if ((m_BBox.max.y - m_BBox.min.y) > (m_BBox.max.z - m_BBox.min.z))
			axis	=	1;

		int	left	=	start;
		int	right	=	end;

		while(right > left) 
		{
			const	float	v	=	ar2[right]->P[axis];
			int				i	=	left-1;
			int				j	=	right;

			for(;;) 
			{
				while(ar2[++i]->P[axis] < v);
				while((ar2[--j]->P[axis] > v) && (j>left));

				if (i >= j)	break;

				TRefSwap(ar2[i],ar2[j]);
			}

			TRefSwap(ar2[i],ar2[right]);

			if (i >= median)	right	=	i-1;
			if (i <= median)	left	=	i+1;
		}

#ifdef _DEBUG //fix
		int	i;
		for (i=start;i<median;i++) 
		{
			assert(ar2[i]->P[axis] <= ar2[median]->P[axis]);
		}

		for (i=median+1;i<=end;i++) 
		{
			assert(ar2[i]->P[axis] >= ar2[median]->P[axis]);
		}
#endif

		ar1[index]			=	ar2[median];
		ar1[index]->flags	=	axis + ( ar1[index]->flags & ~0xf );

		if (median > start) 
		{
			if (start < (median-1)) 
			{
				const	float	tmp	=	m_BBox.max[axis];
				m_BBox.max[axis]	=	ar1[index]->P[axis];
				Balance(ar1,ar2,2*index,start,median-1);
				m_BBox.max[axis]	=	tmp;
			} else 
			{
				ar1[2*index]		=	ar2[start];
			}
		}

		if (median < end) 
		{
			if ((median+1) < end) 
			{
				const	float	tmp	=	m_BBox.min[axis];
				m_BBox.min[axis]			=	ar1[index]->P[axis];
				Balance(ar1,ar2,2*index+1,median+1,end);
				m_BBox.min[axis]			=	tmp;
			} else 
			{
				ar1[2*index+1]		=	ar2[end];
			}
		}
	}

public:


	//===================================================================================
	// Method				:	lookup
	// Description			:	Locate the nearest maxFoundPhoton photons
	void	LookupWithN(CLookup *l,int index)
	{
		const T		*pPhoton	=	&m_pPhotons[index];
		float		d,t;
		Vec3		D;
		int			axis	=	pPhoton->flags & 0xf;

		if (index < m_numPhotonsh) 
		{
			d	=	l->P[axis] - pPhoton->P[axis];

			if (d > 0) 
			{
				LookupWithN(l,2*index+1);

				if (d*d < l->pfDistances[0]) 
				{
						LookupWithN(l,2*index);
				}

			} else 
			{
				LookupWithN(l,2*index);

				if (d*d < l->pfDistances[0]) 
				{
					LookupWithN(l,2*index+1);
				}
			}
		}

		D = (pPhoton->P) - (l->P);
		d	=	D.Dot(D);
		t	=	fabs_tpl( D.Dot( l->N ) );
		d	-=	t*t*16;

		if (d < l->pfDistances[0]) 
		{

			if (l->nNumFound < l->nMaxFound) 
			{
				l->nNumFound++;
				l->pfDistances[l->nNumFound]	=	d;
				l->indices[l->nNumFound]		=	pPhoton;
			} else 
			{
				int	j,parent;

				if (l->bGotHeap == false) 
				{
					int		halfPhotons	=	l->nNumFound >> 1;
					int		k;
					float	dtmp;
					const T	*ptmp;

					for (k=halfPhotons;k>=1;k--) 
					{
						parent	=	k;
						ptmp	=	l->indices[k];
						dtmp	=	l->pfDistances[k];

						while(parent <= halfPhotons) 
						{
							j	=	parent + parent;
							if ((j < l->nNumFound) && (l->pfDistances[j] < l->pfDistances[j+1])) j++;
							if (dtmp >= l->pfDistances[j])	break;

							l->pfDistances[parent]	=	l->pfDistances[j];
							l->indices[parent]		=	l->indices[j];
							parent					=	j;
						}

						l->pfDistances[parent]		=	dtmp;
						l->indices[parent]			=	ptmp;
					}

					l->bGotHeap	=	true;

					//The new photon is too far - Tamas
					if( d > l->pfDistances[1] )
					{
						l->pfDistances[0] = l->pfDistances[1];
						return;
					}
				}

				for (parent=1,j=2;j<=l->nNumFound;) 
				{
					if ((j < l->nNumFound) && (l->pfDistances[j] < l->pfDistances[j+1]))
								j++;

					if (d > l->pfDistances[j]) break;

					l->pfDistances[parent]	=	l->pfDistances[j];
					l->indices[parent]		=	l->indices[j];
					parent	=	j;
					j		+=	j;
				}

				l->pfDistances[parent]	=	d;
				l->indices[parent]		=	pPhoton;
				assert(l->pfDistances[1] <= l->pfDistances[0]);
				l->pfDistances[0]			=	l->pfDistances[1];
			}
		}
	}

	//===================================================================================
	// Method				:	lookup
	// Description			:	Locate the nearest maxFoundPhoton photons
	void	Lookup(CLookup *l,int index) 
	{
		const T		*pPhoton	=	&m_pPhotons[index];
		float		d;
		Vec3		D;
		int			axis	=	pPhoton->flags & 0xf;

		if (index < m_numPhotonsh) 
		{
			d	=	l->P[axis] - pPhoton->P[axis];

			if (d > 0) 
			{
				Lookup(l,2*index+1);

				if (d*d < l->pfDistances[0])
				{
					Lookup(l,2*index);
				}

			} else 
			{
				Lookup(l,2*index);

				if (d*d < l->pfDistances[0]) 
				{
					Lookup(l,2*index+1);
				}
			}
		}

		D = (pPhoton->P) - (l->P);
		d	=	D.Dot(D);

		if (d < l->pfDistances[0]) 
		{

			if (l->nNumFound < l->nMaxFound) 
			{
				l->nNumFound++;
				l->pfDistances[l->nNumFound]	=	d;
				l->indices[l->nNumFound]		=	pPhoton;
			} else 
			{
				int	j,parent;

				if (l->bGotHeap == false) 
				{
					int		halfPhotons	=	l->nNumFound >> 1;
					int		k;
					float	dtmp;
					const T	*ptmp;

					for (k=halfPhotons;k>=1;k--) 
					{
						parent	=	k;
						ptmp	=	l->indices[k];
						dtmp	=	l->pfDistances[k];

						while(parent <= halfPhotons) 
						{
							j	=	parent + parent;
							if ((j < l->nNumFound) && (l->pfDistances[j] < l->pfDistances[j+1])) j++;
							if (dtmp >= l->pfDistances[j])	break;

							l->pfDistances[parent]	=	l->pfDistances[j];
							l->indices[parent]		=	l->indices[j];
							parent					=	j;
						}

						l->pfDistances[parent]		=	dtmp;
						l->indices[parent]			=	ptmp;
					}

					l->bGotHeap	=	true;

					//The new photon is too far - Tamas
					if( d > l->pfDistances[1] )
					{
						l->pfDistances[0] = l->pfDistances[1];
						return;
					}
				}

				for (parent=1,j=2;j<=l->nNumFound;) 
				{
					if ((j < l->nNumFound) && (l->pfDistances[j] < l->pfDistances[j+1]))
								j++;

					if (d > l->pfDistances[j]) break;

					l->pfDistances[parent]	=	l->pfDistances[j];
					l->indices[parent]		=	l->indices[j];
					parent	=	j;
					j		+=	j;
				}

				l->pfDistances[parent]	=	d;
				l->indices[parent]		=	pPhoton;
				assert(l->pfDistances[1] <= l->pfDistances[0]);
				l->pfDistances[0]			=	l->pfDistances[1];
			}
		}
	}

//members declaration


//fix to protected
public:

		AABB	m_BBox;
		T*		m_pPhotons;
		int64		m_numPhotons;
		int64		m_MaxPhotons;
		int64		m_numPhotonsh;
		int32		m_nStepSize;

};

#endif
