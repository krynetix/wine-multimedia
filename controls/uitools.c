/*
 * User Interface Functions
 *
 * Copyright 1997 Dimitrie O. Paun
 * Copyright 1997 Bertho A. Stultiens
 */

#include <stdio.h>
#include "windows.h"
#include "debug.h"

static const WORD wPattern_AA55[8] = { 0xaaaa, 0x5555, 0xaaaa, 0x5555,
                                       0xaaaa, 0x5555, 0xaaaa, 0x5555 };

/* These tables are used in:
 * UITOOLS_DrawDiagEdge()
 * UITOOLS_DrawRectEdge()
 */
static const char LTInnerNormal[] = {
    -1,           -1,                 -1,                 -1,
    -1,           COLOR_BTNHIGHLIGHT, COLOR_BTNHIGHLIGHT, -1,
    -1,           COLOR_3DDKSHADOW,   COLOR_3DDKSHADOW,   -1,
    -1,           -1,                 -1,                 -1
};

static const char LTOuterNormal[] = {
    -1,                 COLOR_3DLIGHT,     COLOR_BTNSHADOW, -1,
    COLOR_BTNHIGHLIGHT, COLOR_3DLIGHT,     COLOR_BTNSHADOW, -1,
    COLOR_3DDKSHADOW,   COLOR_3DLIGHT,     COLOR_BTNSHADOW, -1,
    -1,                 COLOR_3DLIGHT,     COLOR_BTNSHADOW, -1
};

static const char RBInnerNormal[] = {
    -1,           -1,                -1,              -1,
    -1,           COLOR_BTNSHADOW,   COLOR_BTNSHADOW, -1,
    -1,           COLOR_3DLIGHT,     COLOR_3DLIGHT,   -1,
    -1,           -1,                -1,              -1
};

static const char RBOuterNormal[] = {
    -1,              COLOR_3DDKSHADOW,  COLOR_BTNHIGHLIGHT, -1,
    COLOR_BTNSHADOW, COLOR_3DDKSHADOW,  COLOR_BTNHIGHLIGHT, -1,
    COLOR_3DLIGHT,   COLOR_3DDKSHADOW,  COLOR_BTNHIGHLIGHT, -1,
    -1,              COLOR_3DDKSHADOW,  COLOR_BTNHIGHLIGHT, -1
};

static const char LTInnerSoft[] = {
    -1,                  -1,                -1,              -1,
    -1,                  COLOR_3DLIGHT,     COLOR_3DLIGHT,   -1,
    -1,                  COLOR_BTNSHADOW,   COLOR_BTNSHADOW, -1,
    -1,                  -1,                -1,              -1
};

static const char LTOuterSoft[] = {
    -1,              COLOR_BTNHIGHLIGHT, COLOR_3DDKSHADOW, -1,
    COLOR_3DLIGHT,   COLOR_BTNHIGHLIGHT, COLOR_3DDKSHADOW, -1,
    COLOR_BTNSHADOW, COLOR_BTNHIGHLIGHT, COLOR_3DDKSHADOW, -1,
    -1,              COLOR_BTNHIGHLIGHT, COLOR_3DDKSHADOW, -1
};

#define RBInnerSoft RBInnerNormal   /* These are the same */
#define RBOuterSoft RBOuterNormal

static const char LTRBOuterMono[] = {
    -1,           COLOR_WINDOWFRAME, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME,
    COLOR_WINDOW, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME,
    COLOR_WINDOW, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME,
    COLOR_WINDOW, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME,
};

static const char LTRBInnerMono[] = {
    -1, -1,           -1,           -1,
    -1, COLOR_WINDOW, COLOR_WINDOW, COLOR_WINDOW,
    -1, COLOR_WINDOW, COLOR_WINDOW, COLOR_WINDOW,
    -1, COLOR_WINDOW, COLOR_WINDOW, COLOR_WINDOW,
};

static const char LTRBOuterFlat[] = {
    -1,                COLOR_BTNSHADOW, COLOR_BTNSHADOW, COLOR_BTNSHADOW,
    COLOR_WINDOWFRAME, COLOR_BTNSHADOW, COLOR_BTNSHADOW, COLOR_BTNSHADOW,
    COLOR_WINDOWFRAME, COLOR_BTNSHADOW, COLOR_BTNSHADOW, COLOR_BTNSHADOW,
    COLOR_WINDOWFRAME, COLOR_BTNSHADOW, COLOR_BTNSHADOW, COLOR_BTNSHADOW,
};

static const char LTRBInnerFlat[] = {
    -1, -1,              -1,              -1,
    -1, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME,
    -1, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME,
    -1, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME, COLOR_WINDOWFRAME,
};

/***********************************************************************
 *           UITOOLS_DrawDiagEdge
 *
 * Same as DrawEdge, but with BF_DIAGONAL
 * I tested it extensively and as far as I can tell it is identical to the
 * implementaion in Win95.
 * I do not like that I create and
 * use the 3 Pens to draw the diagonals. It would be better to draw them
 * using the brushes returned by GetSysColorBrush func, but I did not have
 * the patience to implement that yet.
 */
/*********************************************************************
 * 03-Dec-1997: Changed by Bertho Stultiens
 *
 * See also comments with UITOOLS_DrawRectEdge()
 */
