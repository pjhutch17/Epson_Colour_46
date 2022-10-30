/*
 *       -- Render.c --
 *       Epson Colour driver v46
 *       Uses ESC/P2 Std Raster Graphics from 360 to 720 dpi.
 *       P.Hutchison 2009
 */

#include "global.h"

#define NUMSTARTCMD     11      /* # of cmd bytes before binary data */
#define NUMENDCMD       1       /* # of cmd bytes after binary data */
#define NUMTOTALCMD     (NUMSTARTCMD + NUMENDCMD)       /* total of above */
#define NUMLFCMD        7       /* # of cmd bytes for linefeed */
#define MAXCOLORBUFS    4       /* max # of color buffers */

#define STARTLEN        37
#define MWEAVE          11
#define DOTCON          18
#define TOPMARG         24
#define BOTMARG         26 
#define UNIT            27

/* Buffers and Data */

extern struct PrinterData *PD;
extern struct PrinterExtendedData *PED;
extern UBYTE GammaTables[15][256];
 
static DITHERDATA_T *dither_data[4];
static UBYTE *render_buffer[4], *print_buffers[8];
union colorEntry *colour_correction_buffer;

static ULONG RowSize; 
static UWORD NumColorBufs, dpi_code;
static UWORD colorcodes[MAXCOLORBUFS];
static LONG num_print_buffers, current_print_buffer;

/* Gamma correction table to use. */
STATIC UBYTE * gamma_table;

/* This routine makes a local copy of the colour data and applies
 * gamma correction to it.
 */

VOID CorrectColours(struct PrtInfo * pi,UBYTE * gamma_table,union colorEntry * colour_buffer)
{
	LONG x,c;

	/* Make a copy of the colour data. */
	memcpy(colour_buffer,pi->pi_ColorInt,sizeof(*colour_buffer) * pi->pi_width);

	for(x = 0 ; x < pi->pi_width ; x++) {
		for(c = PCMYELLOW ; c <= PCMBLACK ; c++)
			colour_buffer->colorByte[c] = gamma_table[colour_buffer->colorByte[c]];
		colour_buffer++;
	}
}

/****************************************************************************/

/* Clear each CMY pixel if there is a K pixel which
 * would cover it.
 */

VOID ClearColour(UBYTE * render_buffer[],LONG num_bytes_per_row)
{
	UBYTE k;
	LONG i,j;

	for(i = 0 ; i < num_bytes_per_row ; i++) {
		k = ~render_buffer[PCMBLACK][i];
		for(j = PCMYELLOW ; j <= PCMCYAN ; j++)
			render_buffer[j][i] &= k;
	}
}


