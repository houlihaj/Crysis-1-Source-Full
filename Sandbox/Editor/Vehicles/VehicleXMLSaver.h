#ifndef __VEHICLEXMLSAVER_H__
#define __VEHICLEXMLSAVER_H__

#pragma once

struct IVehicleData;

XmlNodeRef VehicleDataSave( const char * definitionFile, IVehicleData* pData );
bool VehicleDataSave( const char * definitionFile, const char * dataFile, IVehicleData* pData );


#endif