static BOOL32 UITOOLS_DrawDiagEdge(HDC32 hdc, LPRECT32 rc, UINT32 uType, UINT32 uFlags)
{
    POINT32 Points[4];
    char InnerI, OuterI;
    HPEN32 InnerPen, OuterPen;
    POINT32 SavePoint;
    HPEN32 SavePen;
    int spx, spy;
    int epx, epy;
    int Width = rc->right - rc->left;
    int Height= rc->bottom - rc->top;
    int SmallDiam = Width > Height ? Height : Width;
    BOOL32 retval = !(   ((uType & BDR_INNER) == BDR_INNER
                       || (uType & BDR_OUTER) == BDR_OUTER)
                      && !(uFlags & (BF_FLAT|BF_MONO)) );
    int add = (LTRBInnerMono[uType & (BDR_INNER|BDR_OUTER)] != -1 ? 1 : 0)
            + (LTRBOuterMono[uType & (BDR_INNER|BDR_OUTER)] != -1 ? 1 : 0);

    /* Init some vars */
    OuterPen = InnerPen = (HPEN32)GetStockObject32(NULL_PEN);
    SavePen = (HPEN32)SelectObject32(hdc, InnerPen);
    spx = spy = epx = epy = 0; /* Satisfy the compiler... */
    
    /* Determine the colors of the edges */
    if(uFlags & BF_MONO)
    {
        InnerI = LTRBInnerMono[uType & (BDR_INNER|BDR_OUTER)];
        OuterI = LTRBOuterMono[uType & (BDR_INNER|BDR_OUTER)];
    }
    else if(uFlags & BF_FLAT)
    {
        InnerI = LTRBInnerFlat[uType & (BDR_INNER|BDR_OUTER)];
        OuterI = LTRBOuterFlat[uType & (BDR_INNER|BDR_OUTER)];
    }
    else if(uFlags & BF_SOFT)
    {
        if(uFlags & BF_BOTTOM)
        {
            InnerI = RBInnerSoft[uType & (BDR_INNER|BDR_OUTER)];
            OuterI = RBOuterSoft[uType & (BDR_INNER|BDR_OUTER)];
        }
        else
        {
            InnerI = LTInnerSoft[uType & (BDR_INNER|BDR_OUTER)];
            OuterI = LTOuterSoft[uType & (BDR_INNER|BDR_OUTER)];
        }
    }
    else
    {
        if(uFlags & BF_BOTTOM)
        {
            InnerI = RBInnerNormal[uType & (BDR_INNER|BDR_OUTER)];
            OuterI = RBOuterNormal[uType & (BDR_INNER|BDR_OUTER)];
        }
        else
        {
            InnerI = LTInnerNormal[uType & (BDR_INNER|BDR_OUTER)];
            OuterI = LTOuterNormal[uType & (BDR_INNER|BDR_OUTER)];
        }
    }

    if(InnerI != -1) InnerPen = GetSysColorPen32(InnerI);
    if(OuterI != -1) OuterPen = GetSysColorPen32(OuterI);

    MoveToEx32(hdc, 0, 0, &SavePoint);

    /* Don't ask me why, but this is what is visible... */
    /* This must be possible to do much simpler, but I fail to */
    /* see the logic in the MS implementation (sigh...). */
    /* So, this might look a bit brute force here (and it is), but */
    /* it gets the job done;) */

    switch(uFlags & BF_RECT)
    {
    case 0:
    case BF_LEFT:
    case BF_BOTTOM:
    case BF_BOTTOMLEFT:
        /* Left bottom endpoint */
        epx = rc->left-1;
        spx = epx + SmallDiam;
        epy = rc->bottom;
        spy = epy - SmallDiam;
        break;

    case BF_TOPLEFT:
    case BF_BOTTOMRIGHT:
        /* Left top endpoint */
        epx = rc->left-1;
        spx = epx + SmallDiam;
        epy = rc->top-1;
        spy = epy + SmallDiam;
        break;

    case BF_TOP:
    case BF_RIGHT:
    case BF_TOPRIGHT:
    case BF_RIGHT|BF_LEFT:
    case BF_RIGHT|BF_LEFT|BF_TOP:
    case BF_BOTTOM|BF_TOP:
    case BF_BOTTOM|BF_TOP|BF_LEFT:
    case BF_BOTTOMRIGHT|BF_LEFT:
    case BF_BOTTOMRIGHT|BF_TOP:
    case BF_RECT:
        /* Right top endpoint */
        spx = rc->left;
        epx = spx + SmallDiam;
        spy = rc->bottom-1;
        epy = spy - SmallDiam;
        break;
    }

    MoveToEx32(hdc, spx, spy, NULL);
    SelectObject32(hdc, OuterPen);
    LineTo32(hdc, epx, epy);

    SelectObject32(hdc, InnerPen);

    switch(uFlags & (BF_RECT|BF_DIAGONAL))
    {
    case BF_DIAGONAL_ENDBOTTOMLEFT:
    case (BF_DIAGONAL|BF_BOTTOM):
    case BF_DIAGONAL:
    case (BF_DIAGONAL|BF_LEFT):
        MoveToEx32(hdc, spx-1, spy, NULL);
        LineTo32(hdc, epx, epy-1);
        Points[0].x = spx-add;
        Points[0].y = spy;
        Points[1].x = rc->left;
        Points[1].y = rc->top;
        Points[2].x = epx+1;
        Points[2].y = epy-1-add;
        Points[3] = Points[2];
        break;

    case BF_DIAGONAL_ENDBOTTOMRIGHT:
        MoveToEx32(hdc, spx-1, spy, NULL);
        LineTo32(hdc, epx, epy+1);
        Points[0].x = spx-add;
        Points[0].y = spy;
        Points[1].x = rc->left;
        Points[1].y = rc->bottom-1;
        Points[2].x = epx+1;
        Points[2].y = epy+1+add;
        Points[3] = Points[2];
        break;

    case (BF_DIAGONAL|BF_BOTTOM|BF_RIGHT|BF_TOP):
    case (BF_DIAGONAL|BF_BOTTOM|BF_RIGHT|BF_TOP|BF_LEFT):
    case BF_DIAGONAL_ENDTOPRIGHT:
    case (BF_DIAGONAL|BF_RIGHT|BF_TOP|BF_LEFT):
        MoveToEx32(hdc, spx+1, spy, NULL);
        LineTo32(hdc, epx, epy+1);
        Points[0].x = epx-1;
        Points[0].y = epy+1+add;
        Points[1].x = rc->right-1;
        Points[1].y = rc->top+add;
        Points[2].x = rc->right-1;
        Points[2].y = rc->bottom-1;
        Points[3].x = spx+add;
        Points[3].y = spy;
        break;

    case BF_DIAGONAL_ENDTOPLEFT:
        MoveToEx32(hdc, spx, spy-1, NULL);
        LineTo32(hdc, epx+1, epy);
        Points[0].x = epx+1+add;
        Points[0].y = epy+1;
        Points[1].x = rc->right-1;
        Points[1].y = rc->top;
        Points[2].x = rc->right-1;
        Points[2].y = rc->bottom-1-add;
        Points[3].x = spx;
        Points[3].y = spy-add;
        break;

    case (BF_DIAGONAL|BF_TOP):
    case (BF_DIAGONAL|BF_BOTTOM|BF_TOP):
    case (BF_DIAGONAL|BF_BOTTOM|BF_TOP|BF_LEFT):
        MoveToEx32(hdc, spx+1, spy-1, NULL);
        LineTo32(hdc, epx, epy);
        Points[0].x = epx-1;
        Points[0].y = epy+1;
        Points[1].x = rc->right-1;
        Points[1].y = rc->top;
        Points[2].x = rc->right-1;
        Points[2].y = rc->bottom-1-add;
        Points[3].x = spx+add;
        Points[3].y = spy-add;
        break;

    case (BF_DIAGONAL|BF_RIGHT):
    case (BF_DIAGONAL|BF_RIGHT|BF_LEFT):
    case (BF_DIAGONAL|BF_RIGHT|BF_LEFT|BF_BOTTOM):
        MoveToEx32(hdc, spx, spy, NULL);
        LineTo32(hdc, epx-1, epy+1);
        Points[0].x = spx;
        Points[0].y = spy;
        Points[1].x = rc->left;
        Points[1].y = rc->top+add;
        Points[2].x = epx-1-add;
        Points[2].y = epy+1+add;
        Points[3] = Points[2];
        break;
    }

    /* Fill the interior if asked */
    if((uFlags & BF_MIDDLE) && retval)
    {
        HBRUSH32 hbsave;
        HBRUSH32 hb = GetSysColorBrush32(uFlags & BF_MONO ? COLOR_WINDOW
					 :COLOR_BTNFACE);
        HPEN32 hpsave;
        HPEN32 hp = GetSysColorPen32(uFlags & BF_MONO ? COLOR_WINDOW
				     : COLOR_BTNFACE);
        hbsave = (HBRUSH32)SelectObject32(hdc, hb);
        hpsave = (HPEN32)SelectObject32(hdc, hp);
        Polygon32(hdc, Points, 4);
        SelectObject32(hdc, hbsave);
        SelectObject32(hdc, hpsave);
    }

    /* Adjust rectangle if asked */
    if(uFlags & BF_ADJUST)
    {
        if(uFlags & BF_LEFT)   rc->left   += add;
        if(uFlags & BF_RIGHT)  rc->right  -= add;
        if(uFlags & BF_TOP)    rc->top    += add;
        if(uFlags & BF_BOTTOM) rc->bottom -= add;
    }

    /* Cleanup */
    SelectObject32(hdc, SavePen);
    MoveToEx32(hdc, SavePoint.x, SavePoint.y, NULL);

    return retval;
}

