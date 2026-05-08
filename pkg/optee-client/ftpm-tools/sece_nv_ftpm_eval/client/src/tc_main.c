#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <tee_client_api.h>

#include "user_ta_header_defines.h"
#include "ta_commands.h"

#define CMD_LICENCE  0
#define CMD_WIPE     1
#define CMD_STATUS   2
#define CMD_UPDATE   3

#ifndef NO_MANIFEST
#define SLI_MANIFEST_SIZE 0x00010000
#define OFFSET_CERTS 0x00000000

#define MANIFEST_PARTITION "/dev/disk/by-partlabel/manifest"
#endif


static size_t loadFile(uint8_t** buffer,char* filename)
{
	size_t res=0;
	*buffer=0;

	FILE* file=fopen(filename,"rb");
	if (file)
	{
		fseek(file,0,SEEK_END);
		res=ftell(file);
		rewind(file);
		
		*buffer=(uint8_t*)malloc(res);
		fread(*buffer,1,res,file);

		fclose(file);
	}
	return res;
}


#ifndef NO_MANIFEST
static void writeManifest(uint8_t* buffer)
{
	FILE* file=fopen(MANIFEST_PARTITION,"wb");
	if (file)
	{
		fwrite(buffer,SLI_MANIFEST_SIZE,1,file);
		fclose(file);
	}
}
#endif


static void printUsage(void)
{
	printf(
				 "sece_nv_ftpm_eval [command] <licence file>\n"
				 "\n"
				 "[command]:\n"
				 "  -l  Ingest an evaluation licence file.  Requires <licence file>\n"
				 "  -w  Wipes the evaluation licence (requires reboot to complete wipe)\n"
				 "  -u  Updates FTPM parameters with evaluation licence\n"
				 "  -s  Returns the FTPM licence status\n"
				 " \n"
				 );
}

//===============================================

#define ING_ERROR_OK        0
#define ING_ERROR_FILE      1


static TEEC_Result ingest_licence(TEEC_Session* session,char* licence_file)
{
	TEEC_Result res=0;
	size_t licence_size=0;
	uint8_t* licence_buffer=0;

	licence_size=loadFile(&licence_buffer,licence_file);
	if (licence_size>0 && licence_buffer)
	{
		TEEC_Operation op;
		uint32_t errorigin=0;

#ifndef NO_MANIFEST
		uint8_t* cert_buffer=(uint8_t*)malloc(SLI_MANIFEST_SIZE);
		memset(cert_buffer,0,SLI_MANIFEST_SIZE);
		
		op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_MEMREF_TEMP_OUTPUT,TEEC_NONE,TEEC_NONE);
		
		op.params[0].tmpref.buffer=licence_buffer;
		op.params[0].tmpref.size=licence_size;

		op.params[1].tmpref.buffer=cert_buffer;
		op.params[1].tmpref.size=SLI_MANIFEST_SIZE;
#else
		op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_NONE,TEEC_NONE,TEEC_NONE);
		
		op.params[0].tmpref.buffer=licence_buffer;
		op.params[0].tmpref.size=licence_size;
#endif

		res=TEEC_InvokeCommand(session,SEQ_FTPMEVAL_PTA_CMD_INGEST,&op,&errorigin);

		if (!res)
		{
#ifndef NO_MANIFEST
			writeManifest(cert_buffer);
#endif
			printf("Evaluation licence successfully ingested\n");
		}
		else
		{
			printf("Could not ingest Evaluation licence:  0x%08x\n",res);
		}

#ifndef NO_MANIFEST
		free(cert_buffer);
#endif
		free(licence_buffer);
	}
	else
		res=ING_ERROR_FILE;
	
	return res;
}


static TEEC_Result wipe_licence(TEEC_Session* session)
{
	TEEC_Result res=0;
	TEEC_Operation op;
	uint32_t errorigin=0;

#ifndef NO_MANIFEST
	uint8_t* cert_buffer=(uint8_t*)malloc(SLI_MANIFEST_SIZE);
	memset(cert_buffer,0,SLI_MANIFEST_SIZE);
	
	op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,TEEC_NONE,TEEC_NONE,TEEC_NONE);
	
	op.params[0].tmpref.buffer=cert_buffer;
	op.params[0].tmpref.size=SLI_MANIFEST_SIZE;
