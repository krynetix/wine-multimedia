/*
 * Win32 user functions
 *
 * Copyright 1995 Martin von Loewis
 */

/* This file contains only wrappers to existing Wine functions or trivial
   stubs. 'Real' implementations go into context specific files */

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include "windows.h"
#include "winerror.h"
#include "heap.h"
#include "struct32.h"
#include "win.h"
#include "debug.h"

/***********************************************************************
 *          GetMessage32A   (USER32.269)
 */
BOOL32 WINAPI GetMessage32A(MSG32* lpmsg,HWND32 hwnd,UINT32 min,UINT32 max)
{
    BOOL32 ret;
    MSG16 *msg = SEGPTR_NEW(MSG16);
    if (!msg) return 0;
    ret=GetMessage16(SEGPTR_GET(msg),(HWND16)hwnd,min,max);
    /* FIXME */
    STRUCT32_MSG16to32(msg,lpmsg);
    SEGPTR_FREE(msg);
    return ret;
}

/***********************************************************************
 *          GetMessage32W   (USER32.273)
 */
BOOL32 WINAPI GetMessage32W(MSG32* lpmsg,HWND32 hwnd,UINT32 min,UINT32 max)
{
    BOOL32 ret;
    MSG16 *msg = SEGPTR_NEW(MSG16);
    if (!msg) return 0;
    ret=GetMessage16(SEGPTR_GET(msg),(HWND16)hwnd,min,max);
    /* FIXME */
    STRUCT32_MSG16to32(msg,lpmsg);
    SEGPTR_FREE(msg);
    return ret;
}

/***********************************************************************
 *         PeekMessageA
 */
BOOL32 WINAPI PeekMessage32A( LPMSG32 lpmsg, HWND32 hwnd,
                              UINT32 min,UINT32 max,UINT32 wRemoveMsg)
{
	MSG16 msg;
	BOOL32 ret;
	ret=PeekMessage16(&msg,hwnd,min,max,wRemoveMsg);
        /* FIXME: should translate the message to Win32 */
	STRUCT32_MSG16to32(&msg,lpmsg);
	return ret;
}

/***********************************************************************
 *         PeekMessageW
 */
BOOL32 WINAPI PeekMessage32W( LPMSG32 lpmsg, HWND32 hwnd,
                              UINT32 min,UINT32 max,UINT32 wRemoveMsg)
{
	/* FIXME: Should perform Unicode translation on specific messages */
	return PeekMessage32A(lpmsg,hwnd,min,max,wRemoveMsg);
}
