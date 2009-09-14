/*
 * (C) Copyright 2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mb86290fb.h"
#include "gdcdef.h"
#include "gdcreg.h"

#include "xf86.h"
#include "xf86_OSproc.h"
#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "xf86cmap.h"
#include "fb.h"
#include "xf86Resources.h"
#include "xf86RAC.h"
#include "fbdevhw.h"
#include "xaa.h"
#include "xf86i2c.h"
#include "compiler.h"

#include "mb86290_driver.h"

#define PCI_CHIP_MB86290     0x2019
#define PCI_VENDOR_FUJITSU   0x10CF

/* -------------------------------------------------------------------- */

static const OptionInfoRec  *MB86290AvailableOptions(int chipid, int busid);
static void                 MB86290Identify(int flags);
static Bool                 MB86290Probe(DriverPtr drv, int flags);
static Bool                 MB86290PreInit(ScrnInfoPtr pScrn, int flags);
static Bool                 MB86290ScreenInit(int Index, ScreenPtr pScreen, 
				int argc, char **argv);
static Bool                 MB86290CloseScreen(int scrnIndex, ScreenPtr pScreen);

static Bool                 MB86290AccelInit(ScreenPtr pScreen);
static void                 MB86290Sync(ScrnInfoPtr pScrn);
static void                 MB86290SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, 
				int xdir, int ydir, int rop, 
				unsigned int planemask, int trans_color);
static void                 MB86290SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, 
				int x1, int y1, int x2, int y2, 
				int width, int height);
static void                 MB86290SetupForSolidFill(ScrnInfoPtr pScrn, 
				int color, int rop, unsigned int planemask);
static void                 MB86290SubsequentSolidFillRect(ScrnInfoPtr pScrn, 
				int x, int y, int w, int h);
static void                 MB86290SetupForSolidLine(ScrnInfoPtr pScrn, 
				int color, int rop, unsigned int planemask);
static void                 MB86290SubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, 
				int x1, int y1, int x2, int y2, int flags);
static void                 MB86290SetClippingRectangle(ScrnInfoPtr pScrn, 
				int left, int top, int right, int bottom);
static void                 MB86290DisableClipping(ScrnInfoPtr pScrn);
#if 1
static void                 MB86290SetupForDashedLine(ScrnInfoPtr pScrn, 
				int fg, int bg, int rop, unsigned int planemask, 
				int length, unsigned char *pattern);
static void                 MB86290SubsequentDashedTwoPointLine(ScrnInfoPtr pScrn, 
				int x1, int y1, int x2, int y2, int flags, int phase);
#endif
static void MB86290SetupForMono8x8PatternFill(ScrnInfoPtr pScrn,
					      int patternx, int patterny,
					      int fg, int bg, int rop,
					      unsigned int planemask);
static void MB86290SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
						    int patternx, int patterny,
						    int x, int y, int w, int h);
static void MB86290SetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
							      int fg, int bg,
							      int rop,
							      unsigned int planemask);
static void MB86290SubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
								int x, int y, int w, int h,
								int skipleft);
static void MB86290SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);


/* -------------------------------------------------------------------- */

#define MB86290_DRIVER_VERSION  1000
#define MB86290_NAME            "MB86290"
#define MB86290_DRIVER_NAME     "mb86290"
#define MB86290_MAJOR_VERSION   0
#define MB86290_MINOR_VERSION   1
#define MB86290_PATCHLEVEL      0

DriverRec MB86290 = {
	MB86290_DRIVER_VERSION,
	MB86290_DRIVER_NAME,
	MB86290Identify,
	MB86290Probe,
	MB86290AvailableOptions,
	NULL,
	0
};

/* Supported chipsets */
static SymTabRec MB86290Chipsets[] = {
	{ PCI_CHIP_MB86290, "mb86290" },
	{-1,                NULL }
};

static PciChipsets MB86290PciChipsets[] = {
	{ PCI_CHIP_MB86290, PCI_CHIP_MB86290, RES_SHARED_VGA },
	{ -1,               -1,               RES_UNDEFINED }
};

/* Supported options */
typedef enum {
	OPTION_NOACCEL,
	OPTION_NOVIDEO
} MB86290Opts;

static const OptionInfoRec MB86290Options[] = {
	{ OPTION_NOACCEL, "NoAccel", OPTV_BOOLEAN, {0}, FALSE },
	{ OPTION_NOVIDEO, "NoVideo", OPTV_BOOLEAN, {0}, FALSE },
	{ -1,             NULL,      OPTV_NONE,    {0}, FALSE }
};

