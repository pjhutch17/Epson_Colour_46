/*
 *
 *       DoSpecial for Stylus_Stylus_Color v52 driver.
 */

#include "global.h"

#define LMARG   3
#define RMARG   6
#define MARGLEN 8

#define QUALITY         33
#define LPI             35
#define TOPMARG         47
#define BOTMARG         49
#define CHARSET         53
#define TYPEFACE        56
#define INITLEN         32

/* Version string */

const char __ver[42] = "$VER: Epson_Stylus_Color 52.00 (26.10.22)";

/* ESC/P2 Special cases */

extern struct PrinterData *PD;
extern struct PrinterExtendedData *PED;

int DoSpecial(UWORD *command, char outputBuffer[], BYTE *vline, BYTE *currentVMI, BYTE *crlfFlag, UBYTE Parms[])
{
        int x = 0, y = 0, papersize, lpi;
        int topmarg, botmarg;
        /*
                00-00   \375    wait
                01-03   \033lL  set left margin
                04-06   \033Qq  set right margin
                07-07   \375    wait
        */
        static char initMarg[MARGLEN+1] = "\375\033lL\033Qq\375";
        /*

                00-01   \033@              Init Printer
                02-04   \033r\000          Set colour to black
                05-07   \033x\000          draft (QUALITY)
                08-09   \0332              6 lpi (LPI)
                10-15   \033(U\001\000\024 Set defined units to 20
                16-24   \033(c\004\000ttbb Set page length 
                25-27   \033R\000          Set Intl char set to US
                28-30   \033k\000          Set typeface (Roman)
                31-31   \015               carriage return
        */
         static char initThisPrinter[INITLEN+1] =
         "\033@\033r\000\033x\000\0332\033(U\001\000\012\033(c\004\000ttbb\033R\000\033k\000\015";
        static BYTE ISOcolorTable[10] = {0, 5, 6, 4, 3, 1, 2, 0};

        if (*command == aRIN) {    /* Initialise */
                while (x < INITLEN) {
                        outputBuffer[x] = initThisPrinter[x];
                        x++;
                }

               if (PD->pd_Preferences.PrintQuality == LETTER) {
                        outputBuffer[QUALITY] = 1;
                }

                *currentVMI = 36; /* assume 1/6 line spacing (36/216 => 1/6) */
                lpi = 6;
                if (PD->pd_Preferences.PrintSpacing == EIGHT_LPI) {
                        outputBuffer[LPI] = '0';
                        lpi = 8;
                        *currentVMI = 27; /* 27/216 => 1/8 */
                }

                papersize = PD->pd_Preferences.PaperSize;
		switch (papersize) 
		 {
		  case US_LETTER:
		       papersize = 10 * lpi;
		       break;
		  case US_LEGAL:
		       papersize = 13 * lpi;
		       break;
		  case EURO_A4:
		       papersize = (lpi == 8 ? 85 : 65);
		       break;
		  default:   /* Custom size */
		       papersize = PD->pd_Preferences.PaperLength;
                 }
                topmarg = (1 / lpi * 3600) / 20; /* 1 line */
                botmarg = (papersize / lpi * 3600) / 20;
                               
                outputBuffer[TOPMARG] = topmarg & 0xff;
                outputBuffer[TOPMARG+1] = topmarg >> 8;
                outputBuffer[BOTMARG] = botmarg & 0xff;
                outputBuffer[BOTMARG+1] = botmarg >> 8;

                Parms[0] = PD->pd_Preferences.PrintLeftMargin;
                Parms[1] = PD->pd_Preferences.PrintRightMargin;
                *command = aSLRM;  /* L&R margins */
        }

        if (*command == aCAM) { /* cancel margins */
                y = PD->pd_Preferences.PaperSize == W_TRACTOR ? 136 : 80;
                if (PD->pd_Preferences.PrintPitch == PICA) {
                        Parms[1] = (10 * y) / 10;
                }
                else if (PD->pd_Preferences.PrintPitch == ELITE) {
                        Parms[1] = (12 * y) / 10;
                }
                else { /* fine */
                        Parms[1] = (17 * y) / 10;
                }
                Parms[0] = 1;
                y = 0;
                *command = aSLRM;
        }

        if (*command == aSLRM) { /* set left and right margins */
                PD->pd_PWaitEnabled = 253;
                if (Parms[0] == 0) {
                        initMarg[LMARG] = 0;
                }
                else {
                        initMarg[LMARG] = Parms[0] - 1;
                }
                initMarg[RMARG] = Parms[1];
                while (y < MARGLEN) {
                        outputBuffer[x++] = initMarg[y++];
                }
                return(x);
        }

        if (*command == aPLU) {     /* Partial line up */
                if (*vline == 0) {
                        *vline = 1;
                        *command = aSUS2; /* Superscript on */
                        return(0);
                }
                if (*vline < 0) {
                        *vline = 0;
                        *command = aSUS3; /* Subscript on */
                        return(0);
                }
                return(-1);
        }

        if (*command == aPLD) {     /* Partial line down */
                if (*vline == 0) {
                        *vline = -1;
                        *command = aSUS4; /* Subscript off */
                        return(0);
                }
                if (*vline > 0) {
                        *vline = 0;
                        *command = aSUS1; /* Superscript off */
                        return(0);
                }
                return(-1);
        }

        if (*command == aSUS0) { /* Normalise the line */
                *vline = 0;
        }
        if (*command == aSUS1) { /* Superscript off */
                *vline = 0;
        }
        if (*command == aSUS2) { /* Superscript on */
                *vline = 1;
        }
        if (*command == aSUS3) { /* Subcript on */
                *vline = 0;
        }
        if (*command == aSUS4) { /* Suscript off */
                *vline = -1;
        }

        if (*command == aVERP0) { /* 1/8" line spacing */
                *currentVMI = 27;
        }

        if (*command == aVERP1) { /* 1/6" line spacing */
                *currentVMI = 36;
        }

        if (*command == aIND) { /* lf */
                outputBuffer[x++] = '\033';
                outputBuffer[x++] = 'J';
                outputBuffer[x++] = *currentVMI;
                return(x);
        }

        if (*command == aSFC) { /* Set foreground colour */
                if (Parms[0] == 39) {
                        Parms[0] = 30; /* set defaults */
                }
                if (Parms[0] > 37) {
                        return(0); /* ni or background color change */
                }
                outputBuffer[x++] = '\033';
                outputBuffer[x++] = 'r';
                outputBuffer[x++] = ISOcolorTable[Parms[0] - 30];
                outputBuffer[x++] = '\033';
                outputBuffer[x++] = 't';
                outputBuffer[x++] = 0;
                return(x);
        }
        
        if (*command == aSLPP) {   /* Set form length */
               outputBuffer[x++] = '\033';
               outputBuffer[x++] = 'C';
               outputBuffer[x++] = PD->pd_Preferences.PaperLength;
               return(x);
        }

        if (*command == aRIS) {
                PD->pd_PWaitEnabled = 253;
        }

        return(0);
}

int ConvFunc(char *buf, char c, int flag)
/* expand lf into lf/cr flag (0-yes, else no ) */
{
        if (c == '\014') { /* if formfeed (page eject) */
                PED->ped_PrintMode = 0; /* no data to print */
        }
        return(-1); /* pass all chars back to the printer device */
}


int Close(ior)
union printerIO *ior;
{
        if (PED->ped_PrintMode) { /* if data has been printed */
                (*(PD->pd_PWrite))("\014",1); /* eject page */
                (*(PD->pd_PBothReady))(); /* wait for it to finish */
                PED->ped_PrintMode = 0; /* no data to print */
        }
        return(0);
}
