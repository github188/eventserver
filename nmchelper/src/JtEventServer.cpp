#include "StdAfx.h"
#include <string>
#include "JtEventPairPipe.h"

#if (defined(WIN32) || defined(WIN64))
	#if (defined(WIN32))
	#include <event2/event-config.h>
	#include <event2/thread.h>
	#include <event2/util.h>
	#include <event2/event_struct.h>
	#include <event2/event.h>
	#include <event2/bufferevent.h>
	#pragma message("using windows i586 include file") 
	#else
	#pragma message("using windows x64 include file") 
	#endif

#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
	#ifndef _LP64
	#warning "using linux i586 include file"
	#include <i586/event2/event_struct.h>
	#include <i586/event2/thread.h>
	#include <i586/event2/event.h>
	#include <i586/event2/bufferevent.h>
	#else
	#warning "using linux x64 include file"
	#include <event2/thread.h>
	#include <event2/event_struct.h>
	#include <event2/event.h>
	#include <event2/bufferevent.h>
	#endif
#endif

#include "jtprintf.h"

#include "NmcCmdDefine.h"
#include "JtEventServer.h"
#include "JtEventTimer.h"

JtEventServer::JtEventServer() : base(NULL), m_Started(0)
{
	pEventPairPipe = NULL;
#ifdef _WIN32	
	WSADATA wsa_data;	
	WSAStartup(0x0201, &wsa_data);
#endif
}
JtEventServer::~JtEventServer()
{
#ifdef _WIN32	
	//WSADATA wsa_data;	
	//WSAStartup(0x0201, &wsa_data);
#endif
}

#if (defined(WIN32) || defined(WIN64))

#else

static pthread_once_t random_is_initialized = PTHREAD_ONCE_INIT;

#endif



static JtEventServer *g_JtEventServer = NULL;

void JtEventServer::InitJtEventServer(void)
{
	if(g_JtEventServer==NULL)
	{
		g_JtEventServer = new JtEventServer();
		g_JtEventServer->Start();
		//sleep(3);
	}
}

JtEventServer *JtEventServer::GetInstance()
{
#if (defined(WIN32) || defined(WIN64))
	if(g_JtEventServer==NULL)
	{
		g_JtEventServer = new JtEventServer();
		g_JtEventServer->Start();
		//sleep(3);
	}
#else

	pthread_once(&random_is_initialized, InitJtEventServer);

#endif

	//jtprintf("[%s]g_JtEventServer %p\n", __FUNCTION__, g_JtEventServer);
	
	return g_JtEventServer;
}

#if (defined(WIN32) || defined(WIN64))

void JtEventServer::Static_StartInThread(void *arg)
{
	JtEventServer *Self = (JtEventServer *)arg;
	Self->EventLoop();
	return;
}

#else

void* JtEventServer::Static_StartInThread(void *arg)
{
	JtEventServer *Self = (JtEventServer *)arg;
	Self->EventLoop();
	return NULL;
}
#endif

int JtEventServer::EventLoop()
{
	//Ĭ�ϴ���һ����ʱ��
	JtEventTimer *Timer = new JtEventTimer();
	Timer->OnAddToServer(this);

	pEventPairPipe = new JtEventPairPipe();
	pEventPairPipe->AddToServer(this);
	
	//jtprintf("[%s]event_base_dispatch before\n", __FUNCTION__);

	int res = event_base_dispatch(base);
	int ss = res;

	delete Timer;

	event_base_free(base);

	//

	return 0;
}
int JtEventServer::OnRecvData(void* Cookie, unsigned char* pData, int dataLen)
{
	if(Cookie==pEventPairPipe)
	{
		//����
		ExCommand *pHead = (ExCommand *)pData;
		pHead->nSrcType;			
		pHead->nCmdType;		
		pHead->nCmdSeq;
		pHead->nContentSize;

		if(pHead->nCmdType==JTEVENT_NOTIFY_ADD_PEER)
		{
			//lock....
			while(!m_PeerWait2AddS.empty())
			{
				JtEventPeer *Peer = m_PeerWait2AddS.front();
				m_PeerWait2AddS.pop_front();
				if(Peer)
					Peer->OnAddToServer(this);			
			}
		}
		else if(pHead->nCmdType==JTEVENT_TEST_CMD)
		{
			MessageBox(NULL, _T("��������"), _T("��������"), MB_OK);
		}
	}
	
	return 0;
}
int JtEventServer::OnStateChanged(void* Cookie)
{
	if(Cookie==pEventPairPipe)
	{
		//����

	}

	return 0;
}
int JtEventServer::Start()
{
	if(!m_Started)
	{
		int res = 0;
		m_Started = 1;

#if (defined(WIN32) || defined(WIN64))
		res = evthread_use_windows_threads();
#else
		res = evthread_use_pthreads();
#endif
		if(res)
		{
			//jtprintf("[%s]evthread_use_pthreads failed\n", __FUNCTION__);
		}
		
		base = event_base_new();
		if(!base)
		{
			jtprintf("[%s]event_base_new failed %p\n", __FUNCTION__, base);
			return -1;
		}

		//res = bufferevent_pair_new(base, int options,
		//struct bufferevent *pair[2]);

#if (defined(WIN32) || defined(WIN64))
		_beginthread(Static_StartInThread, NULL, this);
#else
		pthread_t tid;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		//����ˣ�stop������Ϊ��PTHREAD_CREATE_DETACHED��
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		res = pthread_create(&tid, &attr, Static_StartInThread, this);
		pthread_attr_destroy(&attr);
#endif
		return res;
	}

	return 0;
}
static int JtEventServer_static_nCmdSeq = 0;
int JtEventServer::GenSeq()
{
	static int sJtEventServerSeq = 0;

	return ++sJtEventServerSeq;
}
int JtEventServer::Stop()
{
	ExCommand Head;
	Head.nSrcType				= 0;
	Head.nCmdType				= JTEVENT_NOTIFY_STOP_LOOP;
	Head.nCmdSeq				= ++JtEventServer_static_nCmdSeq;
	Head.nContentSize			= 0;

	pEventPairPipe->SendCmd((const char*)&Head, sizeof(Head));

	return 0;
}
int JtEventServer::AddPeer(JtEventPeer *Peer)
{
	//lock.... to do.....
	//assert(Peer);

	m_PeerWait2AddS.push_back(Peer);

	NotifyAddPeer();
	return 0;
}
int JtEventServer::TestCmd()
{
	ExCommand Head;
	Head.nSrcType				= 0;
	Head.nCmdType				= JTEVENT_TEST_CMD;
	Head.nCmdSeq				= ++JtEventServer_static_nCmdSeq;
	Head.nContentSize			= 0;

	return pEventPairPipe->SendCmd((const char*)&Head, sizeof(Head));
}
int JtEventServer::NotifyAddPeer()
{
	ExCommand Head;
	Head.nSrcType				= 0;
	Head.nCmdType				= JTEVENT_NOTIFY_ADD_PEER;
	Head.nCmdSeq				= ++JtEventServer_static_nCmdSeq;
	Head.nContentSize			= 0;

	//char ddd[1024];
	//pEventPairPipe->SendCmd((const char*)ddd,1024);
	pEventPairPipe->SendCmd((const char*)&Head, sizeof(Head));

	return 0;
}