LONG Render(long ct, long x, long y, long status)
{
 /* Variables: Varies for each Case. See below */
 

       struct PrtInfo *pi;
        int threshold,i ;
        LONG size, method, compressed_size, columns;
        UBYTE *buffer;
        static ULONG dataoffset;
        union colorEntry *colour_buffer;
        
        /*
                00-05   \033(G\001\000\001              Set graphics mode
                06-11   \033(i\001\000\000              Set microweave off
                12-18   \033(e\002\000\000\002          Set dot control
                19-21   \033U\001                       Set bi-directional mode
                22-27   \033(U\001\000\012              Set unit to 'm'
       */

        static UBYTE StartBuf[STARTLEN+1] =
"\033(G\001\000\001\033(i\001\000\000\033(e\002\000\000\002\033U\001\033(U\001\000\012";

        /* Set colour and Print Raster Graphics
                00-02   \033r0                 Set colour
                03-11   \033.\000\005\005\001\000\000  Send raster data
        */
        static UBYTE ColourLine[12] = "\033r0\033.\000\005\005\001\000\000";
        
        static UBYTE EndLine[9] = "\015\033(v\002\000\001\000";
        
        int err=0, density=0, papersize, lpi; /* Any warnings here can be ignored */
        int topmarg, botmarg;
        
        switch(status) {
                case 0 : /* Master Initialization */
                        /*
                                ct      - pointer to IODRPReq structure.
                                x       - width of printed picture in pixels.
                                y       - height of printed picture in pixels.
                        */
                        RowSize = (x+7) /8 ;
                        NumColorBufs = (PD->pd_Preferences.PrintShade == SHADE_COLOR) ? 4 : 1;
                        colorcodes[0] = 4;
                        colorcodes[1] = 1;
                        colorcodes[2] = 2;
                        colorcodes[3] = 0;
                        
                        /* Alloc render buffers */
                        if (PD->pd_Preferences.PrintShade == SHADE_COLOR) {
                            for (i = PCMYELLOW; i<= PCMBLACK; i++) {
                               render_buffer[i] = IExec->AllocVecTags(RowSize, AVT_Type,  MEMF_SHARED, TAG_END);
                               if (render_buffer[i] == NULL) {
                                    err = PDERR_BUFFERMEMORY;
                                    break;
                               }
                             }
                        } else {
                            render_buffer[PCMBLACK] = IExec->AllocVecTags(RowSize, AVT_Type, MEMF_SHARED, TAG_END);
                            if (render_buffer[PCMBLACK] == NULL) {
                                    err = PDERR_BUFFERMEMORY;
                                    break;
                            }
                        }
                        
                        if (err != PDERR_NOERR)
                                break;
                        
                        /* Alloc print buffers */
                        num_print_buffers = 2 * NumColorBufs;
                        for (i = 0; i< num_print_buffers; i++) {
                            print_buffers[i] = IExec->AllocVecTags(RowSize, AVT_Type, MEMF_SHARED, TAG_END);
                            if (print_buffers[i] == NULL) {
                               err = PDERR_BUFFERMEMORY;
                               break;
                            }
                        }
                       if (err != PDERR_NOERR)
                                break;
                                 
                        /* Line buffers for Floyd-Steinburg filter */
                        if ((PD->pd_Preferences.PrintFlags & DITHERING_MASK) == FLOYD_DITHERING) {
                            if (PD->pd_Preferences.PrintShade == SHADE_COLOR) { 
                                for (i=PCMYELLOW; i <= PCMBLACK; i++) {
                                   dither_data[i] = CreateDitherData(x,2,1);
                                   if (dither_data[i] == NULL) {
                                        err = PDERR_BUFFERMEMORY;
                                        break;
                                   }
                                }
                             } else {
                                    dither_data[PCMBLACK] = CreateDitherData(x,2,1);
                                    if (dither_data[PCMBLACK] == NULL) {
                                        err = PDERR_BUFFERMEMORY;
                                        break;
                                    }
                             }
                         
                             if (err != PDERR_NOERR)
                                break;
                        }
                        
                        threshold = PD->pd_Preferences.PrintThreshold;
                        if (threshold < 1)
                           threshold = 1;
                        else if (threshold>15)
                           threshold = 15;
                            
                        /* Gamma Correction table for Color */      
                        if (threshold > 1 && NumColorBufs == 4) {
                           gamma_table = GammaTables[threshold - 2];
                           colour_correction_buffer = IExec->AllocVecTags(sizeof(*colour_correction_buffer) * x, AVT_Type, MEMF_SHARED, TAG_END);
                           if (colour_correction_buffer == NULL) {
                              err = PDERR_BUFFERMEMORY;
                              break;
                           }
                         }
                         else {
                            gamma_table = NULL;
                            colour_correction_buffer = NULL;
                         }
                           
                        
                         dataoffset = NUMSTARTCMD;

                         StartBuf[UNIT] = dpi_code;
                         if (PD->pd_Preferences.PrintQuality == LETTER)
                                StartBuf[MWEAVE] = 1;   /* Set Microweave */
                         if (density >= 6){
                             StartBuf[DOTCON] = 1;   /* Set Mircodot on */
                             StartBuf[MWEAVE] = 1;   /* Set Microweave */
                         }
                         lpi=6;      
                         if (PD->pd_Preferences.PrintSpacing == EIGHT_LPI)
                            lpi = 8;
                            
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

                         /* units = (length/spacing x 3600 )/ units */
                         topmarg = (1 / lpi * 3600) / dpi_code; /* 1 line */
                         botmarg = (papersize / lpi * 3600) / dpi_code;
                                
                     /*  StartBuf[TOPMARG] = topmarg & 0xff;
                         StartBuf[TOPMARG+1] = topmarg >> 8;
                         StartBuf[BOTMARG] = botmarg & 0xff;
                         StartBuf[BOTMARG+1] = botmarg >> 8; */
                         err = (*(PD->pd_PWrite))(StartBuf, STARTLEN); /* Print settings */
                          
                         break;

                case 1 : /* Scale, Dither and Render */
                        /*
                                ct      - pointer to PrtInfo structure.
                                x       - 0.
                                y       - row # (0 to Height - 1).
                        */
                        
                        pi = (struct PrtInfo *)ct;
                        
                        if (gamma_table != NULL) {
                            /* Perform gamma correction on colours */
                            CorrectColours(pi, gamma_table, colour_correction_buffer);
                            colour_buffer = colour_correction_buffer;
                        }
                        else 
                            /* Use uncorrected data */
                            colour_buffer = pi->pi_ColorInt;
                            
                        if (PD->pd_Preferences.PrintShade == SHADE_COLOR) {
                            for (i = PCMYELLOW; i<=PCMBLACK; i++)
                              Transfer(pi, colour_buffer, y, render_buffer[i], dither_data[i], i);
                            ClearColour(render_buffer, (LONG)RowSize);
                         } else {
                            Transfer(pi, colour_buffer, y, render_buffer[PCMBLACK], dither_data[PCMBLACK], PCMBLACK);
                         }                          
                         break;

                case 2 : /* Dump Buffer to Printer */
                        /*
                                ct      - 0.
                                x       - 0.
                                y       - # of rows sent (1 to NumRows).

                        */
                        
                       if (PD->pd_Preferences.PrintShade == SHADE_COLOR) {
                          buffer = NULL;
                          
                          for (i=PCMBLACK; i>=PCMYELLOW; i--) {
                              size = StripWhiteSpace(render_buffer[i], (LONG)RowSize);
                              columns = 8 * size; /* Number of exact pixel columns for ESC. command */
                              if (size > 0) {
                                 buffer = print_buffers[current_print_buffer];
                                 current_print_buffer = (current_print_buffer + 1)  % num_print_buffers;
                                 compressed_size = CompressMethod2(render_buffer[i], buffer, size);
                                 if (compressed_size <= 0) {
                                    memcpy(buffer, render_buffer[i], (size_t)size);
                                    method = 0;
                                 }
                                 else {
                                    size = compressed_size;
                                    method = 1;
                                 }
                              }
                              else {
                                   method = 0;
                              }
                              ColourLine[2] = colorcodes[i]; /* Colour */
                              ColourLine[5] = method; /*Compression */
                              ColourLine[6] = dpi_code; /* Density */
                              ColourLine[7] = dpi_code;
                              ColourLine[9] = columns & 0xff; /* Row size in dots */
                              ColourLine[10] = columns >> 8;
                              if (err == PDERR_NOERR && size > 0) {
                                  (*(PD->pd_PWrite))(ColourLine, 11); /* Send command */
                                  (*(PD->pd_PWrite))(buffer,(int)size);    /* Send data */
                                  err = (*(PD->pd_PWrite))("\015",1);
                              }
                              if (err != PDERR_NOERR)
                                  break;
                          }
                       }
                       else {
                            buffer = NULL;
                            size = StripWhiteSpace(render_buffer[PCMBLACK], (LONG)RowSize);
                            columns = 8 * size; /* Number of exact pixel columns for ESC. command */
                            if (size > 0) {
                                buffer = print_buffers[current_print_buffer];
                                current_print_buffer = (current_print_buffer + 1)  % num_print_buffers;
                                compressed_size = CompressMethod2(render_buffer[PCMBLACK], buffer, size);
                                if (compressed_size <= 0) {
                                    memcpy(buffer, render_buffer[PCMBLACK], (size_t)size);
                                    method = 0;
                                }
                                else {
                                    size = compressed_size;
                                    method = 1;
                                }
                            }
                            else {
                                method = 0;
                            }
                            
                            ColourLine[2] = colorcodes[3]; /* Black */
                            ColourLine[5] = method; /*Compression */
                            ColourLine[6] = dpi_code; /* Density */
                            ColourLine[7] = dpi_code;
                            ColourLine[9] = columns & 0xff; /* Row size in dots */
                            ColourLine[10] = columns >> 8;
                            if (err == PDERR_NOERR && size > 0) {
                               (*(PD->pd_PWrite))(ColourLine, 11); /* Send command */
                               (*(PD->pd_PWrite))(buffer,(int)size);
                               err = (*(PD->pd_PWrite))("\015", 1);
                            }
                       }

                      if (err == PDERR_NOERR) {
                         EndLine[6] = y;
                         err = (*(PD->pd_PWrite))(EndLine,8); /* Move to next postion */
                       }

                       break;

                case 3 : /* Clear and Init Buffer */
                        /*
                                ct      - 0.
                                x       - 0.
                                y       - 0.
                        */
                        if (NumColorBufs == 4) {
                           /* Clear colour buffers */
                           for (i= 0 ; i < NumColorBufs; i++) {
                               if (render_buffer[i] != NULL)
                                   memset(render_buffer[i],0, (size_t)RowSize);
                            }
                        } 
                        else { /* Clear black buffer only */
                            if (render_buffer[PCMBLACK] != NULL)
                                memset(render_buffer[PCMBLACK],0, (size_t)RowSize);
                        }
                        
                        break;

                case 4 : /* Close Down */
                        /*
                                ct      - error code.
                                x       - io_Special flag from IODRPReq.
                                y       - 0.
                        */

                        err = PDERR_NOERR; /* assume all ok */
                        /* if user did not cancel the print */
                        if (ct != PDERR_CANCEL) {
                                /* end raster graphics, bi-dir printing */
                                if ((err = (*(PD->pd_PWrite))("\033@\033U\000", 5)) == PDERR_NOERR) {
                                        /* if want to unload paper */
                                        if (!(x & SPECIAL_NOFORMFEED)) {
                                                /* eject paper */
                                                err = (*(PD->pd_PWrite))("\014", 1); 
                                        }
                                } 
                        }
                        /*
                                flag that there is no alpha data waiting that
                                needs a formfeed (since we just did one)
                        */
                        PED->ped_PrintMode = 0;
                         /* wait for both buffers to empty */
                        (*(PD->pd_PBothReady))();
                        /* Free the dithering and printing buffers. */
			for(i = 0 ; i < NumColorBufs ; i++)
			{
				DeleteDitherData(dither_data[i]);
				dither_data[i] = NULL;
			}

			for(i = 0 ; i < num_print_buffers ; i++)
			{
				IExec->FreeVec(print_buffers[i]);
				print_buffers[i] = NULL;
			}

			for(i = 0 ; i < NumColorBufs ; i++)
			{
				IExec->FreeVec(render_buffer[i]);
				render_buffer[i] = NULL;
			}

			IExec->FreeVec(colour_correction_buffer);
			colour_correction_buffer = NULL;

                       break;

                case 5 :  /* Pre-Master Initialization */
                        /*
                                ct      - 0 or pointer to IODRPReq structure.
                                x       - io_Special flag from IODRPReq.
                                y       - 0.
                        */
                        /* kludge for sloppy tractor mechanism */
                        density = (x & SPECIAL_DENSITYMASK);
                        dpi_code = SetDensity((ULONG)density);
                        break;                        
                        
        }
        return(err);
}