/***********************************************************************
 *           UITOOLS_DrawRectEdge
 *
 * Same as DrawEdge, but without BF_DIAGONAL
 * I tested this function and it works very well. You should not change it
 * unless you find a bug. If you don't like the colors, it it not its 
 * fault - the system colors are not OK.
 * Again, I tested this function on Win95 and I compared the output with the
 * one generated by the native DrawEdge and it is identical on all cases that
 * I tried, and I tried quite a few.
 */
/*********************************************************************
 * 23-Nov-1997: Changed by Bertho Stultiens
 *
 * Well, I started testing this and found out that there are a few things
 * that weren't quite as win95. The following rewrite should reproduce
 * win95 results completely.
 * The colorselection is table-driven to avoid awfull if-statements.
 * The table below show the color settings.
 *
 * Pen selection table for uFlags = 0
 *
 * uType |  LTI  |  LTO  |  RBI  |  RBO
 * ------+-------+-------+-------+-------
 *  0000 |   x   |   x   |   x   |   x
 *  0001 |   x   |  22   |   x   |  21
 *  0010 |   x   |  16   |   x   |  20
 *  0011 |   x   |   x   |   x   |   x
 * ------+-------+-------+-------+-------
 *  0100 |   x   |  20   |   x   |  16
 *  0101 |  20   |  22   |  16   |  21
 *  0110 |  20   |  16   |  16   |  20
 *  0111 |   x   |   x   |   x   |   x
 * ------+-------+-------+-------+-------
 *  1000 |   x   |  21   |   x   |  22
 *  1001 |  21   |  22   |  22   |  21
 *  1010 |  21   |  16   |  22   |  20
 *  1011 |   x   |   x   |   x   |   x
 * ------+-------+-------+-------+-------
 *  1100 |   x   |   x   |   x   |   x
 *  1101 |   x   | x (22)|   x   | x (21)
 *  1110 |   x   | x (16)|   x   | x (20)
 *  1111 |   x   |   x   |   x   |   x
 *
 * Pen selection table for uFlags = BF_SOFT
 *
 * uType |  LTI  |  LTO  |  RBI  |  RBO
 * ------+-------+-------+-------+-------
 *  0000 |   x   |   x   |   x   |   x
 *  0001 |   x   |  20   |   x   |  21
 *  0010 |   x   |  21   |   x   |  20
 *  0011 |   x   |   x   |   x   |   x
 * ------+-------+-------+-------+-------
 *  0100 |   x   |  22   |   x   |  16
 *  0101 |  22   |  20   |  16   |  21
 *  0110 |  22   |  21   |  16   |  20
 *  0111 |   x   |   x   |   x   |   x
 * ------+-------+-------+-------+-------
 *  1000 |   x   |  16   |   x   |  22
 *  1001 |  16   |  20   |  22   |  21
 *  1010 |  16   |  21   |  22   |  20
 *  1011 |   x   |   x   |   x   |   x
 * ------+-------+-------+-------+-------
 *  1100 |   x   |   x   |   x   |   x
 *  1101 |   x   | x (20)|   x   | x (21)
 *  1110 |   x   | x (21)|   x   | x (20)
 *  1111 |   x   |   x   |   x   |   x
 *
 * x = don't care; (n) = is what win95 actually uses
 * LTI = left Top Inner line
 * LTO = left Top Outer line
 * RBI = Right Bottom Inner line
 * RBO = Right Bottom Outer line
 * 15 = COLOR_BTNFACE
 * 16 = COLOR_BTNSHADOW
 * 20 = COLOR_BTNHIGHLIGHT
 * 21 = COLOR_3DDKSHADOW
 * 22 = COLOR_3DLIGHT
 */


static BOOL32 UITOOLS_DrawRectEdge(HDC32 hdc, LPRECT32 rc, UINT32 uType, UINT32 uFlags)
{
    char LTInnerI, LTOuterI;
    char RBInnerI, RBOuterI;
    HPEN32 LTInnerPen, LTOuterPen;
    HPEN32 RBInnerPen, RBOuterPen;
    RECT32 InnerRect = *rc;
    POINT32 SavePoint;
    HPEN32 SavePen;
    int LBpenplus = 0;
    int LTpenplus = 0;
    int RTpenplus = 0;
    int RBpenplus = 0;
    BOOL32 retval = !(   ((uType & BDR_INNER) == BDR_INNER
                       || (uType & BDR_OUTER) == BDR_OUTER)
                      && !(uFlags & (BF_FLAT|BF_MONO)) );
        
    /* Init some vars */
    LTInnerPen = LTOuterPen = RBInnerPen = RBOuterPen = (HPEN32)GetStockObject32(NULL_PEN);
    SavePen = (HPEN32)SelectObject32(hdc, LTInnerPen);

    /* Determine the colors of the edges */
    if(uFlags & BF_MONO)
    {
        LTInnerI = RBInnerI = LTRBInnerMono[uType & (BDR_INNER|BDR_OUTER)];
        LTOuterI = RBOuterI = LTRBOuterMono[uType & (BDR_INNER|BDR_OUTER)];
    }
    else if(uFlags & BF_FLAT)
    {
        LTInnerI = RBInnerI = LTRBInnerFlat[uType & (BDR_INNER|BDR_OUTER)];
        LTOuterI = RBOuterI = LTRBOuterFlat[uType & (BDR_INNER|BDR_OUTER)];
    }
    else if(uFlags & BF_SOFT)
    {
        LTInnerI = LTInnerSoft[uType & (BDR_INNER|BDR_OUTER)];
        LTOuterI = LTOuterSoft[uType & (BDR_INNER|BDR_OUTER)];
        RBInnerI = RBInnerSoft[uType & (BDR_INNER|BDR_OUTER)];
        RBOuterI = RBOuterSoft[uType & (BDR_INNER|BDR_OUTER)];
    }
    else
    {
        LTInnerI = LTInnerNormal[uType & (BDR_INNER|BDR_OUTER)];
        LTOuterI = LTOuterNormal[uType & (BDR_INNER|BDR_OUTER)];
        RBInnerI = RBInnerNormal[uType & (BDR_INNER|BDR_OUTER)];
        RBOuterI = RBOuterNormal[uType & (BDR_INNER|BDR_OUTER)];
    }

    if((uFlags & BF_BOTTOMLEFT) == BF_BOTTOMLEFT)   LBpenplus = 1;
    if((uFlags & BF_TOPRIGHT) == BF_TOPRIGHT)       RTpenplus = 1;
    if((uFlags & BF_BOTTOMRIGHT) == BF_BOTTOMRIGHT) RBpenplus = 1;
    if((uFlags & BF_TOPLEFT) == BF_TOPLEFT)         LTpenplus = 1;

    if(LTInnerI != -1) LTInnerPen = GetSysColorPen32(LTInnerI);
    if(LTOuterI != -1) LTOuterPen = GetSysColorPen32(LTOuterI);
    if(RBInnerI != -1) RBInnerPen = GetSysColorPen32(RBInnerI);
    if(RBOuterI != -1) RBOuterPen = GetSysColorPen32(RBOuterI);

    if((uFlags & BF_MIDDLE) && retval)
    {
        FillRect32(hdc, &InnerRect, GetSysColorBrush32(uFlags & BF_MONO ? 
					    COLOR_WINDOW : COLOR_BTNFACE));
    }

    MoveToEx32(hdc, 0, 0, &SavePoint);

    /* Draw the outer edge */
    SelectObject32(hdc, LTOuterPen);
    if(uFlags & BF_TOP)
    {
        MoveToEx32(hdc, InnerRect.left, InnerRect.top, NULL);
        LineTo32(hdc, InnerRect.right, InnerRect.top);
    }
    if(uFlags & BF_LEFT)
    {
        MoveToEx32(hdc, InnerRect.left, InnerRect.top, NULL);
        LineTo32(hdc, InnerRect.left, InnerRect.bottom);
    }
    SelectObject32(hdc, RBOuterPen);
    if(uFlags & BF_BOTTOM)
    {
        MoveToEx32(hdc, InnerRect.right-1, InnerRect.bottom-1, NULL);
        LineTo32(hdc, InnerRect.left-1, InnerRect.bottom-1);
    }
    if(uFlags & BF_RIGHT)
    {
        MoveToEx32(hdc, InnerRect.right-1, InnerRect.bottom-1, NULL);
        LineTo32(hdc, InnerRect.right-1, InnerRect.top-1);
    }

    /* Draw the inner edge */
    SelectObject32(hdc, LTInnerPen);
    if(uFlags & BF_TOP)
    {
        MoveToEx32(hdc, InnerRect.left+LTpenplus, InnerRect.top+1, NULL);
        LineTo32(hdc, InnerRect.right-RTpenplus, InnerRect.top+1);
    }
    if(uFlags & BF_LEFT)
    {
        MoveToEx32(hdc, InnerRect.left+1, InnerRect.top+LTpenplus, NULL);
        LineTo32(hdc, InnerRect.left+1, InnerRect.bottom-LBpenplus);
    }
    SelectObject32(hdc, RBInnerPen);
    if(uFlags & BF_BOTTOM)
    {
        MoveToEx32(hdc, InnerRect.right-1-RBpenplus, InnerRect.bottom-2, NULL);
        LineTo32(hdc, InnerRect.left-1+LBpenplus, InnerRect.bottom-2);
    }
    if(uFlags & BF_RIGHT)
    {
        MoveToEx32(hdc, InnerRect.right-2, InnerRect.bottom-1-RBpenplus, NULL);
        LineTo32(hdc, InnerRect.right-2, InnerRect.top-1+RTpenplus);
    }

    /* Adjust rectangle if asked */
    if(uFlags & BF_ADJUST)
    {
        int add = (LTRBInnerMono[uType & (BDR_INNER|BDR_OUTER)] != -1 ? 1 : 0)
                + (LTRBOuterMono[uType & (BDR_INNER|BDR_OUTER)] != -1 ? 1 : 0);
        if(uFlags & BF_LEFT)   rc->left   += add;
        if(uFlags & BF_RIGHT)  rc->right  -= add;
        if(uFlags & BF_TOP)    rc->top    += add;
        if(uFlags & BF_BOTTOM) rc->bottom -= add;
    }

    /* Cleanup */
    SelectObject32(hdc, SavePen);
    MoveToEx32(hdc, SavePoint.x, SavePoint.y, NULL);
    return retval;
}