/* -------------------------------------------------------------------- */

static const char *mb86290_fbSymbols[] = {
	"fbScreenInit",
	"fbPictureInit",
	NULL
};

static const char *fbdevHWSymbols[] = {
	"fbdevHWInit",
	"fbdevHWProbe",
	"fbdevHWSetVideoModes",
	"fbdevHWUseBuildinMode",
	"fbdevHWGetDepth",
	"fbdevHWGetLineLength",
	"fbdevHWGetName",
	"fbdevHWGetType",
	"fbdevHWGetVidmem",
	"fbdevHWLinearOffset",
	"fbdevHWLoadPalette",
	"fbdevHWMapVidmem",
	"fbdevHWUnmapVidmem",
	"fbdevHWLoadpalette",
	"fbdevHWAdjustFrame",
	"fbdevHWEnterVT",
	"fbdevHWLeaveVT",
	"fbdevHWModeInit",
	"fbdevHWRestore",
	"fbdevHWSwitchMode",
	"fbdevHWValidMode",
	"fbdevHWDPMSSet",
	NULL
};

static const char *xaaSymbols[] = {
	"XAACreateInfoRec",
	"XAADestroyInfoRec",
	"XAAInit",
	NULL
};

static const char *i2cSymbols[] =
{
	"xf86CreateI2CBusRec",
	"xf86CreateI2CDevRec",
	"xf86DestroyI2CBusRec",
	"xf86DestroyI2CDevRec",
	"xf86I2CBusInit",
	"xf86I2CDevInit",
	"xf86I2CWriteByte",
	NULL
};

/* -------------------------------------------------------------------- */

#ifdef XFree86LOADER

static XF86ModuleVersionInfo MB86290VersRec =
{
	"mb86290",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	MB86290_MAJOR_VERSION, MB86290_MINOR_VERSION, MB86290_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

unsigned char xaaScanLineBuf[1024 / 8];
unsigned char *xaaScanLineBufs[] = { xaaScanLineBuf };

#define MB86290_REGS_MMAP_SIZE (256*1024)

void *mb86290_acc_regs;
const int draw_reg_off = GDC_DRAW_BASE - GDC_HOST_BASE;
const int geo_reg_off = GDC_GEO_BASE - GDC_HOST_BASE;
const int i2c_reg_off = GDC_I2C_BASE - GDC_HOST_BASE;
const int capture_reg_off = GDC_CAP_BASE - GDC_HOST_BASE;
const int disp_reg_off = GDC_DISP_BASE - GDC_HOST_BASE;

long 
MB86290ReadDrawReg(long off)
{
	return MMIO_IN32(mb86290_acc_regs, draw_reg_off + off);
}

void 
MB86290WriteDrawReg(long off, long val)
{
	MMIO_OUT32(mb86290_acc_regs, draw_reg_off + off, val);
}

long 
MB86290ReadGeoReg(long off)
{
	return MMIO_IN32(mb86290_acc_regs, geo_reg_off + off);
}

void 
MB86290WriteGeoReg(long off, long val)
{
	MMIO_OUT32(mb86290_acc_regs, geo_reg_off + off, val);
}

long 
MB86290ReadI2CReg(long off)
{
	return MMIO_IN8(mb86290_acc_regs, i2c_reg_off + off);
}

void 
MB86290WriteI2CReg(long off, long val)
{
	MMIO_OUT8(mb86290_acc_regs, i2c_reg_off + off, val);
}

long 
MB86290ReadCaptureReg(long off)
{
	return MMIO_IN32(mb86290_acc_regs, capture_reg_off + off);
}

void 
MB86290WriteCaptureReg(long off, long val)
{
	MMIO_OUT32(mb86290_acc_regs, capture_reg_off + off, val);
}

long 
MB86290ReadDispReg(long off)
{
	return MMIO_IN32(mb86290_acc_regs, disp_reg_off + off);
}

void 
MB86290WriteDispReg(long off, long val)
{
	MMIO_OUT32(mb86290_acc_regs, disp_reg_off + off, val);
}

static pointer
MB86290Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	if (!setupDone) {
		setupDone = TRUE;
		xf86AddDriver(&MB86290, module, 0);
		LoaderRefSymLists(mb86290_fbSymbols, xaaSymbols, i2cSymbols, NULL);
		return (pointer)1;
	} else {
		if (errmaj) *errmaj = LDR_ONCEONLY;
		return NULL;
	}
}

