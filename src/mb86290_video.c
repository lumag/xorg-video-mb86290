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

#include "gdcreg.h"

#include "xf86.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xf86i2c.h"
#include "xf86xv.h"
#include "X11/extensions/Xv.h"
#include "fourcc.h"

#include "mb86290_driver.h"

#define COUNTOF(a) (sizeof(a) / sizeof(a[0]))
#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

#define IMAGE_MAX_WIDTH     704
#define IMAGE_MAX_HEIGHT    570

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] =
{
	{
		0,
		"XV_IMAGE",
		IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
		{1, 1}
	}
};

static XF86VideoFormatRec Formats[] = 
{
	{ 15, TrueColor }
};

#define XV_ENCODING_NAME        "XV_ENCODING"
#define XV_BRIGHTNESS_NAME      "XV_BRIGHTNESS"
#define XV_CONTRAST_NAME        "XV_CONTRAST"
#define XV_SATURATION_NAME      "XV_SATURATION"
#define XV_HUE_NAME             "XV_HUE"

static XF86AttributeRec Attributes[] =
{
/*	{ XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY" }, */
	{ XvSettable | XvGettable, 0, 255, XV_BRIGHTNESS_NAME },
	{ XvSettable | XvGettable, -128, 127, XV_CONTRAST_NAME },
	{ XvSettable | XvGettable, -128, 127, XV_SATURATION_NAME },
	{ XvSettable | XvGettable, -128, 127, XV_HUE_NAME }
};

static Atom xvEncoding, xvBrightness, xvContrast, xvSaturation, xvHue;

#define FOURCC_RV15	0x35315652

static XF86ImageRec MB86290VideoImages[] =
{
	XVIMAGE_YUY2,
	XVIMAGE_YV12,
	XVIMAGE_I420,
	{
		FOURCC_RV15,					/* id						*/
		XvRGB,							/* type						*/
		LSBFirst,						/* byte_order				*/
		{ 'R', 'V' ,'1', '5',
		  0x00, '5',  0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00 },		/* guid						*/
		16,								/* bits_per_pixel			*/
		XvPacked,						/* format					*/
		1,								/* num_planes				*/
		15,								/* depth					*/
		0x001F, 0x03E0, 0x7C00,			/* red_mask, green, blue	*/
		0, 0, 0,						/* y_sample_bits, u, v		*/
		0, 0, 0,						/* horz_y_period, u, v		*/
		0, 0, 0,						/* vert_y_period, u, v		*/
		{ 'R', 'V', 'B' },				/* component_order			*/
		XvTopToBottom					/* scaline_order			*/
	}
};

typedef struct {
	FBLinearPtr  fbLinearPtr;
	CARD32       bufAddr[2];
	
	short drw_x, drw_y, drw_w, drw_h;
	short src_x, src_y, src_w, src_h;   
	int id;
	short srcPitch, height;
	
	unsigned char brightness;
	unsigned char contrast;
	CARD32       colorKey;
	CARD32       videoStatus;
	
	I2CDevRec    I2CDev;

	Bool         videoBeenShutdown;

/*    RegionRec    clip;

    Time         offTime;
    Time         freeTime;

    CARD32 displayMode;*/
} MB86290PortPrivRec, *MB86290PortPrivPtr;        

static unsigned char SAA7113InitData[] = {
        0x01, 0x08, 

        0x02, 0xc1, /* c7 s-video */ 
        0x03, 0x23, 
        0x04, 0x00, 
        0x05, 0x00, 
        0x06, 0xe9, /* eb */
        0x07, 0x0d, /* e0 */

        0x08, 0x98, /* 88 */ 
        0x09, 0x00, 
        0x0a, 0x80, 
        0x0b, 0x47, 
        0x0c, 0x40, 
        0x0d, 0x00,
        0x0e, 0x01, 
        0x0f, 0xa4, /* aa */
        
        0x10, 0x00, 
        0x11, 0x1C, 
        0x12, 0x01, 
        0x13, 0x00,
        0x15, 0x00,
        0x16, 0x00, 
        0x17, 0x00,

        0x40, 0x82,
        0x58, 0x00,
        0x59, 0x54,
        0x5a, 0x0a,
        0x5b, 0x83,
        0x5e, 0x00,
};

static int 
MB86290SetPortAttribute(ScrnInfoPtr pScrn, 
		    Atom attribute,
		    INT32 value, 
		    pointer data)
{
	ENTER();
	MB86290PortPrivPtr pPriv = (MB86290PortPrivPtr)data;
	I2CByte reg;

	if (attribute == xvEncoding) {
		LEAVE(Success);
	}
	else if (attribute == xvBrightness) {
		reg = 0x0A;
	}
	else if (attribute == xvContrast) {
		reg = 0x0B;
	}
	else if (attribute == xvSaturation) {
		reg = 0x0C;
	}
	else if (attribute == xvHue) {
		reg = 0x0D;
	}
	else {
		LEAVE(BadMatch);
	}

	if (xf86I2CWriteByte(&pPriv->I2CDev, reg, value))
		LEAVE(Success);
	LEAVE(BadMatch);
}

