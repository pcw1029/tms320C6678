/*
 * EDMA_HLD.c
 * Higher-Level Driver for the EDMA3 peripheral.  Work in progress.
 */

#include <xdc/cfg/global.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/runtime/System.h>
#include <ti/sdo/edma3/drv/edma3_drv.h>
#include <ti/sdo/edma3/rm/edma3_common.h>
#include <ti/sdo/edma3/rm/edma3_rm.h>
#include <xdc/runtime/knl/Cache.h>
#include <xdc/runtime/Memory.h>

#include <stdio.h>

#include "EDMA_HLD.h"

void EDMA_writeTransferPaRAMs(EDMA_Config *edma_cfg, UInt qdNum);

void checkRes(EDMA3_DRV_Result res, char *s) {
	char temp[50]; // 배열 temp는 인쇄해야하는 출력 문자를 보유하고 있습니다. 긴 텍스트를 사용하는 경우 배열 크기를 확장 할 수 있습니다.
	System_sprintf(temp, "Error: %s\n", s);
	if (res != EDMA3_DRV_SOK) {
		System_sprintf(temp, "Error: %s\n", s);
		System_abort(temp);
	}
	System_sprintf(temp, "Success: %s\n", s);
	System_printf(temp);
}

void EDMA_Init(EDMA_Config **p_edma_cfg, UInt EDMA_num) {
	EDMA3_DRV_Result res;
	*p_edma_cfg = Memory_alloc(NULL, sizeof(EDMA_Config), 0, NULL);
	EDMA_Config *edma_cfg = *p_edma_cfg;
	edma_cfg->CC_num = EDMA_num;
	/*간단한 EDMA3 초기화
     * 이 함수는 EDMA3 Driver를 초기화하고 인터럽트 핸들러를 등록합니다.
     * 성공하면 EDMA3_DRV_SOK를 반환하고 그렇지 않으면 오류 코드를 반환합니다.
     * */
	edma_cfg->edma_handle = edma3init(EDMA_num, &res);
	checkRes(res, "Opening Channel Controller");
	UInt numchannels = (EDMA_num == 0) ? 16 : 64;
	UInt numlinks = (EDMA_num == 0) ? 128 : 512;
	UInt numevtqs = (EDMA_num == 0) ? 2 : 4;
	edma_cfg->num_channels = numchannels;
	edma_cfg->num_params = numlinks;
	edma_cfg->num_evtqs = numevtqs;
	edma_cfg->num_qchans = 8;
	edma_cfg->channels = Memory_alloc(NULL, numchannels * sizeof(Channel_Options), 0, NULL);
	edma_cfg->links = Memory_alloc(NULL, numlinks * sizeof(PaRAM_Options), 0, NULL);
	edma_cfg->qchans = Memory_alloc(NULL, 8 * sizeof(QChan_Options), 0, NULL);
	edma_cfg->qdma_base = 0;
}

