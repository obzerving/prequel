/*
 * prequel.cpp
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#include <time.h>
#include <sys/types.h>
#include <iostream>
#include <sys/stat.h>
#undef UNICODE
#include "Options.h"
#include "Manager.h"
#include "Node.h"
#include "Group.h"
#include "ValueStore.h"
#include "Value.h"
#include "ValueBool.h"
#include "Log.h"
#include "CommandClass.h"
#include "CommandClasses.h"
#include "Basic.h"
#include "prequel.h"
using namespace OpenZWave;
using namespace std;

#define zwDebug 0
uint32 g_homeId = 0;
bool g_nodesQueried = false;

list<dvInfo> dvl;
FILE *zl; // Log file
char s[24];
uint8 level;
bool sw;

NodeInfo* GetNodeInfo(Notification const* _notification)
{
	uint32 const homeId = _notification->GetHomeId();
	uint8 const nodeId = _notification->GetNodeId();
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
	{
		NodeInfo* nodeInfo = *it;
		if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
		{
			return nodeInfo;
		}
	}
	return NULL;
};

void OnNotification(Notification const* _notification, void* _context)
{
	// Must do this inside a critical section to avoid conflicts with the main thread
	pthread_mutex_lock( &(g_criticalSection) );
	switch( _notification->GetType() )
	{
		case Notification::Type_AllNodesQueried:
		{
			if(zwDebug) fprintf(zl, "Notificaton:  AllNodesQueried\n" );
			g_nodesQueried = true;
			pthread_cond_broadcast(&(initCond));
			break;
		}
		case Notification::Type_AllNodesQueriedSomeDead:
		{
			if(zwDebug) fprintf(zl, "Notificaton:  AllNodesQueriedSomeDead\n" );
			g_nodesQueried = true;
			pthread_cond_broadcast(&(initCond));
			break;
		}
		case Notification::Type_AwakeNodesQueried:
		{
			if(zwDebug) fprintf(zl, "Notificaton:  AwakeNodesQueried\n" );
			g_nodesQueried = true;
			pthread_cond_broadcast(&(initCond));
			break;
		}
		case Notification::Type_DriverReady:
		{
			if(zwDebug) fprintf(zl, "Notification:  DriverReady\n" );
			g_homeId = _notification->GetHomeId();
			break;
		}
		case Notification::Type_DriverReset:
		{
			if(zwDebug) fprintf(zl, "Notification:  DriverReset\n" );
			break;
		}
		case Notification::Type_DriverFailed:
		{
			if(zwDebug) fprintf(zl, "Notification:  DriverFailed\n" );
			pthread_cond_broadcast(&(initCond));
			break;
		}
		case Notification::Type_Group:
		{
			if(zwDebug) fprintf(zl, "Notification:  Group\n" );
			break;
		}
		case Notification::Type_NodeAdded:
		{
			if(zwDebug)
			{
				cout << "Added Node " << (int) _notification->GetNodeId() << endl;
				fprintf(zl, "Notification:  NodeAdded\n" );
			};
			// Add the new node to our list
			NodeInfo* nodeInfo = new NodeInfo();
			nodeInfo->m_homeId = _notification->GetHomeId();
			nodeInfo->m_nodeId = _notification->GetNodeId();
			nodeInfo->m_polled = false;
			g_nodes.push_back( nodeInfo );
			break;
		}
		case Notification::Type_NodeEvent:
		{
			if(zwDebug) fprintf(zl, "Notification:  Event\n" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				if(zwDebug) cout << "Received Event from Node " << (int) nodeInfo->m_nodeId << endl;
				// We have received an event from the node, caused by a
				// basic_set or hail message.
				// TBD...
			}
			break;
		}
		case Notification::Type_NodeNaming:
		{
			if(zwDebug) fprintf(zl, "Notification:  NodeNaming\n" );
			break;
		}
		case Notification::Type_NodeProtocolInfo:
		{
			if(zwDebug) fprintf(zl, "Notification:  NodeProtocolInfo\n" );
			break;
		}
		case Notification::Type_NodeQueriesComplete:
		{
			if(zwDebug) fprintf(zl, "Notification:  NodeQueriesComplete\n" );
			break;
		}
		case Notification::Type_NodeRemoved:
		{
			if(zwDebug) fprintf(zl, "Notification:  NodeRemoved\n" );
			// Remove the node from our list
			uint32 const homeId = _notification->GetHomeId();
			uint8 const nodeId = _notification->GetNodeId();
			for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
			{
				NodeInfo* nodeInfo = *it;
				if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
				{
					if(zwDebug) cout << "Removed Node " << (int) nodeId << endl;
					g_nodes.erase( it );
					break;
				}
			}
			break;
		}
		case Notification::Type_PollingDisabled:
		{
			if(zwDebug) fprintf(zl, "Notification:  PollingDisabled\n" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo->m_polled = false;
			}
			break;
		}
		case Notification::Type_PollingEnabled:
		{
			if(zwDebug) fprintf(zl, "Notification:  PollingEnabled\n" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo->m_polled = true;
			}
			break;
		}
		case Notification::Type_ValueAdded:
		{
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				ValueID valueid = _notification->GetValueID();
				if(zwDebug)
				{
					fprintf(zl, "\nValue %016llX added.", valueid.GetId() );
					fprintf(zl, "\nHome ID: 0x%.8x  Node ID: %d,  Polled: %s", nodeInfo->m_homeId, nodeInfo->m_nodeId, nodeInfo->m_polled?"true":"false" );
					fprintf(zl, "\nValue is: \n part of command class: 0x%.2x, of genre: %d, with index %d, instance %d, and type %d",
						valueid.GetCommandClassId(), valueid.GetGenre(), valueid.GetIndex(), valueid.GetInstance(), valueid.GetType());
			                fprintf(zl, "\nValue label is %s",(Manager::Get()->GetValueLabel(valueid)).c_str());
                    			if( ((valueid.GetCommandClassId() == (uint8) 37) && (!strcmp((Manager::Get()->GetValueLabel(valueid)).c_str(),"Switch")))  ||
                                    ((valueid.GetCommandClassId() == (uint8) 38) && (!strcmp((Manager::Get()->GetValueLabel(valueid)).c_str(),"Level"))) ){
                                    printf("Command Class Name: %s\n", CommandClasses::GetName(valueid.GetCommandClassId()).c_str());
                        			dvInfo dvi;
                        			dvi.nodeId = nodeInfo->m_homeId;
                        			dvl.push_back(dvi);
                        			printf("\nAdded to list");
                    			}

				};
				// Add the new value to our list
				nodeInfo->m_values.push_back( _notification->GetValueID() );
			}
			break;
		}
		case Notification::Type_ValueChanged:
		{
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				ValueID valueid = _notification->GetValueID();
				if(zwDebug)
				{
					fprintf(zl, "\nValue %016llX changed.", valueid.GetId() );
					fprintf(zl, "\nHome ID: 0x%.8x  Node ID: %d,  Polled: %s", nodeInfo->m_homeId, nodeInfo->m_nodeId, nodeInfo->m_polled?"true":"false" );
					fprintf(zl, "\nValue is: \n part of command class: 0x%.2x, of genre: %d, with index %d, and instance %d",
						valueid.GetCommandClassId(), valueid.GetGenre(), valueid.GetIndex(), valueid.GetInstance());
				};
			}
			break;
		}
		case Notification::Type_ValueRemoved:
		{
			if(zwDebug) fprintf(zl, "\nNotification:  ValueRemoved" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				// Remove the value from out list
				for( list<ValueID>::iterator it = nodeInfo->m_values.begin(); it != nodeInfo->m_values.end(); ++it )
				{
					if( (*it) == _notification->GetValueID() )
					{
						nodeInfo->m_values.erase( it );
						break;
					}
				}
			}
			break;
		}
		case Notification::Type_ValueRefreshed:
		case Notification::Type_NodeNew:
		case Notification::Type_CreateButton:
		case Notification::Type_DeleteButton:
		case Notification::Type_ButtonOn:
		case Notification::Type_ButtonOff:
		case Notification::Type_EssentialNodeQueriesComplete:
		default:
			break;
	}
	pthread_mutex_unlock( &(g_criticalSection) );
};

int main(int argc, char* argv[])
{
	//struct stat finfo;
	//time_t current_time;
	zl = fopen("zwLog.txt","w");
	if(zl == NULL)
	{
		fprintf(stderr, "Could not open log file\n");
		return(-911);
	}
	if(ferror(zl)) {
		fprintf(stderr, "Error in fprintf\n");
		return(-910);
	}
	fprintf(zl, "Opening Log File\n"); fflush(zl);
			pthread_mutexattr_t mutexattr;
			pthread_mutexattr_init(&mutexattr);
			pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&g_criticalSection, &mutexattr);
			pthread_mutexattr_destroy(&mutexattr);
			pthread_mutex_lock(&initMutex);
			Options::Create("/home/pi/Documents/open-zwave-1.5/config/", "", "" );
			Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Detail );
			Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
			Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );
			Options::Get()->AddOptionInt( "PollInterval", 500 );
			Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
			Options::Get()->AddOptionBool("ValidateValueChanges", true);
			Options::Get()->Lock();
			Manager::Create();
			Manager::Get()->AddWatcher( OnNotification, NULL );
			Manager::Get()->AddDriver( "/dev/ttyUSB0" );
			pthread_cond_wait(&initCond, &initMutex);
			while( !g_homeId )
			{
				sleep(1);
			}
			while( !g_nodesQueried )
			{
				sleep(1);
			}
			Manager::Get()->WriteConfig( g_homeId );
			sleep(1);

	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) // find node in nodelist
	{
		NodeInfo* nodeInfo = *it;
		printf("This is Node %d\n",nodeInfo->m_nodeId);
		strcpy(s,(Manager::Get()->GetNodeLocation(nodeInfo->m_homeId,nodeInfo->m_nodeId)).c_str());
		printf("It's current location is %s\n",s);
		printf("Change it? ");
		gets(s);
		if(!strncmp(s,"yes",3)) {
			printf("New Location? ");
			gets(s);
			string sl(s);
			Manager::Get()->SetNodeLocation(nodeInfo->m_homeId,nodeInfo->m_nodeId,sl);
		}
		strcpy(s,(Manager::Get()->GetNodeName(nodeInfo->m_homeId,nodeInfo->m_nodeId)).c_str());
                printf("It's current name is %s\n",s);
                printf("Change it? ");
		gets(s);
                if(!strncmp(s,"yes",3)) {
                        printf("New Name? ");
			gets(s);
			string sl(s);
                        Manager::Get()->SetNodeName(nodeInfo->m_homeId,nodeInfo->m_nodeId,sl);
                }
        printf("Node %d, Named %s, Located at %s\n", nodeInfo->m_nodeId,
        (Manager::Get()->GetNodeName(nodeInfo->m_homeId,nodeInfo->m_nodeId)).c_str(),
        (Manager::Get()->GetNodeLocation(nodeInfo->m_homeId,nodeInfo->m_nodeId)).c_str());
        printf("Basic=%X, Generic= %X, Specific=%X, Type=%s\n",
        Manager::Get()->GetNodeBasic(nodeInfo->m_homeId,nodeInfo->m_nodeId),
        Manager::Get()->GetNodeGeneric(nodeInfo->m_homeId,nodeInfo->m_nodeId),
        Manager::Get()->GetNodeSpecific(nodeInfo->m_homeId,nodeInfo->m_nodeId),
        (Manager::Get()->GetNodeType(nodeInfo->m_homeId,nodeInfo->m_nodeId)).c_str());
        printf("Set Level? ");
        gets(s);
        while (strcmp(s, "no")) {
            level = atoi(s);
            Manager::Get()->SetNodeLevel(nodeInfo->m_homeId,nodeInfo->m_nodeId,(uint8) level);
            gets(s);
        }
	}
	Manager::Get()->WriteConfig( g_homeId );
	pthread_mutex_destroy( &g_criticalSection );
	fprintf(zl,"Program Terminated\n"); fflush(zl);
	fclose(zl);
	string dum1;
	scanf("%s", dum1.c_str());
	return 0; // we're done
}