XF86ModuleData mb86290ModuleData = { &MB86290VersRec, MB86290Setup, NULL };

MODULESETUPPROTO(MB86290Setup);

#endif

static Bool
MB86290GetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate != NULL)
		return TRUE;
	
	pScrn->driverPrivate = xnfcalloc(sizeof(MB86290Rec), 1);
	return TRUE;
}

static void
MB86290FreeRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate == NULL)
		return;
	xfree(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

/* -------------------------------------------------------------------- */

static const OptionInfoRec *
MB86290AvailableOptions(int chipid, int busid)
{
	return MB86290Options;
}

static void
MB86290Identify(int flags)
{
	xf86PrintChipsets(MB86290_NAME, "Fujitsu chip", MB86290Chipsets);
}

static Bool
MB86290Probe(DriverPtr drv, int flags)
{
	int         numUsed;
	int         numDevSections;
	int         *usedChips;
	GDevPtr     *devSections;
	Bool        foundScreen = FALSE;
	ScrnInfoPtr pScrn = NULL;
	int         i;

	if (!xf86GetPciVideoInfo())
		return FALSE;
	
	if ((numDevSections = xf86MatchDevice(MB86290_DRIVER_NAME, &devSections)) <= 0)
		return FALSE;
	
	if (!xf86LoadDrvSubModule(drv, "fbdevhw"))
		return FALSE;
	xf86LoaderReqSymLists(fbdevHWSymbols, NULL);

	if ((numUsed = xf86MatchPciInstances(MB86290_NAME, 
			PCI_VENDOR_FUJITSU,
			MB86290Chipsets, 
			MB86290PciChipsets, 
			devSections, 
			numDevSections, 
			drv, 
			&usedChips)) <= 0)
	return FALSE;

	for (i = 0; i < numUsed; i++)
	{
		if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i], 
			MB86290PciChipsets, NULL, NULL, NULL, NULL, NULL)))
		{
			pScrn->driverVersion = MB86290_DRIVER_VERSION;
			pScrn->driverName    = MB86290_DRIVER_NAME;
			pScrn->name          = MB86290_NAME;
			pScrn->Probe         = MB86290Probe;
			pScrn->PreInit       = MB86290PreInit;
			pScrn->ScreenInit    = MB86290ScreenInit;
			pScrn->SwitchMode    = fbdevHWSwitchMode;
			pScrn->AdjustFrame   = fbdevHWAdjustFrame;
			pScrn->EnterVT       = fbdevHWEnterVT;
			pScrn->LeaveVT       = fbdevHWLeaveVT;
			pScrn->ValidMode     = fbdevHWValidMode;
			foundScreen = TRUE;
		}
	}

	xfree(usedChips);
	xfree(devSections);

	return foundScreen;
}

static Bool
MB86290PreInit(ScrnInfoPtr pScrn, int flags)
{
	MB86290Ptr fPtr;
	int        depth;
	int        fbbpp;
	
	if (pScrn->numEntities != 1)
		return FALSE;

	MB86290GetRec(pScrn);
	
	fPtr = MB86290PTR(pScrn);
	
	fPtr->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
	if (fPtr->pEnt->location.type != BUS_PCI)
		goto fail;
	
	fPtr->PciInfo = xf86GetPciInfoForEntity(fPtr->pEnt->index);

	if (xf86RegisterResources(fPtr->pEnt->index, NULL, ResExclusive))
		goto fail;

	pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR;
	pScrn->monitor = pScrn->confScreen->monitor;

	/* Open framebuffer device */
	if (!fbdevHWInit(pScrn, fPtr->PciInfo, NULL))
		goto fail;
	
	/* Set default depth */
	depth = fbdevHWGetDepth(pScrn, &fbbpp);
	if (!xf86SetDepthBpp(pScrn, depth, depth, fbbpp, 0))
		goto fail;
	if ((pScrn->depth != depth) || (pScrn->bitsPerPixel != fbbpp)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ONLY ONE MODE IS SUPPORTED NOW - DEPTH %d, FRAMEBUFFER BPP %d\n", depth, fbbpp);
		goto fail;
	}
	xf86PrintDepthBpp(pScrn);

	/* Color weight */
	if (pScrn->depth > 8) {
		rgb zeros = { 0, 0, 0 };
		if (!xf86SetWeight(pScrn, zeros, zeros))
			goto fail;
	}

	/* Visual init */
	if (!xf86SetDefaultVisual(pScrn, -1))
		goto fail;
	if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
			"Default visual (%s) is not supported at depth %d\n", 
			xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
		goto fail;
	}
	
	/* Gamma correction */
	{
		Gamma zeros = { 0.0, 0.0, 0.0 };
		if (!xf86SetGamma(pScrn, zeros))
			goto fail;
	}

	/* Handle options */
	xf86CollectOptions(pScrn, NULL);
	if (!(fPtr->Options = xalloc(sizeof(MB86290Options))))
		goto fail;
	memcpy(fPtr->Options, MB86290Options, sizeof(MB86290Options));
	xf86ProcessOptions(pScrn->scrnIndex, fPtr->pEnt->device->options, fPtr->Options);

	pScrn->progClock = TRUE;