void EDMA_UnInit(EDMA_Config **p_edma_cfg) {
	EDMA_Config *edma_cfg = *p_edma_cfg;
	int i;
	for (i = 0; i < edma_cfg->num_channels; i++) {
		if (edma_cfg->channels[i].isAllocated) {
			EDMA3_DRV_freeChannel(edma_cfg->edma_handle,
					edma_cfg->channels[i].channel_id);
			edma_cfg->channels[i].isAllocated = FALSE;
			edma_cfg->links[edma_cfg->channels[i].init_param_set].isAllocated =
			FALSE;
		}
	}
	for (i = 0; i < edma_cfg->num_qchans; i++) {
		if (edma_cfg->qchans[i].isAllocated) {
			EDMA3_DRV_freeChannel(edma_cfg->edma_handle,
					edma_cfg->qchans[i].channel_id);
			edma_cfg->qchans[i].isAllocated = FALSE;
			edma_cfg->links[edma_cfg->qchans[i].init_param_set].isAllocated =
			FALSE;
		}
	}
	for (i = 0; i < edma_cfg->num_params; i++) {
		if (edma_cfg->links[i].isAllocated) {
			EDMA3_DRV_freeChannel(edma_cfg->edma_handle,
					edma_cfg->links[i].param_id);
			edma_cfg->links[i].isAllocated = FALSE;
		}
	}
	edma3deinit(edma_cfg->CC_num, edma_cfg->edma_handle);
	Memory_free(NULL, edma_cfg->links,
			edma_cfg->num_params * sizeof(PaRAM_Options));
	edma_cfg->links = NULL;
	Memory_free(NULL, edma_cfg->channels,
			edma_cfg->num_channels * sizeof(Channel_Options));
	edma_cfg->channels = NULL;
	Memory_free(NULL, edma_cfg->qchans,
			edma_cfg->num_qchans * sizeof(QChan_Options));
	edma_cfg->qchans = NULL;
	Memory_free(NULL, edma_cfg, sizeof(EDMA_Config));
	*p_edma_cfg = NULL;
}
//EDMA_getDMAChan(edma, EDMA3_DRV_DMA_CHANNEL_ANY, EDMA3_DRV_TCC_ANY, 0, (EDMA3_RM_TccCallback) cbckFunc, NULL);
UInt EDMA_getDMAChan(EDMA_Config *edma_cfg, UInt reqChanNumber,
		UInt reqTccNumber, UInt reqEvtQNumber, EDMA3_RM_TccCallback callback,
		void *callback_data) {
	if (reqChanNumber > edma_cfg->num_channels) {
		reqChanNumber = EDMA3_DRV_DMA_CHANNEL_ANY;
	}
	if (reqTccNumber > edma_cfg->num_channels) {
		reqTccNumber = EDMA3_DRV_TCC_ANY;
	}
	if (reqEvtQNumber > edma_cfg->num_evtqs) {
		reqEvtQNumber = edma_cfg->num_evtqs - 1;
	}
	/* brief DMA / QDMA / Link 채널을 요청합니다.
	 * EDMA3_DRV_Result EDMA3_DRV_requestChannel (EDMA3_DRV_Handle hEdma, //eDma드라이버 인스턴스에 대한 핸들러
                                    uint32_t *pLCh, // 요청된 논리 채널 ID, EDMA3_DRV_DMA_CHANNEL_ANY:이벤트 매핑이없는 DMA 마스터 채널을 요청합니다.
                                    uint32_t *pTcc, // 완료/오류 인터럽트가 생성된 채널 번호, EDMA3_DRV_TCC_ANY 채널 매핑이없는 TCC를 요청합니다.
                                    EDMA3_RM_EventQueue evtQueue, //채널이 매핑될 이벤트 대기열 번호 (마스터 채널 (DMA/QDMA) 요청에 대해서만 유효 함)
                                    EDMA3_RM_TccCallback tccCb, //TCC callback- "Event Miss Error"또는 "Transfer Complete"와 같은 채널 별 이벤트를 처리합니다.
                                    void *cbData // tccCb 콜백 함수로 직접 전달 될 데이터
                                    ); */
	EDMA3_DRV_requestChannel(edma_cfg->edma_handle, &reqChanNumber, &reqTccNumber, reqEvtQNumber, callback, callback_data);
	Channel_Options *chopt = edma_cfg->channels + reqChanNumber;
/* 논리 채널과 관련된 PaRAM Set Physical Address 가져 오기
 * EDMA3_DRV_Result EDMA3_DRV_getPaRAMPhyAddr(EDMA3_DRV_Handle hEdma,  //eDma드라이버 인스턴스에 대한 핸들러
                    uint32_t lCh,   // PaRAM 세트 물리적 주소가 필요한 논리적 채널
                    uint32_t *paramPhyAddr // PaRAM 설정된 물리적 주소가 여기에 반환됩니다.
                    );  */
	EDMA3_DRV_getPaRAMPhyAddr(edma_cfg->edma_handle, reqChanNumber, &chopt->init_param_set);
	chopt->init_param_set &= 0x3FFFu;
	chopt->init_param_set >>= 5;
	chopt->assoc_evtQ = reqEvtQNumber;
	chopt->assoc_tcc = reqTccNumber;
	chopt->callback = callback;
	chopt->callback_data = callback_data;
	chopt->channel_id = reqChanNumber;
	chopt->isAllocated = TRUE;
	edma_cfg->links[chopt->init_param_set].isValid = FALSE;
	edma_cfg->links[chopt->init_param_set].isAllocated = TRUE;
	edma_cfg->links[chopt->init_param_set].param_id = chopt->init_param_set;
	return reqChanNumber;
}