/**********************************************************************
 *          DrawEdge16   (USER.659)
 */
BOOL16 WINAPI DrawEdge16( HDC16 hdc, LPRECT16 rc, UINT16 edge, UINT16 flags )
{
    RECT32 rect32;
    BOOL32 ret;

    CONV_RECT16TO32( rc, &rect32 );
    ret = DrawEdge32( hdc, &rect32, edge, flags );
    CONV_RECT32TO16( &rect32, rc );
    return ret;
}

/**********************************************************************
 *          DrawEdge32   (USER32.154)
 */
BOOL32 WINAPI DrawEdge32( HDC32 hdc, LPRECT32 rc, UINT32 edge, UINT32 flags )
{
    dprintf_info(graphics, "DrawEdge: %04x %d,%d-%d,%d %04x %04x\n",
                      hdc, rc->left, rc->top, rc->right, rc->bottom,
                      edge, flags );

    if(flags & BF_DIAGONAL)
      return UITOOLS_DrawDiagEdge(hdc, rc, edge, flags);
    else
      return UITOOLS_DrawRectEdge(hdc, rc, edge, flags);
}


/************************************************************************
 *	UITOOLS_MakeSquareRect
 *
 * Utility to create a square rectangle and returning the width
 */
static int UITOOLS_MakeSquareRect(LPRECT32 src, LPRECT32 dst)
{
    int Width  = src->right - src->left;
    int Height = src->bottom - src->top;
    int SmallDiam = Width > Height ? Height : Width;

    *dst = *src;

    /* Make it a square box */
    if(Width < Height)      /* SmallDiam == Width */
    {
        dst->top += (Height-Width)/2;
        dst->bottom = dst->top + SmallDiam;
    }
    else if(Width > Height) /* SmallDiam == Height */
    {
        dst->left += (Width-Height)/2;
        dst->right = dst->left + SmallDiam;
    }

   return SmallDiam;
}


/************************************************************************
 *	UITOOLS_DFC_ButtonPush
 *
 * Draw a push button coming from DrawFrameControl()
 *
 * Does a pretty good job in emulating MS behavior. Some quirks are
 * however there because MS uses a TrueType font (Marlett) to draw
 * the buttons.
 */
static BOOL32 UITOOLS_DFC_ButtonPush(HDC32 dc, LPRECT32 r, UINT32 uFlags)
{
    UINT32 edge;
    RECT32 myr = *r;

    if(uFlags & (DFCS_PUSHED | DFCS_CHECKED | DFCS_FLAT))
        edge = EDGE_SUNKEN;
    else
        edge = EDGE_RAISED;

    if(uFlags & DFCS_CHECKED)
    {
        if(uFlags & DFCS_MONO)
            UITOOLS_DrawRectEdge(dc, &myr, edge, BF_MONO|BF_RECT|BF_ADJUST);
        else
            UITOOLS_DrawRectEdge(dc, &myr, edge, (uFlags&DFCS_FLAT)|BF_RECT|BF_SOFT|BF_ADJUST);

        if(GetSysColor32(COLOR_BTNHIGHLIGHT) == RGB(255, 255, 255))
        {
            HBITMAP32 hbm = CreateBitmap32(8, 8, 1, 1, wPattern_AA55);
            HBRUSH32 hbsave;
            HBRUSH32 hb = CreatePatternBrush32(hbm);

            FillRect32(dc, &myr, GetSysColorBrush32(COLOR_BTNFACE));
            hbsave = (HBRUSH32)SelectObject32(dc, hb);
            PatBlt32(dc, myr.left, myr.top, myr.right-myr.left, myr.bottom-myr.top, 0x00FA0089);
            SelectObject32(dc, hbsave);
            DeleteObject32(hb);
            DeleteObject32(hbm);
        }
        else
        {
            FillRect32(dc, &myr, GetSysColorBrush32(COLOR_BTNHIGHLIGHT));
        }
    }
    else
    {
        if(uFlags & DFCS_MONO)
        {
            UITOOLS_DrawRectEdge(dc, &myr, edge, BF_MONO|BF_RECT|BF_ADJUST);
            FillRect32(dc, &myr, GetSysColorBrush32(COLOR_BTNFACE));
        }
        else
        {
            UITOOLS_DrawRectEdge(dc, r, edge, (uFlags&DFCS_FLAT) | BF_MIDDLE |BF_SOFT| BF_RECT);
        }
    }

    /* Adjust rectangle if asked */
    if(uFlags & DFCS_ADJUSTRECT)
    {
        r->left   += 2;
        r->right  -= 2;
        r->top    += 2;
        r->bottom -= 2;
    }

    return TRUE;
}


/************************************************************************
 *	UITOOLS_DFC_ButtonChcek
 *
 * Draw a check/3state button coming from DrawFrameControl()
 *
 * Does a pretty good job in emulating MS behavior. Some quirks are
 * however there because MS uses a TrueType font (Marlett) to draw
 * the buttons.
 */