static int 
MB86290GetPortAttribute(ScrnInfoPtr pScrn, 
		    Atom attribute,
		    INT32 *value, 
		    pointer data)
{
	ENTER();
	MB86290PortPrivPtr pPriv = (MB86290PortPrivPtr)data;
	I2CByte reg, val;

	if (attribute == xvEncoding) {
		LEAVE(Success);
	}
	else if (attribute == xvBrightness) {
		reg = 0x0A;
	}
	else if (attribute == xvContrast) {
		reg = 0x0B;
	}
	else if (attribute == xvSaturation) {
		reg = 0x0C;
	}
	else if (attribute == xvHue) {
		reg = 0x0D;
	}
	else {
		LEAVE(BadMatch);
	}

	if (xf86I2CReadByte(&pPriv->I2CDev, reg, &val)) {
		*value = val;
		LEAVE(Success);
	}

	LEAVE(BadMatch);
}

static void 
MB86290QueryBestSize(ScrnInfoPtr pScrn, 
		 Bool motion,
		 short vid_w, short vid_h, 
		 short drw_w, short drw_h, 
		 unsigned int *p_w, unsigned int *p_h, 
		 pointer data)
{
	ENTER();
	*p_w = drw_w;
	*p_h = drw_h; 
	
	/* TODO: report the HW limitation */
	LEAVE();
}

static int
MB86290PutVideo(ScrnInfoPtr	pScrn,
		short		vid_x,
		short		vid_y,
		short		drw_x,
		short		drw_y,
		short		vid_w,
		short		vid_h,
		short		drw_w,
		short		drw_h,
		RegionPtr	clipBoxes,
		pointer		data,
		DrawablePtr	pDraw)
{
	ENTER();
	int l1_ext_mode = 0;
	MB86290PortPrivPtr pPriv = (MB86290PortPrivPtr)data;
	long off = pScrn->virtualX * pScrn->virtualY * 2 + pPriv->fbLinearPtr->offset;
	
	if (pPriv->videoBeenShutdown) {
		MB86290WriteCaptureReg(GDC_CAP_REG_MODE, 0x83100001);
		MB86290WriteCaptureReg(GDC_CAP_REG_BUFFER_MODE, 0x00160000);
		MB86290WriteCaptureReg(GDC_CAP_REG_BUFFER_SADDR, off);
		MB86290WriteCaptureReg(GDC_CAP_REG_BUFFER_EADDR, off + pPriv->fbLinearPtr->size);

		if ((vid_w >= drw_w) && (vid_h >= drw_h)) { /* downscaling */
			MB86290WriteCaptureReg(GDC_CAP_REG_SCALE, 
					       (((vid_h << 11) / drw_h) << 16) | ((vid_w << 11) / drw_w));
		}
		else if ((vid_w <= drw_w) && (vid_h <= drw_h)) { /* upscaling */
			l1_ext_mode = GDC_EXTEND_MAGNIFY_MODE;
			MB86290WriteCaptureReg(GDC_CAP_REG_SCALE, 
					       (((vid_h << 11) / drw_h) << 16) | ((vid_w << 11) / drw_w));
			MB86290WriteCaptureReg(GDC_CAP_REG_CMSS, ((vid_w >> 1) << 16) | vid_h);
			MB86290WriteCaptureReg(GDC_CAP_REG_CMDS, ((drw_w >> 1) << 16) | drw_h);
		}
		MB86290WriteDispReg(GDC_DISP_REG_L1_EXT_MODE, l1_ext_mode);
		MB86290WriteDispReg(GDC_DISP_REG_W_ORG_ADR, off);
	}
	
	/* doc doesn't say vid_y should be halved, but experiments do */
	MB86290WriteCaptureReg(GDC_CAP_REG_IMAGE_START, ((vid_y >> 1) << 16) | vid_x); 
	MB86290WriteCaptureReg(GDC_CAP_REG_IMAGE_END, ((vid_y + vid_h) << 16) | (vid_x + vid_w));
	MB86290WriteDispReg(GDC_DISP_REG_W_WIDTH, 0xF0160000);
	MB86290WriteDispReg(GDC_DISP_REG_L1_WIN_POS, (drw_y << 16) | drw_x);
	MB86290WriteDispReg(GDC_DISP_REG_L1_WIN_SIZE, (drw_h << 16) | drw_w);
 	MB86290WriteDispReg(GDC_DISP_REG_LAYER_SELECT, 1);

/*	MB86290WriteCaptureReg(GDC_CAP_REG_MODE, MB86290ReadCaptureReg(GDC_CAP_REG_MODE) | 0x80000000);*/
	MB86290WriteDispReg(GDC_DISP_REG_EXT_MODE, MB86290ReadDispReg(GDC_DISP_REG_EXT_MODE) | 0x80020000);

	LEAVE(Success);
}