UInt EDMA_getLink(EDMA_Config *edma_cfg, UInt reqLinkNumber, UInt reqTccNumber,
		UInt reqEvtQNumber, EDMA3_RM_TccCallback callback, void *callback_data) {
	if (reqLinkNumber < edma_cfg->num_channels
			|| reqLinkNumber == EDMA3_DRV_DMA_CHANNEL_ANY
			|| reqLinkNumber == EDMA3_DRV_QDMA_CHANNEL_ANY
			|| (reqLinkNumber >= EDMA3_DRV_QDMA_CHANNEL_0
					&& reqLinkNumber <= EDMA3_DRV_QDMA_CHANNEL_7)) {
		System_printf(
				"Error: tried to use EDMA_getLink(...) to request a PaRAM set associated with a physical channel.  \
				Make sure to use a value above %i for channel controller %i",
				edma_cfg->num_channels, edma_cfg->CC_num);
		System_exit(1);
	}
	if (reqLinkNumber - edma_cfg->num_channels > edma_cfg->num_params) {
		if (reqTccNumber
				> edma_cfg->num_channels&& reqLinkNumber != EDMA3_DRV_LINK_CHANNEL_WITH_TCC) {
			reqLinkNumber = EDMA3_DRV_LINK_CHANNEL;
		} else {
			reqLinkNumber = EDMA3_DRV_LINK_CHANNEL_WITH_TCC;
		}
	}
	if (reqTccNumber > edma_cfg->num_channels) {
		reqTccNumber = EDMA3_DRV_TCC_ANY;
	}
	if (reqEvtQNumber > edma_cfg->num_evtqs) {
		reqEvtQNumber = edma_cfg->num_evtqs - 1;
	}
	EDMA3_DRV_requestChannel(edma_cfg->edma_handle, &reqLinkNumber,
			&reqTccNumber, reqEvtQNumber, callback, callback_data);
	UInt prm_id;
	EDMA3_DRV_getPaRAMPhyAddr(edma_cfg->edma_handle, reqLinkNumber, &prm_id);
	prm_id &= 0x3FFFu;
	prm_id >>= 5;
	PaRAM_Options *propt = edma_cfg->links + prm_id;
	propt->param_id = prm_id;
	propt->isValid = FALSE;
	propt->isAllocated = TRUE;
	propt->src.circSz = EDMA3_DRV_W8BIT;
	propt->dst.circSz = EDMA3_DRV_W8BIT;
	return prm_id;
}

