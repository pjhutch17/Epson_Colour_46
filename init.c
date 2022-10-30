/* Init.c 46
 *
 *  Combination of init.asm and printertag.asm into one file 
 *  and rewritten in C for easy migration
 *
*/

#include "global.h"


char printerName[20] = {"Epson_Stylus_Color\0"};
UWORD Version = 46;
UWORD Revision = 0;

extern STRPTR CommandTable[];
extern STRPTR ExtendedCharTable[];
extern struct TagItem PropertyTagList[];

/* This is defined in printertag.asm which seems to work better than in C 
   Using (void *) stops the incompatible warning type message.
*/

/* New for Amiga OS 4 */
struct PrinterSegment PrinterSegment = {
  ZERO,
  0x70144E75,
  46,
  0,   
  /* struct PrinterExtendedData PEDData */
 {
  printerName,
  (void *)Init,
  (void (*)())Expunge,
  (void *)Open,
  (void (*)())Close,
  PPC_COLORGFX,
  PCC_YMCB,
  136,
  10,
  8,
  1632,
  0,
  120,
  72,
  (STRPTR **)CommandTable,
  (void *)DoSpecial,
  (LONG (*)())Render,
  30,
  (STRPTR *)ExtendedCharTable,
  0,
  (LONG (*)())ConvFunc
 }
};

struct PrinterData *PD;
struct PrinterExtendedData *PED;
struct Library *SysBase;
struct Library *DOSBase;
struct Library *GfxBase;
struct Library *IntuitionBase;
struct ExecIFace *IExec;
struct Interface *INewlib = NULL;

/* Return a fail if file executed 
 Replaces MOVEQ #0,D0, RTS in 
 printertag.asm 
*/
int _start(void)
{
  return RETURN_FAIL;
}

int Init(struct PrinterData *pd)
{
        BOOL fail = FALSE;
        PD = pd;
        /* PED = PEDData; */
        PED = &PrinterSegment.ps_PED; /* New of AmigaOS 4 for Ped data */
        
        SysBase = *(struct Library **)4; /* Abs address for system base */
   
        IExec = (struct ExecIFace *)((struct ExecBase *)SysBase)->MainInterface;

        if (!(DOSBase = (struct Library *)IExec->OpenLibrary("dos.library", 50)))
                fail = TRUE;
        if (!(GfxBase = (struct Library *)IExec->OpenLibrary("graphics.library", 50)))
                fail = TRUE;               
        if (!(IntuitionBase = (struct Library *)IExec->OpenLibrary("intuition.library", 50)))
                fail = TRUE;
                
        if (fail == TRUE) {
                if (GfxBase) IExec->CloseLibrary((struct Library *)GfxBase);
                if (DOSBase) IExec->CloseLibrary((struct Library *)DOSBase);
                if (IntuitionBase) IExec->CloseLibrary((struct Library *)IntuitionBase);
                return(-1);
        }
        return(0);
       
} 
                 
int Expunge(void)
{
    if (IntuitionBase) IExec->CloseLibrary((struct Library *)IntuitionBase);
    if (GfxBase) IExec->CloseLibrary((struct Library *)GfxBase);
    if (DOSBase) IExec->CloseLibrary((struct Library *)DOSBase);    
    return(0);
}

int Open(ior)
union printerIO *ior;
{
    
    return 0;
}
