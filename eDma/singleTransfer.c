/*
 * singleTransfer.c
 *
 *  Created on: 2020. 9. 29.
 *      Author: pcw10
 */

#include <xdc/cfg/global.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/runtime/System.h>
#include <ti/sdo/edma3/drv/edma3_drv.h>
#include <ti/sdo/edma3/rm/edma3_common.h>
#include <ti/sdo/edma3/rm/edma3_rm.h>
#include <xdc/runtime/knl/Cache.h>

#include "EDMA_HLD.h"

#define ARRAY_SIZE 26 // this is the number of transfers
#define LINK_NULL_SET 0xFFFF //Link to the null set
#pragma DATA_ALIGN(src_address, 128)
#pragma DATA_ALIGN(dst_address, 128)
#pragma DATA_SECTION(src_address, "SRC_DDR3")
#pragma DATA_SECTION(dst_address, "DST_MSMCSRAM")
char src_address[ARRAY_SIZE];
char dst_address[ARRAY_SIZE];

extern void registerEdma3Interrupts(unsigned int);
extern EDMA3_DRV_Result edma3OsSemCreate(int, const Semaphore_Params *, EDMA3_OS_Sem_Handle *);
extern EDMA3_DRV_Handle edma3init(unsigned int, EDMA3_DRV_Result *);
extern EDMA3_DRV_Result Edma3_CacheFlush(unsigned int, unsigned int);
extern EDMA3_DRV_Result Edma3_CacheInvalidate(unsigned int, unsigned int);

void cbckFunc(uint32_t tcc, EDMA3_RM_TccStatus status, void *appData) {
    if (status != EDMA3_RM_XFER_COMPLETE) {
        System_abort("Error in transfer");
    }
    Semaphore_post(semaphore0);
}

void tasking(UArg a1, UArg a2) {
    System_printf("Started task\n");
    int count;

    //Print the contents of the Source and the destination
    memset(src_address, '\0', sizeof(src_address));

    for (count = 0; count < ARRAY_SIZE; count++) {
        src_address[count] = 0x41 + count;
        dst_address[count] = 0x00;
        System_printf("%d. Source String : [%c]\n", count, src_address[count]);
    }
    System_printf("Source String : [%s][%d]\n", src_address, sizeof(src_address));

    Cache_wb(src_address, ARRAY_SIZE * sizeof(char), TRUE, NULL);
    Cache_wb(dst_address, ARRAY_SIZE * sizeof(char), TRUE, NULL);

    EDMA_Config *edma;
    EDMA_Init(&edma, 0);
    UInt channel = EDMA_getDMAChan(edma, EDMA3_DRV_DMA_CHANNEL_ANY, EDMA3_DRV_TCC_ANY, 0, (EDMA3_RM_TccCallback) cbckFunc, NULL);
    Channel_Options *chopt = edma->channels + channel;
    PaRAM_Options *propt = edma->links + chopt->init_param_set;
    chopt->trig_mode = EDMA3_DRV_TRIG_MODE_MANUAL;              //Trigger the EDMA Manually (CPU)
    propt->src.address = (UInt) src_address;                    //Start address of source data
    propt->src.mode = EDMA3_DRV_ADDR_MODE_INCR;                 //소스에 대해 증분 (표준) 주소 지정 모드 사용
    propt->src.array_strobe = 0;                                //배열 시작 사이의 거리는 소스의 경우 0 바이트입니다.
    propt->src.frame_strobe = 0;                                //프레임 시작 사이의 거리는 소스의 경우 0 바이트입니다.
    propt->dst.address = (UInt) dst_address;                    //Start address of destination data
    propt->dst.mode = EDMA3_DRV_ADDR_MODE_INCR;                 //dest에 증분 (표준) 주소 지정 모드 사용
    propt->dst.array_strobe = 0;                                //배열 시작 사이의 거리는 dest의 경우 0 바이트입니다.
    propt->dst.frame_strobe = 0;                                //프레임 시작 사이의 거리는 dest의 경우 0 바이트입니다.
    propt->cpy.array_count = ARRAY_SIZE * sizeof(char);          //배열의 바이트 수는 ARRAY_SIZE * sizeof (int)입니다.
    propt->cpy.frame_count = 1;                                 //첫 번째 프레임 (A-Sync) 또는 모든 프레임 (AB-Sync)의 어레이 1 개
    propt->cpy.frame_count_reload = 1;                          //후속 프레임의 어레이 1 개 (A-Sync)
    propt->cpy.block_count = 1;                                 //한 블록의 프레임 1 개
    propt->cpy.sync_mode = EDMA3_DRV_SYNC_A;                    //프레임 당 이벤트 필요 (A-Sync)
    propt->chnint.chain_target = 0;                             //활성화 된 경우 채널 0에 체인
    propt->chnint.early_completion = FALSE;                     //데이터 복사가 완료된 경우에만 체인 / 중단
    propt->chnint.final_chain_en = FALSE;                       //블록 완료시 연결하지 마십시오.
    propt->chnint.final_int_en = TRUE;                          //블록 완료시 중단
    propt->chnint.intermediate_chain_en = FALSE;                //최종 프레임 완료가 아닌 경우 연결하지 마십시오.
    propt->chnint.intermediate_int_en = FALSE;                  //비 최종 프레임 완료시 중단하지 마십시오.
    propt->lnk.isLinking = FALSE;                               //다른 세트 (링크 채널)에서 PaRAM 세트를 다시로드하지 마십시오.
    propt->isValid = TRUE;                                      //이제 옵션이 모두 유효합니다.
    EDMA_programDMAChan(edma, channel);                         //Program the DMA channel
    EDMA_printRegDMA(edma, channel);                            //Print out all registers
    EDMA_enableChan(edma, channel);                             //Enable (and due to manual mode, trigger) the DMA channel.

    System_printf("Release the semaphore\n");
    Semaphore_pend(semaphore0, BIOS_WAIT_FOREVER);
    System_printf("do a Cache_inv\n");
    Cache_inv(dst_address, ARRAY_SIZE * sizeof(char), TRUE, NULL);

    //Print the contents of the Source and the destination
    System_printf("Print the contents of both source and destination\n");
    System_printf("Source String : [%s]\n", dst_address);

    EDMA_UnInit(&edma);

    BIOS_exit(0);
}

void main(int argc, char **argv) {
    BIOS_start();
}