//TODO: Complete
UInt EDMA_getQDMAChan(EDMA_Config *edma_cfg, UInt reqChanNumber,
		UInt reqTccNumber, UInt reqEvtQNumber, EDMA3_RM_TccCallback callback,
		void *callback_data) {
	if (reqChanNumber != EDMA3_DRV_QDMA_CHANNEL_ANY
			&& (reqChanNumber < EDMA3_DRV_QDMA_CHANNEL_0
					|| reqChanNumber > EDMA3_DRV_QDMA_CHANNEL_7)) {
		System_printf(
				"Error: getQDMAChan(...) used on something other than a QDMA channel");
	}
	if (reqTccNumber > edma_cfg->num_channels) {
		reqTccNumber = EDMA3_DRV_TCC_ANY;
	}
	if (reqEvtQNumber > edma_cfg->num_evtqs) {
		reqEvtQNumber = edma_cfg->num_evtqs - 1;
	}
	EDMA3_DRV_requestChannel(edma_cfg->edma_handle, &reqChanNumber,
			&reqTccNumber, reqEvtQNumber, callback, callback_data);
	if (edma_cfg->qdma_base == 0) {
		edma_cfg->qdma_base = reqChanNumber & 0xFFFFFFF8;
	}
	QChan_Options *qcopt = edma_cfg->qchans
			+ (reqChanNumber - edma_cfg->qdma_base);

	EDMA3_DRV_getPaRAMPhyAddr(edma_cfg->edma_handle, reqChanNumber,
			&qcopt->init_param_set);
	qcopt->init_param_set &= 0x3FFFu;
	qcopt->init_param_set >>= 5;
	qcopt->assoc_evtQ = reqEvtQNumber;
	qcopt->assoc_tcc = reqTccNumber;
	qcopt->callback = callback;
	qcopt->callback_data = callback_data;
	qcopt->channel_id = reqChanNumber;
	qcopt->isAllocated = TRUE;
	PaRAM_Options *propt = edma_cfg->links + qcopt->init_param_set;
	propt->isAllocated = TRUE;
	propt->isValid = FALSE;
	propt->param_id = qcopt->init_param_set;
	EDMA3_DRV_setQdmaTrigWord(edma_cfg->edma_handle, reqChanNumber,
			EDMA3_RM_QDMA_TRIG_CCNT);
	return reqChanNumber;
}

//TODO: Complete
void EDMA_startQDMA(EDMA_Config *edma_cfg, UInt qdNum) {
	if (!edma_cfg->qchans[qdNum - edma_cfg->qdma_base].isAllocated) {
		System_printf("Error: QDMA channel not allocated\n");
		System_exit(1);
	}
	PaRAM_Options *propt = edma_cfg->links
			+ edma_cfg->qchans[qdNum - edma_cfg->qdma_base].init_param_set;
	if (!propt->isAllocated || !propt->isValid) {
		System_printf("Error: QDMA PaRAM set not allocated (?) or not valid\n");
		System_exit(1);
	}
	EDMA3_DRV_setSrcParams(edma_cfg->edma_handle, qdNum, propt->src.address,
			propt->src.mode, propt->src.circSz);
	EDMA3_DRV_setDestParams(edma_cfg->edma_handle, qdNum, propt->dst.address,
			propt->dst.mode, propt->dst.circSz);
	EDMA3_DRV_setSrcIndex(edma_cfg->edma_handle, qdNum, propt->src.array_strobe,
			propt->src.frame_strobe);
	EDMA3_DRV_setDestIndex(edma_cfg->edma_handle, qdNum,
			propt->dst.array_strobe, propt->dst.frame_strobe);
	EDMA3_DRV_setOptField(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_OPT_FIELD_ITCINTEN, propt->chnint.intermediate_int_en);
	EDMA3_DRV_setOptField(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_OPT_FIELD_TCINTEN, propt->chnint.final_int_en);
	EDMA3_DRV_setOptField(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_OPT_FIELD_ITCCHEN, propt->chnint.intermediate_chain_en);
	EDMA3_DRV_setOptField(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_OPT_FIELD_TCCHEN, propt->chnint.final_chain_en);
	EDMA3_DRV_setOptField(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_OPT_FIELD_TCCMODE, propt->chnint.early_completion);
	if (propt->lnk.isLinking) {
		EDMA3_DRV_linkChannel(edma_cfg->edma_handle, qdNum,
				propt->lnk.link_addr);
		EDMA3_DRV_setOptField(edma_cfg->edma_handle, qdNum,
				EDMA3_DRV_OPT_FIELD_STATIC, 0);
	} else {
		EDMA3_DRV_unlinkChannel(edma_cfg->edma_handle, qdNum);
		EDMA3_DRV_setOptField(edma_cfg->edma_handle, qdNum,
				EDMA3_DRV_OPT_FIELD_STATIC, 1);
	}
	EDMA_writeTransferPaRAMs(edma_cfg, qdNum);
}

