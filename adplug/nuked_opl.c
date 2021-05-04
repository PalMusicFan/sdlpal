/*
 * SDLPAL
 * Copyright (c) 2011-2020, SDLPAL development team.
 * All rights reserved.
 *
 * This file is part of SDLPAL.
 *
 * SDLPAL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * nuked_opl.c - Wrapper of NUKED's OPL core code.
 *
 */


#pragma once

#ifdef __cplusplus  
extern "C" {
#endif 

#include <pspdisplay.h>
#include <pspdebug.h>
#define printf pspDebugScreenPrintf
#include <time.h>
#include <stdbool.h>
#include <pspkerneltypes.h>
#include <psptypes.h>
#include <me.h>
#include <psprtc.h>

/**
* The mode of execution for a specific job.
*/
#define MELIB_EXEC_DEFAULT	0x0 /** Executes on the ME, unless Dynamic Rebalancing is turned on */
#define MELIB_EXEC_CPU		0x1 /**  Executes specifically on the main CPU, regardless of job mode. Always runs synchronously (gives a small delay between jobs).*/
#define MELIB_EXEC_ME		0x2 /** Executes specifically on the Media Engine, regardless of job mode. Always runs asynchronously.*/


/**
* This structure is used to determine job execution.
* Given the properties in this structure, the manager will execute correctly
*/
struct JobInfo {
	unsigned char id; /** This ID is purely useless for now - but may be used in the future for performance tracking */
	unsigned char execMode; /** Uses execution mode to specify where the code will run and/or if said code can be dynamically rebalanced */
};

struct Job;

/**
* Job Data is an integer pointer to an address with the data.
*/
typedef int JobData;
	
/**
* This typedef defines a JobFunction as an integer function with given data.
*/
typedef int (*JobFunction)(JobData ptr);

/**
* This structure is used to give job information alongside the job itself and the data needed.
*/
struct Job {
	struct JobInfo jobInfo;
	JobFunction function;
	JobData data;
};

/** 
* JobManager class. This class only can have one instance for the ME.
*/

void J_Init(bool dynamicRebalance); /** Initialize the job manager with the option to dynamically rebalance loads. */
void J_Cleanup(); /** Cleans up and ends execution. */

void J_AddJob(struct Job* job); /** Adds a job to the queue. If the queue is full (max 256) then it will force a dispatch before adding more. */
void J_ClearJob(); /** Clears and deletes all jobs */

void J_DispatchJobs(float cpuTime); /** Starts a thread to dispatch jobs and execute! CPU Time is the time used by the rest of the system for Dynamic Rebalancing - it's unused otherwise. */
void J_Update(float cpuTime); /** Dispatches and runs jobs on this thread. See above for CPU Time */

float J_GetMETime();
float J_GetCPUTime();

struct me_struct* mei;

#ifdef __cplusplus  
}
#endif 


#include "nuked/opl3.c.h"
 