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
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xf86i2c.h"

#include "mb86290_driver.h"

#define	I2C_CLOCK_AND_ENABLE 		0x0000003fL
#define	I2C_START			0x00000010L
#define	I2C_REPEATED_START  		0x00000030L
#define	I2C_STOP			0x00000000L
#define	I2C_DISABLE			0x00000000L
#define	I2C_READY			0x01
#define	I2C_INT				0x01
#define	I2C_INTE			0x02
#define	I2C_ACK				0x08
#define	I2C_BER				0x80
#define	I2C_BEIE			0x40
#define I2C_TRX                         0x80
#define I2C_LRB                         0x10

static Bool 
MB86290WaitI2CEvent()
{
	long status;
	while (!((status = MB86290ReadI2CReg(GDC_I2C_REG_BUS_CONTROL)) & (I2C_INT | I2C_BER)));
	return !(status & I2C_BER);
}

static Bool
MB86290I2CAddress(I2CDevPtr d, I2CSlaveAddr addr)
{
	MB86290WriteI2CReg(GDC_I2C_REG_DATA, addr);
	MB86290WriteI2CReg(GDC_I2C_REG_CLOCK, I2C_CLOCK_AND_ENABLE);
	MB86290WriteI2CReg(GDC_I2C_REG_BUS_CONTROL, 
			   d->pI2CBus->DriverPrivate.val ? I2C_REPEATED_START : I2C_START);
	if (!MB86290WaitI2CEvent())
		return FALSE;
	d->pI2CBus->DriverPrivate.val = !(MB86290ReadI2CReg(GDC_I2C_REG_BUS_STATUS) & I2C_LRB);
	return d->pI2CBus->DriverPrivate.val;
}

static Bool
MB86290I2CPutByte(I2CDevPtr d, I2CByte data)
{
	MB86290WriteI2CReg(GDC_I2C_REG_DATA, data);
	MB86290WriteI2CReg(GDC_I2C_REG_BUS_CONTROL, I2C_START);
	if (!MB86290WaitI2CEvent())
		return FALSE;
	return !(MB86290ReadI2CReg(GDC_I2C_REG_BUS_STATUS) & I2C_LRB);
}

static Bool
MB86290I2CGetByte(I2CDevPtr d, I2CByte *data, Bool last)
{
	MB86290WriteI2CReg(GDC_I2C_REG_BUS_CONTROL, I2C_START | (last ? 0 : I2C_ACK));
	if (!MB86290WaitI2CEvent())
		return FALSE;
	*data = MB86290ReadI2CReg(GDC_I2C_REG_DATA);
	return TRUE;
}

static void
MB86290I2CStop(I2CDevPtr d)
{
	MB86290WriteI2CReg(GDC_I2C_REG_BUS_CONTROL, I2C_STOP);
	MB86290WriteI2CReg(GDC_I2C_REG_CLOCK, I2C_DISABLE);
	d->pI2CBus->DriverPrivate.val = 0;
}

Bool
MB86290I2CInit(ScrnInfoPtr pScrn)
{
	MB86290Ptr fPtr = MB86290PTR(pScrn);

	if (fPtr->I2C == NULL)
	{
		I2CBusPtr I2CPtr = xf86CreateI2CBusRec();
		if (I2CPtr == NULL)
		{
			return(FALSE);
		}

		I2CPtr->BusName    = "MB86290 I2C bus";
		I2CPtr->scrnIndex  = pScrn->scrnIndex;
		I2CPtr->I2CAddress = MB86290I2CAddress;
		I2CPtr->I2CPutByte = MB86290I2CPutByte;
		I2CPtr->I2CGetByte = MB86290I2CGetByte;
		I2CPtr->I2CStop = MB86290I2CStop;
		I2CPtr->DriverPrivate.val = 0; /* !0 if the bus has been already occupied by us */

		if (!xf86I2CBusInit(I2CPtr))
		{
			xf86DestroyI2CBusRec(I2CPtr, TRUE, TRUE);
			return(FALSE);
		}

		fPtr->I2C = I2CPtr;
	}

	return(TRUE);
}


