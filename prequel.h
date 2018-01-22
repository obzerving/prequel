/*
 * prequel.h
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#ifndef PREQUEL_H_
#define PREQUEL_H_
#include <unistd.h>
#include "Notification.h"
#include "ValueID.h"
#include <list>
using namespace OpenZWave;
using namespace std;
typedef struct
{
    int typeindex;
    float points[10]; // used by sensors
    float lastreading; // used by sensors and wifi switches
} readInfo;
typedef struct
{
    string nodename; // used by zwRules_ingest
    int nodeId; // used by doAction, q_action, zwRules_ingest
    uint8 bccmap; // command class mapped to Basic command class
    string name; // device name
    string location; // device location
    string restURL; // used by wifi sensors and switches
    bool isFan; // Capability = 128
    bool isLight; // Capability = 64
    bool canDim; // Capability = 32
    bool reportsBattery; // Capability = 16
    bool isLightSensor; // Capability = 8
    bool isTempSensor; // Capability = 4
    bool isHumiditySensor; // Capability = 2
    bool isMotionSensor; // Capability = 1
    float voltage; // used by sensors and devices
    list<readInfo> reading;
    uint8 in_use; // for sensor nodes: number of currently firing rules using sensor
}dvInfo;
	typedef struct
	{
		uint32			m_homeId;
		uint8			m_nodeId;
		bool			m_polled;
		list<ValueID>	m_values;
	}NodeInfo;

void OnNotification(Notification const* _notification, void* _context);
	list<NodeInfo*> g_nodes;
	static pthread_mutex_t g_criticalSection;
	static pthread_cond_t initCond;
	static pthread_mutex_t initMutex;

#endif /* PREQUEL_H_ */


