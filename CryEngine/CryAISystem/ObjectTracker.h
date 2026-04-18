#ifndef OBJECTTRACKER_H
#define OBJECTTRACKER_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "SerializeFwd.h"
#include "AILog.h"
#include <vector>
#include <list>

/// Used to register objects during serialisation, which happens in two stages 
/// (separating out the object creation from linking/populating objects). Each object
/// is represented by its pointer (in the current image), a unique ID, and functions
/// to create/retrieve the object.
class CObjectTracker
{
public:
	/// pointer to string that indicates the class name/type is unset
	static const char* const s_unsetClassName;
	/// pointer to string that indicates the class name/type should not be checked - e.g. when
	/// assigning to a base class pointer when you don't know the full derived type
	static const char* const s_uncheckedClassName;

	typedef unsigned TID;
	/// fns returning an invalid ID will use this
	static const TID s_invalidID;

	/// Let the caller add a record to the tracker. Pass in a pointer to the object and whether 
	/// or not this object needs to be created when read-serialising. Returns
	/// a unique ID. 
	template<typename T>
	TID AddObject(T* pObject, bool needToCreate, TID externalID = 0);

	/// As other AddObject but allows you to explictly pass the type - should be from
	/// typeid(Type).name()
	template<typename T>
	TID AddObject(T* pObject, const char* typeName, bool needToCreate, TID externalID = 0);

	/// Serialises pObject into/out of ser, using name, so long as that object is already in our
	/// database. ownObject indicates if the caller owns this object,
	/// 1. On reading then a lookup is done on name to get the object ID. If ownObject the object
	///    should already be created - this is just updating the database with the pointer. 
	///    If !ownObject then this function sets the value of pObject from the database.
	/// 2. On writing then this converts the pointer to an ID and serialises that.
	/// Returns true if successful
	template <typename T>
	bool SerializeObjectPointer(TSerialize ser, const char* name, T*& pObject, bool ownObject, bool touch = true);

	/// 
	template<typename T>
	bool RestoreEntityObject(T*& pObject, unsigned entityId);

	/// If the caller owns an object it can call this instead of SerializeObjectPointer without having
	/// to make a temporary pointer 
	template <typename T>
	bool SerializeObjectReference(TSerialize ser, const char* name, T& object);

	template <typename T>
	bool WasTouchedDuringSerialization(T*& pObject);

	/// Serialises a container of pointers
#define CONTAINER_POINTER_TRACKER(container_type) \
	template <typename T>\
	bool SerializeObjectPointerContainer(TSerialize ser, const char* containerName, container_type<T*> & container, bool ownObject, bool touch = true)\
	{\
		bool success = true;\
		ser.BeginGroup(containerName);\
		unsigned numItems = container.size();\
		ser.Value("numItems", numItems);\
		if (ser.IsReading())\
		{\
			if (ownObject)\
			{\
				if (numItems != container.size())\
				{\
					AIWarning("container is wrong size = read %d found %d", numItems, container.size());\
					return false;\
				}\
			}\
			else\
			{\
				container.resize(numItems);\
			}\
		}\
		for(typename container_type<T*>::iterator itr(container.begin()); itr!=container.end(); ++itr)\
		{\
			ser.BeginGroup("i");\
			if (!SerializeObjectPointer(ser, "ptr", (*itr), ownObject, touch))\
				success = false;\
			ser.EndGroup();\
		}\
		ser.EndGroup();\
		return success;\
	}\

	CONTAINER_POINTER_TRACKER(std::vector);
	CONTAINER_POINTER_TRACKER(std::list);