#ifndef XSERVER_LIBPCIACCESS
	pScrn->chipset   = (char *)xf86TokenToString(MB86290Chipsets, fPtr->PciInfo->chipType);
#else
	pScrn->chipset   = (char *)xf86TokenToString(MB86290Chipsets, fPtr->PciInfo->device_id);
#endif
	pScrn->videoRam  = fbdevHWGetVidmem(pScrn);
	
	/* Select video modes */
	fbdevHWUseBuildinMode(pScrn);
	pScrn->currentMode = pScrn->modes;
	pScrn->displayWidth = pScrn->virtualX;
	xf86PrintModes(pScrn);
	
	/* Set display resolution */
	xf86SetDpi(pScrn, 0, 0);

	/* Load bpp-specific modules */
#if X_BYTE_ORDER != X_LITTLE_ENDIAN
	if (!xf86LoadSubModule(pScrn, "mb86290_fb"))
#else
	if (!xf86LoadSubModule(pScrn, "fb"))
#endif
		goto fail;
	xf86LoaderReqSymLists(mb86290_fbSymbols, NULL);

	/* Load XAA if needed */
	if (!xf86ReturnOptValBool(fPtr->Options, OPTION_NOACCEL, FALSE)) {
		if (!xf86LoadSubModule(pScrn, "xaa")) {
			goto fail;
		}
		xf86LoaderReqSymLists(xaaSymbols, NULL);
	}

	if (!xf86ReturnOptValBool(fPtr->Options, OPTION_NOVIDEO, FALSE)) {
		if (!xf86LoadSubModule(pScrn, "i2c"))
			goto fail;
		xf86LoaderReqSymLists(i2cSymbols, NULL);
		MB86290I2CInit(pScrn);
	}

	return TRUE;
fail:
	MB86290FreeRec(pScrn);
	return FALSE;
}

