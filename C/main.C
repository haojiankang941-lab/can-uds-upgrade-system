/**
 ******************************************************************************
 * @file    main.C
 * @brief   Bootloader entry (CAN / ISO-TP / UDS)
 * @note    Power-on -> Boot at 0x08D00000:
 *         1. Initialize clock, GPIO, UART (debug).
 *         2. If INFO.request_flag is set, or APP is not valid, start the
 *            CAN/UDS server and wait for a standard UDS download.
 *         3. Otherwise jump directly to APP.
 ******************************************************************************
 */

#include "main.h"
#include "boot.h"
#include "ota_flash.h"
#include "iap.h"
#include "W1251.h"
#include "boot_can.h"
#include "isotp.h"
#include "uds.h"

/**
 * @brief  Set the vector table to the boot region.
 */
void SystemInit(void)
{
    SCB->VTOR = 0x08D00000UL;
}

/**
 * @brief  Bootloader main entry.
 */
int main(void)
{
    BootCan_FrameTypeDef frame;

    Boot_Init();

    /* Enter UDS mode when APP requested an upgrade, or when there is no
     * valid APP to jump to (recovery). */
    if ((OTA_CheckRequest() != 0U) || (OTA_IsAppValid() == 0U))
    {
        BootCan_Init();
        IsoTp_Init();
        Uds_Init();

        for (;;)
        {
            while (BootCan_Poll(&frame) != 0U)
            {
                IsoTp_RxFrame(&frame);
            }

            IsoTp_Poll(BootCan_GetMs());
            Uds_Poll(BootCan_GetMs());
        }
    }

    Jump_To_App();

    while (1)
    {
        ;
    }
}