	template <typename T>
	bool SerializeObjectPointerSet(TSerialize ser, const char* containerName, VectorSet<T*> & container)
	{
		bool success = true;
		ser.BeginGroup(containerName);
		unsigned numItems = container.size();
		ser.Value("numItems", numItems);
		if (ser.IsReading())
		{
			for(unsigned i = 0; i < numItems; ++i)
			{
				ser.BeginGroup("i");
				T* ptr = 0;
				if (!SerializeObjectPointer(ser, "ptr", ptr, false))
					success = false;
				if (ptr)
					container.insert(ptr);
				ser.EndGroup();
			}
		}
		else
		{
			for(typename VectorSet<T*>::iterator itr(container.begin()); itr!=container.end(); ++itr)
			{
				ser.BeginGroup("i");
				if (!SerializeObjectPointer(ser, "ptr", (*itr), false))
					success = false;
				ser.EndGroup();
			}
		}
		ser.EndGroup();
		return success;
	}

/*	
#define SET_POINTER_TRACKER(container_type) \
	template <typename T>\
	bool SerializeObjectPointerSet(TSerialize ser, const char* containerName, container_type<const T*> & container, bool ownObject)\
	{\
		bool success = true;\
		ser.BeginGroup(containerName);\
		unsigned numItems = container.size();\
		ser.Value("numItems", numItems);\
		if (ser.IsReading())\
		{\
			if (ownObject)\
			{\
				if (numItems != container.size())\
				{\
					AIWarning("container is wrong size = read %d found %d", numItems, container.size());\
					return false;\
				}\
			}\
			else\
			{\
				container.clear();\
			}\
		}\
		for( typename container_type<const T*>::iterator itr(container.begin()); itr!=container.end(); ++itr)\
		{\
			if (!SerializeObjectPointer(ser, "pointer", (*itr), ownObject))\
				success = false;\
		}\
		ser.EndGroup();\
		return success;\
	}

	SET_POINTER_TRACKER(std::set);
*/
	/// Serialises a container of objects
#define CONTAINER_OBJECT_TRACKER(container_type, insert_function) \
	template <typename T>\
	void SerializeObjectContainer(TSerialize ser, const char* containerName, container_type<T>& container)\
	{\
		if(ser.BeginOptionalGroup(containerName, !container.empty()))\
		{\
			if (ser.IsWriting())\
			{ \
				uint32 count = container.size();\
				ser.Value("Size", count);\
				for (typename container_type<T>::iterator iter( container.begin() ); iter != container.end(); ++iter) {\
					ser.BeginGroup("i");\
					(*iter).Serialize(ser, *this);\
					ser.EndGroup();\
				}\
			}\
			else \
			{ \
				container.clear(); \
				uint32 count; \
				ser.Value("Size", count); \
				while (count--) \
				{ \
					T temp; \
					ser.BeginGroup("i");\
					temp.Serialize(ser, *this);\
					ser.EndGroup();\
					container.insert_function(temp); \
				} \
			} \
			ser.EndGroup();\
		}\
	}

	CONTAINER_OBJECT_TRACKER(std::vector, push_back);
	CONTAINER_OBJECT_TRACKER(std::list, push_back);

#define CONTAINER_VALUE_OBJECT_TRACKER(container_type) \
	template <typename T1, typename T2>\
	void SerializeValueObjectContainer(TSerialize ser, const char* containerName, container_type<T1,T2>& container)\
	{\
		if(ser.BeginOptionalGroup(containerName, !container.empty()))\
		{\
			if (ser.IsWriting())\
			{ \
				uint32 count = container.size();\
				ser.Value("Size", count);\
				for (typename container_type<T1,T2>::iterator iter( container.begin() ); iter != container.end(); ++iter)\
				{\
					ser.BeginGroup("i");\
					T1 temp1 = iter->first;\
					ser.Value("key",temp1);\
					iter->second.Serialize(ser, *this);\
					ser.EndGroup();\
				}\
			}\
			else \
			{ \
				container.clear(); \
				uint32 count; \
				ser.Value("Size", count); \
				while (count--) \
				{ \
					ser.BeginGroup("i");\
					T1 temp1; \
					T2 temp2; \
					ser.Value("key",temp1);\
					temp2.Serialize(ser, *this);\
					container.insert(std::make_pair(temp1,temp2)); \
					ser.EndGroup();\
				} \
			} \
			ser.EndGroup();\
		}\
	}

	CONTAINER_VALUE_OBJECT_TRACKER(std::map);
	CONTAINER_VALUE_OBJECT_TRACKER(std::multimap);

	/// Serialises a container of objects
	template <typename T>
	void SerializeValueContainer(TSerialize ser, const char* containerName, std::vector<T>& container)
	{
		ser.Value( containerName,container );
	}
	template <typename T, typename A>
	void SerializeValueContainer(TSerialize ser, const char* containerName, std::list<T,A>& container)
	{
		ser.Value( containerName,container );
	}