static Bool
MB86290ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MB86290Ptr  fPtr  = MB86290PTR(pScrn);
	BoxRec AvailFBArea;

	if ((fPtr->fbmem = fbdevHWMapVidmem(pScrn)) == NULL)
		return FALSE;
	fPtr->fbstart = fPtr->fbmem + fbdevHWLinearOffset(pScrn);

	mb86290_acc_regs = fPtr->fbmem + GDC_HOST_BASE;

	fbdevHWSave(pScrn);

	if (!fbdevHWModeInit(pScrn, pScrn->currentMode))
		return FALSE;
	
	fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
	
	fbdevHWAdjustFrame(scrnIndex, 0, 0, 0);

	miClearVisualTypes();
	if (pScrn->bitsPerPixel > 8) {
		if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits, TrueColor))
			return FALSE;
	} else {
		if (!miSetVisualTypes(pScrn->depth,
				      miGetDefaultVisualMask(pScrn->depth),
				      pScrn->rgbBits, pScrn->defaultVisual))
			return FALSE;
	}

	if (!miSetPixmapDepths())
		return FALSE;

	if (!fbScreenInit(pScreen, fPtr->fbstart, pScrn->virtualX, pScrn->virtualY, 
			pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth, pScrn->bitsPerPixel))
		return FALSE;

	xf86SetBlackWhitePixels(pScreen);
	
	if (pScrn->bitsPerPixel > 8) {
		VisualPtr visual;
		visual = pScreen->visuals + pScreen->numVisuals;
		while (--visual >= pScreen->visuals) {
			if ((visual->class | DynamicClass) == DirectColor) {
				visual->offsetRed   = pScrn->offset.red;
				visual->offsetGreen = pScrn->offset.green;
				visual->offsetBlue  = pScrn->offset.blue;
				visual->redMask     = pScrn->mask.red;
				visual->greenMask   = pScrn->mask.green;
				visual->blueMask    = pScrn->mask.blue;
			}
		}
	}

	if (!fbPictureInit(pScreen, NULL, 0))
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			"RENDER extension initialisation failed.\n");

	/* Backing store setup */
	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);

	/* Acceleration setup */
	if (!xf86ReturnOptValBool(fPtr->Options, OPTION_NOACCEL, FALSE)) {
		if (MB86290AccelInit(pScreen)) {
			xf86DrvMsg(scrnIndex, X_INFO, "Acceleration enabled\n");
			fPtr->AccelOn = TRUE;
		} else {
			xf86DrvMsg(scrnIndex, X_ERROR, 
				"Acceleration initialization failed\n");
			xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
			fPtr->AccelOn = FALSE;
		}
	} else {
		xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
		fPtr->AccelOn = FALSE;
	}

	/* Cursor setup */
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
	
	/* Colormap setup */
	if (!miCreateDefColormap(pScreen))
		return FALSE;
	if (!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPalette, NULL, CMAP_PALETTED_TRUECOLOR))
		return FALSE;

	if (!xf86I2CProbeAddress(fPtr->I2C, I2C_SAA7113)) 
		return FALSE;

	/* FIXME board actually has 64M, but there's 256K area of inner registers at 32M offset
	   and it should be protected from allocation somehow */
	AvailFBArea.x1 = 0;
	AvailFBArea.y1 = 0;
	AvailFBArea.x2 = pScrn->displayWidth;
	AvailFBArea.y2 = (32 * 1024 * 1024) / (pScrn->displayWidth * 2);

	if (!xf86InitFBManager(pScreen, &AvailFBArea)) 
		return FALSE;

	if (!xf86ReturnOptValBool(fPtr->Options, OPTION_NOVIDEO, FALSE))
		MB86290InitVideo(pScreen);

	/* Provide SaveScreen */
	pScreen->SaveScreen = fbdevHWSaveScreen;

	/* Wrap CloseScreen */
	fPtr->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = MB86290CloseScreen;

	return TRUE;
}

static Bool
MB86290CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	MB86290Ptr  fPtr  = MB86290PTR(pScrn);
	
	fbdevHWRestore(pScrn);
	fbdevHWUnmapVidmem(pScrn);

	if (fPtr->AccelOn)
		 XAADestroyInfoRec(fPtr->Accel);

	pScrn->vtSema = FALSE;

	pScreen->CloseScreen = fPtr->CloseScreen;

	return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

/* -------------------------------------------------------------------- */

static Bool
MB86290AccelInit(ScreenPtr pScreen)
{
	ScrnInfoPtr   pScrn;
	MB86290Ptr    fPtr;
	XAAInfoRecPtr a;
	
	pScrn = xf86Screens[pScreen->myNum];
	fPtr  = MB86290PTR(pScrn);

	if (!(a = fPtr->Accel = XAACreateInfoRec())) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
				"XAACreateInfoRec Error\n");
		return FALSE;
	}
	
	/* Sync function */
	a->Sync                                         = MB86290Sync;

	/* Screen To Screen Copies */
	a->ScreenToScreenCopyFlags                      = NO_PLANEMASK | NO_TRANSPARENCY;
	a->SetupForScreenToScreenCopy                   = MB86290SetupForScreenToScreenCopy;
	a->SubsequentScreenToScreenCopy                 = MB86290SubsequentScreenToScreenCopy;

	/* Solid Fills */
	a->SolidFillFlags                               = GXCOPY_ONLY | NO_PLANEMASK;
	a->SetupForSolidFill                            = MB86290SetupForSolidFill;
	a->SubsequentSolidFillRect                      = MB86290SubsequentSolidFillRect;
	
	/* Solid Lines */
	a->SolidLineFlags                               = NO_PLANEMASK;
	a->SetupForSolidLine                            = MB86290SetupForSolidLine;
	a->SubsequentSolidTwoPointLine                  = MB86290SubsequentSolidTwoPointLine;
