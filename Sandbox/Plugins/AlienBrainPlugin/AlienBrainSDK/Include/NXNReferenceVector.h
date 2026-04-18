// \addtodoc
#ifndef _NXNREFERENCEVECTOR_H_
#define _NXNREFERENCEVECTOR_H_

/*! \file NXNReferenceVector.h
*  NXNReferenceVector.h contains declaration of class CNXNReferenceVector.
*/

#include "NXNIntegrationTypes.h"

#ifdef NXNINTEGRATIONSTOOLS_EXPORTS
#define NXNREFERENCEVECTOR_EXPORTS __declspec(dllexport)
#else
#define NXNREFERENCEVECTOR_EXPORTS __declspec(dllimport)
#endif  // NXNINTEGRATIONSTOOLS_EXPORTS

//
// error code constants
//
const unsigned long ReferenceVectorNoError                      = 0L;
const unsigned long ReferenceVectorInvalidReferenceTypeError    = 1L;
const unsigned long ReferenceVectorInvalidFilePathError         = 2L;
const unsigned long ReferenceVectorInvalidIndexError            = 3L;

class CNXNReferenceVectorImpl;

/*!	\class		CNXNReferenceVector NXNReferenceVector.h	
 *	\brief		This class is a container for file references.
 *				
 *				CNXNReferenceVector is used in method CNXNIntegrationObserver::GetReferenceVector() for
 *              returning a vector of file references.<p><br>
 *	
 *	\author		Robert Meisenecker, (c) 2003 by NxN Software AG
 *	\version	1.00
 *	\date		2003
*/
class NXNREFERENCEVECTOR_EXPORTS CNXNReferenceVector : public CNxNObject
{
	public:
	
		CNXNReferenceVector();
        CNXNReferenceVector( const CNXNReferenceVector& src );        
		~CNXNReferenceVector();

        CNXNReferenceVector& operator=( const CNXNReferenceVector& rhs );

        // mutator methods
        bool Add( const CNxNPath& filePath, WIF::NXNReferenceType referenceType );        
        void RemoveAll( void );

        // accessor methods
        bool    IsEmpty( void ) const;
        bool    GetAt( unsigned int index, CNxNPath& filePath, WIF::NXNReferenceType& referenceType ) const;
        int     GetCount( void ) const;        

        unsigned long   GetLastError( void ) const;
        CNxNString      GetLastErrorMessage( void ) const;        
		
	protected:
		
	private:

        // pointer to implementation class
        CNXNReferenceVectorImpl* m_pImp;		
};


#endif // _NXNREFERENCEVECTOR_H_