	template <typename T1,typename T2>
	void SerializeValueValueContainer(TSerialize ser, const char* containerName, std::map<T1,T2>& container)
	{
		if(ser.BeginOptionalGroup(containerName, !container.empty()))
		{
			ser.Value("map",container);
			ser.EndGroup();
		}
	}

	template <typename T1,typename T2, class A>
	void SerializeValueValueContainer(TSerialize ser, const char* containerName, std::list<std::pair<T1,T2>, A >& container)
	{
		if(ser.BeginOptionalGroup(containerName, !container.empty()))
		{
			ser.Value( "pairs",container );
			ser.EndGroup();
		}
	}

#define CONTAINER_POINTER_VALUE_TRACKER(container_type,insert_function) \
	template <typename T1, typename T2>\
	bool SerializeObjectPointerValueContainer(TSerialize ser, const char* containerName, container_type<T1*,T2> & container, bool ownObject)\
	{\
		bool success = true;\
		ser.BeginGroup(containerName);\
		unsigned numItems = container.size();\
		ser.Value("numItems", numItems);\
		if (ser.IsReading())\
		{\
			if (ownObject)\
			{\
				if (numItems != container.size())\
				{\
					ser.EndGroup();\
					AIWarning("container is wrong size = read %d found %d", numItems, container.size());\
					return false;\
				}\
			}\
			else\
				container.clear();\
			for(int i=0;i<numItems;i++)\
			{\
				ser.BeginGroup("i");\
				T1* pointer;\
				T2 value;\
				if (!SerializeObjectPointer(ser, "ptr", pointer, ownObject))\
					success= false;\
				ser.Value("v",value);\
				container.insert_function(std::make_pair(pointer,value));\
				ser.EndGroup();\
			}\
		}\
		else\
			for(typename container_type<T1*,T2>::iterator itr(container.begin()); itr!=container.end(); ++itr)\
			{\
				ser.BeginGroup("i");\
				T1* pointer=itr->first;\
				if (!SerializeObjectPointer(ser, "ptr", pointer, ownObject))\
					success= false;\
				ser.Value("v",itr->second);\
				ser.EndGroup();\
			}\
		ser.EndGroup();\
		return success;\
	}

	CONTAINER_POINTER_VALUE_TRACKER(std::map,insert);

	/// serialise this database. Note that on loading the pointers will all be 0
	void Serialize(TSerialize ser);

	/// Used during object creation after read-serialising
	struct CObjectCreator
	{
		virtual ~CObjectCreator() {}
		/// Derived class returns a pointer to the new object. Note that the
		/// object tracker still needs to be told that the application code
		/// takes ownership of this object, since generally objects created this
		/// way will be ownerless until they are subsequently stitched up.
		virtual void* CreateObject(TID ID, TID externalID, const char* objectType) const = 0;
	};
	/// For each ID labeled with needToCreate then the object creator is called
	void CreateObjects( const CObjectCreator & objectCreator);

	/// Checks that after read-serialising all objects that were flagged as "needToCreate" have had ownership
	/// taken of them. Returns true if all is OK.
	bool ValidateOwnership() const;

	/// Clears everything. 
	void Clear();

private:
	// For now RetrieveObjectFromID and RetrieveIDFromObject can be internal - not sure they need to
	// be public since you can use SerializeObjectPointer.

	/// Retrieve the object with a particular ID. The type is used for checking. If the object
	/// cannot be found 0 is returned, and a error message displayed.
	/// takingOwnership is used for debugging - the caller should set it to true if it (or something)
	/// is taking ownership of the object being requested - i.e. it says that it will 
	/// delete it. After read-serialising then the calling code can check that all
	/// objects have become owned.
void* RetrieveObjectFromID(TID ID, const char* objectType, bool takingOwnership,bool& found, bool touch=true);

	/// Retrieve the ID of an object
	TID RetrieveIDFromObject(void* pObject, const char* objectType);

