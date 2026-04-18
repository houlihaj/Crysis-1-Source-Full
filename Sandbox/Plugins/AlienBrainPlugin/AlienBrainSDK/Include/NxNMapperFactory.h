// \addtodoc

#ifndef INC_NXN_MAPPERFACTORY_H
#define INC_NXN_MAPPERFACTORY_H


/*  \class      CNxNMapperFactory NxNMapperFactory.h
 *
 *  \brief      This class is the main entrance point for multiple access of the mapper.
 *
 *  \author     Benedikt Ostermaier
 *
 *  \version    1.00
 *
 *  \date       2003
 *
 *	\mod
 *		[bo] 28-Oct-2003 file created.
 *	\endmod
 */

class NXNINTEGRATORSDK_API CNxNMapperFactory
{
private:
    static CNxNMapper* m_pMapper;
    static int         m_iRefCount;

public:
    static CNxNMapper*      GetMapper();
    static bool             ReleaseMapper();
};

#endif