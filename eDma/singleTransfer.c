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
    propt->src.mode = EDMA3_DRV_ADDR_MODE_INCR;                 //�ҽ��� ���� ���� (ǥ��) �ּ� ���� ��� ���
    propt->src.array_strobe = 0;                                //�迭 ���� ������ �Ÿ��� �ҽ��� ��� 0 ����Ʈ�Դϴ�.
    propt->src.frame_strobe = 0;                                //������ ���� ������ �Ÿ��� �ҽ��� ��� 0 ����Ʈ�Դϴ�.
    propt->dst.address = (UInt) dst_address;                    //Start address of destination data
    propt->dst.mode = EDMA3_DRV_ADDR_MODE_INCR;                 //dest�� ���� (ǥ��) �ּ� ���� ��� ���
    propt->dst.array_strobe = 0;                                //�迭 ���� ������ �Ÿ��� dest�� ��� 0 ����Ʈ�Դϴ�.
    propt->dst.frame_strobe = 0;                                //������ ���� ������ �Ÿ��� dest�� ��� 0 ����Ʈ�Դϴ�.
    propt->cpy.array_count = ARRAY_SIZE * sizeof(char);          //�迭�� ����Ʈ ���� ARRAY_SIZE * sizeof (int)�Դϴ�.
    propt->cpy.frame_count = 1;                                 //ù ��° ������ (A-Sync) �Ǵ� ��� ������ (AB-Sync)�� ��� 1 ��
    propt->cpy.frame_count_reload = 1;                          //�ļ� �������� ��� 1 �� (A-Sync)
    propt->cpy.block_count = 1;                                 //�� ����� ������ 1 ��
    propt->cpy.sync_mode = EDMA3_DRV_SYNC_A;                    //������ �� �̺�Ʈ �ʿ� (A-Sync)
    propt->chnint.chain_target = 0;                             //Ȱ��ȭ �� ��� ä�� 0�� ü��
    propt->chnint.early_completion = FALSE;                     //������ ���簡 �Ϸ�� ��쿡�� ü�� / �ߴ�
    propt->chnint.final_chain_en = FALSE;                       //��� �Ϸ�� �������� ���ʽÿ�.
    propt->chnint.final_int_en = TRUE;                          //��� �Ϸ�� �ߴ�
    propt->chnint.intermediate_chain_en = FALSE;                //���� ������ �Ϸᰡ �ƴ� ��� �������� ���ʽÿ�.
    propt->chnint.intermediate_int_en = FALSE;                  //�� ���� ������ �Ϸ�� �ߴ����� ���ʽÿ�.
    propt->lnk.isLinking = FALSE;                               //�ٸ� ��Ʈ (��ũ ä��)���� PaRAM ��Ʈ�� �ٽ÷ε����� ���ʽÿ�.
    propt->isValid = TRUE;                                      //���� �ɼ��� ��� ��ȿ�մϴ�.
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