#define DFC_CHECKPOINTSMAX      6

static BOOL32 UITOOLS_DFC_ButtonCheck(HDC32 dc, LPRECT32 r, UINT32 uFlags)
{
    RECT32 myr;
    int SmallDiam = UITOOLS_MakeSquareRect(r, &myr);
    int BorderShrink = SmallDiam / 16;

    if(BorderShrink < 1) BorderShrink = 1;

    /* FIXME: The FillRect() sequence doesn't work for sizes less than */
    /* 4 pixels in diameter. Not really a problem but it isn't M$'s */
    /* 100% equivalent. */
    if(uFlags & (DFCS_FLAT|DFCS_MONO))
    {
        FillRect32(dc, &myr, GetSysColorBrush32(COLOR_WINDOWFRAME));
        myr.left   += 2 * BorderShrink;
        myr.right  -= 2 * BorderShrink;
        myr.top    += 2 * BorderShrink;
        myr.bottom -= 2 * BorderShrink;
    }
    else
    {
        FillRect32(dc, &myr, GetSysColorBrush32(COLOR_BTNHIGHLIGHT));
        myr.right  -= BorderShrink;
        myr.bottom -= BorderShrink;
        FillRect32(dc, &myr, GetSysColorBrush32(COLOR_BTNSHADOW));
        myr.left   += BorderShrink;
        myr.top    += BorderShrink;
        FillRect32(dc, &myr, GetSysColorBrush32(COLOR_3DLIGHT));
        myr.right  -= BorderShrink;
        myr.bottom -= BorderShrink;
        FillRect32(dc, &myr, GetSysColorBrush32(COLOR_3DDKSHADOW));
        myr.left   += BorderShrink;
        myr.top    += BorderShrink;
    }

    if(uFlags & (DFCS_INACTIVE|DFCS_PUSHED))
    {
        FillRect32(dc, &myr, GetSysColorBrush32(COLOR_BTNFACE));
    }
    else if(uFlags & DFCS_CHECKED)
    {
        if(GetSysColor32(COLOR_BTNHIGHLIGHT) == RGB(255, 255, 255))
        {
            HBITMAP32 hbm = CreateBitmap32(8, 8, 1, 1, wPattern_AA55);
            HBRUSH32 hbsave;
            HBRUSH32 hb = CreatePatternBrush32(hbm);

            FillRect32(dc, &myr, GetSysColorBrush32(COLOR_BTNFACE));
            hbsave = (HBRUSH32)SelectObject32(dc, hb);
            PatBlt32(dc, myr.left, myr.top, myr.right-myr.left, myr.bottom-myr.top, 0x00FA0089);
            SelectObject32(dc, hbsave);
            DeleteObject32(hb);
            DeleteObject32(hbm);
        }
        else
        {
            FillRect32(dc, &myr, GetSysColorBrush32(COLOR_BTNHIGHLIGHT));
        }
    }
    else
    {
        FillRect32(dc, &myr, GetSysColorBrush32(COLOR_WINDOW));
    }

    if(uFlags & DFCS_CHECKED)
    {
        POINT32 CheckPoints[DFC_CHECKPOINTSMAX];
        int i;
        HBRUSH32 hbsave;
        HPEN32 hpsave;

        /* FIXME: This comes very close to M$'s checkmark, but not */
        /* exactly... When small or large there is a few pixels */
        /* shift. Not bad, but could be better :) */
        UITOOLS_MakeSquareRect(r, &myr);
        CheckPoints[0].x = myr.left + 253*SmallDiam/1000;
        CheckPoints[0].y = myr.top  + 345*SmallDiam/1000;
        CheckPoints[1].x = myr.left + 409*SmallDiam/1000;
        CheckPoints[1].y = CheckPoints[0].y + (CheckPoints[1].x-CheckPoints[0].x);
        CheckPoints[2].x = myr.left + 690*SmallDiam/1000;
        CheckPoints[2].y = CheckPoints[1].y - (CheckPoints[2].x-CheckPoints[1].x);
        CheckPoints[3].x = CheckPoints[2].x;
        CheckPoints[3].y = CheckPoints[2].y + 3*SmallDiam/16;
        CheckPoints[4].x = CheckPoints[1].x;
        CheckPoints[4].y = CheckPoints[1].y + 3*SmallDiam/16;
        CheckPoints[5].x = CheckPoints[0].x;
        CheckPoints[5].y = CheckPoints[0].y + 3*SmallDiam/16;

        i = (uFlags & DFCS_INACTIVE) || (uFlags & 0xff) == DFCS_BUTTON3STATE ? COLOR_BTNSHADOW : COLOR_WINDOWTEXT;
        hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(i));
        hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(i));
        Polygon32(dc, CheckPoints, DFC_CHECKPOINTSMAX);
        SelectObject32(dc, hpsave);
        SelectObject32(dc, hbsave);
    }
    return TRUE;
}


/************************************************************************
 *	UITOOLS_DFC_ButtonRadio
 *
 * Draw a radio/radioimage/radiomask button coming from DrawFrameControl()
 *
 * Does a pretty good job in emulating MS behavior. Some quirks are
 * however there because MS uses a TrueType font (Marlett) to draw
 * the buttons.
 */
static BOOL32 UITOOLS_DFC_ButtonRadio(HDC32 dc, LPRECT32 r, UINT32 uFlags)
{
    RECT32 myr;
    int i;
    int SmallDiam = UITOOLS_MakeSquareRect(r, &myr);
    int BorderShrink = SmallDiam / 16;
    HPEN32 hpsave;
    HBRUSH32 hbsave;
    int xe, ye;
    int xc, yc;

    if(BorderShrink < 1) BorderShrink = 1;

    if((uFlags & 0xff) == DFCS_BUTTONRADIOIMAGE)
    {
        FillRect32(dc, r, (HBRUSH32)GetStockObject32(BLACK_BRUSH));
    }

    xe = myr.left;
    ye = myr.top  + SmallDiam - SmallDiam/2;

    xc = myr.left + SmallDiam - SmallDiam/2;
    yc = myr.top  + SmallDiam - SmallDiam/2;

    /* Define bounding box */
    i = 14*SmallDiam/16;
    myr.left   = xc - i+i/2;
    myr.right  = xc + i/2;
    myr.top    = yc - i+i/2;
    myr.bottom = yc + i/2;

    if((uFlags & 0xff) == DFCS_BUTTONRADIOMASK)
    {
        hbsave = (HBRUSH32)SelectObject32(dc, GetStockObject32(BLACK_BRUSH));
        Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, xe, ye, xe, ye);
        SelectObject32(dc, hbsave);
    }
    else
    {
        if(uFlags & (DFCS_FLAT|DFCS_MONO))
        {
            hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(COLOR_WINDOWFRAME));
            hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(COLOR_WINDOWFRAME));
            Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, xe, ye, xe, ye);
            SelectObject32(dc, hbsave);
            SelectObject32(dc, hpsave);
        }
        else
        {
            hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(COLOR_BTNHIGHLIGHT));
            hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(COLOR_BTNHIGHLIGHT));
            Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, myr.left-1, myr.bottom, myr.right-1, myr.top);

            SelectObject32(dc, GetSysColorPen32(COLOR_BTNSHADOW));
            SelectObject32(dc, GetSysColorBrush32(COLOR_BTNSHADOW));
            Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, myr.right+1, myr.top, myr.left+1, myr.bottom);

            myr.left   += BorderShrink;
            myr.right  -= BorderShrink;
            myr.top    += BorderShrink;
            myr.bottom -= BorderShrink;

            SelectObject32(dc, GetSysColorPen32(COLOR_3DLIGHT));
            SelectObject32(dc, GetSysColorBrush32(COLOR_3DLIGHT));
            Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, myr.left-1, myr.bottom, myr.right-1, myr.top);

            SelectObject32(dc, GetSysColorPen32(COLOR_3DDKSHADOW));
            SelectObject32(dc, GetSysColorBrush32(COLOR_3DDKSHADOW));
            Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, myr.right+1, myr.top, myr.left+1, myr.bottom);
            SelectObject32(dc, hbsave);
            SelectObject32(dc, hpsave);
        }

        i = 10*SmallDiam/16;
        myr.left   = xc - i+i/2;
        myr.right  = xc + i/2;
        myr.top    = yc - i+i/2;
        myr.bottom = yc + i/2;
        i= !(uFlags & (DFCS_INACTIVE|DFCS_PUSHED)) ? COLOR_WINDOW : COLOR_BTNFACE;
        hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(i));
        hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(i));
        Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, xe, ye, xe, ye);
        SelectObject32(dc, hbsave);
        SelectObject32(dc, hpsave);
    }

    if(uFlags & DFCS_CHECKED)
    {
        i = 6*SmallDiam/16;
        i = i < 1 ? 1 : i;
        myr.left   = xc - i+i/2;
        myr.right  = xc + i/2;
        myr.top    = yc - i+i/2;
        myr.bottom = yc + i/2;

        i = uFlags & DFCS_INACTIVE ? COLOR_BTNSHADOW : COLOR_WINDOWTEXT;
        hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(i));
        hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(i));
        Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, xe, ye, xe, ye);
        SelectObject32(dc, hpsave);
        SelectObject32(dc, hbsave);
    }

    /* FIXME: M$ has a polygon in the center at relative points: */
    /* 0.476, 0.476 (times SmallDiam, SmallDiam) */
    /* 0.476, 0.525 */
    /* 0.500, 0.500 */
    /* 0.500, 0.499 */
    /* when the button is unchecked. The reason for it is unknown. The */
    /* color is COLOR_BTNHIGHLIGHT, although the polygon gets painted at */
    /* least 3 times (it looks like a clip-region when you see it happen). */
    /* I do not really see a reason why this should be implemented. If you */
    /* have a good reason, let me know. Maybe this is a quirk in the Marlett */
    /* font. */

    return TRUE;
}

