/*
 * DOS interrupt 25h handler
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "msdos.h"
#include "ldt.h"
#include "miscemu.h"
#include "drive.h"
#include "debug.h"

/**********************************************************************
 *	    INT_Int25Handler
 *
 * Handler for int 25h (absolute disk read).
 */
void WINAPI INT_Int25Handler( CONTEXT *context )
{
    BYTE *dataptr = PTR_SEG_OFF_TO_LIN( DS_reg(context), BX_reg(context) );
    DWORD begin, length;
    int fd;

    if (!DRIVE_IsValid(AL_reg(context)))
    {
        SET_CFLAG(context);
        AX_reg(context) = 0x0101;        /* unknown unit */
        return;
    }

    if (CX_reg(context) == 0xffff)
    {
        begin   = *(DWORD *)dataptr;
        length  = *(WORD *)(dataptr + 4);
        dataptr = (BYTE *)PTR_SEG_TO_LIN( *(SEGPTR *)(dataptr + 6) );
    }
    else
    {
        begin  = DX_reg(context);
        length = CX_reg(context);
    }
    dprintf_info(int, "int25: abs diskread, drive %d, sector %ld, "
                 "count %ld, buffer %d\n",
                 AL_reg(context), begin, length, (int) dataptr);

    if ((fd = DRIVE_OpenDevice( AL_reg(context), O_RDONLY )) != -1)
    {
        lseek( fd, begin * 512, SEEK_SET );
        /* FIXME: check errors */
        read( fd, dataptr, length * 512 );
        close( fd );
    }
    else
    {
        memset(dataptr, 0, length * 512);
        if (begin == 0 && length > 1) *(dataptr + 512) = 0xf8;
        if (begin == 1) *dataptr = 0xf8;
    }
    RESET_CFLAG(context);
}

