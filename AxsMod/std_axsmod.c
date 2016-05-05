/*
 *  Teradata Access Module
 *  Bridge external pre-process program, which stdout is var-Text
 *
 *  Copyright (C) 2015, XiaoYi
 *
 *  Use in BTEQ, Fastload, TPT
 *  Example - BTEQ
 *     .IMPORT INDICDATA FILE='C:\tmp\test.txt' AXSMOD std_axsmod.dll "Text2TDIndic.exe A9AA 3 D "
 *     .QUIET ON
 *     .REPEAT * PACK 6000
 *     USING c1 (VARCHAR(102)),c2 (VARCHAR(102)),c3 (VARCHAR(102))
 *     INSERT INTO STAGE.test_1215(c1,c2,c3) VALUES(:c1,:c2,:c3);
 */

#include <stddef.h>

#pragma warning (disable: 4996)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pmdcomt.h"
#include "pmddamt.h"

#ifdef WIN32
#define pclose _pclose
#define popen  _popen
#endif

#define MODULE_NAME "STD_AXSMOD"
#define ENVIRONMENT "Teradata Utilities"
#define VERSION "1.1"

#define BUFFER_LENGTH 65536
static char ErrorBuffer[pmiMAX_ETEXT_LEN + 1];
static size_t ErrorLength = 0;

static FILE *fInput = NULL;
static char cmdBuff[1024];

/****************************************************************************/
/* Main routine */
/****************************************************************************/
#ifdef WIN32
__declspec(dllexport)
#endif
void PIDMMain(pmiCmdBlock_t *Opts, void *OptParms)
{
	pmReturnType retcode = pmrcOK;
	static pmUInt32 SG_BufferLen;
	static pmUInt32 SG_rec_cnt;
	static pmUInt32 SG_block_cnt;
	static bHeadersAnnounced = 0;
	char AtrName[pmMAX_ATR_NAME_LEN + 1];
	char logBuffer[1024];
	char *line;
	int rb;

	if (!bHeadersAnnounced)
	{
		printf(PM_COPYRIGHT_NOTICE);
		printf("\n\n *** Access Module '%s', Version: %s, Copyright (C) 2015, XiaoYi.\n",
				MODULE_NAME,
				VERSION);
		sprintf(logBuffer, " *** Compiled for %s (%s).\n", PLATFORM_ID, ENVIRONMENT);
		printf(logBuffer);
		bHeadersAnnounced = 1;
	}

	switch (Opts->Reqtype)
	{
	case pmiPIDMOptInit:
		if (((pmiInit_t*) OptParms)->InterfaceVerNo != pmInterfaceVersionD
				|| ((pmiInit_t*) OptParms)->InterfaceVerNoD
						!= pmiInterfaceVersion)
		{
			retcode = pmrcBadVer;
		}
		else
		{
			pmiInit_t *p = (pmiInit_t*) OptParms;
			if (p->InitStrL <= 0)
			{
				retcode = pmrcFailure;
				sprintf(ErrorBuffer, " *** Init String is NULL");
				printf(ErrorBuffer);
				ErrorLength = strlen(ErrorBuffer);
			}
			else
			{
				strncpy(cmdBuff, p->InitStr, p->InitStrL);
			}
		}
		((pmiInit_t*) OptParms)->PIData = 0;
		break;
		
	case pmiPIDMOptOpen:
		((pmiOpen_t*) OptParms)->FileName[((pmiOpen_t*) OptParms)->FileNameL] =
				0;
		strcat(cmdBuff, ((pmiOpen_t*) OptParms)->FileName);
		printf(" *** CMD=%s\n\n", cmdBuff);

		fInput = popen(cmdBuff, "r");
		if (fInput == NULL)
		{
			sprintf(ErrorBuffer, " *** Pipe open(%s) failed, %s\n", cmdBuff,
					_strerror(NULL));
			printf(ErrorBuffer);
			ErrorLength = strlen(ErrorBuffer);
			retcode = pmrcFailure;
			break;
		}

		((pmiOpen_t*) OptParms)->BlkSize = BUFFER_LENGTH;
		((pmiOpen_t*) OptParms)->BlkHeaderLen = 0;
		((pmiOpen_t*) OptParms)->FIData = 0;
		retcode = pmrcOK;
		break;

	case pmiPIDMOptClose:
	case pmiPIDMOptCloseR:
		if (fInput != NULL)
			rb = pclose(fInput);
		else
			rb = 0;
		fInput = NULL;
		if (rb != 0)
		{
			sprintf(ErrorBuffer, " *** External program error.\n");
			retcode = pmrcFailure;
		}
		break;

	case pmiPIDMOptRead:
		rb = fread(((pmiRW_t*) OptParms)->Buffer, 1, BUFFER_LENGTH, fInput);
		if (rb <= 0)
		{
			((pmiRW_t*) OptParms)->BufferLen = 0;
			retcode = pmrcEOF;
			rb = pclose(fInput);
			fInput = NULL;
			if (rb != 0)
			{
				sprintf(ErrorBuffer, " *** External program error=%d.\n", rb);
				retcode = pmrcFailure;
			}
			break;
		}
		((pmiRW_t*) OptParms)->BufferLen = rb;
		break;
	case pmiPIDMOptGetA_A:
		break;
	case pmiPIDMOptGetF_A:
		break;
	case pmiPIDMOptPutA_A:
		break;
	case pmiPIDMOptPutF_A:
		break;
	case pmiPIDMOptShut:
		retcode = pmrcOK;
		break;
	case pmiPIDMOptID:
		strcpy(((pmiID_t*) OptParms)->VerIDList.ModuleName, MODULE_NAME);
		strcpy(((pmiID_t*) OptParms)->VerIDList.ModuleVers, VERSION);
		((pmiID_t*) OptParms)->VerIDList.Next = 0;
		break;
	case pmiPIDMOptGetPos:
		break;
	case pmiPIDMOptSetPos:
		fflush(stdout);
		if (fInput != NULL)
			pclose(fInput);
		printf(" *** STD_AXSMOD re-open the file\n");
		fflush(stdout);
		fInput = popen(cmdBuff, "r");
		break;
	default:
		retcode = pmrcUnsupported;
	}

	Opts->Retcode = retcode;
	if (retcode == pmrcFailure)
	{
		Opts->ErrMsg.Data = &ErrorBuffer[0];
		Opts->ErrMsg.DataLength = ErrorLength;
	}
	else
	{
		Opts->ErrMsg.Data = 0;
		Opts->ErrMsg.DataLength = 0;
	}
}