/***********************************************************************
 *           UITOOLS_DrawFrameButton
 */
static BOOL32 UITOOLS_DrawFrameButton(HDC32 hdc, LPRECT32 rc, UINT32 uState)
{
    switch(uState & 0xff)
    {
    case DFCS_BUTTONPUSH:
        return UITOOLS_DFC_ButtonPush(hdc, rc, uState);

    case DFCS_BUTTONCHECK:
    case DFCS_BUTTON3STATE:
        return UITOOLS_DFC_ButtonCheck(hdc, rc, uState);

    case DFCS_BUTTONRADIOIMAGE:
    case DFCS_BUTTONRADIOMASK:
    case DFCS_BUTTONRADIO:
        return UITOOLS_DFC_ButtonRadio(hdc, rc, uState);

    default:
        fprintf(stdnimp, "UITOOLS_DrawFrameButton: Report this: Invalid button state: 0x%04x\n", uState);
    }

    return FALSE;
}

/***********************************************************************
 *           UITOOLS_DrawFrameCaption
 *
 * Draw caption buttons (win95), coming from DrawFrameControl()
 */

static BOOL32 UITOOLS_DrawFrameCaption(HDC32 dc, LPRECT32 r, UINT32 uFlags)
{
    POINT32 Line1[10];
    POINT32 Line2[10];
    int Line1N;
    int Line2N;
    RECT32 myr;
    int SmallDiam = UITOOLS_MakeSquareRect(r, &myr)-2;
    int i;
    HBRUSH32 hbsave;
    HPEN32 hpsave;
    HFONT32 hfsave, hf;
    int xc = (myr.left+myr.right)/2;
    int yc = (myr.top+myr.bottom)/2;
    int edge, move;
    char str[2] = "?";
    UINT32 alignsave;
    int bksave;
    COLORREF clrsave;
    SIZE32 size;

    UITOOLS_DFC_ButtonPush(dc, r, uFlags & 0xff00);

    switch(uFlags & 0xff)
    {
    case DFCS_CAPTIONCLOSE:
        edge = 328*SmallDiam/1000;
        move = 95*SmallDiam/1000;
        Line1[0].x = Line2[0].x = Line1[1].x = Line2[1].x = xc - edge;
        Line1[2].y = Line2[5].y = Line1[1].y = Line2[4].y = yc - edge;
        Line1[3].x = Line2[3].x = Line1[4].x = Line2[4].x = xc + edge;
        Line1[5].y = Line2[2].y = Line1[4].y = Line2[1].y = yc + edge;
        Line1[2].x = Line2[2].x = Line1[1].x + move;
        Line1[0].y = Line2[3].y = Line1[1].y + move;
        Line1[5].x = Line2[5].x = Line1[4].x - move;
        Line1[3].y = Line2[0].y = Line1[4].y - move;
        Line1N = 6;
        Line2N = 6;
        break;

    case DFCS_CAPTIONHELP:
        /* This one breaks the flow */
        /* FIXME: We need the Marlett font in order to get this right. */

        hf = CreateFont32A(-SmallDiam, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, FIXED_PITCH|FF_DONTCARE, "System");
        alignsave = SetTextAlign32(dc, TA_TOP|TA_LEFT);
        bksave = SetBkMode32(dc, TRANSPARENT);
        clrsave = GetTextColor32(dc);
        hfsave = (HFONT32)SelectObject32(dc, hf);
        GetTextExtentPoint32A(dc, str, 1, &size);

        if(uFlags & DFCS_INACTIVE)
        {
            SetTextColor32(dc, GetSysColor32(COLOR_BTNHIGHLIGHT));
            TextOut32A(dc, xc-size.cx/2+1, yc-size.cy/2+1, str, 1);
        }
        SetTextColor32(dc, GetSysColor32(uFlags & DFCS_INACTIVE ? COLOR_BTNSHADOW : COLOR_BTNTEXT));
        TextOut32A(dc, xc-size.cx/2, yc-size.cy/2, str, 1);

        SelectObject32(dc, hfsave);
        SetTextColor32(dc, clrsave);
        SetBkMode32(dc, bksave);
        SetTextAlign32(dc, alignsave);
        DeleteObject32(hf);
        return TRUE;

    case DFCS_CAPTIONMIN:
        Line1[0].x = Line1[3].x = myr.left   +  96*SmallDiam/750+2;
        Line1[1].x = Line1[2].x = Line1[0].x + 372*SmallDiam/750;
        Line1[0].y = Line1[1].y = myr.top    + 563*SmallDiam/750+1;
        Line1[2].y = Line1[3].y = Line1[0].y +  92*SmallDiam/750;
        Line1N = 4;
        Line2N = 0;
        break;

    case DFCS_CAPTIONMAX:
        edge = 47*SmallDiam/750;
        Line1[0].x = Line1[5].x = myr.left +  57*SmallDiam/750+3;
        Line1[0].y = Line1[1].y = myr.top  + 143*SmallDiam/750+1;
        Line1[1].x = Line1[2].x = Line1[0].x + 562*SmallDiam/750;
        Line1[5].y = Line1[4].y = Line1[0].y +  93*SmallDiam/750;
        Line1[2].y = Line1[3].y = Line1[0].y + 513*SmallDiam/750;
        Line1[3].x = Line1[4].x = Line1[1].x -  edge;

        Line2[0].x = Line2[5].x = Line1[0].x;
        Line2[3].x = Line2[4].x = Line1[1].x;
        Line2[1].x = Line2[2].x = Line1[0].x + edge;
        Line2[0].y = Line2[1].y = Line1[0].y;
        Line2[4].y = Line2[5].y = Line1[2].y;
        Line2[2].y = Line2[3].y = Line1[2].y - edge;
        Line1N = 6;
        Line2N = 6;
        break;

    case DFCS_CAPTIONRESTORE:
        /* FIXME: this one looks bad at small sizes < 15x15 :( */
        edge = 47*SmallDiam/750;
        move = 420*SmallDiam/750;
        Line1[0].x = Line1[9].x = myr.left + 198*SmallDiam/750+2;
        Line1[0].y = Line1[1].y = myr.top  + 169*SmallDiam/750+1;
        Line1[6].y = Line1[7].y = Line1[0].y + 93*SmallDiam/750;
        Line1[7].x = Line1[8].x = Line1[0].x + edge;
        Line1[1].x = Line1[2].x = Line1[0].x + move;
        Line1[5].x = Line1[6].x = Line1[1].x - edge;
        Line1[9].y = Line1[8].y = Line1[0].y + 187*SmallDiam/750;
        Line1[2].y = Line1[3].y = Line1[0].y + 327*SmallDiam/750;
        Line1[4].y = Line1[5].y = Line1[2].y - edge;
        Line1[3].x = Line1[4].x = Line1[2].x - 140*SmallDiam/750;

        Line2[1].x = Line2[2].x = Line1[3].x;
        Line2[7].x = Line2[8].x = Line2[1].x - edge;
        Line2[0].x = Line2[9].x = Line2[3].x = Line2[4].x = Line2[1].x - move;
        Line2[5].x = Line2[6].x = Line2[0].x + edge;
        Line2[0].y = Line2[1].y = Line1[9].y;
        Line2[4].y = Line2[5].y = Line2[8].y = Line2[9].y = Line2[0].y + 93*SmallDiam/750;
        Line2[2].y = Line2[3].y = Line2[0].y + 327*SmallDiam/750;
        Line2[6].y = Line2[7].y = Line2[2].y - edge;
        Line1N = 10;
        Line2N = 10;
        break;

    default:
        fprintf(stdnimp, "UITOOLS_DrawFrameCaption: Report this: Invalid caption; flags: 0x%04x\n", uFlags);
        return FALSE;
    }

    /* Here the drawing takes place */
    if(uFlags & DFCS_INACTIVE)
    {
        /* If we have an inactive button, then you see a shadow */
        hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(COLOR_BTNHIGHLIGHT));
        hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(COLOR_BTNHIGHLIGHT));
        Polygon32(dc, Line1, Line1N);
        if(Line2N > 0)
            Polygon32(dc, Line2, Line2N);
        SelectObject32(dc, hpsave);
        SelectObject32(dc, hbsave);
    }

    /* Correct for the shadow shift */
    for(i = 0; i < Line1N; i++)
    {
        Line1[i].x--;
        Line1[i].y--;
    }
    for(i = 0; i < Line2N; i++)
    {
        Line2[i].x--;
        Line2[i].y--;
    }

    /* Make the final picture */
    i = uFlags & DFCS_INACTIVE ? COLOR_BTNSHADOW : COLOR_BTNTEXT;
    hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(i));
    hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(i));

    Polygon32(dc, Line1, Line1N);
    if(Line2N > 0)
        Polygon32(dc, Line2, Line2N);
    SelectObject32(dc, hpsave);
    SelectObject32(dc, hbsave);

    return TRUE;
}