//TODO: Complete
void EDMA_writeTransferPaRAMs(EDMA_Config *edma_cfg, UInt qdNum) {
	PaRAM_Options *propt = edma_cfg->links
			+ edma_cfg->qchans[qdNum - edma_cfg->qdma_base].init_param_set;

	UInt abcnt = propt->cpy.array_count
			| ((propt->cpy.frame_count & 0xFFFFu) << 16u);

	EDMA3_DRV_setPaRAMEntry(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_PARAM_ENTRY_ACNT_BCNT, abcnt);

	EDMA3_DRV_setOptField(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_OPT_FIELD_SYNCDIM, propt->cpy.sync_mode);

	UInt bcntReloadLinkField;
	EDMA3_DRV_getPaRAMField(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_PARAM_FIELD_LINKADDR, &bcntReloadLinkField);
	bcntReloadLinkField = ((bcntReloadLinkField & 0x0000FFFFu)
			| (propt->cpy.frame_count_reload << 16));
	EDMA3_DRV_setPaRAMEntry(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_PARAM_ENTRY_LINK_BCNTRLD, bcntReloadLinkField);

	EDMA3_DRV_setPaRAMEntry(edma_cfg->edma_handle, qdNum,
			EDMA3_DRV_PARAM_ENTRY_CCNT, propt->cpy.block_count);
}

void EDMA_programDMAChan(EDMA_Config *edma_cfg, UInt chanNum) {
	if (!edma_cfg->links[chanNum].isAllocated) {
		System_printf("Error: DMA channel not allocated\n");
		System_exit(1);
	}
	EDMA_programLink(edma_cfg, edma_cfg->channels[chanNum].init_param_set);
}

