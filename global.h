/* Epson Global.h */

/* Headers */

#include <exec/exec.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <exec/lists.h>
#include <proto/exec.h>
#include <clib/macros.h>
#include <string.h>
#include <devices/printer.h>
#include <devices/prtbase.h>
#include <devices/prtgfx.h>

#include "transfer.h"

#ifdef memset
#undef memset
#endif
#ifdef memcpy
#undef memcpy
#endif

/* Define printerIO - three different request structures sharing same memory - hence union 
 See https://wiki.amigaos.net/wiki/Printer_Device#Printer_Device, Device Interface */
union printerIO 
{
 struct IOStdReq ios;
 struct IODRPReq iodrp;
 struct IOPrtCmdReq iopc;
};

/* Prototypes */
int _start(void);
int Init(struct PrinterData *PD);
int Expunge(void);
int Open(union printerIO*);


LONG StripWhiteSpace(UBYTE * source,LONG size);
LONG CompressMethod2(UBYTE * src,UBYTE * dest,LONG count);
int SetDensity(ULONG code);
int DoSpecial(UWORD *c, char o[], BYTE *v, BYTE *cu, BYTE *cr, UBYTE p[]);
int ConvFunc(char *buf, char c, int flag);
int Close(union printerIO*);

LONG  Render(long ct, long x, long y, long st);
VOID CorrectColours(struct PrtInfo *pi, UBYTE *gamma_table, union colorEntry *c);
VOID ClearColour(UBYTE * render_buffer[],LONG num_bytes_per_row);

DITHERDATA_T *CreateDitherData(LONG samplesPerLine,LONG numLines,LONG pad);
VOID DeleteDitherData(DITHERDATA_T * dd);
VOID RotateDitherLines(DITHERDATA_T * dd);

VOID TransferFloydSteinberg(UBYTE * ptr,UWORD width,union colorEntry *ColorInt,int component,DITHERDATA_T * dd);
VOID TransferBayer(UBYTE *ptr,UWORD width,union colorEntry *ColorInt,int component,LONG y);
VOID TransferHalftone(UBYTE *ptr,UWORD width,union colorEntry *ColorInt,int component,LONG y);
VOID TransferThreshold(UBYTE *ptr,UWORD width,union colorEntry *ColorInt,int component,UWORD threshold);
VOID Transfer(struct PrtInfo *PInfo, union colorEntry *ColorInt, LONG y, UBYTE *ptr, DITHERDATA_T *dd,	int component);	