/************************************************************************
 *      UITOOLS_DrawFrameScroll
 *
 * Draw a scroll-bar control coming from DrawFrameControl()
 */
static BOOL32 UITOOLS_DrawFrameScroll(HDC32 dc, LPRECT32 r, UINT32 uFlags)
{
    POINT32 Line[4];
    RECT32 myr;
    int SmallDiam = UITOOLS_MakeSquareRect(r, &myr) - 2;
    int i;
    HBRUSH32 hbsave, hb, hb2;
    HPEN32 hpsave, hp, hp2;
    int tri = 310*SmallDiam/1000;
    int d46, d93;

    switch(uFlags & 0xff)
    {
    case DFCS_SCROLLCOMBOBOX:
    case DFCS_SCROLLDOWN:
        Line[2].x = myr.left + 470*SmallDiam/1000 + 2;
        Line[2].y = myr.top  + 687*SmallDiam/1000 + 1;
        Line[0].x = Line[2].x - tri;
        Line[1].x = Line[2].x + tri;
        Line[0].y = Line[1].y = Line[2].y - tri;
        break;

    case DFCS_SCROLLUP:
        Line[2].x = myr.left + 470*SmallDiam/1000 + 2;
        Line[2].y = myr.top  + 313*SmallDiam/1000 + 1;
        Line[0].x = Line[2].x - tri;
        Line[1].x = Line[2].x + tri;
        Line[0].y = Line[1].y = Line[2].y + tri;
        break;

    case DFCS_SCROLLLEFT:
        Line[2].x = myr.left + 313*SmallDiam/1000 + 1;
        Line[2].y = myr.top  + 470*SmallDiam/1000 + 2;
        Line[0].y = Line[2].y - tri;
        Line[1].y = Line[2].y + tri;
        Line[0].x = Line[1].x = Line[2].x + tri;
        break;

    case DFCS_SCROLLRIGHT:
        Line[2].x = myr.left + 687*SmallDiam/1000 + 1;
        Line[2].y = myr.top  + 470*SmallDiam/1000 + 2;
        Line[0].y = Line[2].y - tri;
        Line[1].y = Line[2].y + tri;
        Line[0].x = Line[1].x = Line[2].x - tri;
        break;

    case DFCS_SCROLLSIZEGRIP:
        /* This one breaks the flow... */
        UITOOLS_DrawRectEdge(dc, r, EDGE_BUMP, BF_MIDDLE | ((uFlags&(DFCS_MONO|DFCS_FLAT)) ? BF_MONO : 0));
        hpsave = (HPEN32)SelectObject32(dc, GetStockObject32(NULL_PEN));
        hbsave = (HBRUSH32)SelectObject32(dc, GetStockObject32(NULL_BRUSH));
        if(uFlags & (DFCS_MONO|DFCS_FLAT))
        {
            hp = hp2 = GetSysColorPen32(COLOR_WINDOWFRAME);
            hb = hb2 = GetSysColorBrush32(COLOR_WINDOWFRAME);
        }
        else
        {
            hp  = GetSysColorPen32(COLOR_BTNHIGHLIGHT);
            hp2 = GetSysColorPen32(COLOR_BTNSHADOW);
            hb  = GetSysColorBrush32(COLOR_BTNHIGHLIGHT);
            hb2 = GetSysColorBrush32(COLOR_BTNSHADOW);
        }
        Line[0].x = Line[1].x = r->right-1;
        Line[2].y = Line[3].y = r->bottom-1;
        d46 = 46*SmallDiam/750;
        d93 = 93*SmallDiam/750;

        i = 586*SmallDiam/750;
        Line[0].y = r->bottom - i - 1;
        Line[3].x = r->right - i - 1;
        Line[1].y = Line[0].y + d46;
        Line[2].x = Line[3].x + d46;
        SelectObject32(dc, hb);
        SelectObject32(dc, hp);
        Polygon32(dc, Line, 4);

        Line[1].y++; Line[2].x++;
        Line[0].y = Line[1].y + d93;
        Line[3].x = Line[2].x + d93;
        SelectObject32(dc, hb2);
        SelectObject32(dc, hp2);
        Polygon32(dc, Line, 4);

        i = 398*SmallDiam/750;
        Line[0].y = r->bottom - i - 1;
        Line[3].x = r->right - i - 1;
        Line[1].y = Line[0].y + d46;
        Line[2].x = Line[3].x + d46;
        SelectObject32(dc, hb);
        SelectObject32(dc, hp);
        Polygon32(dc, Line, 4);

        Line[1].y++; Line[2].x++;
        Line[0].y = Line[1].y + d93;
        Line[3].x = Line[2].x + d93;
        SelectObject32(dc, hb2);
        SelectObject32(dc, hp2);
        Polygon32(dc, Line, 4);

        i = 210*SmallDiam/750;
        Line[0].y = r->bottom - i - 1;
        Line[3].x = r->right - i - 1;
        Line[1].y = Line[0].y + d46;
        Line[2].x = Line[3].x + d46;
        SelectObject32(dc, hb);
        SelectObject32(dc, hp);
        Polygon32(dc, Line, 4);

        Line[1].y++; Line[2].x++;
        Line[0].y = Line[1].y + d93;
        Line[3].x = Line[2].x + d93;
        SelectObject32(dc, hb2);
        SelectObject32(dc, hp2);
        Polygon32(dc, Line, 4);

        SelectObject32(dc, hpsave);
        SelectObject32(dc, hbsave);
        return TRUE;

    default:
        fprintf(stdnimp, "UITOOLS_DrawFrameScroll: Report this: Invalid scroll; flags: 0x%04x\n", uFlags);
        return FALSE;
    }

    /* Here do the real scroll-bar controls end up */
    UITOOLS_DFC_ButtonPush(dc, r, uFlags & 0xff00);

    if(uFlags & DFCS_INACTIVE)
    {
        hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(COLOR_BTNHIGHLIGHT));
        hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(COLOR_BTNHIGHLIGHT));
        Polygon32(dc, Line, 3);
        SelectObject32(dc, hpsave);
        SelectObject32(dc, hbsave);
    }

    for(i = 0; i < 3; i++)
    {
        Line[i].x--;
        Line[i].y--;
    }

    i = uFlags & DFCS_INACTIVE ? COLOR_BTNSHADOW : COLOR_BTNTEXT;
    hbsave = (HBRUSH32)SelectObject32(dc, GetSysColorBrush32(i));
    hpsave = (HPEN32)SelectObject32(dc, GetSysColorPen32(i));
    Polygon32(dc, Line, 3);
    SelectObject32(dc, hpsave);
    SelectObject32(dc, hbsave);

    return TRUE;
}