#else
	op.paramTypes=TEEC_PARAM_TYPES(TEEC_NONE,TEEC_NONE,TEEC_NONE,TEEC_NONE);
#endif

	res=TEEC_InvokeCommand(session,SEQ_FTPMEVAL_PTA_CMD_WIPE,&op,&errorigin);
	
	if (!res)
	{
#ifndef NO_MANIFEST
		writeManifest(cert_buffer);
#endif
		printf("Evaluation licence wiped - please reboot device to complete\n");
	}
	else
	{
		printf("Could not wipe licence: 0x%08x\n",res);
	}

#ifndef NO_MANIFEST
	free(cert_buffer);
#endif
	
	return res;
}


static TEEC_Result get_status(TEEC_Session* session)
{
	TEEC_Result res=0;
	TEEC_Operation op;
	uint32_t errorigin=0;

	op.paramTypes=TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT,TEEC_NONE,TEEC_NONE,TEEC_NONE);

	res=TEEC_InvokeCommand(session,SEQ_FTPMEVAL_PTA_CMD_STATUS,&op,&errorigin);

	if (!res)
	{
		switch (op.params[0].value.a)
		{
		case 1:
			printf("FTPM status: EVALUATION\n");
			break;
		case 2:
			printf("FTPM status: Normal\n");
			break;
		default:
			printf("FTPM status: UNKNOWN\n");
		}
	}
	
	return res;
}


static TEEC_Result update_licence(TEEC_Session* session)
{
	TEEC_Result res=0;
	TEEC_Operation op;
	uint32_t errorigin=0;

	op.paramTypes=TEEC_PARAM_TYPES(TEEC_NONE,TEEC_NONE,TEEC_NONE,TEEC_NONE);

	res=TEEC_InvokeCommand(session,SEQ_FTPMEVAL_PTA_CMD_UPDATE,&op,&errorigin);

	if (!res)
		printf("FTPM parameters updated with evaluation licence\n");
	else
		printf("FTPM parameters could NOT be updated: 0x%08x\n",res);
	
	return res;
}

//===============================================

#define TEEC_ERROR_OK        0
#define TEEC_ERROR_CONTEXT   1
#define TEEC_ERROR_SESSION   2

int main(int argc,char** argv)
{
	int res=0;
	char* licence_file=0;
	int command=-1;

	int opt=0;
	// w: wipe  s: status  l: licence  u: update
	while ((opt=getopt(argc,argv,"wsul:"))!=-1)
	{
		switch (opt)
		{
		case 'l':
			command=CMD_LICENCE;
			licence_file=optarg;
			break;
		case 's':
			command=CMD_STATUS;
			break;
		case 'w':
			command=CMD_WIPE;
			break;
		case 'u':
			command=CMD_UPDATE;
			break;
		default:
			exit(1);
		}
	};


	if (command==-1)
	{
		printUsage();
		exit(1);
	}


	TEEC_Result teeres=TEEC_SUCCESS;
	TEEC_Context ctx;

	teeres=TEEC_InitializeContext(0,&ctx);
	if (!teeres)
	{
		TEEC_Session session;
		const TEEC_UUID ftpm_pta=TA_UUID;
		uint32_t errorigin;
		
		teeres=TEEC_OpenSession(&ctx,&session,&ftpm_pta,TEEC_LOGIN_PUBLIC,0,0,&errorigin);
		if (!teeres)
		{
			int cmdres __attribute__((unused))=0;

			switch (command)
			{
			case CMD_LICENCE:
				if (!licence_file)
				{
					printUsage();
					exit(1);
				}
				cmdres=ingest_licence(&session,licence_file);
				break;
			case CMD_STATUS:
				cmdres=get_status(&session);
				break;
			case CMD_WIPE:
				cmdres=wipe_licence(&session);
				break;
			case CMD_UPDATE:
				cmdres=update_licence(&session);
				break;
			}

			TEEC_CloseSession(&session);
		}
		else
		{
			res=TEEC_ERROR_SESSION;
			printf("Could not open session: 0x%08x\n",teeres);
		}
	}
	else
	{
		res=TEEC_ERROR_CONTEXT;
		printf("Could not create context: 0x%08x\n",teeres);
	}

	return res;
}