void EDMA_programLink(EDMA_Config *edma_cfg, UInt linkNum) {
	if (!edma_cfg->links[linkNum].isAllocated
			|| !edma_cfg->links[linkNum].isValid) {
		System_printf(
				"Error: Link channel either not allocated or not valid.  \
				Make sure to set the valid byte in the PaRAM_Options!\n");
		System_exit(1);
	}
	PaRAM_Options *propt = edma_cfg->links + linkNum;
	/* 간단한 DMA 소스 매개 변수 설정
	 * EDMA3_DRV_Result EDMA3_DRV_setSrcParams ( EDMA3_DRV_Handle hEdma, //EDMA 드라이버 인스턴스에 대한 핸들
                    uint32_t lCh, // 소스 매개 변수를 구성 할 논리 채널
                    uint32_t srcAddr, //소스 주소
                    EDMA3_DRV_AddrMode addrMode, //주소 모드 [FIFO 또는 Increment]
                    EDMA3_DRV_FifoWidth fifoWidth // FIFO의 너비 (addrMode가 FIFO 인 경우에만 유효 함)
                    );
     */
	EDMA3_DRV_setSrcParams(edma_cfg->edma_handle, linkNum, propt->src.address,
			propt->src.mode, propt->src.circSz);

	EDMA3_DRV_setDestParams(edma_cfg->edma_handle, linkNum, propt->dst.address,
			propt->dst.mode, propt->dst.circSz);
	/* 간단한 DMA 소스 색인 설정 [소스 B 인덱스와 소스 C 인덱스를 프로그래밍하는 데 사용됩니다.]
	 * EDMA3_DRV_Result EDMA3_DRV_setSrcIndex ( EDMA3_DRV_Handle hEdma, //EDMA 드라이버 인스턴스에 대한 핸들
                    uint32_t lCh, //소스에 대한 논리 채널
                    int32_t srcBIdx, //소스 B 인덱스
                    int32_t srcCIdx   //소스 C 인덱스
                    );
	 */
	EDMA3_DRV_setSrcIndex(edma_cfg->edma_handle, linkNum,
			propt->src.array_strobe, propt->src.frame_strobe);

	EDMA3_DRV_setDestIndex(edma_cfg->edma_handle, linkNum,
			propt->dst.array_strobe, propt->dst.frame_strobe);

	/*DMA 전송 매개 변수 설정
	 * EDMA3_DRV_Result EDMA3_DRV_setTransferParams (
                        EDMA3_DRV_Handle hEdma, //EDMA 드라이버 인스턴스에 대한 핸들
                        uint32_t lCh, //전송 매개 변수를 구성 할 논리 채널
                        uint32_t aCnt, //1 차원 계수.
                        uint32_t bCnt, //2 차원 계수.
                        uint32_t cCnt, //3 차원 계수.
                        uint32_t bCntReload, // bCnt 값을 다시로드합니다.
                        EDMA3_DRV_SyncType syncType //전송 동기화 차원   [0:비 동기화. 각 이벤트는 ACNT 바이트의 단일 배열 전송을 트리거합니다, 1:AB 동기화 됨. 각 이벤트는 ACNT 바이트의 BCNT 배열 전송을 트리거합니다]
                        );
	 */
	EDMA3_DRV_setTransferParams(edma_cfg->edma_handle, linkNum,
			propt->cpy.array_count, propt->cpy.frame_count,
			propt->cpy.block_count, propt->cpy.frame_count_reload,
			propt->cpy.sync_mode);
	EDMA3_DRV_ChainOptions chopt;
	chopt.itcchEn = (EDMA3_DRV_ItcchEn) propt->chnint.intermediate_chain_en;
	chopt.tcchEn = (EDMA3_DRV_TcchEn) propt->chnint.final_chain_en;
	chopt.itcintEn = (EDMA3_DRV_ItcintEn) propt->chnint.intermediate_int_en;
	chopt.tcintEn = (EDMA3_DRV_TcintEn) propt->chnint.final_int_en;

	/*두 개의 지정된 채널을 연결합니다.
	 * EDMA3_DRV_Result EDMA3_DRV_chainChannel (EDMA3_DRV_Handle hEdma, //EDMA 드라이버 인스턴스에 대한 핸들입니다.
                                    uint32_t lCh1, //특정 DMA 채널이 연결될 DMA / QDMA 채널.
                                    uint32_t lCh2, //첫 번째 DMA / QDMA 채널에 연결되어야하는 DMA 채널입니다.
                                    const EDMA3_DRV_ChainOptions *chainOptions //중간 인터럽트와 같은 옵션이 필요한지 여부, 중간 / 최종 체인이 활성화되었는지 여부 등.
                                    );
	 */
	EDMA3_DRV_chainChannel(edma_cfg->edma_handle, linkNum,
			propt->chnint.chain_target, &chopt);
	/*논리 채널 'lCh'와 연관된 PaRAM 세트에서 특정 OPT 필드를 설정하십시오.
	 * EDMA3_DRV_Result EDMA3_DRV_setOptField (EDMA3_DRV_Handle hEdma, //EDMA 드라이버 인스턴스에 대한 핸들입니다.
                    uint32_t lCh, //PaRAM set OPT 필드를 설정해야하는 바인딩 된 논리 채널.
                    EDMA3_DRV_OptField optField, //설정이 필요한 OPT Word의 특정 필드
                    uint32_t newOptFieldVal //새 OPT 필드 값
                    );
	 *
	 */
	EDMA3_DRV_setOptField(edma_cfg->edma_handle, linkNum,
			EDMA3_DRV_OPT_FIELD_TCCMODE, propt->chnint.early_completion);
	if (propt->lnk.isLinking) {
	    /*두 개의 논리 채널을 연결합니다.
	     * EDMA3_DRV_Result EDMA3_DRV_linkChannel ( EDMA3_DRV_Handle hEdma, //EDMA 드라이버 인스턴스에 대한 핸들입니다.
                                                uint32_t lCh1, //특정 채널이 링크 될 논리 채널.
                                                uint32_t lCh2 //첫 번째 채널에 연결되어야하는 논리 채널입니다.[ lCh1의 PaRAM 세트를 기반으로 한 전송이 끝나면 lCh2의 PaRAM 세트가 lCh1의 PaRAM 세트로 복사되고 전송이 재개됩니다, DMA 채널의 경우 링크 채널에서 전송을 시작하려면 다른 동기화 이벤트가 필요합니다 ]
                                                );
	     */
		EDMA3_DRV_linkChannel(edma_cfg->edma_handle, linkNum,
				propt->lnk.link_addr);
		EDMA3_DRV_setOptField(edma_cfg->edma_handle, linkNum,
				EDMA3_DRV_OPT_FIELD_STATIC, 0);
	} else {
		EDMA3_DRV_unlinkChannel(edma_cfg->edma_handle, linkNum);
		EDMA3_DRV_setOptField(edma_cfg->edma_handle, linkNum,
				EDMA3_DRV_OPT_FIELD_STATIC, 1);
	}
}

