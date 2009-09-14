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

#ifndef __MB86290_DRIVER_H
#define __MB86290_DRIVER_H

/* -------------------------------------------------------------------- */
/* Our private data, and two functions to allocate/free this            */

typedef struct {
	unsigned char        *fbstart;
	unsigned char        *fbmem;
	CloseScreenProcPtr   CloseScreen;
	EntityInfoPtr        pEnt;
#ifdef XSERVER_LIBPCIACCESS
	struct pci_device    *PciInfo;
#else
	pciVideoPtr          PciInfo;
#endif
	OptionInfoPtr        Options;
	Bool                 AccelOn;
	XAAInfoRecPtr        Accel;

	I2CBusPtr            I2C;
	
	unsigned long        bmp[8];               /* Mono8x8Pattern expanded to 32x8 */
	int                  left, cury, width;    /* Color expand fill params */
} MB86290Rec, *MB86290Ptr;

#define MB86290PTR(p) ((MB86290Ptr)((p)->driverPrivate))

#define I2C_SAA7113    0x4a /* or 0x48 */

long MB86290ReadDrawReg(long off);
void MB86290WriteDrawReg(long off, long val);
long MB86290ReadGeoReg(long off);
void MB86290WriteGeoReg(long off, long val);
long MB86290ReadI2CReg(long off);
void MB86290WriteI2CReg(long off, long val);
long MB86290ReadCaptureReg(long off);
void MB86290WriteCaptureReg(long off, long val);
long MB86290ReadDispReg(long off);
void MB86290WriteDispReg(long off, long val);

Bool MB86290I2CInit(ScrnInfoPtr pScrn);

void MB86290InitVideo(ScreenPtr pScreen);

#endif /* __MB86290_DRIVER_H */
