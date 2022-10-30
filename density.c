/*
 *
 *       Density module for Epson Colour v46 driver.
 *
 */

#include "global.h"

int SetDensity(ULONG density_code)
{
        extern struct PrinterData *PD;
        extern struct PrinterExtendedData *PED;

        /* SPECIAL_DENSITY     0    1    2    3    4    5    6    7 */
        static int XDPI[8] = {180, 180, 360, 360, 720, 720, 720, 720};
        static int YDPI[8] = {180, 180, 360, 360, 720, 720, 720, 720};

        /* Unit size for ESC(U command for YDPI */
        static int codes[8] = {20, 20, 10, 10, 5, 5, 5, 5};

        PED->ped_MaxColumns = 80;
        density_code /= SPECIAL_DENSITY1;
        
        /* default is 80 chars (8.0 in.) */

        PED->ped_MaxXDots = (XDPI[density_code] * PED->ped_MaxColumns) / 10;
        PED->ped_MaxYDots = PD->pd_Preferences.PaperSize == US_LEGAL ? 14 : 10;
        PED->ped_MaxYDots *= XDPI[density_code];

        PED->ped_XDotsInch = XDPI[density_code];
        PED->ped_YDotsInch = YDPI[density_code];
        PED->ped_NumRows = 1;
        
        return(codes[density_code]);
}
