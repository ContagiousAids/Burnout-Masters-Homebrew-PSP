#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>

PSP_MODULE_INFO("BM PSP Diagnostic", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(void)
{
    SceCtrlData pad;

    pspDebugScreenInit();
    pspDebugScreenClear();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenPrintf("BURNOUT MASTERS PSP");
    pspDebugScreenSetXY(2, 4);
    pspDebugScreenPrintf("STARTUP DIAGNOSTIC");
    pspDebugScreenSetXY(2, 7);
    pspDebugScreenPrintf("If you can read this,");
    pspDebugScreenSetXY(2, 8);
    pspDebugScreenPrintf("the PSP program has started.");
    pspDebugScreenSetXY(2, 11);
    pspDebugScreenPrintf("Press buttons to test input.");
    pspDebugScreenSetXY(2, 14);
    pspDebugScreenPrintf("TRIANGLE = Exit");

    while (1)
    {
        sceCtrlReadBufferPositive(&pad, 1);

        pspDebugScreenSetXY(2, 17);
        pspDebugScreenPrintf("Button data: %04X     ", pad.Buttons);

        if (pad.Buttons & PSP_CTRL_TRIANGLE)
            break;

        sceDisplayWaitVblankStart();
    }

    sceKernelExitGame();
    return 0;
}