/************************************************************************
 *      UITOOLS_DrawFrameMenu
 *
 * Draw a menu control coming from DrawFrameControl()
 */
static BOOL32 UITOOLS_DrawFrameMenu(HDC32 dc, LPRECT32 r, UINT32 uFlags)
{
    POINT32 Points[6];
    RECT32 myr;
    int SmallDiam = UITOOLS_MakeSquareRect(r, &myr);
    int i;
    HBRUSH32 hbsave;
    HPEN32 hpsave;
    int xe, ye;
    int xc, yc;
    BOOL32 retval = TRUE;

    /* Using black and white seems to be utterly wrong, but win95 doesn't */
    /* use anything else. I think I tried all sys-colors to change things */
    /* without luck. It seems as if this behavior is inherited from the */
    /* win31 DFC() implementation... (you remember, B/W menus). */

    FillRect32(dc, r, (HBRUSH32)GetStockObject32(WHITE_BRUSH));

    hbsave = (HBRUSH32)SelectObject32(dc, GetStockObject32(BLACK_BRUSH));
    hpsave = (HPEN32)SelectObject32(dc, GetStockObject32(BLACK_PEN));

    switch(uFlags & 0xff)
    {
    case DFCS_MENUARROW:
        i = 187*SmallDiam/750;
        Points[2].x = myr.left + 468*SmallDiam/750;
        Points[2].y = myr.top  + 352*SmallDiam/750+1;
        Points[0].y = Points[2].y - i;
        Points[1].y = Points[2].y + i;
        Points[0].x = Points[1].x = Points[2].x - i;
        Polygon32(dc, Points, 3);
        break;

    case DFCS_MENUBULLET:
        xe = myr.left;
        ye = myr.top  + SmallDiam - SmallDiam/2;
        xc = myr.left + SmallDiam - SmallDiam/2;
        yc = myr.top  + SmallDiam - SmallDiam/2;
        i = 234*SmallDiam/750;
        i = i < 1 ? 1 : i;
        myr.left   = xc - i+i/2;
        myr.right  = xc + i/2;
        myr.top    = yc - i+i/2;
        myr.bottom = yc + i/2;
        Pie32(dc, myr.left, myr.top, myr.right, myr.bottom, xe, ye, xe, ye);
        break;

    case DFCS_MENUCHECK:
        Points[0].x = myr.left + 253*SmallDiam/1000;
        Points[0].y = myr.top  + 445*SmallDiam/1000;
        Points[1].x = myr.left + 409*SmallDiam/1000;
        Points[1].y = Points[0].y + (Points[1].x-Points[0].x);
        Points[2].x = myr.left + 690*SmallDiam/1000;
        Points[2].y = Points[1].y - (Points[2].x-Points[1].x);
        Points[3].x = Points[2].x;
        Points[3].y = Points[2].y + 3*SmallDiam/16;
        Points[4].x = Points[1].x;
        Points[4].y = Points[1].y + 3*SmallDiam/16;
        Points[5].x = Points[0].x;
        Points[5].y = Points[0].y + 3*SmallDiam/16;
        Polygon32(dc, Points, 6);
        break;

    default:
        fprintf(stdnimp, "UITOOLS_DrawFrameMenu: Report this: Invalid menu; flags: 0x%04x\n", uFlags);
        retval = FALSE;
        break;
    }

    SelectObject32(dc, hpsave);
    SelectObject32(dc, hbsave);
    return retval;
}


/**********************************************************************
 *          DrawFrameControl16  (USER.656)
 */
BOOL16 WINAPI DrawFrameControl16( HDC16 hdc, LPRECT16 rc, UINT16 uType,
                                  UINT16 uState )
{
    RECT32 rect32;
    BOOL32 ret;

    CONV_RECT16TO32( rc, &rect32 );
    ret = DrawFrameControl32( hdc, &rect32, uType, uState );
    CONV_RECT32TO16( &rect32, rc );
    return ret;
}


/**********************************************************************
 *          DrawFrameControl32  (USER32.157)
 */
BOOL32 WINAPI DrawFrameControl32( HDC32 hdc, LPRECT32 rc, UINT32 uType,
                                  UINT32 uState )
{
    /* Win95 doesn't support drawing in other mapping modes */
    if(GetMapMode32(hdc) != MM_TEXT)
        return FALSE;
        
    switch(uType)
    {
    case DFC_BUTTON:
      return UITOOLS_DrawFrameButton(hdc, rc, uState);
    case DFC_CAPTION:
      return UITOOLS_DrawFrameCaption(hdc, rc, uState);
    case DFC_MENU:
      return UITOOLS_DrawFrameMenu(hdc, rc, uState);
    case DFC_SCROLL:
      return UITOOLS_DrawFrameScroll(hdc, rc, uState);
    default:
      fprintf( stdnimp,"DrawFrameControl32(%x,%p,%d,%x), bad type!\n",
	       hdc,rc,uType,uState );
    }
    return FALSE;
}
