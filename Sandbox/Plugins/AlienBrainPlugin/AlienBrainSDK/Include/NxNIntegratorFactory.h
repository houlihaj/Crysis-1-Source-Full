// \addtodoc

#ifndef INC_NXN_INTEGRATORFACTORY_H
#define INC_NXN_INTEGRATORFACTORY_H


/*  \class      CNxNIntegratorFactory NxNIntegratorFactory.h
 *
 *  \brief      This class is the main entrance point for multiple access of the SDK.
 *
 *  \author     Thomas Gawlik
 *
 *  \version    1.00
 *
 *  \date       2003
 *
 *	\mod
 *		[tg]-01-April-2003 file created.
 *	\endmod
 */
class NXNINTEGRATORSDK_API CNxNIntegratorFactory
{
public:
    static CNxNIntegrator*  GetIntegrator( bool bInitialize = true );
    static void             ReleaseIntegrator();
};

#endif