	struct CRecord
	{
		CRecord(void* pObject = 0, const char* type = s_unsetClassName, bool needToCreate = false, TID ID = s_invalidID, TID externalID = 0) 
			: m_pObject(pObject), m_ID(ID), m_externalID(externalID), m_objectType(type), m_needToCreate(needToCreate), m_waitingToBeOwned(true), m_touched(false) {}

		bool operator==(void* pObject) {return pObject == m_pObject;}
		bool operator==(TID ID) {return ID == m_ID;}

		/// pointer to the object in the current image
		void* m_pObject;
		/// unique ID, assigned when exporting - stays the same when importing. 0 is an invalid ID.
		TID m_ID;
		/// external ID - used to link objects using the tracker to external objects (e.g. entity id)
		TID m_externalID;
		/// Type of object - used when the object tracker is used to create
		/// instances of the object
		string m_objectType;
		/// Flag to indicate if an instance of this object needs to be created
		/// on the heap. If false the this object is directly owned by another
		/// object, and the owning object will simply register it with us
		bool m_needToCreate;
		/// Flag to indicate if this object still needs to be taken ownership of during read-serialisation
		bool m_waitingToBeOwned;

		
		bool m_touched;
	};
	typedef std::vector<CRecord> TRecords;
	TRecords m_records;
};

//====================================================================
// Inline implementations for templates
//====================================================================

/// can't rely on RTTI even though type name is known at compile time
static const char* GetObjectTrackerTypeName(const struct PathPointDescriptor*) {return "PathPointDescriptor";}
static const char* GetObjectTrackerTypeName(const class CUnitAction*) {return "CUnitAction";}
static const char* GetObjectTrackerTypeName(const class CGraph*) {return "CGraph";}
static const char* GetObjectTrackerTypeName(const class CAStarSolver*) {return "CAStarSolver";}
static const char* GetObjectTrackerTypeName(const class CStandardHeuristic*) {return "CStandardHeuristic";}
static const char* GetObjectTrackerTypeName(const class CNavPath*) {return "CNavPath";}
static const char* GetObjectTrackerTypeName(const class CFormation*) {return "CFormation";}
static const char* GetObjectTrackerTypeName(const class CGoalOp*) {return "CGoalOp";}
static const char* GetObjectTrackerTypeName(const class CTrackDummy*) {return "CTrackDummy";}
static const char* GetObjectTrackerTypeName(const class CHeuristic*) {return "CHeuristic";}
static const char* GetObjectTrackerTypeName(const class CLeader*) {return "CLeader";}
static const char* GetObjectTrackerTypeName(const class CUnitImg*) {return "CUnitImg";}
static const char* GetObjectTrackerTypeName(const class CAIObject*) {return "CAIObject";}
static const char* GetObjectTrackerTypeName(const struct IAIObject*) {return "IAIObject";}
static const char* GetObjectTrackerTypeName(const void*) {return "void";}


//====================================================================
// SerializeObjectPointer
//====================================================================
template <typename T>
inline bool CObjectTracker::RestoreEntityObject(T*& pObject, unsigned entityId)
{
	TRecords::iterator it = m_records.begin();
	for(; it!=m_records.end(); ++it)
		if(it->m_externalID == entityId)
			break;

	if(it==m_records.end())
	{
		AIWarning( "CObjectTracker::RestoreEntityObject Trying to retrieve object for entotyID %d - can't find", entityId);
		return false;
	}

	it->m_pObject = (void*) pObject;
	if (!it->m_waitingToBeOwned && pObject)
		AIWarning("SerializeObjectPointer: object %p already owned", it->m_pObject);
	it->m_waitingToBeOwned = false;
	return true;
}