void EDMA_enableChan(EDMA_Config *edma_cfg, UInt chanNum) {
	if (!edma_cfg->links[chanNum].isAllocated) {
		System_printf("Error: DMA channel not allocated\n");
		System_exit(1);
	}
	/*지정된 채널에서 EDMA 전송을 시작합니다.
	 * EDMA3_DRV_Result EDMA3_DRV_enableTransfer (EDMA3_DRV_Handle hEdma, //EDMA 드라이버 인스턴스에 대한 핸들
                        uint32_t lCh, //전송을 시작해야하는 채널
                        EDMA3_DRV_TrigMode trigMode // 전송 시작을 트리거하는 모드 (수동, QDMA 또는 이벤트)
                        );
	 */
	EDMA3_DRV_enableTransfer(edma_cfg->edma_handle, chanNum,
			edma_cfg->channels[chanNum].trig_mode);
}

void EDMA_printRegDMA(EDMA_Config *edma_cfg, UInt chanNum) {
	if (chanNum < edma_cfg->num_channels
			&& !(edma_cfg->channels[chanNum].isAllocated
					&& edma_cfg->links[edma_cfg->channels[chanNum].init_param_set].isAllocated)) {
		System_printf("Error: DMA channel not allocated\n");
		System_exit(1);
	} else if (chanNum >= edma_cfg->num_channels
			&& (chanNum < edma_cfg->qdma_base
					|| chanNum >= edma_cfg->qdma_base + edma_cfg->num_qchans)
			&& !edma_cfg->links[chanNum].isAllocated) {
		System_printf("Error: Link channel not allocated\n");
		System_exit(1);
	}
	UInt result;
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_ACNT_BCNT, &result);
	System_printf("A count: %x,\tB count: %x\n", result & 0xFFFF,
			(result >> 16) & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_CCNT, &result);
	System_printf("C count: %x\n", result & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_LINK_BCNTRLD, &result);
	System_printf("B count reload: %x,\tLink: %x\n", (result >> 16) & 0xFFFF,
			result & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_SRC, &result);
	System_printf("Source address: %x\n", result);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_DST, &result);
	System_printf("Destination address: %x\n", result);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_SRC_DST_BIDX, &result);
	System_printf("Source array strobe: %x,\tDestination array strobe: %x\n",
			result & 0xFFFF, (result >> 16) & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_SRC_DST_CIDX, &result);
	System_printf("Source frame strobe: %x,\tDestination frame strobe: %x\n",
			result & 0xFFFF, (result >> 16) & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_OPT, &result);
	System_printf("Entire OPT field: %x\n", result);
	System_printf("Bit 31: Priv: %x\n", (result >> 31) & 0x1);
	System_printf("Bits 30-28: Reserved\n");
	System_printf("Bits 27-24: PrivID: %x\n", (result >> 24) & 0xF);
	System_printf("Bit 23: Intermediate Chain Enable: %x\n",
			(result >> 23) & 0x1);
	System_printf("Bit 22: Completion Chain Enable: %x\n",
			(result >> 22) & 0x1);
	System_printf("Bit 21: Intermediate Interrupt Enable: %x\n",
			(result >> 21) & 0x1);
	System_printf("Bit 20: Completion Interrupt Enable: %x\n",
			(result >> 20) & 0x1);
	System_printf("Bits 19-18: Reserved\n");
	System_printf("Bits 17-12: Transfer Completion Code: %x\n",
			(result >> 12) & 0x3F);
	System_printf("Bit 11: Transfer Completion Mode: %x\n",
			(result >> 11) & 0x1);
	System_printf("Bits 10-8: Fifo Width: %x\n", (result >> 8) & 0x7);
	System_printf("Bits 7-4: Reserved\n");
	System_printf("Bit 3: Static (linking disabled): %x\n",
			(result >> 3) & 0x1);
	System_printf("Bit 2: Synchronisation Dimension: %x\n",
			(result >> 2) & 0x1);
	System_printf("Bit 1: Destination Addressing Mode: %x\n",
			(result >> 1) & 0x1);
	System_printf("Bit 0: Source Addressing Mode: %x\n", result & 0x1);
 
}