#define DASH_LINES
#ifdef DASH_LINES
	/* Dashed Lines */
	a->DashPatternMaxLength                         = 32;
	a->DashedLineFlags                              = NO_PLANEMASK | NO_TRANSPARENCY
		| LINE_PATTERN_MSBFIRST_MSBJUSTIFIED;
	a->SetupForDashedLine                           = MB86290SetupForDashedLine;
	a->SubsequentDashedTwoPointLine                 = MB86290SubsequentDashedTwoPointLine;
#endif
	/* Clipping */
	a->ClippingFlags                                = HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY
		| HARDWARE_CLIP_SOLID_FILL
		| HARDWARE_CLIP_SOLID_LINE;
	a->SetClippingRectangle                         = MB86290SetClippingRectangle;
	a->DisableClipping                              = MB86290DisableClipping;

	a->SetupForMono8x8PatternFill                   = MB86290SetupForMono8x8PatternFill;
	a->SubsequentMono8x8PatternFillRect             = MB86290SubsequentMono8x8PatternFillRect;
	a->Mono8x8PatternFillFlags                      = HARDWARE_PATTERN_PROGRAMMED_BITS
		| BIT_ORDER_IN_BYTE_LSBFIRST
		| NO_PLANEMASK;

	a->SetupForScanlineCPUToScreenColorExpandFill   = MB86290SetupForScanlineCPUToScreenColorExpandFill;
	a->SubsequentScanlineCPUToScreenColorExpandFill = MB86290SubsequentScanlineCPUToScreenColorExpandFill;
	a->SubsequentColorExpandScanline                = MB86290SubsequentColorExpandScanline;
	a->ScanlineCPUToScreenColorExpandFillFlags      = 
#if X_BYTE_ORDER != X_LITTLE_ENDIAN
		BIT_ORDER_IN_BYTE_LSBFIRST 
#else
		BIT_ORDER_IN_BYTE_MSBFIRST
#endif
		| NO_PLANEMASK;
	a->NumScanlineColorExpandBuffers                = sizeof(xaaScanLineBufs) / sizeof(xaaScanLineBufs[0]);
	a->ScanlineColorExpandBuffers                   = xaaScanLineBufs;

	if (!XAAInit(pScreen, a)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"XAAInit Error\n");
		return FALSE;
	}

	/* Init drawing engine */
	MB86290WriteDrawReg(GDC_REG_DRAW_BASE, 0);
	MB86290WriteDrawReg(GDC_REG_MODE_MISC, 0x8000);   /* 16 bpp */
	MB86290WriteDrawReg(GDC_REG_X_RESOLUTION, 0x400); /* 1024 */

	return TRUE;
}

static void
MB86290Wait(int needed_slots)
{
	while (MB86290ReadDrawReg(GDC_REG_FIFO_COUNT) < needed_slots);
}

static void
MB86290WriteFifo(int count, ...)
{
	va_list params;
	int i;

	MB86290Wait(count);

	va_start(params, count);
	for (i = 0; i < count; i++) {
		MB86290WriteGeoReg(GDC_GEO_REG_INPUT_FIFO, va_arg(params, unsigned long));
	}
	va_end(params);
}

static void
udelay(int usec)
{
	long b_secs, b_usecs;
	long a_secs, a_usecs;
	long d_secs, d_usecs;
	long diff;
	
	if (usec > 0) {
		xf86getsecs(&b_secs, &b_usecs);
		do {
			/* It would be nice to use {xf86}usleep, 
			 * but usleep (1) takes >10000 usec !
			 */
			xf86getsecs(&a_secs, &a_usecs);
			d_secs  = (a_secs - b_secs);
			d_usecs = (a_usecs - b_usecs);
			diff = d_secs*1000000 + d_usecs;
		} while (diff>0 && diff< (usec + 1));
	}
}

static void
MB86290Sync(ScrnInfoPtr pScrn)
{
	int i = 1000;

	/* sometimes the chip interminably reports that it's busy,
	   so 1s timeout helps to avoid eternal loop */
	while (MB86290ReadDrawReg(GDC_REG_ENGINE_STATUS) && --i)
		udelay(1000);

	if (!i) {
		ErrorF("MB86290Sync timed out\n");
	}
}

static void
MB86290SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop, unsigned int planemask, int trans_color)
{
	MB86290WriteDrawReg(GDC_REG_MODE_BITMAP, (2 << 7) | (rop << 9)); /* Set raster operation */
}