template <typename T>
inline bool CObjectTracker::SerializeObjectPointer(TSerialize ser, const char* name, T*& pObject, bool ownObject, bool touch)
{
	const char* objectType = GetObjectTrackerTypeName(pObject);
	bool success = true;
	if (ser.IsReading())
	{
		TID ID;
		ser.Value(name, ID);

		if (ownObject)
		{
			if (ID == s_invalidID)
			{
				AIWarning("CObjectTracker::SerializeObjectPointer Trying to retrieve object %s of type %s but it has no ID",
					name, objectType);
				return false;
			}

			TRecords::iterator it = std::find(m_records.begin(), m_records.end(), ID);

			if (it == m_records.end())
			{
				AIWarning("CObjectTracker::SerializeObjectPointer Trying to retrieve object ID %d of type %s but it's not registered",
					ID, objectType);
				return false;
			}
			else
			{
				AIAssert(ID == it->m_ID);
				it->m_pObject = (void*) pObject;
				if (!it->m_waitingToBeOwned && pObject)
					AIWarning("SerializeObjectPointer: object %p already owned", it->m_pObject);
				it->m_waitingToBeOwned = false;
				if (!it->m_touched && touch)
					it->m_touched = true;
				return true;
			}
		}
		else
		{
			if (ID == s_invalidID)
			{
				pObject = 0;
				return true;
			}
			bool found;
			pObject = (T*) RetrieveObjectFromID(ID, objectType, ownObject, found);
			if (!found)
			{
				AIWarning("CObjectTracker::SerializeObjectPointer can't retrieve object pointer of type %s from name %s and object id %d", objectType, name, ID);
				success = false;
			}
		}
	}
	else
	{
		TID ID;
		if (pObject)
		{
			ID = RetrieveIDFromObject((void*) pObject, objectType);
			if (ID == s_invalidID)
			{
				AIWarning("CObjectTracker::SerializeObjectPointer can't retrieve ID from pointer %p for name %s", pObject, name);
				success = false;
			}
		}
		else
		{
			ID = s_invalidID;
		}
		ser.Value(name, ID);
	}
	return success;
}


//====================================================================
// SerializeObjectPointer
//====================================================================
template <typename T>
inline bool CObjectTracker::WasTouchedDuringSerialization(T*& pObject)
{
	const char* objectType = GetObjectTrackerTypeName(pObject);
	bool success = true;

	TRecords::iterator it = std::find(m_records.begin(), m_records.end(), pObject);
	if (it == m_records.end())
	{
		AIWarning("CObjectTracker::WasTouchedDuringSerialization Trying to retrieve object of type %s but it's not registered",
			objectType);
		return false;
	}

	return it->m_touched;
}

//====================================================================
// SerializeObjectReference
//====================================================================
template <typename T>
inline bool CObjectTracker::SerializeObjectReference(TSerialize ser, const char* name, T& object)
{
	T* temp = &object;
	bool result = SerializeObjectPointer(ser, name, temp, true);
	AIAssert(&object == temp);
	return result;
}

/*
//====================================================================
// SerializeObjectPointerVector
//====================================================================
template <typename T>
bool CObjectTracker::SerializeObjectPointerVector(
	TSerialize ser, const char* containerName, std::vector<T*>& container, bool ownObject)
{
	bool success = true;
	ser.BeginGroup(containerName);
	unsigned numItems = container.size();
	ser.Value("numItems", numItems);
	if (ser.IsReading())
	{
		if (ownObject)
		{
			if (numItems != container.size())
			{
				AIWarning("container is wrong size = read %d found %d", numItems, container.size());
				return false;
			}
		}
		else
		{
			container.resize(numItems);
		}
	}
	for (unsigned i = 0 ; i < numItems ; ++i)
	{
		if (!SerializeObjectPointer(ser, "pointer", container[i], ownObject))
			success = false;
	}
	ser.EndGroup();
	return success;
}
*/


//====================================================================
// AddObject
//====================================================================
template<typename T>
CObjectTracker::TID CObjectTracker::AddObject(T* pObject, const char* objectType, bool needToCreate, TID externalID)
{
	TRecords::iterator it = std::find(m_records.begin(), m_records.end(), pObject);
	if (it != m_records.end() && pObject)
	{
		AIWarning("Trying to add object %p of type %s to object tracker but it's already registered",
			pObject, objectType);
		return s_invalidID;
	}
	else
	{
		m_records.push_back(CRecord(pObject, objectType, needToCreate, m_records.size()+1, externalID));
	}
	return m_records.rbegin()->m_ID;
}

//====================================================================
// AddObject
//====================================================================
template<typename T>
CObjectTracker::TID CObjectTracker::AddObject(T* pObject, bool needToCreate, TID externalID)
{
	const char* objectType = GetObjectTrackerTypeName(pObject);
	return AddObject(pObject, objectType, needToCreate, externalID);
}


#endif