static void 
MB86290StopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
	ENTER();
	MB86290PortPrivPtr pPriv = (MB86290PortPrivPtr)data;

	if (shutdown)
		MB86290WriteCaptureReg(GDC_CAP_REG_MODE,
				       MB86290ReadCaptureReg(GDC_CAP_REG_MODE) 
				       & ~0x80000000);
	MB86290WriteDispReg(GDC_DISP_REG_EXT_MODE,
			    MB86290ReadDispReg(GDC_DISP_REG_EXT_MODE) 
			    & ~0x00020000);
	pPriv->videoBeenShutdown = shutdown;
	LEAVE();
}

static XF86VideoAdaptorPtr 
MB86290SetupVideo(ScreenPtr pScreen)
{
	ENTER();
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MB86290Ptr fPtr = MB86290PTR(pScrn);
	XF86VideoAdaptorPtr adapt;
	MB86290PortPrivPtr pPriv;
	
	if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			     sizeof(MB86290PortPrivRec) +
			     sizeof(DevUnion))))
		LEAVE(NULL);
	
	adapt->type = XvWindowMask | XvInputMask | XvVideoMask;
	adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT | VIDEO_NO_CLIPPING;
	adapt->name = "MB86290 Video Overlay";
	adapt->nEncodings = COUNTOF(DummyEncoding);
	adapt->pEncodings = DummyEncoding;
	adapt->nFormats = COUNTOF(Formats);
	adapt->pFormats = Formats;
	adapt->nPorts = 1;
	adapt->pPortPrivates = (DevUnion*)(&adapt[1]);
	
	pPriv = (MB86290PortPrivPtr)(&adapt->pPortPrivates[1]);
    
	adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
	adapt->nAttributes = COUNTOF(Attributes);
	adapt->pAttributes = Attributes;
	adapt->nImages = COUNTOF(MB86290VideoImages);
	adapt->pImages = MB86290VideoImages;
	adapt->PutVideo = MB86290PutVideo;
	adapt->PutStill = NULL;
	adapt->GetVideo = NULL;
	adapt->GetStill = NULL;
	adapt->StopVideo = MB86290StopVideo;
	adapt->SetPortAttribute = MB86290SetPortAttribute;
	adapt->GetPortAttribute = MB86290GetPortAttribute;
	adapt->QueryBestSize = MB86290QueryBestSize;
	adapt->PutImage = NULL;
	adapt->QueryImageAttributes = NULL;

	pPriv->videoBeenShutdown = TRUE;
	
	pPriv->colorKey = 0x000101fe;
	pPriv->videoStatus = 0;
	pPriv->brightness = 0;
	pPriv->contrast = 128;
	
	pPriv->I2CDev.DevName = "SAA 7113";
	pPriv->I2CDev.SlaveAddr = I2C_SAA7113;
	pPriv->I2CDev.pI2CBus = fPtr->I2C;

	pPriv->fbLinearPtr = xf86AllocateOffscreenLinear(pScreen, pScrn->virtualX * pScrn->virtualY * 2 * 2.2, 0,
							 NULL, NULL, NULL);

	if (NULL == pPriv->fbLinearPtr
	    || !xf86I2CDevInit(&pPriv->I2CDev)
	    || !xf86I2CWriteVec(&pPriv->I2CDev, SAA7113InitData, COUNTOF(SAA7113InitData) / 2)) {
		xfree(adapt);
		LEAVE(NULL);
	}

	xvEncoding   = MAKE_ATOM(XV_ENCODING_NAME);
	xvHue        = MAKE_ATOM(XV_HUE_NAME);
	xvSaturation = MAKE_ATOM(XV_SATURATION_NAME);
	xvBrightness = MAKE_ATOM(XV_BRIGHTNESS_NAME);
	xvContrast   = MAKE_ATOM(XV_CONTRAST_NAME);

	LEAVE(adapt);
}

void
MB86290InitVideo(ScreenPtr pScreen)
{
	ENTER();
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	XF86VideoAdaptorPtr *ptrAdaptors, *newAdaptors = NULL;
	XF86VideoAdaptorPtr newAdaptor = NULL;
	int numAdaptors;
	MB86290Ptr  fPtr  = MB86290PTR(pScrn);
	
	if (!xf86I2CProbeAddress(fPtr->I2C, I2C_SAA7113))
		LEAVE();

	numAdaptors = xf86XVListGenericAdaptors(pScrn, &ptrAdaptors);
	
	newAdaptor = MB86290SetupVideo(pScreen);
	
	if (newAdaptor != NULL) {
		if (numAdaptors == 0) {
			numAdaptors = 1;
			ptrAdaptors = &newAdaptor;
		}
		else {
			newAdaptors = xalloc((numAdaptors + 1) *
					     sizeof(XF86VideoAdaptorPtr*));
			if (newAdaptors != NULL) {
				memcpy(newAdaptors, ptrAdaptors,
				       numAdaptors * sizeof(XF86VideoAdaptorPtr));
				newAdaptors[numAdaptors++] = newAdaptor;
				ptrAdaptors = newAdaptors;
			}
		}
	}
	
	if (numAdaptors != 0) {
		xf86XVScreenInit(pScreen, ptrAdaptors, numAdaptors);
	}
	
	if (newAdaptors != NULL) {
		xfree(newAdaptors);
	}
	LEAVE();
}