static void
MB86290SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2, int width, int height)
{
	unsigned long cmd = GDC_TYPE_BLTCOPYP << 24; 

	if (x1 >= x2 && y1 >= y2)
		cmd |= GDC_CMD_BLTCOPY_TOP_LEFT << 16;
	else if (x1 >= x2 && y1 <= y2)
		cmd |= GDC_CMD_BLTCOPY_BOTTOM_LEFT << 16;
	else if (x1 <= x2 && y1 >= y2)
		cmd |= GDC_CMD_BLTCOPY_TOP_RIGHT << 16;
	else
		cmd |= GDC_CMD_BLTCOPY_BOTTOM_RIGHT << 16;
	MB86290WriteFifo(4, cmd, (y1 << 16) | x1, (y2 << 16) | x2, (height << 16) | width);
}

static void
MB86290SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop, unsigned int planemask)
{
	MB86290WriteFifo(2, (GDC_TYPE_SETCOLORREGISTER << 24) | (GDC_CMD_BODY_FORE_COLOR << 16),
			 color);
}

static void
MB86290SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
	MB86290WriteFifo(3, (GDC_TYPE_DRAWRECTP << 24) | (GDC_CMD_BLT_FILL << 16),
			 (y << 16) | x,
			 (h << 16) | w);
}

static void
MB86290SetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop, unsigned int planemask)
{
	MB86290WriteFifo(4, (GDC_TYPE_SETCOLORREGISTER << 24) | (GDC_CMD_BODY_FORE_COLOR << 16), color,
			 (GDC_TYPE_SETMODEREGISTER << 24) | (GDC_CMD_MDR1 << 16), (2 << 7) | (rop << 9));
}

static void
MB86290SubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2, int flags)
{
	long cmd2 = (GDC_TYPE_DRAWLINE2I << 24) | 1;

	if (flags & OMIT_LAST)					
		cmd2 |= GDC_CMD_0_VECTOR_NOEND << 16;
	else
		cmd2 |= GDC_CMD_0_VECTOR << 16;

	MB86290WriteFifo(6, GDC_TYPE_DRAWLINE2I << 24, x1 << 16, y1 << 16,
			 cmd2, x2 << 16, y2 << 16);
}
#ifdef DASH_LINES
static void
MB86290SetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop, unsigned int planemask, 
				int length, unsigned char *pattern)
{
        xf86DrvMsg(pScrn->scrnIndex, X_NOTICE, "MB86290SetupForDashedLine\n");
	MB86290WriteFifo(6, (GDC_TYPE_SETCOLORREGISTER << 24) | (GDC_CMD_BODY_FORE_COLOR << 16), fg,
			 (GDC_TYPE_SETCOLORREGISTER << 24) | (GDC_CMD_BODY_BACK_COLOR << 16), bg,
			 (GDC_TYPE_SETMODEREGISTER << 24) | (GDC_CMD_MDR1 << 16), (2 << 7) | (rop << 9) | (1 << 19));
	MB86290WriteDrawReg(GDC_REG_LINE_PATTERN, *(long*)pattern);
}

static void
MB86290SubsequentDashedTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2, int flags, int phase)
{
        xf86DrvMsg(pScrn->scrnIndex, X_NOTICE, "MB86290SubsequentDashedTwoPointLine\n");
	MB86290WriteDrawReg(GDC_REG_LINE_PATTERN_OFFSET, phase);
	MB86290SubsequentSolidTwoPointLine(pScrn, x1, y1, x2, y2, flags);
}
#endif
static void
MB86290SetClippingRectangle(ScrnInfoPtr pScrn, int left, int top, int right, int bottom)
{
	MB86290WriteDrawReg(GDC_REG_CLIP_XMIN, left);
	MB86290WriteDrawReg(GDC_REG_CLIP_XMAX, right);
	MB86290WriteDrawReg(GDC_REG_CLIP_YMIN, top);
	MB86290WriteDrawReg(GDC_REG_CLIP_YMAX, bottom);
	MB86290WriteDrawReg(GDC_REG_MODE_MISC,
		       MB86290ReadDrawReg(GDC_REG_MODE_MISC) | (3 << 8)); /* Enable clipping */
}

static void
MB86290DisableClipping(ScrnInfoPtr pScrn)
{
	MB86290WriteDrawReg(GDC_REG_MODE_MISC,
		       MB86290ReadDrawReg(GDC_REG_MODE_MISC) & ~(3 << 8)); /* Disable clipping */
}

