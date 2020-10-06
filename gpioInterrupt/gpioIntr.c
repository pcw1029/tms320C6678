/*
 *  ======== main.c ========
 */

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/platform/evmc6678l/platform_lib/include/evmc66x_gpio.h>
#include <ti/sysbios/knl/Clock.h>
#include <xdc/cfg/global.h>
#define ON      1
#define OFF     0

int gpio12IntrOccursFlag;
int gpio13IntrOccursFlag;
int g_Inter14;
int g_Inter15;

void gpio14Interrupt(UArg arg)
{
    g_Inter14++;
    if(g_Inter14 >= 1000)
        g_Inter14 = 0;
}

void gpio15Interrupt(UArg arg)
{
    g_Inter15++;
    if(g_Inter15 >= 1000)
        g_Inter15 = 0;
}

void hwiCombinGpio14Gpio15(UArg arg)
{
    System_printf("Gpio14 또는 Gpio15 트리거 발생으로 인해 이베트 88 또는 89 핸들링\n");
    System_printf("GPIO14 Interrupt count : %d, GPIO15 Interrupt count : %d\n", g_Inter14, g_Inter15);
}


void gpio13InterruptOccursEveryCycle()
{
    if(gpio13IntrOccursFlag == ON){
        gpioSetOutput(GPIO_13);
        gpio13IntrOccursFlag = OFF;
    }else{
        gpioClearOutput(GPIO_13);
        gpio13IntrOccursFlag = ON;
    }
}

void gpioInterruptPinSettings()
{
    gpioDisableGlobalInterrupt();
    gpioInit();

    gpioSetDirection(GPIO_13, GPIO_OUT);
    gpioClearOutput(GPIO_13);
    System_printf("GPIO#13핀 인터럽트 발생 신호원 설정\n");

    //GPIO#14, #15를 인터럽트로 사용하므로 방향을 IN으로 설정
    gpioSetDirection(GPIO_14, GPIO_IN);
    gpioSetDirection(GPIO_15, GPIO_IN);

    //GPIO#14, #15를 RisingEdge로 인터럽트 등록
    gpioSetRisingEdgeInterrupt(GPIO_14);
    gpioSetRisingEdgeInterrupt(GPIO_15);
    gpioEnableGlobalInterrupt();

    System_printf("GPIO#14, #15핀 인터럽트 설정 완료\n");
}

void iDleFx()
{
    if(gpio13IntrOccursFlag == ON){
        System_flush();
    }
}

/*
 *  ======== main ========
 */
Int main()
{ 
    g_Inter14 = 0;
    g_Inter15 = 0;
    gpio12IntrOccursFlag = ON;
    gpio13IntrOccursFlag = ON;
    gpioInterruptPinSettings();
    System_flush(); // print the buffer that was set in the hwi functions
    BIOS_start();    /* does not return */
    return(0);
}
