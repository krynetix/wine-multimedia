name	w32sys
type	win16

#1 WEP
2 stub ISPEFORMAT
3 stub EXECPE
4 stub GETPEEXEINFO
5 return GETW32SYSVERSION 0 0x100
6 stub LOADPERESOURCE
7 stub GETPERESOURCETABLE
8 stub EXECPEEX
9 stub ITSME
10 stub W32SERROR
11 stub EXP1
12 pascal16 GetWin32sInfo(ptr) GetWin32sInfo
