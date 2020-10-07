/*
 * EDMA_HLD.h
 * API for the Higher-Level Driver for the EDMA3 peripheral.  Work in progress.
 */

#ifndef EDMA3CHAINEDTRANSFER_EDMA_HLD_H_
#define EDMA3CHAINEDTRANSFER_EDMA_HLD_H_

#include <xdc/cfg/global.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/runtime/System.h>
#include <ti/sdo/edma3/drv/edma3_drv.h>
#include <ti/sdo/edma3/rm/edma3_common.h>
#include <ti/sdo/edma3/rm/edma3_rm.h>
#include <xdc/runtime/knl/Cache.h>

extern void registerEdma3Interrupts(unsigned int);
extern EDMA3_DRV_Result edma3OsSemCreate(int, const Semaphore_Params *,
		EDMA3_OS_Sem_Handle *);
extern EDMA3_DRV_Handle edma3init(unsigned int, EDMA3_DRV_Result *);
extern EDMA3_DRV_Result edma3deinit(unsigned int, EDMA3_DRV_Handle);
extern EDMA3_DRV_Result Edma3_CacheFlush(unsigned int, unsigned int);
extern EDMA3_DRV_Result Edma3_CacheInvalidate(unsigned int, unsigned int);

//typedef unsigned char byte;
//typedef byte bool;

void checkRes(EDMA3_DRV_Result res, char *s);

typedef struct {
	UInt address;
	UInt array_strobe;
	UInt frame_strobe;
	EDMA3_DRV_AddrMode mode;
	EDMA3_DRV_FifoWidth circSz;
} Address_Options;

typedef struct {
	UInt array_count;
	UInt frame_count;
	UInt frame_count_reload;
	UInt block_count;
	EDMA3_DRV_SyncType sync_mode;
} Copy_Options;

typedef struct {
	UInt intermediate_chain_en;
	UInt final_chain_en;
	UInt chain_target;
	UInt intermediate_int_en;
	UInt final_int_en;
	UInt early_completion;
} ChainInt_Options;

typedef struct {
	UInt link_addr;
	UInt isLinking;
} Link_Options;

typedef struct {
	UInt param_id;
	Address_Options src;
	Address_Options dst;
	Copy_Options cpy;
	ChainInt_Options chnint;
	Link_Options lnk;
	bool isValid;
	bool isAllocated;
} PaRAM_Options;

typedef struct {
	bool isAllocated;
	UInt channel_id;
	UInt init_param_set;
	UInt assoc_tcc;
	UInt assoc_evtQ;
	EDMA3_RM_TccCallback callback;
	void *callback_data;
}QChan_Options;

typedef struct {
	UInt channel_id;
	UInt assoc_tcc;
	UInt assoc_evtQ;
	EDMA3_RM_TccCallback callback;
	void *callback_data;
	EDMA3_DRV_TrigMode trig_mode;
	UInt init_param_set;
	bool isAllocated;
} Channel_Options;

typedef struct {
	EDMA3_DRV_Handle edma_handle;
	UInt CC_num;
	UInt num_channels;
	UInt num_params;
	UInt num_evtqs;
	UInt num_qchans;
	Channel_Options *channels;
	PaRAM_Options *links;
	QChan_Options *qchans;
	UInt QDMA_trig_word;
	UInt qdma_base;
} EDMA_Config;

void EDMA_Init(EDMA_Config **edma_cfg, UInt EDMA_num);
void EDMA_UnInit(EDMA_Config **edma_cfg);
UInt EDMA_getDMAChan(EDMA_Config *edma_cfg, UInt reqChanNumber,
		UInt reqTccNumber, UInt reqEvtQNumber, EDMA3_RM_TccCallback callback,
		void *callback_data);
UInt EDMA_getLink(EDMA_Config *edma_cfg, UInt reqLinkNumber, UInt reqTccNumber,
		UInt reqEvtQNumber, EDMA3_RM_TccCallback callback, void *callback_data);
void EDMA_programDMAChan(EDMA_Config *edma_cfg, UInt chanNum);
void EDMA_programLink(EDMA_Config *edma_cfg, UInt linkNum);
void EDMA_enableChan(EDMA_Config *edma_cfg, UInt chanNum);
UInt EDMA_getQDMAChan(EDMA_Config *edma_cfg, UInt reqChanNumber,
		UInt reqTccNumber, UInt reqEvtQNumber, EDMA3_RM_TccCallback callback,
		void *callback_data);
void EDMA_startQDMA(EDMA_Config *edma_cfg, UInt qdNum);
void EDMA_printRegDMA(EDMA_Config *edma_cfg, UInt chanNum);
void EDMA_printRegQDMA(EDMA_Config *edma_cfg, UInt chanNum);

#endif /* EDMA3CHAINEDTRANSFER_EDMA_HLD_H_ */