unsigned long p[8];

static void 
MB86290SetupForMono8x8PatternFill(ScrnInfoPtr pScrn,
				  int patternx, int patterny,
				  int fg, int bg, int rop,
				  unsigned int planemask)
{
	int i;
	long mdr4 = rop << 9;
	unsigned long* p = MB86290PTR(pScrn)->bmp;

	for (i = 0; i < 4; i++) {
		int ptrn = (patternx >> ((3 - i) * 8)) & 0xff;
		p[i] = (ptrn << 24) | (ptrn << 16) | (ptrn << 8) | ptrn;
	}
	for (; i < 8; i++) {
		int ptrn = (patterny >> ((7 - i) * 8)) & 0xff;
		p[i] = (ptrn << 24) | (ptrn << 16) | (ptrn << 8) | ptrn;
	}
	MB86290WriteFifo(2, (GDC_TYPE_SETCOLORREGISTER << 24) | (GDC_CMD_BODY_FORE_COLOR << 16), fg);
	if (bg == -1) {
		mdr4 |= GDC_BLT_TRANS_MODE_MASK;
		bg = fg + 1; /* bg == transient color */
		MB86290WriteFifo(2, (GDC_TYPE_SETREGISTER << 24) | (1 << 16) | GDC_REG_BLT_TRANS_COLOR,
			 bg);
	}
	MB86290WriteFifo(2, (GDC_TYPE_SETCOLORREGISTER << 24) | (GDC_CMD_BODY_BACK_COLOR << 16), bg);
	MB86290WriteDrawReg(GDC_REG_MODE_BITMAP, mdr4);
}

static void 
MB86290SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
					int patternx, int patterny,
					int x, int y, int w, int h)
{
	int i, j, k;
	unsigned long* p = MB86290PTR(pScrn)->bmp;

	for (j = y; j < y + h; j += 32) {
		for (i = x; i < x + w; i += 32) {
			int dx = i + 32 <= x + w ? 32 : x + w - i;
			int dy = j + 32 <= y + h ? 32 : y + h - j;
			MB86290WriteFifo(3, (GDC_TYPE_DRAWBITMAPP << 24) | (GDC_CMD_BITMAP << 16) | (dy + 2),
					 (j << 16) | i, (dy << 16) | dx);
			for (k = 0; k < dy; k += 8)
				MB86290WriteFifo(k + 8 < dy ? 8 : dy - k,
						 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
		}
	}
}

static void 
MB86290SetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
						  int fg, int bg,
						  int rop,
						  unsigned int planemask)
{
	long mdr4 = rop << 9;

	MB86290WriteFifo(2, (GDC_TYPE_SETCOLORREGISTER << 24) | (GDC_CMD_BODY_FORE_COLOR << 16), fg);
	if (bg == -1) {
		mdr4 |= GDC_BLT_TRANS_MODE_MASK;
		bg = fg + 1; /* bg == transient color */
		MB86290WriteFifo(2, (GDC_TYPE_SETREGISTER << 24) | (1 << 16) | GDC_REG_BLT_TRANS_COLOR,
			 bg);
	}
	MB86290WriteFifo(2, (GDC_TYPE_SETCOLORREGISTER << 24) | (GDC_CMD_BODY_BACK_COLOR << 16), bg);
	MB86290WriteDrawReg(GDC_REG_MODE_BITMAP, mdr4);
}

static void 
MB86290SubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
						    int x, int y, int w, int h,
						    int skipleft )
{
	MB86290PTR(pScrn)->left = x;
	MB86290PTR(pScrn)->cury = y;
	MB86290PTR(pScrn)->width = w;
}

static void 
MB86290SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
	int i;
	int width = MB86290PTR(pScrn)->width;

	for (i = 0; i < width; i += 32) {
		long bmp = xaaScanLineBuf[i/8+3] | (xaaScanLineBuf[i/8+2] << 8) |
			(xaaScanLineBuf[i/8+1] << 16) | (xaaScanLineBuf[i/8] << 24);
		MB86290WriteFifo(4, (GDC_TYPE_DRAWBITMAPP << 24) | (GDC_CMD_BITMAP << 16) | 3,
				 (MB86290PTR(pScrn)->cury << 16) | (MB86290PTR(pScrn)->left + i),
				 (1 << 16) | (i + 32 < width ? 32 : width - i),
				 bmp);
	}
	MB86290PTR(pScrn)->cury++;
}


