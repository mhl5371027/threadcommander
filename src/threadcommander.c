#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "threadcommander.h"

/**
 *  @struct THREAD_POOL_T
 *  @brief The thread pool struct
 *
 *  @var ThreadInfoQueue		thread name
 *  @var maxNum					threads in pool max num
 *  @var head					first thread info index in ThreadInfoQueue
 *  @var tail					last thread info  index in ThreadInfoQueue
 *  @var count					urrent threads num
 */

typedef struct {
	THREAD_INFO_T * ThreadInfoQueue;
	int maxNum;
	int head;
	int tail;
	int count;
}THREAD_POOL_T;



static THREAD_POOL_T gstThreadPool;

int threadcommander_init(int threadMaxNum)
{
	int i = 0, index = 0;
	if(threadMaxNum <= 0)
		return threadcommander_param_err;

	memset(&gstThreadPool, 0, sizeof(gstThreadPool));
	gstThreadPool.maxNum = threadMaxNum;
	gstThreadPool.ThreadInfoQueue = (THREAD_INFO_T *)malloc(sizeof(THREAD_INFO_T) * threadMaxNum);
	if(gstThreadPool.ThreadInfoQueue)
	{
		memset(gstThreadPool.ThreadInfoQueue, 0, sizeof(THREAD_INFO_T) * threadMaxNum);
		gstThreadPool.head = 0;
		gstThreadPool.tail = 0;
		gstThreadPool.count = 0;
		return threadcommander_ok;
	}
	else
		return threadcommander_err;
}

static THREAD_INFO_T * GetThreadInfoPoint(char * pThreadName, int * pIndex)
{
	int index = 0, i = 0;
	THREAD_INFO_T * pThreadInfo = 0;
	
	if(!pThreadName)
		return 0;

	index = gstThreadPool.head;

	for(i = 0; i < gstThreadPool.count; i++)
	{
		pThreadInfo = &gstThreadPool.ThreadInfoQueue[index];
		if(!strcmp(pThreadName, pThreadInfo->threadName))
		{
			*pIndex = index;
			break;
		}

		index = (index+ 1) % gstThreadPool.maxNum;

		pThreadInfo = 0;
	}

	return pThreadInfo;
}


int threadcommander_create(char * pThreadName, void (*start_rtn)(void *), void *arg, int isRun, int bEnableCtrl)
{
	int index = 0;
	THREAD_INFO_T * pThreadInfo = 0;
	if(!pThreadName || !start_rtn || !strlen(pThreadName))
		return threadcommander_param_err;


	if(gstThreadPool.count == gstThreadPool.maxNum)
		return threadcommander_queue_full;

	pThreadInfo = GetThreadInfoPoint(pThreadName, &index);
	if(!pThreadInfo)
		pThreadInfo = &gstThreadPool.ThreadInfoQueue[gstThreadPool.tail];

	strcpy(pThreadInfo->threadName, pThreadName);
	pThreadInfo->start_rtn = start_rtn;
	pThreadInfo->arg = arg;
	pThreadInfo->bEnableCtrl = bEnableCtrl;

	sem_init(&pThreadInfo->ctrlSem, 0, 0);
	sem_init(&pThreadInfo->ctrlRetSem, 0, 0);

	if (0 != pthread_create(&pThreadInfo->threadID, NULL, pThreadInfo->start_rtn, (void *)pThreadInfo));
		perror("pthread_create");

	pThreadInfo->isRun = isRun;

	gstThreadPool.tail = (gstThreadPool.tail+ 1) % gstThreadPool.maxNum;
	gstThreadPool.count++;

	return threadcommander_ok;
}


void threadcommander_waitctrl(THREAD_INFO_T * arg)
{
	if(!arg->bEnableCtrl)
		return;

	if(arg->isRun)
	{
		if(0 == sem_trywait(&arg->ctrlSem))
		{
			arg->isRun = 0;
			sem_post(&arg->ctrlRetSem);
		}
	}

	if(!arg->isRun)
	{
		sem_wait(&arg->ctrlSem);
		arg->isRun = 1;
		sem_post(&arg->ctrlRetSem);
	}
}


int threadcommander_ctrl(char * pThreadName, int isRun)
{
	int index = 0;
	int ret = threadcommander_err;
	THREAD_INFO_T * pThreadInfo = 0;
	if(!pThreadName || !strlen(pThreadName))
		return threadcommander_param_err;

	do{
		pThreadInfo = GetThreadInfoPoint(pThreadName, &index);
		if(!pThreadInfo)
		{
			break;
		}
			
		if(pThreadInfo->isRun == isRun)
		{
			ret = threadcommander_ok;
			break;
		}
		if(!pThreadInfo->bEnableCtrl)
		{
			ret = threadcommander_ctl_unable;
			break;
		}
		if(pThreadInfo->bEnableCtrl)
		{
			sem_post(&pThreadInfo->ctrlSem);
			sem_wait(&pThreadInfo->ctrlRetSem);
		}

		ret = threadcommander_ok;
	}while(0);

	return ret;
}

int threadcommander_kill(char * pThreadName)
{
	int index = 0, ret = threadcommander_err;
	void *status;
	THREAD_INFO_T * pThreadInfo = 0;

	if(!pThreadName || !strlen(pThreadName))
		return threadcommander_param_err;

	do{
		pThreadInfo = GetThreadInfoPoint(pThreadName, &index);
		if(!pThreadInfo)
		{
			ret = threadcommander_ok;
			break;
		}
		
		pthread_cancel(pThreadInfo->threadID);
		pthread_join(pThreadInfo->threadID,&status);

		sem_destroy(&pThreadInfo->ctrlSem);
		sem_destroy(&pThreadInfo->ctrlRetSem);

		if(index != gstThreadPool.head)
			memcpy(&gstThreadPool.ThreadInfoQueue[index],
				&gstThreadPool.ThreadInfoQueue[gstThreadPool.head], sizeof(THREAD_INFO_T));
		else
			memset(&gstThreadPool.ThreadInfoQueue[gstThreadPool.head], 0, sizeof(THREAD_INFO_T));

		gstThreadPool.head = (gstThreadPool.head+ 1) % gstThreadPool.maxNum;
		gstThreadPool.count--;

		ret = threadcommander_ok;
	}while(0);

	return ret;
}