void EDMA_printRegQDMA(EDMA_Config *edma_cfg, UInt chanNum) {
	if (chanNum >= edma_cfg->qdma_base
			&& chanNum < edma_cfg->qdma_base + edma_cfg->num_qchans
			&& !(edma_cfg->qchans[chanNum - edma_cfg->qdma_base].isAllocated
					&& edma_cfg->links[edma_cfg->qchans[chanNum
							- edma_cfg->qdma_base].init_param_set].isAllocated)) {
		System_printf("Error: QDMA channel not allocated");
		System_exit(1);
	} else if (chanNum >= edma_cfg->num_channels
			&& (chanNum < edma_cfg->qdma_base
					|| chanNum >= edma_cfg->qdma_base + edma_cfg->num_qchans)
			&& !edma_cfg->links[chanNum].isAllocated) {
		System_printf("Error: Link channel not allocated\n");
		System_exit(1);
	}
	UInt result;
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_ACNT_BCNT, &result);
	System_printf("A count: %x,\tB count: %x\n", result & 0xFFFF,
			(result >> 16) & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_CCNT, &result);
	System_printf("C count: %x\n", result & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_LINK_BCNTRLD, &result);
	System_printf("B count reload: %x,\tLink: %x\n", (result >> 16) & 0xFFFF,
			result & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_SRC, &result);
	System_printf("Source address: %x\n", result);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_DST, &result);
	System_printf("Destination address: %x\n", result);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_SRC_DST_BIDX, &result);
	System_printf("Source array strobe: %x,\tDestination array strobe: %x\n",
			result & 0xFFFF, (result >> 16) & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_SRC_DST_CIDX, &result);
	System_printf("Source frame strobe: %x,\tDestination frame strobe: %x\n",
			result & 0xFFFF, (result >> 16) & 0xFFFF);
	EDMA3_DRV_getPaRAMEntry(edma_cfg->edma_handle, chanNum,
			EDMA3_DRV_PARAM_ENTRY_OPT, &result);
	System_printf("Entire OPT field: %x\n", result);
	System_printf("Bit 31: Priv: %x\n", (result >> 31) & 0x1);
	System_printf("Bits 30-28: Reserved\n");
	System_printf("Bits 27-24: PrivID: %x\n", (result >> 24) & 0xF);
	System_printf("Bit 23: Intermediate Chain Enable: %x\n",
			(result >> 23) & 0x1);
	System_printf("Bit 22: Completion Chain Enable: %x\n",
			(result >> 22) & 0x1);
	System_printf("Bit 21: Intermediate Interrupt Enable: %x\n",
			(result >> 21) & 0x1);
	System_printf("Bit 20: Completion Interrupt Enable: %x\n",
			(result >> 20) & 0x1);
	System_printf("Bits 19-18: Reserved\n");
	System_printf("Bits 17-12: Transfer Completion Code: %x\n",
			(result >> 12) & 0x3F);
	System_printf("Bit 11: Transfer Completion Mode: %x\n",
			(result >> 11) & 0x1);
	System_printf("Bits 10-8: Fifo Width: %x\n", (result >> 8) & 0x7);
	System_printf("Bits 7-4: Reserved\n");
	System_printf("Bit 3: Static (linking disabled): %x\n",
			(result >> 3) & 0x1);
	System_printf("Bit 2: Synchronisation Dimension: %x\n",
			(result >> 2) & 0x1);
	System_printf("Bit 1: Destination Addressing Mode: %x\n",
			(result >> 1) & 0x1);
	System_printf("Bit 0: Source Addressing Mode: %x\n", result & 0x1);
	System_printf("Press any key to continue...\n");
	getchar();
}
