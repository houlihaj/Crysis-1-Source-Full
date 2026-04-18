#ifndef _RLSINGLETON_H__
#define _RLSINGLETON_H__

//===================================================================================
// Template				:	RLSingleton
// Description		:	Template for creating single-instance global classes
//===================================================================================
template <typename T> class RLSingleton
{
	public:
		RLSingleton()
		{
			assert( !ms_pSingleton );
			ms_pSingleton = static_cast< T* >( this );
		}

		~RLSingleton()
		{  
			assert( ms_pSingleton );
			ms_pSingleton = NULL;  
		}

		static T& GetInstance()
		{	
			assert( ms_pSingleton );  
			return ( *ms_pSingleton ); 
		}

	protected:
		static T* ms_pSingleton; //static instance

};

#endif
