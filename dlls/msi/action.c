/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2004 Aric Stewart for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Pages I need
 *
http://msdn.microsoft.com/library/default.asp?url=/library/en-us/msi/setup/installexecutesequence_table.asp

http://msdn.microsoft.com/library/default.asp?url=/library/en-us/msi/setup/standard_actions_reference.asp
 */

#include <stdarg.h>
#include <stdio.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "wine/debug.h"
#include "fdi.h"
#include "msi.h"
#include "msiquery.h"
#include "msidefs.h"
#include "msvcrt/fcntl.h"
#include "objbase.h"
#include "objidl.h"
#include "msipriv.h"
#include "winnls.h"
#include "winuser.h"
#include "shlobj.h"
#include "wine/unicode.h"
#include "winver.h"
#include "action.h"

#define REG_PROGRESS_VALUE 13200
#define COMPONENT_PROGRESS_VALUE 24000

WINE_DEFAULT_DEBUG_CHANNEL(msi);

/*
 * Prototypes
 */
static UINT ACTION_ProcessExecSequence(MSIPACKAGE *package, BOOL UIran);
static UINT ACTION_ProcessUISequence(MSIPACKAGE *package);
static UINT ACTION_PerformActionSequence(MSIPACKAGE *package, UINT seq, BOOL UI);
static UINT build_icon_path(MSIPACKAGE *package, LPCWSTR icon_name, 
                            LPWSTR *FilePath);

/* 
 * action handlers
 */
typedef UINT (*STANDARDACTIONHANDLER)(MSIPACKAGE*);

static UINT ACTION_LaunchConditions(MSIPACKAGE *package);
static UINT ACTION_CostInitialize(MSIPACKAGE *package);
static UINT ACTION_CreateFolders(MSIPACKAGE *package);
static UINT ACTION_CostFinalize(MSIPACKAGE *package);
static UINT ACTION_FileCost(MSIPACKAGE *package);
static UINT ACTION_InstallFiles(MSIPACKAGE *package);
static UINT ACTION_DuplicateFiles(MSIPACKAGE *package);
static UINT ACTION_WriteRegistryValues(MSIPACKAGE *package);
static UINT ACTION_InstallInitialize(MSIPACKAGE *package);
static UINT ACTION_InstallValidate(MSIPACKAGE *package);
static UINT ACTION_ProcessComponents(MSIPACKAGE *package);
static UINT ACTION_RegisterTypeLibraries(MSIPACKAGE *package);
static UINT ACTION_RegisterClassInfo(MSIPACKAGE *package);
static UINT ACTION_RegisterProgIdInfo(MSIPACKAGE *package);
static UINT ACTION_RegisterExtensionInfo(MSIPACKAGE *package);
static UINT ACTION_RegisterMIMEInfo(MSIPACKAGE *package);
static UINT ACTION_RegisterUser(MSIPACKAGE *package);
static UINT ACTION_CreateShortcuts(MSIPACKAGE *package);
static UINT ACTION_PublishProduct(MSIPACKAGE *package);
static UINT ACTION_WriteIniValues(MSIPACKAGE *package);
static UINT ACTION_SelfRegModules(MSIPACKAGE *package);
static UINT ACTION_PublishFeatures(MSIPACKAGE *package);
static UINT ACTION_RegisterProduct(MSIPACKAGE *package);
static UINT ACTION_InstallExecute(MSIPACKAGE *package);
static UINT ACTION_InstallFinalize(MSIPACKAGE *package);
static UINT ACTION_ForceReboot(MSIPACKAGE *package);
static UINT ACTION_ResolveSource(MSIPACKAGE *package);
static UINT ACTION_ExecuteAction(MSIPACKAGE *package);
static UINT ACTION_RegisterFonts(MSIPACKAGE *package);
static UINT ACTION_PublishComponents(MSIPACKAGE *package);

 
/*
 * consts and values used
 */
static const WCHAR cszSourceDir[] = {'S','o','u','r','c','e','D','i','r',0};
static const WCHAR cszRootDrive[] = {'R','O','O','T','D','R','I','V','E',0};
static const WCHAR cszTargetDir[] = {'T','A','R','G','E','T','D','I','R',0};
static const WCHAR cszTempFolder[]= {'T','e','m','p','F','o','l','d','e','r',0};
static const WCHAR cszDatabase[]={'D','A','T','A','B','A','S','E',0};
static const WCHAR c_colon[] = {'C',':','\\',0};
static const WCHAR szProductCode[]=
    {'P','r','o','d','u','c','t','C','o','d','e',0};
static const WCHAR cszbs[]={'\\',0};
const static WCHAR szCreateFolders[] =
    {'C','r','e','a','t','e','F','o','l','d','e','r','s',0};
const static WCHAR szCostFinalize[] =
    {'C','o','s','t','F','i','n','a','l','i','z','e',0};
const static WCHAR szInstallFiles[] =
    {'I','n','s','t','a','l','l','F','i','l','e','s',0};
const static WCHAR szDuplicateFiles[] =
    {'D','u','p','l','i','c','a','t','e','F','i','l','e','s',0};
const static WCHAR szWriteRegistryValues[] =
    {'W','r','i','t','e','R','e','g','i','s','t','r','y',
            'V','a','l','u','e','s',0};
const static WCHAR szCostInitialize[] =
    {'C','o','s','t','I','n','i','t','i','a','l','i','z','e',0};
const static WCHAR szFileCost[] = 
    {'F','i','l','e','C','o','s','t',0};
const static WCHAR szInstallInitialize[] = 
    {'I','n','s','t','a','l','l','I','n','i','t','i','a','l','i','z','e',0};
const static WCHAR szInstallValidate[] = 
    {'I','n','s','t','a','l','l','V','a','l','i','d','a','t','e',0};
const static WCHAR szLaunchConditions[] = 
    {'L','a','u','n','c','h','C','o','n','d','i','t','i','o','n','s',0};
const static WCHAR szProcessComponents[] = 
    {'P','r','o','c','e','s','s','C','o','m','p','o','n','e','n','t','s',0};
const static WCHAR szRegisterTypeLibraries[] = 
    {'R','e','g','i','s','t','e','r','T','y','p','e',
            'L','i','b','r','a','r','i','e','s',0};
const static WCHAR szRegisterClassInfo[] = 
    {'R','e','g','i','s','t','e','r','C','l','a','s','s','I','n','f','o',0};
const static WCHAR szRegisterProgIdInfo[] = 
    {'R','e','g','i','s','t','e','r','P','r','o','g','I','d','I','n','f','o',0};
const static WCHAR szCreateShortcuts[] = 
    {'C','r','e','a','t','e','S','h','o','r','t','c','u','t','s',0};
const static WCHAR szPublishProduct[] = 
    {'P','u','b','l','i','s','h','P','r','o','d','u','c','t',0};
const static WCHAR szWriteIniValues[] = 
    {'W','r','i','t','e','I','n','i','V','a','l','u','e','s',0};
const static WCHAR szSelfRegModules[] = 
    {'S','e','l','f','R','e','g','M','o','d','u','l','e','s',0};
const static WCHAR szPublishFeatures[] = 
    {'P','u','b','l','i','s','h','F','e','a','t','u','r','e','s',0};
const static WCHAR szRegisterProduct[] = 
    {'R','e','g','i','s','t','e','r','P','r','o','d','u','c','t',0};
const static WCHAR szInstallExecute[] = 
    {'I','n','s','t','a','l','l','E','x','e','c','u','t','e',0};
const static WCHAR szInstallExecuteAgain[] = 
    {'I','n','s','t','a','l','l','E','x','e','c','u','t','e',
            'A','g','a','i','n',0};
const static WCHAR szInstallFinalize[] = 
    {'I','n','s','t','a','l','l','F','i','n','a','l','i','z','e',0};
const static WCHAR szForceReboot[] = 
    {'F','o','r','c','e','R','e','b','o','o','t',0};
const static WCHAR szResolveSource[] =
    {'R','e','s','o','l','v','e','S','o','u','r','c','e',0};
const static WCHAR szAppSearch[] = 
    {'A','p','p','S','e','a','r','c','h',0};
const static WCHAR szAllocateRegistrySpace[] = 
    {'A','l','l','o','c','a','t','e','R','e','g','i','s','t','r','y',
            'S','p','a','c','e',0};
const static WCHAR szBindImage[] = 
    {'B','i','n','d','I','m','a','g','e',0};
const static WCHAR szCCPSearch[] = 
    {'C','C','P','S','e','a','r','c','h',0};
const static WCHAR szDeleteServices[] = 
    {'D','e','l','e','t','e','S','e','r','v','i','c','e','s',0};
const static WCHAR szDisableRollback[] = 
    {'D','i','s','a','b','l','e','R','o','l','l','b','a','c','k',0};
const static WCHAR szExecuteAction[] = 
    {'E','x','e','c','u','t','e','A','c','t','i','o','n',0};
const static WCHAR szFindRelatedProducts[] = 
    {'F','i','n','d','R','e','l','a','t','e','d',
            'P','r','o','d','u','c','t','s',0};
const static WCHAR szInstallAdminPackage[] = 
    {'I','n','s','t','a','l','l','A','d','m','i','n',
            'P','a','c','k','a','g','e',0};
const static WCHAR szInstallSFPCatalogFile[] = 
    {'I','n','s','t','a','l','l','S','F','P','C','a','t','a','l','o','g',
            'F','i','l','e',0};
const static WCHAR szIsolateComponents[] = 
    {'I','s','o','l','a','t','e','C','o','m','p','o','n','e','n','t','s',0};
const static WCHAR szMigrateFeatureStates[] = 
    {'M','i','g','r','a','t','e','F','e','a','t','u','r','e',
            'S','t','a','t','e','s',0};
const static WCHAR szMoveFiles[] = 
    {'M','o','v','e','F','i','l','e','s',0};
const static WCHAR szMsiPublishAssemblies[] = 
    {'M','s','i','P','u','b','l','i','s','h',
            'A','s','s','e','m','b','l','i','e','s',0};
const static WCHAR szMsiUnpublishAssemblies[] = 
    {'M','s','i','U','n','p','u','b','l','i','s','h',
            'A','s','s','e','m','b','l','i','e','s',0};
const static WCHAR szInstallODBC[] = 
    {'I','n','s','t','a','l','l','O','D','B','C',0};
const static WCHAR szInstallServices[] = 
    {'I','n','s','t','a','l','l','S','e','r','v','i','c','e','s',0};
const static WCHAR szPatchFiles[] = 
    {'P','a','t','c','h','F','i','l','e','s',0};
const static WCHAR szPublishComponents[] = 
    {'P','u','b','l','i','s','h','C','o','m','p','o','n','e','n','t','s',0};
const static WCHAR szRegisterComPlus[] =
    {'R','e','g','i','s','t','e','r','C','o','m','P','l','u','s',0};
const static WCHAR szRegisterExtensionInfo[] =
    {'R','e','g','i','s','t','e','r','E','x','t','e','n','s','i','o','n',
            'I','n','f','o',0};
const static WCHAR szRegisterFonts[] =
    {'R','e','g','i','s','t','e','r','F','o','n','t','s',0};
const static WCHAR szRegisterMIMEInfo[] =
    {'R','e','g','i','s','t','e','r','M','I','M','E','I','n','f','o',0};
const static WCHAR szRegisterUser[] =
    {'R','e','g','i','s','t','e','r','U','s','e','r',0};
const static WCHAR szRemoveDuplicateFiles[] =
    {'R','e','m','o','v','e','D','u','p','l','i','c','a','t','e',
            'F','i','l','e','s',0};
const static WCHAR szRemoveEnvironmentStrings[] =
    {'R','e','m','o','v','e','E','n','v','i','r','o','n','m','e','n','t',
            'S','t','r','i','n','g','s',0};
const static WCHAR szRemoveExistingProducts[] =
    {'R','e','m','o','v','e','E','x','i','s','t','i','n','g',
            'P','r','o','d','u','c','t','s',0};
const static WCHAR szRemoveFiles[] =
    {'R','e','m','o','v','e','F','i','l','e','s',0};
const static WCHAR szRemoveFolders[] =
    {'R','e','m','o','v','e','F','o','l','d','e','r','s',0};
const static WCHAR szRemoveIniValues[] =
    {'R','e','m','o','v','e','I','n','i','V','a','l','u','e','s',0};
const static WCHAR szRemoveODBC[] =
    {'R','e','m','o','v','e','O','D','B','C',0};
const static WCHAR szRemoveRegistryValues[] =
    {'R','e','m','o','v','e','R','e','g','i','s','t','r','y',
            'V','a','l','u','e','s',0};
const static WCHAR szRemoveShortcuts[] =
    {'R','e','m','o','v','e','S','h','o','r','t','c','u','t','s',0};
const static WCHAR szRMCCPSearch[] =
    {'R','M','C','C','P','S','e','a','r','c','h',0};
const static WCHAR szScheduleReboot[] =
    {'S','c','h','e','d','u','l','e','R','e','b','o','o','t',0};
const static WCHAR szSelfUnregModules[] =
    {'S','e','l','f','U','n','r','e','g','M','o','d','u','l','e','s',0};
const static WCHAR szSetODBCFolders[] =
    {'S','e','t','O','D','B','C','F','o','l','d','e','r','s',0};
const static WCHAR szStartServices[] =
    {'S','t','a','r','t','S','e','r','v','i','c','e','s',0};
const static WCHAR szStopServices[] =
    {'S','t','o','p','S','e','r','v','i','c','e','s',0};
const static WCHAR szUnpublishComponents[] =
    {'U','n','p','u','b','l','i','s','h',
            'C','o','m','p','o','n','e','n','t','s',0};
const static WCHAR szUnpublishFeatures[] =
    {'U','n','p','u','b','l','i','s','h','F','e','a','t','u','r','e','s',0};
const static WCHAR szUnregisterClassInfo[] =
    {'U','n','r','e','g','i','s','t','e','r','C','l','a','s','s',
            'I','n','f','o',0};
const static WCHAR szUnregisterComPlus[] =
    {'U','n','r','e','g','i','s','t','e','r','C','o','m','P','l','u','s',0};
const static WCHAR szUnregisterExtensionInfo[] =
    {'U','n','r','e','g','i','s','t','e','r',
            'E','x','t','e','n','s','i','o','n','I','n','f','o',0};
const static WCHAR szUnregisterFonts[] =
    {'U','n','r','e','g','i','s','t','e','r','F','o','n','t','s',0};
const static WCHAR szUnregisterMIMEInfo[] =
    {'U','n','r','e','g','i','s','t','e','r','M','I','M','E','I','n','f','o',0};
const static WCHAR szUnregisterProgIdInfo[] =
    {'U','n','r','e','g','i','s','t','e','r','P','r','o','g','I','d',
            'I','n','f','o',0};
const static WCHAR szUnregisterTypeLibraries[] =
    {'U','n','r','e','g','i','s','t','e','r','T','y','p','e',
            'L','i','b','r','a','r','i','e','s',0};
const static WCHAR szValidateProductID[] =
    {'V','a','l','i','d','a','t','e','P','r','o','d','u','c','t','I','D',0};
const static WCHAR szWriteEnvironmentStrings[] =
    {'W','r','i','t','e','E','n','v','i','r','o','n','m','e','n','t',
            'S','t','r','i','n','g','s',0};

struct _actions {
    LPCWSTR action;
    STANDARDACTIONHANDLER handler;
};

static struct _actions StandardActions[] = {
    { szAllocateRegistrySpace, NULL},
    { szAppSearch, ACTION_AppSearch },
    { szBindImage, NULL},
    { szCCPSearch, NULL},
    { szCostFinalize, ACTION_CostFinalize },
    { szCostInitialize, ACTION_CostInitialize },
    { szCreateFolders, ACTION_CreateFolders },
    { szCreateShortcuts, ACTION_CreateShortcuts },
    { szDeleteServices, NULL},
    { szDisableRollback, NULL},
    { szDuplicateFiles, ACTION_DuplicateFiles },
    { szExecuteAction, ACTION_ExecuteAction },
    { szFileCost, ACTION_FileCost },
    { szFindRelatedProducts, NULL},
    { szForceReboot, ACTION_ForceReboot },
    { szInstallAdminPackage, NULL},
    { szInstallExecute, ACTION_InstallExecute },
    { szInstallExecuteAgain, ACTION_InstallExecute },
    { szInstallFiles, ACTION_InstallFiles},
    { szInstallFinalize, ACTION_InstallFinalize },
    { szInstallInitialize, ACTION_InstallInitialize },
    { szInstallSFPCatalogFile, NULL},
    { szInstallValidate, ACTION_InstallValidate },
    { szIsolateComponents, NULL},
    { szLaunchConditions, ACTION_LaunchConditions },
    { szMigrateFeatureStates, NULL},
    { szMoveFiles, NULL},
    { szMsiPublishAssemblies, NULL},
    { szMsiUnpublishAssemblies, NULL},
    { szInstallODBC, NULL},
    { szInstallServices, NULL},
    { szPatchFiles, NULL},
    { szProcessComponents, ACTION_ProcessComponents },
    { szPublishComponents, ACTION_PublishComponents },
    { szPublishFeatures, ACTION_PublishFeatures },
    { szPublishProduct, ACTION_PublishProduct },
    { szRegisterClassInfo, ACTION_RegisterClassInfo },
    { szRegisterComPlus, NULL},
    { szRegisterExtensionInfo, ACTION_RegisterExtensionInfo },
    { szRegisterFonts, ACTION_RegisterFonts },
    { szRegisterMIMEInfo, ACTION_RegisterMIMEInfo },
    { szRegisterProduct, ACTION_RegisterProduct },
    { szRegisterProgIdInfo, ACTION_RegisterProgIdInfo },
    { szRegisterTypeLibraries, ACTION_RegisterTypeLibraries },
    { szRegisterUser, ACTION_RegisterUser},
    { szRemoveDuplicateFiles, NULL},
    { szRemoveEnvironmentStrings, NULL},
    { szRemoveExistingProducts, NULL},
    { szRemoveFiles, NULL},
    { szRemoveFolders, NULL},
    { szRemoveIniValues, NULL},
    { szRemoveODBC, NULL},
    { szRemoveRegistryValues, NULL},
    { szRemoveShortcuts, NULL},
    { szResolveSource, ACTION_ResolveSource},
    { szRMCCPSearch, NULL},
    { szScheduleReboot, NULL},
    { szSelfRegModules, ACTION_SelfRegModules },
    { szSelfUnregModules, NULL},
    { szSetODBCFolders, NULL},
    { szStartServices, NULL},
    { szStopServices, NULL},
    { szUnpublishComponents, NULL},
    { szUnpublishFeatures, NULL},
    { szUnregisterClassInfo, NULL},
    { szUnregisterComPlus, NULL},
    { szUnregisterExtensionInfo, NULL},
    { szUnregisterFonts, NULL},
    { szUnregisterMIMEInfo, NULL},
    { szUnregisterProgIdInfo, NULL},
    { szUnregisterTypeLibraries, NULL},
    { szValidateProductID, NULL},
    { szWriteEnvironmentStrings, NULL},
    { szWriteIniValues, ACTION_WriteIniValues },
    { szWriteRegistryValues, ACTION_WriteRegistryValues},
    { NULL, NULL},
};


/******************************************************** 
 * helper functions to get around current HACKS and such
 ********************************************************/
inline static void reduce_to_longfilename(WCHAR* filename)
{
    LPWSTR p = strchrW(filename,'|');
    if (p)
        memmove(filename, p+1, (strlenW(p+1)+1)*sizeof(WCHAR));
}

inline static void reduce_to_shortfilename(WCHAR* filename)
{
    LPWSTR p = strchrW(filename,'|');
    if (p)
        *p = 0;
}

WCHAR *load_dynamic_stringW(MSIRECORD *row, INT index)
{
    UINT rc;
    DWORD sz;
    LPWSTR ret;
   
    sz = 0; 
    if (MSI_RecordIsNull(row,index))
        return NULL;

    rc = MSI_RecordGetStringW(row,index,NULL,&sz);

    /* having an empty string is different than NULL */
    if (sz == 0)
    {
        ret = HeapAlloc(GetProcessHeap(),0,sizeof(WCHAR));
        ret[0] = 0;
        return ret;
    }

    sz ++;
    ret = HeapAlloc(GetProcessHeap(),0,sz * sizeof (WCHAR));
    rc = MSI_RecordGetStringW(row,index,ret,&sz);
    if (rc!=ERROR_SUCCESS)
    {
        ERR("Unable to load dynamic string\n");
        HeapFree(GetProcessHeap(), 0, ret);
        ret = NULL;
    }
    return ret;
}

LPWSTR load_dynamic_property(MSIPACKAGE *package, LPCWSTR prop, UINT* rc)
{
    DWORD sz = 0;
    LPWSTR str;
    UINT r;

    r = MSI_GetPropertyW(package, prop, NULL, &sz);
    if (r != ERROR_SUCCESS && r != ERROR_MORE_DATA)
    {
        if (rc)
            *rc = r;
        return NULL;
    }
    sz++;
    str = HeapAlloc(GetProcessHeap(),0,sz*sizeof(WCHAR));
    r = MSI_GetPropertyW(package, prop, str, &sz);
    if (r != ERROR_SUCCESS)
    {
        HeapFree(GetProcessHeap(),0,str);
        str = NULL;
    }
    if (rc)
        *rc = r;
    return str;
}

int get_loaded_component(MSIPACKAGE* package, LPCWSTR Component )
{
    int rc = -1;
    DWORD i;

    for (i = 0; i < package->loaded_components; i++)
    {
        if (strcmpW(Component,package->components[i].Component)==0)
        {
            rc = i;
            break;
        }
    }
    return rc;
}

int get_loaded_feature(MSIPACKAGE* package, LPCWSTR Feature )
{
    int rc = -1;
    DWORD i;

    for (i = 0; i < package->loaded_features; i++)
    {
        if (strcmpW(Feature,package->features[i].Feature)==0)
        {
            rc = i;
            break;
        }
    }
    return rc;
}

int get_loaded_file(MSIPACKAGE* package, LPCWSTR file)
{
    int rc = -1;
    DWORD i;

    for (i = 0; i < package->loaded_files; i++)
    {
        if (strcmpW(file,package->files[i].File)==0)
        {
            rc = i;
            break;
        }
    }
    return rc;
}

int track_tempfile(MSIPACKAGE *package, LPCWSTR name, LPCWSTR path)
{
    DWORD i;
    DWORD index;

    if (!package)
        return -2;

    for (i=0; i < package->loaded_files; i++)
        if (strcmpW(package->files[i].File,name)==0)
            return -1;

    index = package->loaded_files;
    package->loaded_files++;
    if (package->loaded_files== 1)
        package->files = HeapAlloc(GetProcessHeap(),0,sizeof(MSIFILE));
    else
        package->files = HeapReAlloc(GetProcessHeap(),0,
            package->files , package->loaded_files * sizeof(MSIFILE));

    memset(&package->files[index],0,sizeof(MSIFILE));

    package->files[index].File = strdupW(name);
    package->files[index].TargetPath = strdupW(path);
    package->files[index].Temporary = TRUE;

    TRACE("Tracking tempfile (%s)\n",debugstr_w(package->files[index].File));  

    return 0;
}

static void remove_tracked_tempfiles(MSIPACKAGE* package)
{
    DWORD i;

    if (!package)
        return;

    for (i = 0; i < package->loaded_files; i++)
    {
        if (package->files[i].Temporary)
        {
            TRACE("Cleaning up %s\n",debugstr_w(package->files[i].TargetPath));
            DeleteFileW(package->files[i].TargetPath);
        }

    }
}

/* wrapper to resist a need for a full rewrite right now */
DWORD deformat_string(MSIPACKAGE *package, LPCWSTR ptr, WCHAR** data )
{
    if (ptr)
    {
        MSIRECORD *rec = MSI_CreateRecord(1);
        DWORD size = 0;

        MSI_RecordSetStringW(rec,0,ptr);
        MSI_FormatRecordW(package,rec,NULL,&size);
        if (size >= 0)
        {
            size++;
            *data = HeapAlloc(GetProcessHeap(),0,size*sizeof(WCHAR));
            if (size > 1)
                MSI_FormatRecordW(package,rec,*data,&size);
            else
                *data[0] = 0;
            msiobj_release( &rec->hdr );
            return sizeof(WCHAR)*size;
        }
        msiobj_release( &rec->hdr );
    }

    *data = NULL;
    return 0;
}

/* Called when the package is being closed */
void ACTION_free_package_structures( MSIPACKAGE* package)
{
    INT i;
    
    TRACE("Freeing package action data\n");

    remove_tracked_tempfiles(package);

    /* No dynamic buffers in features */
    if (package->features && package->loaded_features > 0)
        HeapFree(GetProcessHeap(),0,package->features);

    for (i = 0; i < package->loaded_folders; i++)
    {
        HeapFree(GetProcessHeap(),0,package->folders[i].Directory);
        HeapFree(GetProcessHeap(),0,package->folders[i].TargetDefault);
        HeapFree(GetProcessHeap(),0,package->folders[i].SourceDefault);
        HeapFree(GetProcessHeap(),0,package->folders[i].ResolvedTarget);
        HeapFree(GetProcessHeap(),0,package->folders[i].ResolvedSource);
        HeapFree(GetProcessHeap(),0,package->folders[i].Property);
    }
    if (package->folders && package->loaded_folders > 0)
        HeapFree(GetProcessHeap(),0,package->folders);

    for (i = 0; i < package->loaded_components; i++)
        HeapFree(GetProcessHeap(),0,package->components[i].FullKeypath);

    if (package->components && package->loaded_components > 0)
        HeapFree(GetProcessHeap(),0,package->components);

    for (i = 0; i < package->loaded_files; i++)
    {
        HeapFree(GetProcessHeap(),0,package->files[i].File);
        HeapFree(GetProcessHeap(),0,package->files[i].FileName);
        HeapFree(GetProcessHeap(),0,package->files[i].ShortName);
        HeapFree(GetProcessHeap(),0,package->files[i].Version);
        HeapFree(GetProcessHeap(),0,package->files[i].Language);
        HeapFree(GetProcessHeap(),0,package->files[i].SourcePath);
        HeapFree(GetProcessHeap(),0,package->files[i].TargetPath);
    }

    if (package->files && package->loaded_files > 0)
        HeapFree(GetProcessHeap(),0,package->files);

    /* clean up extension, progid, class and verb structures */
    for (i = 0; i < package->loaded_classes; i++)
    {
        HeapFree(GetProcessHeap(),0,package->classes[i].Description);
        HeapFree(GetProcessHeap(),0,package->classes[i].FileTypeMask);
        HeapFree(GetProcessHeap(),0,package->classes[i].IconPath);
        HeapFree(GetProcessHeap(),0,package->classes[i].DefInprocHandler);
        HeapFree(GetProcessHeap(),0,package->classes[i].DefInprocHandler32);
        HeapFree(GetProcessHeap(),0,package->classes[i].Argument);
        HeapFree(GetProcessHeap(),0,package->classes[i].ProgIDText);
    }

    if (package->classes && package->loaded_classes > 0)
        HeapFree(GetProcessHeap(),0,package->classes);

    for (i = 0; i < package->loaded_extensions; i++)
    {
        HeapFree(GetProcessHeap(),0,package->extensions[i].ProgIDText);
    }

    if (package->extensions && package->loaded_extensions > 0)
        HeapFree(GetProcessHeap(),0,package->extensions);

    for (i = 0; i < package->loaded_progids; i++)
    {
        HeapFree(GetProcessHeap(),0,package->progids[i].ProgID);
        HeapFree(GetProcessHeap(),0,package->progids[i].Description);
        HeapFree(GetProcessHeap(),0,package->progids[i].IconPath);
    }

    if (package->progids && package->loaded_progids > 0)
        HeapFree(GetProcessHeap(),0,package->progids);

    for (i = 0; i < package->loaded_verbs; i++)
    {
        HeapFree(GetProcessHeap(),0,package->verbs[i].Verb);
        HeapFree(GetProcessHeap(),0,package->verbs[i].Command);
        HeapFree(GetProcessHeap(),0,package->verbs[i].Argument);
    }

    if (package->verbs && package->loaded_verbs > 0)
        HeapFree(GetProcessHeap(),0,package->verbs);

    for (i = 0; i < package->loaded_mimes; i++)
        HeapFree(GetProcessHeap(),0,package->mimes[i].ContentType);

    if (package->mimes && package->loaded_mimes > 0)
        HeapFree(GetProcessHeap(),0,package->mimes);

    for (i = 0; i < package->loaded_appids; i++)
    {
        HeapFree(GetProcessHeap(),0,package->appids[i].RemoteServerName);
        HeapFree(GetProcessHeap(),0,package->appids[i].LocalServer);
        HeapFree(GetProcessHeap(),0,package->appids[i].ServiceParameters);
        HeapFree(GetProcessHeap(),0,package->appids[i].DllSurrogate);
    }

    if (package->appids && package->loaded_appids > 0)
        HeapFree(GetProcessHeap(),0,package->appids);

    if (package->script)
    {
        for (i = 0; i < TOTAL_SCRIPTS; i++)
        {
            int j;
            for (j = 0; j < package->script->ActionCount[i]; j++)
                HeapFree(GetProcessHeap(),0,package->script->Actions[i][j]);
        
            HeapFree(GetProcessHeap(),0,package->script->Actions[i]);
        }
        HeapFree(GetProcessHeap(),0,package->script);
    }

    HeapFree(GetProcessHeap(),0,package->PackagePath);

    /* cleanup control event subscriptions */
    ControlEvent_CleanupSubscriptions(package);
}

static void ce_actiontext(MSIPACKAGE* package, LPCWSTR action)
{
    static const WCHAR szActionText[] = 
        {'A','c','t','i','o','n','T','e','x','t',0};
    MSIRECORD *row;

    row = MSI_CreateRecord(1);
    MSI_RecordSetStringW(row,1,action);
    ControlEvent_FireSubscribedEvent(package,szActionText, row);
    msiobj_release(&row->hdr);
}

static void ui_progress(MSIPACKAGE *package, int a, int b, int c, int d )
{
    MSIRECORD * row;

    row = MSI_CreateRecord(4);
    MSI_RecordSetInteger(row,1,a);
    MSI_RecordSetInteger(row,2,b);
    MSI_RecordSetInteger(row,3,c);
    MSI_RecordSetInteger(row,4,d);
    MSI_ProcessMessage(package, INSTALLMESSAGE_PROGRESS, row);
    msiobj_release(&row->hdr);

    msi_dialog_check_messages(NULL);
}

static void ui_actiondata(MSIPACKAGE *package, LPCWSTR action, MSIRECORD * record)
{
    static const WCHAR Query_t[] = 
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','A','c','t','i','o', 'n','T','e','x','t','`',' ',
         'W','H','E','R','E',' ', '`','A','c','t','i','o','n','`',' ','=', 
         ' ','\'','%','s','\'',0};
    WCHAR message[1024];
    MSIRECORD * row = 0;
    DWORD size;
    static const WCHAR szActionData[] = 
        {'A','c','t','i','o','n','D','a','t','a',0};

    if (!package->LastAction || strcmpW(package->LastAction,action))
    {
        row = MSI_QueryGetRecord(package->db, Query_t, action);
        if (!row)
            return;

        if (MSI_RecordIsNull(row,3))
        {
            msiobj_release(&row->hdr);
            return;
        }

        /* update the cached actionformat */
        HeapFree(GetProcessHeap(),0,package->ActionFormat);
        package->ActionFormat = load_dynamic_stringW(row,3);

        HeapFree(GetProcessHeap(),0,package->LastAction);
        package->LastAction = strdupW(action);

        msiobj_release(&row->hdr);
    }

    MSI_RecordSetStringW(record,0,package->ActionFormat);
    size = 1024;
    MSI_FormatRecordW(package,record,message,&size);

    row = MSI_CreateRecord(1);
    MSI_RecordSetStringW(row,1,message);
 
    MSI_ProcessMessage(package, INSTALLMESSAGE_ACTIONDATA, row);

    ControlEvent_FireSubscribedEvent(package,szActionData, row);

    msiobj_release(&row->hdr);
}


static void ui_actionstart(MSIPACKAGE *package, LPCWSTR action)
{
    static const WCHAR template_s[]=
        {'A','c','t','i','o','n',' ','%','s',':',' ','%','s','.',' ', '%','s',
         '.',0};
    static const WCHAR format[] = 
        {'H','H','\'',':','\'','m','m','\'',':','\'','s','s',0};
    static const WCHAR Query_t[] = 
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','A','c','t','i','o', 'n','T','e','x','t','`',' ',
         'W','H','E','R','E', ' ','`','A','c','t','i','o','n','`',' ','=', 
         ' ','\'','%','s','\'',0};
    WCHAR message[1024];
    WCHAR timet[0x100];
    MSIRECORD * row = 0;
    WCHAR *ActionText=NULL;

    GetTimeFormatW(LOCALE_USER_DEFAULT, 0, NULL, format, timet, 0x100);

    row = MSI_QueryGetRecord( package->db, Query_t, action );
    if (!row)
        return;

    ActionText = load_dynamic_stringW(row,2);
    msiobj_release(&row->hdr);

    sprintfW(message,template_s,timet,action,ActionText);

    row = MSI_CreateRecord(1);
    MSI_RecordSetStringW(row,1,message);
 
    MSI_ProcessMessage(package, INSTALLMESSAGE_ACTIONSTART, row);
    msiobj_release(&row->hdr);
    HeapFree(GetProcessHeap(),0,ActionText);
}

static void ui_actioninfo(MSIPACKAGE *package, LPCWSTR action, BOOL start, 
                          UINT rc)
{
    MSIRECORD * row;
    static const WCHAR template_s[]=
        {'A','c','t','i','o','n',' ','s','t','a','r','t',' ','%','s',':',' ',
         '%','s', '.',0};
    static const WCHAR template_e[]=
        {'A','c','t','i','o','n',' ','e','n','d','e','d',' ','%','s',':',' ',
         '%','s', '.',' ','R','e','t','u','r','n',' ','v','a','l','u','e',' ',
         '%','i','.',0};
    static const WCHAR format[] = 
        {'H','H','\'',':','\'','m','m','\'',':','\'','s','s',0};
    WCHAR message[1024];
    WCHAR timet[0x100];

    GetTimeFormatW(LOCALE_USER_DEFAULT, 0, NULL, format, timet, 0x100);
    if (start)
        sprintfW(message,template_s,timet,action);
    else
        sprintfW(message,template_e,timet,action,rc);
    
    row = MSI_CreateRecord(1);
    MSI_RecordSetStringW(row,1,message);
 
    MSI_ProcessMessage(package, INSTALLMESSAGE_INFO, row);
    msiobj_release(&row->hdr);
}

/*
 *  build_directory_name()
 *
 *  This function is to save messing round with directory names
 *  It handles adding backslashes between path segments, 
 *   and can add \ at the end of the directory name if told to.
 *
 *  It takes a variable number of arguments.
 *  It always allocates a new string for the result, so make sure
 *   to free the return value when finished with it.
 *
 *  The first arg is the number of path segments that follow.
 *  The arguments following count are a list of path segments.
 *  A path segment may be NULL.
 *
 *  Path segments will be added with a \ separating them.
 *  A \ will not be added after the last segment, however if the
 *    last segment is NULL, then the last character will be a \
 * 
 */
static LPWSTR build_directory_name(DWORD count, ...)
{
    DWORD sz = 1, i;
    LPWSTR dir;
    va_list va;

    va_start(va,count);
    for(i=0; i<count; i++)
    {
        LPCWSTR str = va_arg(va,LPCWSTR);
        if (str)
            sz += strlenW(str) + 1;
    }
    va_end(va);

    dir = HeapAlloc(GetProcessHeap(), 0, sz*sizeof(WCHAR));
    dir[0]=0;

    va_start(va,count);
    for(i=0; i<count; i++)
    {
        LPCWSTR str = va_arg(va,LPCWSTR);
        if (!str)
            continue;
        strcatW(dir, str);
        if( ((i+1)!=count) && dir[strlenW(dir)-1]!='\\')
            strcatW(dir, cszbs);
    }
    return dir;
}

static BOOL ACTION_VerifyComponentForAction(MSIPACKAGE* package, INT index, 
                                            INSTALLSTATE check )
{
    if (package->components[index].Installed == check)
        return FALSE;

    if (package->components[index].ActionRequest == check)
        return TRUE;
    else
        return FALSE;
}

static BOOL ACTION_VerifyFeatureForAction(MSIPACKAGE* package, INT index, 
                                            INSTALLSTATE check )
{
    if (package->features[index].Installed == check)
        return FALSE;

    if (package->features[index].ActionRequest == check)
        return TRUE;
    else
        return FALSE;
}


/****************************************************
 * TOP level entry points 
 *****************************************************/

UINT ACTION_DoTopLevelINSTALL(MSIPACKAGE *package, LPCWSTR szPackagePath,
                              LPCWSTR szCommandLine)
{
    DWORD sz;
    WCHAR buffer[10];
    UINT rc;
    BOOL ui = FALSE;
    static const WCHAR szUILevel[] = {'U','I','L','e','v','e','l',0};
    static const WCHAR szAction[] = {'A','C','T','I','O','N',0};
    static const WCHAR szInstall[] = {'I','N','S','T','A','L','L',0};

    MSI_SetPropertyW(package, szAction, szInstall);

    package->script = HeapAlloc(GetProcessHeap(),0,sizeof(MSISCRIPT));
    memset(package->script,0,sizeof(MSISCRIPT));

    if (szPackagePath)   
    {
        LPWSTR p, check, path;
 
        package->PackagePath = strdupW(szPackagePath);
        path = strdupW(szPackagePath);
        p = strrchrW(path,'\\');    
        if (p)
        {
            p++;
            *p=0;
        }
        else
        {
            HeapFree(GetProcessHeap(),0,path);
            path = HeapAlloc(GetProcessHeap(),0,MAX_PATH*sizeof(WCHAR));
            GetCurrentDirectoryW(MAX_PATH,path);
            strcatW(path,cszbs);
        }

        check = load_dynamic_property(package, cszSourceDir,NULL);
        if (!check)
            MSI_SetPropertyW(package, cszSourceDir, path);
        else
            HeapFree(GetProcessHeap(), 0, check);

        HeapFree(GetProcessHeap(), 0, path);
    }

    if (szCommandLine)
    {
        LPWSTR ptr,ptr2;
        ptr = (LPWSTR)szCommandLine;
       
        while (*ptr)
        {
            WCHAR *prop = NULL;
            WCHAR *val = NULL;

            TRACE("Looking at %s\n",debugstr_w(ptr));

            ptr2 = strchrW(ptr,'=');
            if (ptr2)
            {
                BOOL quote=FALSE;
                DWORD len = 0;

                while (*ptr == ' ') ptr++;
                len = ptr2-ptr;
                prop = HeapAlloc(GetProcessHeap(),0,(len+1)*sizeof(WCHAR));
                memcpy(prop,ptr,len*sizeof(WCHAR));
                prop[len]=0;
                ptr2++;
           
                len = 0; 
                ptr = ptr2; 
                while (*ptr && (quote || (!quote && *ptr!=' ')))
                {
                    if (*ptr == '"')
                        quote = !quote;
                    ptr++;
                    len++;
                }
               
                if (*ptr2=='"')
                {
                    ptr2++;
                    len -= 2;
                }
                val = HeapAlloc(GetProcessHeap(),0,(len+1)*sizeof(WCHAR));
                memcpy(val,ptr2,len*sizeof(WCHAR));
                val[len] = 0;

                if (strlenW(prop) > 0)
                {
                    TRACE("Found commandline property (%s) = (%s)\n", 
                                       debugstr_w(prop), debugstr_w(val));
                    MSI_SetPropertyW(package,prop,val);
                }
                HeapFree(GetProcessHeap(),0,val);
                HeapFree(GetProcessHeap(),0,prop);
            }
            ptr++;
        }
    }
  
    sz = 10; 
    if (MSI_GetPropertyW(package,szUILevel,buffer,&sz) == ERROR_SUCCESS)
    {
        if (atoiW(buffer) >= INSTALLUILEVEL_REDUCED)
        {
            rc = ACTION_ProcessUISequence(package);
            ui = TRUE;
            if (rc == ERROR_SUCCESS)
                rc = ACTION_ProcessExecSequence(package,TRUE);
        }
        else
            rc = ACTION_ProcessExecSequence(package,FALSE);
    }
    else
        rc = ACTION_ProcessExecSequence(package,FALSE);
    
    if (rc == -1)
    {
        /* install was halted but should be considered a success */
        rc = ERROR_SUCCESS;
    }

    package->script->CurrentlyScripting= FALSE;

    /* process the ending type action */
    if (rc == ERROR_SUCCESS)
        ACTION_PerformActionSequence(package,-1,ui);
    else if (rc == ERROR_INSTALL_USEREXIT) 
        ACTION_PerformActionSequence(package,-2,ui);
    else if (rc == ERROR_INSTALL_SUSPEND) 
        ACTION_PerformActionSequence(package,-4,ui);
    else  /* failed */
        ACTION_PerformActionSequence(package,-3,ui);

    /* finish up running custom actions */
    ACTION_FinishCustomActions(package);
    
    return rc;
}

static UINT ACTION_PerformActionSequence(MSIPACKAGE *package, UINT seq, BOOL UI)
{
    UINT rc = ERROR_SUCCESS;
    WCHAR buffer[0x100];
    DWORD sz = 0x100;
    MSIRECORD * row = 0;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','n','s','t','a','l','l','E','x','e','c','u','t','e',
         'S','e','q','u','e','n','c','e','`',' ', 'W','H','E','R','E',' ',
         '`','S','e','q','u','e','n','c','e','`',' ', '=',' ','%','i',0};

    static const WCHAR UISeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
     '`','I','n','s','t','a','l','l','U','I','S','e','q','u','e','n','c','e',
     '`', ' ', 'W','H','E','R','E',' ','`','S','e','q','u','e','n','c','e','`',
	 ' ', '=',' ','%','i',0};

    if (UI)
        row = MSI_QueryGetRecord(package->db, UISeqQuery, seq);
    else
        row = MSI_QueryGetRecord(package->db, ExecSeqQuery, seq);

    if (row)
    {
        TRACE("Running the actions\n"); 

        /* check conditions */
        if (!MSI_RecordIsNull(row,2))
        {
            LPWSTR cond = NULL;
            cond = load_dynamic_stringW(row,2);

            if (cond)
            {
                /* this is a hack to skip errors in the condition code */
                if (MSI_EvaluateConditionW(package, cond) == MSICONDITION_FALSE)
                {
                    HeapFree(GetProcessHeap(),0,cond);
                    goto end;
                }
                else
                    HeapFree(GetProcessHeap(),0,cond);
            }
        }

        sz=0x100;
        rc =  MSI_RecordGetStringW(row,1,buffer,&sz);
        if (rc != ERROR_SUCCESS)
        {
            ERR("Error is %x\n",rc);
            msiobj_release(&row->hdr);
            goto end;
        }

        if (UI)
            rc = ACTION_PerformUIAction(package,buffer);
        else
            rc = ACTION_PerformAction(package,buffer, FALSE);
end:
        msiobj_release(&row->hdr);
    }
    else
        rc = ERROR_SUCCESS;

    return rc;
}

static UINT ACTION_ProcessExecSequence(MSIPACKAGE *package, BOOL UIran)
{
    MSIQUERY * view;
    UINT rc;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','I','n','s','t','a','l','l','E','x','e','c','u','t','e',
         'S','e','q','u','e','n','c','e','`',' ', 'W','H','E','R','E',' ',
         '`','S','e','q','u','e','n','c','e','`',' ', '>',' ','%','i',' ',
         'O','R','D','E','R',' ', 'B','Y',' ',
         '`','S','e','q','u','e','n','c','e','`',0 };
    MSIRECORD * row = 0;
    static const WCHAR IVQuery[] =
        {'S','E','L','E','C','T',' ','`','S','e','q','u','e','n','c','e','`',
         ' ', 'F','R','O','M',' ','`','I','n','s','t','a','l','l',
         'E','x','e','c','u','t','e','S','e','q','u','e','n','c','e','`',' ',
         'W','H','E','R','E',' ','`','A','c','t','i','o','n','`',' ','=',
         ' ','\'', 'I','n','s','t','a','l','l',
         'V','a','l','i','d','a','t','e','\'', 0};
    INT seq = 0;


    if (package->script->ExecuteSequenceRun)
    {
        TRACE("Execute Sequence already Run\n");
        return ERROR_SUCCESS;
    }

    package->script->ExecuteSequenceRun = TRUE;
    
    /* get the sequence number */
    if (UIran)
    {
        row = MSI_QueryGetRecord(package->db, IVQuery);
        if( !row )
            return ERROR_FUNCTION_FAILED;
        seq = MSI_RecordGetInteger(row,1);
        msiobj_release(&row->hdr);
    }

    rc = MSI_OpenQuery(package->db, &view, ExecSeqQuery, seq);
    if (rc == ERROR_SUCCESS)
    {
        rc = MSI_ViewExecute(view, 0);

        if (rc != ERROR_SUCCESS)
        {
            MSI_ViewClose(view);
            msiobj_release(&view->hdr);
            goto end;
        }
       
        TRACE("Running the actions\n"); 

        while (1)
        {
            WCHAR buffer[0x100];
            DWORD sz = 0x100;

            rc = MSI_ViewFetch(view,&row);
            if (rc != ERROR_SUCCESS)
            {
                rc = ERROR_SUCCESS;
                break;
            }

            /* check conditions */
            if (!MSI_RecordIsNull(row,2))
            {
                LPWSTR cond = NULL;
                cond = load_dynamic_stringW(row,2);

                if (cond)
                {
                    /* this is a hack to skip errors in the condition code */
                    if (MSI_EvaluateConditionW(package, cond) ==
                            MSICONDITION_FALSE)
                    {
                        HeapFree(GetProcessHeap(),0,cond);
                        msiobj_release(&row->hdr);
                        continue; 
                    }
                    else
                        HeapFree(GetProcessHeap(),0,cond);
                }
            }

            sz=0x100;
            rc =  MSI_RecordGetStringW(row,1,buffer,&sz);
            if (rc != ERROR_SUCCESS)
            {
                ERR("Error is %x\n",rc);
                msiobj_release(&row->hdr);
                break;
            }

            rc = ACTION_PerformAction(package,buffer, FALSE);

            if (rc == ERROR_FUNCTION_NOT_CALLED)
                rc = ERROR_SUCCESS;

            if (rc != ERROR_SUCCESS)
            {
                ERR("Execution halted due to error (%i)\n",rc);
                msiobj_release(&row->hdr);
                break;
            }

            msiobj_release(&row->hdr);
        }

        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
    }

end:
    return rc;
}


static UINT ACTION_ProcessUISequence(MSIPACKAGE *package)
{
    MSIQUERY * view;
    UINT rc;
    static const WCHAR ExecSeqQuery [] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','n','s','t','a','l','l',
         'U','I','S','e','q','u','e','n','c','e','`',
         ' ','W','H','E','R','E',' ', 
         '`','S','e','q','u','e','n','c','e','`',' ',
         '>',' ','0',' ','O','R','D','E','R',' ','B','Y',' ',
         '`','S','e','q','u','e','n','c','e','`',0};
    
    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    
    if (rc == ERROR_SUCCESS)
    {
        rc = MSI_ViewExecute(view, 0);

        if (rc != ERROR_SUCCESS)
        {
            MSI_ViewClose(view);
            msiobj_release(&view->hdr);
            goto end;
        }
       
        TRACE("Running the actions \n"); 

        while (1)
        {
            WCHAR buffer[0x100];
            DWORD sz = 0x100;
            MSIRECORD * row = 0;

            rc = MSI_ViewFetch(view,&row);
            if (rc != ERROR_SUCCESS)
            {
                rc = ERROR_SUCCESS;
                break;
            }

            sz=0x100;
            rc =  MSI_RecordGetStringW(row,1,buffer,&sz);
            if (rc != ERROR_SUCCESS)
            {
                ERR("Error is %x\n",rc);
                msiobj_release(&row->hdr);
                break;
            }


            /* check conditions */
            if (!MSI_RecordIsNull(row,2))
            {
                LPWSTR cond = NULL;
                cond = load_dynamic_stringW(row,2);

                if (cond)
                {
                    /* this is a hack to skip errors in the condition code */
                    if (MSI_EvaluateConditionW(package, cond) ==
                            MSICONDITION_FALSE)
                    {
                        HeapFree(GetProcessHeap(),0,cond);
                        msiobj_release(&row->hdr);
                        TRACE("Skipping action: %s (condition is false)\n",
                                    debugstr_w(buffer));
                        continue; 
                    }
                    else
                        HeapFree(GetProcessHeap(),0,cond);
                }
            }

            rc = ACTION_PerformUIAction(package,buffer);

            if (rc == ERROR_FUNCTION_NOT_CALLED)
                rc = ERROR_SUCCESS;

            if (rc != ERROR_SUCCESS)
            {
                ERR("Execution halted due to error (%i)\n",rc);
                msiobj_release(&row->hdr);
                break;
            }

            msiobj_release(&row->hdr);
        }

        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
    }

end:
    return rc;
}

/********************************************************
 * ACTION helper functions and functions that perform the actions
 *******************************************************/
static BOOL ACTION_HandleStandardAction(MSIPACKAGE *package, LPCWSTR action, 
                                        UINT* rc, BOOL force )
{
    BOOL ret = FALSE; 
    BOOL run = force;
    int i;

    if (!run && !package->script->CurrentlyScripting)
        run = TRUE;
   
    if (!run)
    {
        if (strcmpW(action,szInstallFinalize) == 0 ||
            strcmpW(action,szInstallExecute) == 0 ||
            strcmpW(action,szInstallExecuteAgain) == 0) 
                run = TRUE;
    }
    
    i = 0;
    while (StandardActions[i].action != NULL)
    {
        if (strcmpW(StandardActions[i].action, action)==0)
        {
            ce_actiontext(package, action);
            if (!run)
            {
                ui_actioninfo(package, action, TRUE, 0);
                *rc = schedule_action(package,INSTALL_SCRIPT,action);
                ui_actioninfo(package, action, FALSE, *rc);
            }
            else
            {
                ui_actionstart(package, action);
                if (StandardActions[i].handler)
                {
                    *rc = StandardActions[i].handler(package);
                }
                else
                {
                    FIXME("UNHANDLED Standard Action %s\n",debugstr_w(action));
                    *rc = ERROR_SUCCESS;
                }
            }
            ret = TRUE;
            break;
        }
        i++;
    }
    return ret;
}

BOOL ACTION_HandleDialogBox(MSIPACKAGE *package, LPCWSTR dialog, UINT* rc)
{
    BOOL ret = FALSE;

    if (ACTION_DialogBox(package,dialog) == ERROR_SUCCESS)
    {
        *rc = package->CurrentInstallState;
        ret = TRUE;
    }
    return ret;
}

BOOL ACTION_HandleCustomAction(MSIPACKAGE* package, LPCWSTR action, UINT* rc, 
                               BOOL force )
{
    BOOL ret=FALSE;
    UINT arc;

    arc = ACTION_CustomAction(package,action, force);

    if (arc != ERROR_CALL_NOT_IMPLEMENTED)
    {
        *rc = arc;
        ret = TRUE;
    }
    return ret;
}

/* 
 * A lot of actions are really important even if they don't do anything
 * explicit... Lots of properties are set at the beginning of the installation
 * CostFinalize does a bunch of work to translate the directories and such
 * 
 * But until I get write access to the database that is hard, so I am going to
 * hack it to see if I can get something to run.
 */
UINT ACTION_PerformAction(MSIPACKAGE *package, const WCHAR *action, BOOL force)
{
    UINT rc = ERROR_SUCCESS; 
    BOOL handled;

    TRACE("Performing action (%s)\n",debugstr_w(action));

    handled = ACTION_HandleStandardAction(package, action, &rc, force);

    if (!handled)
        handled = ACTION_HandleCustomAction(package, action, &rc, force);

    if (!handled)
    {
        FIXME("UNHANDLED MSI ACTION %s\n",debugstr_w(action));
        rc = ERROR_FUNCTION_NOT_CALLED;
    }

    package->CurrentInstallState = rc;
    return rc;
}

UINT ACTION_PerformUIAction(MSIPACKAGE *package, const WCHAR *action)
{
    UINT rc = ERROR_SUCCESS;
    BOOL handled = FALSE;

    TRACE("Performing action (%s)\n",debugstr_w(action));

    handled = ACTION_HandleStandardAction(package, action, &rc,TRUE);

    if (!handled)
        handled = ACTION_HandleCustomAction(package, action, &rc, FALSE);

    if (!handled)
        handled = ACTION_HandleDialogBox(package, action, &rc);

    msi_dialog_check_messages( NULL );

    if (!handled)
    {
        FIXME("UNHANDLED MSI ACTION %s\n",debugstr_w(action));
        rc = ERROR_FUNCTION_NOT_CALLED;
    }

    package->CurrentInstallState = rc;
    return rc;
}

/***********************************************************************
 *            create_full_pathW
 *
 * Recursively create all directories in the path.
 *
 * shamelessly stolen from setupapi/queue.c
 */
static BOOL create_full_pathW(const WCHAR *path)
{
    BOOL ret = TRUE;
    int len;
    WCHAR *new_path;

    new_path = HeapAlloc(GetProcessHeap(), 0, (strlenW(path) + 1) *
                                              sizeof(WCHAR));

    strcpyW(new_path, path);

    while((len = strlenW(new_path)) && new_path[len - 1] == '\\')
    new_path[len - 1] = 0;

    while(!CreateDirectoryW(new_path, NULL))
    {
        WCHAR *slash;
        DWORD last_error = GetLastError();
        if(last_error == ERROR_ALREADY_EXISTS)
            break;

        if(last_error != ERROR_PATH_NOT_FOUND)
        {
            ret = FALSE;
            break;
        }

        if(!(slash = strrchrW(new_path, '\\')))
        {
            ret = FALSE;
            break;
        }

        len = slash - new_path;
        new_path[len] = 0;
        if(!create_full_pathW(new_path))
        {
            ret = FALSE;
            break;
        }
        new_path[len] = '\\';
    }

    HeapFree(GetProcessHeap(), 0, new_path);
    return ret;
}

/*
 * Also we cannot enable/disable components either, so for now I am just going 
 * to do all the directories for all the components.
 */
static UINT ACTION_CreateFolders(MSIPACKAGE *package)
{
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ',
         '`','D','i','r','e','c','t','o','r','y','_','`',
         ' ','F','R','O','M',' ',
         '`','C','r','e','a','t','e','F','o','l','d','e','r','`',0 };
    UINT rc;
    MSIQUERY *view;
    MSIFOLDER *folder;

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view );
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }
    
    while (1)
    {
        WCHAR dir[0x100];
        LPWSTR full_path;
        DWORD sz;
        MSIRECORD *row = NULL, *uirow;

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }

        sz=0x100;
        rc = MSI_RecordGetStringW(row,1,dir,&sz);

        if (rc!= ERROR_SUCCESS)
        {
            ERR("Unable to get folder id \n");
            msiobj_release(&row->hdr);
            continue;
        }

        sz = MAX_PATH;
        full_path = resolve_folder(package,dir,FALSE,FALSE,&folder);
        if (!full_path)
        {
            ERR("Unable to resolve folder id %s\n",debugstr_w(dir));
            msiobj_release(&row->hdr);
            continue;
        }

        TRACE("Folder is %s\n",debugstr_w(full_path));

        /* UI stuff */
        uirow = MSI_CreateRecord(1);
        MSI_RecordSetStringW(uirow,1,full_path);
        ui_actiondata(package,szCreateFolders,uirow);
        msiobj_release( &uirow->hdr );

        if (folder->State == 0)
            create_full_pathW(full_path);

        folder->State = 3;

        msiobj_release(&row->hdr);
        HeapFree(GetProcessHeap(),0,full_path);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);
   
    return rc;
}

static int load_component(MSIPACKAGE* package, MSIRECORD * row)
{
    int index = package->loaded_components;
    DWORD sz;

    /* fill in the data */

    package->loaded_components++;
    if (package->loaded_components == 1)
        package->components = HeapAlloc(GetProcessHeap(),0,
                                        sizeof(MSICOMPONENT));
    else
        package->components = HeapReAlloc(GetProcessHeap(),0,
            package->components, package->loaded_components * 
            sizeof(MSICOMPONENT));

    memset(&package->components[index],0,sizeof(MSICOMPONENT));

    sz = IDENTIFIER_SIZE;       
    MSI_RecordGetStringW(row,1,package->components[index].Component,&sz);

    TRACE("Loading Component %s\n",
           debugstr_w(package->components[index].Component));

    sz = 0x100;
    if (!MSI_RecordIsNull(row,2))
        MSI_RecordGetStringW(row,2,package->components[index].ComponentId,&sz);
            
    sz = IDENTIFIER_SIZE;       
    MSI_RecordGetStringW(row,3,package->components[index].Directory,&sz);

    package->components[index].Attributes = MSI_RecordGetInteger(row,4);

    sz = 0x100;       
    MSI_RecordGetStringW(row,5,package->components[index].Condition,&sz);

    sz = IDENTIFIER_SIZE;       
    MSI_RecordGetStringW(row,6,package->components[index].KeyPath,&sz);

    package->components[index].Installed = INSTALLSTATE_ABSENT;
    package->components[index].Action = INSTALLSTATE_UNKNOWN;
    package->components[index].ActionRequest = INSTALLSTATE_UNKNOWN;

    package->components[index].Enabled = TRUE;

    return index;
}

static void load_feature(MSIPACKAGE* package, MSIRECORD * row)
{
    int index = package->loaded_features;
    DWORD sz;
    static const WCHAR Query1[] = 
        {'S','E','L','E','C','T',' ',
         '`','C','o','m','p','o','n','e','n','t','_','`',
         ' ','F','R','O','M',' ','`','F','e','a','t','u','r','e',
         'C','o','m','p','o','n','e','n','t','s','`',' ',
         'W','H','E','R','E',' ',
         '`','F','e', 'a','t','u','r','e','_','`',' ','=','\'','%','s','\'',0};
    static const WCHAR Query2[] = 
        {'S','E','L','E','C','T',' ','*',' ','F','R', 'O','M',' ', 
         '`','C','o','m','p','o','n','e','n','t','`',' ',
         'W','H','E','R','E',' ', 
         '`','C','o','m','p','o','n','e','n','t','`',' ',
         '=','\'','%','s','\'',0};
    MSIQUERY * view;
    MSIQUERY * view2;
    MSIRECORD * row2;
    MSIRECORD * row3;
    UINT    rc;

    /* fill in the data */

    package->loaded_features ++;
    if (package->loaded_features == 1)
        package->features = HeapAlloc(GetProcessHeap(),0,sizeof(MSIFEATURE));
    else
        package->features = HeapReAlloc(GetProcessHeap(),0,package->features,
                                package->loaded_features * sizeof(MSIFEATURE));

    memset(&package->features[index],0,sizeof(MSIFEATURE));
    
    sz = IDENTIFIER_SIZE;       
    MSI_RecordGetStringW(row,1,package->features[index].Feature,&sz);

    TRACE("Loading feature %s\n",debugstr_w(package->features[index].Feature));

    sz = IDENTIFIER_SIZE;
    if (!MSI_RecordIsNull(row,2))
        MSI_RecordGetStringW(row,2,package->features[index].Feature_Parent,&sz);

    sz = 0x100;
     if (!MSI_RecordIsNull(row,3))
        MSI_RecordGetStringW(row,3,package->features[index].Title,&sz);

     sz = 0x100;
     if (!MSI_RecordIsNull(row,4))
        MSI_RecordGetStringW(row,4,package->features[index].Description,&sz);

    if (!MSI_RecordIsNull(row,5))
        package->features[index].Display = MSI_RecordGetInteger(row,5);
  
    package->features[index].Level= MSI_RecordGetInteger(row,6);

     sz = IDENTIFIER_SIZE;
     if (!MSI_RecordIsNull(row,7))
        MSI_RecordGetStringW(row,7,package->features[index].Directory,&sz);

    package->features[index].Attributes= MSI_RecordGetInteger(row,8);

    package->features[index].Installed = INSTALLSTATE_ABSENT;
    package->features[index].Action = INSTALLSTATE_UNKNOWN;
    package->features[index].ActionRequest = INSTALLSTATE_UNKNOWN;

    /* load feature components */

    rc = MSI_OpenQuery(package->db, &view, Query1, package->features[index].Feature);
    if (rc != ERROR_SUCCESS)
        return;
    rc = MSI_ViewExecute(view,0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return;
    }
    while (1)
    {
        DWORD sz = 0x100;
        WCHAR buffer[0x100];
        DWORD rc;
        INT c_indx;
        INT cnt = package->features[index].ComponentCount;

        rc = MSI_ViewFetch(view,&row2);
        if (rc != ERROR_SUCCESS)
            break;

        sz = 0x100;
        MSI_RecordGetStringW(row2,1,buffer,&sz);

        /* check to see if the component is already loaded */
        c_indx = get_loaded_component(package,buffer);
        if (c_indx != -1)
        {
            TRACE("Component %s already loaded at %i\n", debugstr_w(buffer),
                  c_indx);
            package->features[index].Components[cnt] = c_indx;
            package->features[index].ComponentCount ++;
            msiobj_release( &row2->hdr );
            continue;
        }

        rc = MSI_OpenQuery(package->db, &view2, Query2, buffer);
        if (rc != ERROR_SUCCESS)
        {
            msiobj_release( &row2->hdr );
            continue;
        }
        rc = MSI_ViewExecute(view2,0);
        if (rc != ERROR_SUCCESS)
        {
            msiobj_release( &row2->hdr );
            MSI_ViewClose(view2);
            msiobj_release( &view2->hdr );  
            continue;
        }
        while (1)
        {
            DWORD rc;

            rc = MSI_ViewFetch(view2,&row3);
            if (rc != ERROR_SUCCESS)
                break;
            c_indx = load_component(package,row3);
            msiobj_release( &row3->hdr );

            package->features[index].Components[cnt] = c_indx;
            package->features[index].ComponentCount ++;
            TRACE("Loaded new component to index %i\n",c_indx);
        }
        MSI_ViewClose(view2);
        msiobj_release( &view2->hdr );
        msiobj_release( &row2->hdr );
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);
}

static UINT load_file(MSIPACKAGE* package, MSIRECORD * row)
{
    DWORD index = package->loaded_files;
    DWORD i;
    LPWSTR buffer;

    /* fill in the data */

    package->loaded_files++;
    if (package->loaded_files== 1)
        package->files = HeapAlloc(GetProcessHeap(),0,sizeof(MSIFILE));
    else
        package->files = HeapReAlloc(GetProcessHeap(),0,
            package->files , package->loaded_files * sizeof(MSIFILE));

    memset(&package->files[index],0,sizeof(MSIFILE));
 
    package->files[index].File = load_dynamic_stringW(row, 1);
    buffer = load_dynamic_stringW(row, 2);

    package->files[index].ComponentIndex = -1;
    for (i = 0; i < package->loaded_components; i++)
        if (strcmpW(package->components[i].Component,buffer)==0)
        {
            package->files[index].ComponentIndex = i;
            break;
        }
    if (package->files[index].ComponentIndex == -1)
        ERR("Unfound Component %s\n",debugstr_w(buffer));
    HeapFree(GetProcessHeap(), 0, buffer);

    package->files[index].FileName = load_dynamic_stringW(row,3);
    reduce_to_longfilename(package->files[index].FileName);

    package->files[index].ShortName = load_dynamic_stringW(row,3);
    reduce_to_shortfilename(package->files[index].ShortName);
    
    package->files[index].FileSize = MSI_RecordGetInteger(row,4);
    package->files[index].Version = load_dynamic_stringW(row, 5);
    package->files[index].Language = load_dynamic_stringW(row, 6);
    package->files[index].Attributes= MSI_RecordGetInteger(row,7);
    package->files[index].Sequence= MSI_RecordGetInteger(row,8);

    package->files[index].Temporary = FALSE;
    package->files[index].State = 0;

    TRACE("File Loaded (%s)\n",debugstr_w(package->files[index].File));  
 
    return ERROR_SUCCESS;
}

static UINT load_all_files(MSIPACKAGE *package)
{
    MSIQUERY * view;
    MSIRECORD * row;
    UINT rc;
    static const WCHAR Query[] =
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','F','i','l','e','`',' ', 'O','R','D','E','R',' ','B','Y',' ',
         '`','S','e','q','u','e','n','c','e','`', 0};

    if (!package)
        return ERROR_INVALID_HANDLE;

    rc = MSI_DatabaseOpenViewW(package->db, Query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;
   
    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return ERROR_SUCCESS;
    }

    while (1)
    {
        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }
        load_file(package,row);
        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);

    return ERROR_SUCCESS;
}


/*
 * I am not doing any of the costing functionality yet. 
 * Mostly looking at doing the Component and Feature loading
 *
 * The native MSI does A LOT of modification to tables here. Mostly adding
 * a lot of temporary columns to the Feature and Component tables. 
 *
 *    note: Native msi also tracks the short filename. But I am only going to
 *          track the long ones.  Also looking at this directory table
 *          it appears that the directory table does not get the parents
 *          resolved base on property only based on their entries in the 
 *          directory table.
 */
static UINT ACTION_CostInitialize(MSIPACKAGE *package)
{
    MSIQUERY * view;
    MSIRECORD * row;
    UINT rc;
    static const WCHAR Query_all[] =
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','F','e','a','t','u','r','e','`',0};
    static const WCHAR szCosting[] =
        {'C','o','s','t','i','n','g','C','o','m','p','l','e','t','e',0 };
    static const WCHAR szZero[] = { '0', 0 };
    WCHAR buffer[3];
    DWORD sz = 3;

    MSI_GetPropertyW(package, szCosting, buffer, &sz);
    if (buffer[0]=='1')
        return ERROR_SUCCESS;
    
    MSI_SetPropertyW(package, szCosting, szZero);
    MSI_SetPropertyW(package, cszRootDrive , c_colon);

    rc = MSI_DatabaseOpenViewW(package->db,Query_all,&view);
    if (rc != ERROR_SUCCESS)
        return rc;
    rc = MSI_ViewExecute(view,0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }
    while (1)
    {
        DWORD rc;

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
            break;
       
        load_feature(package,row); 
        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);

    load_all_files(package);

    return ERROR_SUCCESS;
}

UINT schedule_action(MSIPACKAGE *package, UINT script, LPCWSTR action)
{
    UINT count;
    LPWSTR *newbuf = NULL;
    if (script >= TOTAL_SCRIPTS)
    {
        FIXME("Unknown script requested %i\n",script);
        return ERROR_FUNCTION_FAILED;
    }
    TRACE("Scheduling Action %s in script %i\n",debugstr_w(action), script);
    
    count = package->script->ActionCount[script];
    package->script->ActionCount[script]++;
    if (count != 0)
        newbuf = HeapReAlloc(GetProcessHeap(),0,
                        package->script->Actions[script],
                        package->script->ActionCount[script]* sizeof(LPWSTR));
    else
        newbuf = HeapAlloc(GetProcessHeap(),0, sizeof(LPWSTR));

    newbuf[count] = strdupW(action);
    package->script->Actions[script] = newbuf;

   return ERROR_SUCCESS;
}

UINT execute_script(MSIPACKAGE *package, UINT script )
{
    int i;
    UINT rc = ERROR_SUCCESS;

    TRACE("Executing Script %i\n",script);

    for (i = 0; i < package->script->ActionCount[script]; i++)
    {
        LPWSTR action;
        action = package->script->Actions[script][i];
        ui_actionstart(package, action);
        TRACE("Executing Action (%s)\n",debugstr_w(action));
        rc = ACTION_PerformAction(package, action, TRUE);
        HeapFree(GetProcessHeap(),0,package->script->Actions[script][i]);
        if (rc != ERROR_SUCCESS)
            break;
    }
    HeapFree(GetProcessHeap(),0,package->script->Actions[script]);

    package->script->ActionCount[script] = 0;
    package->script->Actions[script] = NULL;
    return rc;
}

static UINT ACTION_FileCost(MSIPACKAGE *package)
{
    return ERROR_SUCCESS;
}


static INT load_folder(MSIPACKAGE *package, const WCHAR* dir)
{
    static const WCHAR Query[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','D','i','r','e','c', 't','o','r','y','`',' ',
         'W','H','E','R','E',' ', '`', 'D','i','r','e','c','t', 'o','r','y','`',
         ' ','=',' ','\'','%','s','\'',
         0};
    LPWSTR ptargetdir, targetdir, parent, srcdir;
    LPWSTR shortname = NULL;
    MSIRECORD * row = 0;
    INT index = -1;
    DWORD i;

    TRACE("Looking for dir %s\n",debugstr_w(dir));

    for (i = 0; i < package->loaded_folders; i++)
    {
        if (strcmpW(package->folders[i].Directory,dir)==0)
        {
            TRACE(" %s retuning on index %lu\n",debugstr_w(dir),i);
            return i;
        }
    }

    TRACE("Working to load %s\n",debugstr_w(dir));

    index = package->loaded_folders++;
    if (package->loaded_folders==1)
        package->folders = HeapAlloc(GetProcessHeap(),0,
                                        sizeof(MSIFOLDER));
    else
        package->folders= HeapReAlloc(GetProcessHeap(),0,
            package->folders, package->loaded_folders* 
            sizeof(MSIFOLDER));

    memset(&package->folders[index],0,sizeof(MSIFOLDER));

    package->folders[index].Directory = strdupW(dir);

    row = MSI_QueryGetRecord(package->db, Query, dir);
    if (!row)
        return -1;

    ptargetdir = targetdir = load_dynamic_stringW(row,3);

    /* split src and target dir */
    if (strchrW(targetdir,':'))
    {
        srcdir=strchrW(targetdir,':');
        *srcdir=0;
        srcdir ++;
    }
    else
        srcdir=NULL;

    /* for now only pick long filename versions */
    if (strchrW(targetdir,'|'))
    {
        shortname = targetdir;
        targetdir = strchrW(targetdir,'|'); 
        *targetdir = 0;
        targetdir ++;
    }
    /* for the sourcedir pick the short filename */
    if (srcdir && strchrW(srcdir,'|'))
    {
        LPWSTR p = strchrW(srcdir,'|'); 
        *p = 0;
    }

    /* now check for root dirs */
    if (targetdir[0] == '.' && targetdir[1] == 0)
        targetdir = NULL;
        
    if (srcdir && srcdir[0] == '.' && srcdir[1] == 0)
        srcdir = NULL;

    if (targetdir)
    {
        TRACE("   TargetDefault = %s\n",debugstr_w(targetdir));
        HeapFree(GetProcessHeap(),0, package->folders[index].TargetDefault);
        package->folders[index].TargetDefault = strdupW(targetdir);
    }

    if (srcdir)
        package->folders[index].SourceDefault = strdupW(srcdir);
    else if (shortname)
        package->folders[index].SourceDefault = strdupW(shortname);
    else if (targetdir)
        package->folders[index].SourceDefault = strdupW(targetdir);
    HeapFree(GetProcessHeap(), 0, ptargetdir);
        TRACE("   SourceDefault = %s\n",debugstr_w(package->folders[index].SourceDefault));

    parent = load_dynamic_stringW(row,2);
    if (parent) 
    {
        i = load_folder(package,parent);
        package->folders[index].ParentIndex = i;
        TRACE("Parent is index %i... %s %s\n",
                    package->folders[index].ParentIndex,
        debugstr_w(package->folders[package->folders[index].ParentIndex].Directory),
                    debugstr_w(parent));
    }
    else
        package->folders[index].ParentIndex = -2;
    HeapFree(GetProcessHeap(), 0, parent);

    package->folders[index].Property = load_dynamic_property(package, dir,NULL);

    msiobj_release(&row->hdr);
    TRACE(" %s retuning on index %i\n",debugstr_w(dir),index);
    return index;
}


LPWSTR resolve_folder(MSIPACKAGE *package, LPCWSTR name, BOOL source, 
                      BOOL set_prop, MSIFOLDER **folder)
{
    DWORD i;
    LPWSTR p, path = NULL;

    TRACE("Working to resolve %s\n",debugstr_w(name));

    /* special resolving for Target and Source root dir */
    if (strcmpW(name,cszTargetDir)==0 || strcmpW(name,cszSourceDir)==0)
    {
        if (!source)
        {
            path = load_dynamic_property(package,cszTargetDir,NULL);
            if (!path)
            {
                path = load_dynamic_property(package,cszRootDrive,NULL);
                if (set_prop)
                    MSI_SetPropertyW(package,cszTargetDir,path);
            }
            if (folder)
            {
                for (i = 0; i < package->loaded_folders; i++)
                {
                    if (strcmpW(package->folders[i].Directory,name)==0)
                        break;
                }
                *folder = &(package->folders[i]);
            }
            return path;
        }
        else
        {
            path = load_dynamic_property(package,cszSourceDir,NULL);
            if (!path)
            {
                path = load_dynamic_property(package,cszDatabase,NULL);
                if (path)
                {
                    p = strrchrW(path,'\\');
                    if (p)
                        *(p+1) = 0;
                }
            }
            if (folder)
            {
                for (i = 0; i < package->loaded_folders; i++)
                {
                    if (strcmpW(package->folders[i].Directory,name)==0)
                        break;
                }
                *folder = &(package->folders[i]);
            }
            return path;
        }
    }

    for (i = 0; i < package->loaded_folders; i++)
    {
        if (strcmpW(package->folders[i].Directory,name)==0)
            break;
    }

    if (i >= package->loaded_folders)
        return NULL;

    if (folder)
        *folder = &(package->folders[i]);

    if (!source && package->folders[i].ResolvedTarget)
    {
        path = strdupW(package->folders[i].ResolvedTarget);
        TRACE("   already resolved to %s\n",debugstr_w(path));
        return path;
    }
    else if (source && package->folders[i].ResolvedSource)
    {
        path = strdupW(package->folders[i].ResolvedSource);
        TRACE("   (source)already resolved to %s\n",debugstr_w(path));
        return path;
    }
    else if (!source && package->folders[i].Property)
    {
        path = build_directory_name(2, package->folders[i].Property, NULL);
                    
        TRACE("   internally set to %s\n",debugstr_w(path));
        if (set_prop)
            MSI_SetPropertyW(package,name,path);
        return path;
    }

    if (package->folders[i].ParentIndex >= 0)
    {
        LPWSTR parent = package->folders[package->folders[i].ParentIndex].Directory;

        TRACE(" ! Parent is %s\n", debugstr_w(parent));

        p = resolve_folder(package, parent, source, set_prop, NULL);
        if (!source)
        {
            TRACE("   TargetDefault = %s\n",debugstr_w(package->folders[i].TargetDefault));
            path = build_directory_name(3, p, package->folders[i].TargetDefault, NULL);
            package->folders[i].ResolvedTarget = strdupW(path);
            TRACE("   resolved into %s\n",debugstr_w(path));
            if (set_prop)
                MSI_SetPropertyW(package,name,path);
        }
        else 
        {
            path = build_directory_name(3, p, package->folders[i].SourceDefault, NULL);
            TRACE("   (source)resolved into %s\n",debugstr_w(path));
            package->folders[i].ResolvedSource = strdupW(path);
        }
        HeapFree(GetProcessHeap(),0,p);
    }
    return path;
}

/* scan for and update current install states */
void ACTION_UpdateInstallStates(MSIPACKAGE *package)
{
    int i;
    LPWSTR productcode;

    productcode = load_dynamic_property(package,szProductCode,NULL);

    for (i = 0; i < package->loaded_components; i++)
    {
        INSTALLSTATE res;
        res = MsiGetComponentPathW(productcode, 
                        package->components[i].ComponentId , NULL, NULL);
        if (res < 0)
            res = INSTALLSTATE_ABSENT;
        package->components[i].Installed = res;
    }

    for (i = 0; i < package->loaded_features; i++)
    {
        INSTALLSTATE res = -10;
        int j;
        for (j = 0; j < package->features[i].ComponentCount; j++)
        {
            MSICOMPONENT* component = &package->components[package->features[i].
                                                           Components[j]];
            if (res == -10)
                res = component->Installed;
            else
            {
                if (res == component->Installed)
                    continue;

                if (res != component->Installed)
                        res = INSTALLSTATE_INCOMPLETE;
            }
        }
    }
}

/* update compoennt state based on a feature change */
void ACTION_UpdateComponentStates(MSIPACKAGE *package, LPCWSTR szFeature)
{
    int i;
    INSTALLSTATE newstate;
    MSIFEATURE *feature;

    i = get_loaded_feature(package,szFeature);
    if (i < 0)
        return;

    feature = &package->features[i];
    newstate = feature->ActionRequest;

    for( i = 0; i < feature->ComponentCount; i++)
    {
        MSICOMPONENT* component = &package->components[feature->Components[i]];

        TRACE("MODIFYING(%i): Component %s (Installed %i, Action %i, Request %i)\n",
            newstate, debugstr_w(component->Component), component->Installed, 
            component->Action, component->ActionRequest);
        
        if (!component->Enabled)
            continue;
        else
        {
            if (newstate == INSTALLSTATE_LOCAL)
            {
                component->ActionRequest = INSTALLSTATE_LOCAL;
                component->Action = INSTALLSTATE_LOCAL;
            }
            else 
            {
                int j,k;

                component->ActionRequest = newstate;
                component->Action = newstate;

                /*if any other feature wants is local we need to set it local*/
                for (j = 0; 
                     j < package->loaded_features &&
                     component->ActionRequest != INSTALLSTATE_LOCAL; 
                     j++)
                {
                    for (k = 0; k < package->features[j].ComponentCount; k++)
                        if ( package->features[j].Components[k] ==
                             feature->Components[i] )
                        {
                            if (package->features[j].ActionRequest == 
                                INSTALLSTATE_LOCAL)
                            {
                                TRACE("Saved by %s\n", debugstr_w(package->features[j].Feature));
                                component->ActionRequest = INSTALLSTATE_LOCAL;
                                component->Action = INSTALLSTATE_LOCAL;
                            }
                            break;
                        }
                }
            }
        }
        TRACE("Result (%i): Component %s (Installed %i, Action %i, Request %i)\n",
            newstate, debugstr_w(component->Component), component->Installed, 
            component->Action, component->ActionRequest);
    } 
}

static BOOL process_state_property (MSIPACKAGE* package, LPCWSTR property, 
                                    INSTALLSTATE state)
{
    static const WCHAR all[]={'A','L','L',0};
    LPWSTR override = NULL;
    INT i;
    BOOL rc = FALSE;

    override = load_dynamic_property(package, property, NULL);
    if (override)
    {
        rc = TRUE;
        for(i = 0; i < package->loaded_features; i++)
        {
            if (strcmpiW(override,all)==0)
            {
                package->features[i].ActionRequest= state;
                package->features[i].Action = state;
            }
            else
            {
                LPWSTR ptr = override;
                LPWSTR ptr2 = strchrW(override,',');

                while (ptr)
                {
                    if ((ptr2 && 
                        strncmpW(ptr,package->features[i].Feature, ptr2-ptr)==0)
                        || (!ptr2 &&
                        strcmpW(ptr,package->features[i].Feature)==0))
                    {
                        package->features[i].ActionRequest= state;
                        package->features[i].Action = state;
                        break;
                    }
                    if (ptr2)
                    {
                        ptr=ptr2+1;
                        ptr2 = strchrW(ptr,',');
                    }
                    else
                        break;
                }
            }
        }
        HeapFree(GetProcessHeap(),0,override);
    } 

    return rc;
}

static UINT SetFeatureStates(MSIPACKAGE *package)
{
    LPWSTR level;
    INT install_level;
    DWORD i;
    INT j;
    static const WCHAR szlevel[] =
        {'I','N','S','T','A','L','L','L','E','V','E','L',0};
    static const WCHAR szAddLocal[] =
        {'A','D','D','L','O','C','A','L',0};
    static const WCHAR szRemove[] =
        {'R','E','M','O','V','E',0};
    BOOL override = FALSE;

    /* I do not know if this is where it should happen.. but */

    TRACE("Checking Install Level\n");

    level = load_dynamic_property(package,szlevel,NULL);
    if (level)
    {
        install_level = atoiW(level);
        HeapFree(GetProcessHeap(), 0, level);
    }
    else
        install_level = 1;

    /* ok hereis the _real_ rub
     * all these activation/deactivation things happen in order and things
     * later on the list override things earlier on the list.
     * 1) INSTALLLEVEL processing
     * 2) ADDLOCAL
     * 3) REMOVE
     * 4) ADDSOURCE
     * 5) ADDDEFAULT
     * 6) REINSTALL
     * 7) COMPADDLOCAL
     * 8) COMPADDSOURCE
     * 9) FILEADDLOCAL
     * 10) FILEADDSOURCE
     * 11) FILEADDDEFAULT
     * I have confirmed that if ADDLOCAL is stated then the INSTALLLEVEL is
     * ignored for all the features. seems strange, especially since it is not
     * documented anywhere, but it is how it works. 
     *
     * I am still ignoring a lot of these. But that is ok for now, ADDLOCAL and
     * REMOVE are the big ones, since we don't handle administrative installs
     * yet anyway.
     */
    override |= process_state_property(package,szAddLocal,INSTALLSTATE_LOCAL);
    override |= process_state_property(package,szRemove,INSTALLSTATE_ABSENT);

    if (!override)
    {
        for(i = 0; i < package->loaded_features; i++)
        {
            BOOL feature_state = ((package->features[i].Level > 0) &&
                             (package->features[i].Level <= install_level));

            if ((feature_state) && 
               (package->features[i].Action == INSTALLSTATE_UNKNOWN))
            {
                if (package->features[i].Attributes & 
                                msidbFeatureAttributesFavorSource)
                {
                    package->features[i].ActionRequest = INSTALLSTATE_SOURCE;
                    package->features[i].Action = INSTALLSTATE_SOURCE;
                }
                else if (package->features[i].Attributes &
                                msidbFeatureAttributesFavorAdvertise)
                {
                    package->features[i].ActionRequest =INSTALLSTATE_ADVERTISED;
                    package->features[i].Action =INSTALLSTATE_ADVERTISED;
                }
                else
                {
                    package->features[i].ActionRequest = INSTALLSTATE_LOCAL;
                    package->features[i].Action = INSTALLSTATE_LOCAL;
                }
            }
        }
    }

    /*
     * now we want to enable or disable components base on feature 
    */

    for(i = 0; i < package->loaded_features; i++)
    {
        MSIFEATURE* feature = &package->features[i];
        TRACE("Examining Feature %s (Installed %i, Action %i, Request %i)\n",
            debugstr_w(feature->Feature), feature->Installed, feature->Action,
            feature->ActionRequest);

        for( j = 0; j < feature->ComponentCount; j++)
        {
            MSICOMPONENT* component = &package->components[
                                                    feature->Components[j]];

            if (!component->Enabled)
            {
                component->Action = INSTALLSTATE_UNKNOWN;
                component->ActionRequest = INSTALLSTATE_UNKNOWN;
            }
            else
            {
                if (feature->Action == INSTALLSTATE_LOCAL)
                {
                    component->Action = INSTALLSTATE_LOCAL;
                    component->ActionRequest = INSTALLSTATE_LOCAL;
                }
                else if (feature->ActionRequest == INSTALLSTATE_SOURCE)
                {
                    if ((component->Action == INSTALLSTATE_UNKNOWN) ||
                        (component->Action == INSTALLSTATE_ABSENT) ||
                        (component->Action == INSTALLSTATE_ADVERTISED))
                           
                    {
                        component->Action = INSTALLSTATE_SOURCE;
                        component->ActionRequest = INSTALLSTATE_SOURCE;
                    }
                }
                else if (feature->ActionRequest == INSTALLSTATE_ADVERTISED)
                {
                    if ((component->Action == INSTALLSTATE_UNKNOWN) ||
                        (component->Action == INSTALLSTATE_ABSENT))
                           
                    {
                        component->Action = INSTALLSTATE_ADVERTISED;
                        component->ActionRequest = INSTALLSTATE_ADVERTISED;
                    }
                }
                else if (feature->ActionRequest == INSTALLSTATE_ABSENT)
                {
                    if (component->Action == INSTALLSTATE_UNKNOWN)
                    {
                        component->Action = INSTALLSTATE_ABSENT;
                        component->ActionRequest = INSTALLSTATE_ABSENT;
                    }
                }
            }
        }
    } 

    for(i = 0; i < package->loaded_components; i++)
    {
        MSICOMPONENT* component= &package->components[i];

        TRACE("Result: Component %s (Installed %i, Action %i, Request %i)\n",
            debugstr_w(component->Component), component->Installed, 
            component->Action, component->ActionRequest);
    }


    return ERROR_SUCCESS;
}

/* 
 * A lot is done in this function aside from just the costing.
 * The costing needs to be implemented at some point but for now I am going
 * to focus on the directory building
 *
 */
static UINT ACTION_CostFinalize(MSIPACKAGE *package)
{
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','D','i','r','e','c','t','o','r','y','`',0};
    static const WCHAR ConditionQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','C','o','n','d','i','t','i','o','n','`',0};
    static const WCHAR szCosting[] =
        {'C','o','s','t','i','n','g','C','o','m','p','l','e','t','e',0 };
    static const WCHAR szlevel[] =
        {'I','N','S','T','A','L','L','L','E','V','E','L',0};
    static const WCHAR szOne[] = { '1', 0 };
    UINT rc;
    MSIQUERY * view;
    DWORD i;
    LPWSTR level;
    DWORD sz = 3;
    WCHAR buffer[3];

    MSI_GetPropertyW(package, szCosting, buffer, &sz);
    if (buffer[0]=='1')
        return ERROR_SUCCESS;

    TRACE("Building Directory properties\n");

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc == ERROR_SUCCESS)
    {
        rc = MSI_ViewExecute(view, 0);
        if (rc != ERROR_SUCCESS)
        {
            MSI_ViewClose(view);
            msiobj_release(&view->hdr);
            return rc;
        }

        while (1)
        {
            WCHAR name[0x100];
            LPWSTR path;
            MSIRECORD * row = 0;
            DWORD sz;

            rc = MSI_ViewFetch(view,&row);
            if (rc != ERROR_SUCCESS)
            {
                rc = ERROR_SUCCESS;
                break;
            }

            sz=0x100;
            MSI_RecordGetStringW(row,1,name,&sz);

            /* This helper function now does ALL the work */
            TRACE("Dir %s ...\n",debugstr_w(name));
            load_folder(package,name);
            path = resolve_folder(package,name,FALSE,TRUE,NULL);
            TRACE("resolves to %s\n",debugstr_w(path));
            HeapFree( GetProcessHeap(), 0, path);

            msiobj_release(&row->hdr);
        }
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
    }

    TRACE("File calculations %i files\n",package->loaded_files);

    for (i = 0; i < package->loaded_files; i++)
    {
        MSICOMPONENT* comp = NULL;
        MSIFILE* file= NULL;

        file = &package->files[i];
        if (file->ComponentIndex >= 0)
            comp = &package->components[file->ComponentIndex];

        if (file->Temporary == TRUE)
            continue;

        if (comp)
        {
            LPWSTR p;

            /* calculate target */
            p = resolve_folder(package, comp->Directory, FALSE, FALSE, NULL);

            HeapFree(GetProcessHeap(),0,file->TargetPath);

            TRACE("file %s is named %s\n",
                   debugstr_w(file->File),debugstr_w(file->FileName));       

            file->TargetPath = build_directory_name(2, p, file->FileName);

            HeapFree(GetProcessHeap(),0,p);

            TRACE("file %s resolves to %s\n",
                   debugstr_w(file->File),debugstr_w(file->TargetPath));       

            if (GetFileAttributesW(file->TargetPath) == INVALID_FILE_ATTRIBUTES)
            {
                file->State = 1;
                comp->Cost += file->FileSize;
            }
            else
            {
                if (file->Version)
                {
                    DWORD handle;
                    DWORD versize;
                    UINT sz;
                    LPVOID version;
                    static const WCHAR name[] = 
                        {'\\',0};
                    static const WCHAR name_fmt[] = 
                        {'%','u','.','%','u','.','%','u','.','%','u',0};
                    WCHAR filever[0x100];
                    VS_FIXEDFILEINFO *lpVer;

                    TRACE("Version comparison.. \n");
                    versize = GetFileVersionInfoSizeW(file->TargetPath,&handle);
                    version = HeapAlloc(GetProcessHeap(),0,versize);
                    GetFileVersionInfoW(file->TargetPath, 0, versize, version);

                    VerQueryValueW(version, name, (LPVOID*)&lpVer, &sz);

                    sprintfW(filever,name_fmt,
                        HIWORD(lpVer->dwFileVersionMS),
                        LOWORD(lpVer->dwFileVersionMS),
                        HIWORD(lpVer->dwFileVersionLS),
                        LOWORD(lpVer->dwFileVersionLS));

                    TRACE("new %s old %s\n", debugstr_w(file->Version),
                          debugstr_w(filever));
                    if (strcmpiW(filever,file->Version)<0)
                    {
                        file->State = 2;
                        FIXME("cost should be diff in size\n");
                        comp->Cost += file->FileSize;
                    }
                    else
                        file->State = 3;
                    HeapFree(GetProcessHeap(),0,version);
                }
                else
                    file->State = 3;
            }
        } 
    }

    TRACE("Evaluating Condition Table\n");

    rc = MSI_DatabaseOpenViewW(package->db, ConditionQuery, &view);
    if (rc == ERROR_SUCCESS)
    {
        rc = MSI_ViewExecute(view, 0);
        if (rc != ERROR_SUCCESS)
        {
            MSI_ViewClose(view);
            msiobj_release(&view->hdr);
            return rc;
        }
    
        while (1)
        {
            WCHAR Feature[0x100];
            MSIRECORD * row = 0;
            DWORD sz;
            int feature_index;

            rc = MSI_ViewFetch(view,&row);

            if (rc != ERROR_SUCCESS)
            {
                rc = ERROR_SUCCESS;
                break;
            }

            sz = 0x100;
            MSI_RecordGetStringW(row,1,Feature,&sz);

            feature_index = get_loaded_feature(package,Feature);
            if (feature_index < 0)
                ERR("FAILED to find loaded feature %s\n",debugstr_w(Feature));
            else
            {
                LPWSTR Condition;
                Condition = load_dynamic_stringW(row,3);

                if (MSI_EvaluateConditionW(package,Condition) == 
                    MSICONDITION_TRUE)
                {
                    int level = MSI_RecordGetInteger(row,2);
                    TRACE("Reseting feature %s to level %i\n",
                           debugstr_w(Feature), level);
                    package->features[feature_index].Level = level;
                }
                HeapFree(GetProcessHeap(),0,Condition);
            }

            msiobj_release(&row->hdr);
        }
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
    }

    TRACE("Enabling or Disabling Components\n");
    for (i = 0; i < package->loaded_components; i++)
    {
        if (package->components[i].Condition[0])
        {
            if (MSI_EvaluateConditionW(package,
                package->components[i].Condition) == MSICONDITION_FALSE)
            {
                TRACE("Disabling component %s\n",
                      debugstr_w(package->components[i].Component));
                package->components[i].Enabled = FALSE;
            }
        }
    }

    MSI_SetPropertyW(package,szCosting,szOne);
    /* set default run level if not set */
    level = load_dynamic_property(package,szlevel,NULL);
    if (!level)
        MSI_SetPropertyW(package,szlevel, szOne);
    else
        HeapFree(GetProcessHeap(),0,level);

    ACTION_UpdateInstallStates(package);

    return SetFeatureStates(package);
}

/*
 * This is a helper function for handling embedded cabinet media
 */
static UINT writeout_cabinet_stream(MSIPACKAGE *package, WCHAR* stream_name,
                                    WCHAR* source)
{
    UINT rc;
    USHORT* data;
    UINT    size;
    DWORD   write;
    HANDLE  the_file;
    WCHAR tmp[MAX_PATH];

    rc = read_raw_stream_data(package->db,stream_name,&data,&size); 
    if (rc != ERROR_SUCCESS)
        return rc;

    write = MAX_PATH;
    if (MSI_GetPropertyW(package, cszTempFolder, tmp, &write))
        GetTempPathW(MAX_PATH,tmp);

    GetTempFileNameW(tmp,stream_name,0,source);

    track_tempfile(package,strrchrW(source,'\\'), source);
    the_file = CreateFileW(source, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    if (the_file == INVALID_HANDLE_VALUE)
    {
        ERR("Unable to create file %s\n",debugstr_w(source));
        rc = ERROR_FUNCTION_FAILED;
        goto end;
    }

    WriteFile(the_file,data,size,&write,NULL);
    CloseHandle(the_file);
    TRACE("wrote %li bytes to %s\n",write,debugstr_w(source));
end:
    HeapFree(GetProcessHeap(),0,data);
    return rc;
}


/* Support functions for FDI functions */
typedef struct
{
    MSIPACKAGE* package;
    LPCSTR cab_path;
    LPCSTR file_name;
} CabData;

static void * cabinet_alloc(ULONG cb)
{
    return HeapAlloc(GetProcessHeap(), 0, cb);
}

static void cabinet_free(void *pv)
{
    HeapFree(GetProcessHeap(), 0, pv);
}

static INT_PTR cabinet_open(char *pszFile, int oflag, int pmode)
{
    DWORD dwAccess = 0;
    DWORD dwShareMode = 0;
    DWORD dwCreateDisposition = OPEN_EXISTING;
    switch (oflag & _O_ACCMODE)
    {
    case _O_RDONLY:
        dwAccess = GENERIC_READ;
        dwShareMode = FILE_SHARE_READ | FILE_SHARE_DELETE;
        break;
    case _O_WRONLY:
        dwAccess = GENERIC_WRITE;
        dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        break;
    case _O_RDWR:
        dwAccess = GENERIC_READ | GENERIC_WRITE;
        dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        break;
    }
    if ((oflag & (_O_CREAT | _O_EXCL)) == (_O_CREAT | _O_EXCL))
        dwCreateDisposition = CREATE_NEW;
    else if (oflag & _O_CREAT)
        dwCreateDisposition = CREATE_ALWAYS;
    return (INT_PTR)CreateFileA(pszFile, dwAccess, dwShareMode, NULL, 
                                dwCreateDisposition, 0, NULL);
}

static UINT cabinet_read(INT_PTR hf, void *pv, UINT cb)
{
    DWORD dwRead;
    if (ReadFile((HANDLE)hf, pv, cb, &dwRead, NULL))
        return dwRead;
    return 0;
}

static UINT cabinet_write(INT_PTR hf, void *pv, UINT cb)
{
    DWORD dwWritten;
    if (WriteFile((HANDLE)hf, pv, cb, &dwWritten, NULL))
        return dwWritten;
    return 0;
}

static int cabinet_close(INT_PTR hf)
{
    return CloseHandle((HANDLE)hf) ? 0 : -1;
}

static long cabinet_seek(INT_PTR hf, long dist, int seektype)
{
    /* flags are compatible and so are passed straight through */
    return SetFilePointer((HANDLE)hf, dist, NULL, seektype);
}

static INT_PTR cabinet_notify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin)
{
    /* FIXME: try to do more processing in this function */
    switch (fdint)
    {
    case fdintCOPY_FILE:
    {
        CabData *data = (CabData*) pfdin->pv;
        ULONG len = strlen(data->cab_path) + strlen(pfdin->psz1);
        char *file;

        LPWSTR trackname;
        LPWSTR trackpath;
        LPWSTR tracknametmp;
        static const WCHAR tmpprefix[] = {'C','A','B','T','M','P','_',0};
       
        if (data->file_name && lstrcmpiA(data->file_name,pfdin->psz1))
                return 0;
        
        file = cabinet_alloc((len+1)*sizeof(char));
        strcpy(file, data->cab_path);
        strcat(file, pfdin->psz1);

        TRACE("file: %s\n", debugstr_a(file));

        /* track this file so it can be deleted if not installed */
        trackpath=strdupAtoW(file);
        tracknametmp=strdupAtoW(strrchr(file,'\\')+1);
        trackname = HeapAlloc(GetProcessHeap(),0,(strlenW(tracknametmp) + 
                                  strlenW(tmpprefix)+1) * sizeof(WCHAR));

        strcpyW(trackname,tmpprefix);
        strcatW(trackname,tracknametmp);

        track_tempfile(data->package, trackname, trackpath);

        HeapFree(GetProcessHeap(),0,trackpath);
        HeapFree(GetProcessHeap(),0,trackname);
        HeapFree(GetProcessHeap(),0,tracknametmp);

        return cabinet_open(file, _O_WRONLY | _O_CREAT, 0);
    }
    case fdintCLOSE_FILE_INFO:
    {
        FILETIME ft;
	    FILETIME ftLocal;
        if (!DosDateTimeToFileTime(pfdin->date, pfdin->time, &ft))
            return -1;
        if (!LocalFileTimeToFileTime(&ft, &ftLocal))
            return -1;
        if (!SetFileTime((HANDLE)pfdin->hf, &ftLocal, 0, &ftLocal))
            return -1;

        cabinet_close(pfdin->hf);
        return 1;
    }
    default:
        return 0;
    }
}

/***********************************************************************
 *            extract_cabinet_file
 *
 * Extract files from a cab file.
 */
static BOOL extract_a_cabinet_file(MSIPACKAGE* package, const WCHAR* source, 
                                 const WCHAR* path, const WCHAR* file)
{
    HFDI hfdi;
    ERF erf;
    BOOL ret;
    char *cabinet;
    char *cab_path;
    char *file_name;
    CabData data;

    TRACE("Extracting %s (%s) to %s\n",debugstr_w(source), 
                    debugstr_w(file), debugstr_w(path));

    hfdi = FDICreate(cabinet_alloc,
                     cabinet_free,
                     cabinet_open,
                     cabinet_read,
                     cabinet_write,
                     cabinet_close,
                     cabinet_seek,
                     0,
                     &erf);
    if (!hfdi)
    {
        ERR("FDICreate failed\n");
        return FALSE;
    }

    if (!(cabinet = strdupWtoA( source )))
    {
        FDIDestroy(hfdi);
        return FALSE;
    }
    if (!(cab_path = strdupWtoA( path )))
    {
        FDIDestroy(hfdi);
        HeapFree(GetProcessHeap(), 0, cabinet);
        return FALSE;
    }

    data.package = package;
    data.cab_path = cab_path;
    if (file)
        file_name = strdupWtoA(file);
    else
        file_name = NULL;
    data.file_name = file_name;

    ret = FDICopy(hfdi, cabinet, "", 0, cabinet_notify, NULL, &data);

    if (!ret)
        ERR("FDICopy failed\n");

    FDIDestroy(hfdi);

    HeapFree(GetProcessHeap(), 0, cabinet);
    HeapFree(GetProcessHeap(), 0, cab_path);
    HeapFree(GetProcessHeap(), 0, file_name);

    return ret;
}

static UINT ready_media_for_file(MSIPACKAGE *package, WCHAR* path, 
                                 MSIFILE* file)
{
    UINT rc = ERROR_SUCCESS;
    MSIRECORD * row = 0;
    static WCHAR source[MAX_PATH];
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','M','e','d','i','a','`',' ','W','H','E','R','E',' ',
         '`','L','a','s','t','S','e','q','u','e','n','c','e','`',' ','>','=',
         ' ','%', 'i',' ','O','R','D','E','R',' ','B','Y',' ',
         '`','L','a','s','t','S','e','q','u','e','n','c','e','`',0};
    WCHAR cab[0x100];
    DWORD sz=0x100;
    INT seq;
    static UINT last_sequence = 0; 

    if (file->Attributes & msidbFileAttributesNoncompressed)
    {
        TRACE("Uncompressed File, no media to ready.\n");
        return ERROR_SUCCESS;
    }

    if (file->Sequence <= last_sequence)
    {
        TRACE("Media already ready (%u, %u)\n",file->Sequence,last_sequence);
        /*extract_a_cabinet_file(package, source,path,file->File); */
        return ERROR_SUCCESS;
    }

    row = MSI_QueryGetRecord(package->db, ExecSeqQuery, file->Sequence);
    if (!row)
        return ERROR_FUNCTION_FAILED;

    seq = MSI_RecordGetInteger(row,2);
    last_sequence = seq;

    if (!MSI_RecordIsNull(row,4))
    {
        sz=0x100;
        MSI_RecordGetStringW(row,4,cab,&sz);
        TRACE("Source is CAB %s\n",debugstr_w(cab));
        /* the stream does not contain the # character */
        if (cab[0]=='#')
        {
            writeout_cabinet_stream(package,&cab[1],source);
            strcpyW(path,source);
            *(strrchrW(path,'\\')+1)=0;
        }
        else
        {
            sz = MAX_PATH;
            if (MSI_GetPropertyW(package, cszSourceDir, source, &sz))
            {
                ERR("No Source dir defined \n");
                rc = ERROR_FUNCTION_FAILED;
            }
            else
            {
                strcpyW(path,source);
                strcatW(source,cab);
                /* extract the cab file into a folder in the temp folder */
                sz = MAX_PATH;
                if (MSI_GetPropertyW(package, cszTempFolder,path, &sz) 
                                    != ERROR_SUCCESS)
                    GetTempPathW(MAX_PATH,path);
            }
        }
        rc = !extract_a_cabinet_file(package, source,path,NULL);
    }
    else
    {
        sz = MAX_PATH;
        MSI_GetPropertyW(package,cszSourceDir,source,&sz);
        strcpyW(path,source);
    }
    msiobj_release(&row->hdr);
    return rc;
}

inline static UINT create_component_directory ( MSIPACKAGE* package, INT component)
{
    UINT rc = ERROR_SUCCESS;
    MSIFOLDER *folder;
    LPWSTR install_path;

    install_path = resolve_folder(package, package->components[component].Directory,
                        FALSE, FALSE, &folder);
    if (!install_path)
        return ERROR_FUNCTION_FAILED; 

    /* create the path */
    if (folder->State == 0)
    {
        create_full_pathW(install_path);
        folder->State = 2;
    }
    HeapFree(GetProcessHeap(), 0, install_path);

    return rc;
}

static UINT ACTION_InstallFiles(MSIPACKAGE *package)
{
    UINT rc = ERROR_SUCCESS;
    DWORD index;
    MSIRECORD * uirow;
    WCHAR uipath[MAX_PATH];

    if (!package)
        return ERROR_INVALID_HANDLE;

    /* increment progress bar each time action data is sent */
    ui_progress(package,1,1,0,0);

    for (index = 0; index < package->loaded_files; index++)
    {
        WCHAR path_to_source[MAX_PATH];
        MSIFILE *file;
        
        file = &package->files[index];

        if (file->Temporary)
            continue;


        if (!ACTION_VerifyComponentForAction(package, file->ComponentIndex, 
                                       INSTALLSTATE_LOCAL))
        {
            ui_progress(package,2,file->FileSize,0,0);
            TRACE("File %s is not scheduled for install\n",
                   debugstr_w(file->File));

            continue;
        }

        if ((file->State == 1) || (file->State == 2))
        {
            LPWSTR p;
            MSICOMPONENT* comp = NULL;

            TRACE("Installing %s\n",debugstr_w(file->File));
            rc = ready_media_for_file(package, path_to_source, file);
            /* 
             * WARNING!
             * our file table could change here because a new temp file
             * may have been created
             */
            file = &package->files[index];
            if (rc != ERROR_SUCCESS)
            {
                ERR("Unable to ready media\n");
                rc = ERROR_FUNCTION_FAILED;
                break;
            }

            create_component_directory( package, file->ComponentIndex);

            /* recalculate file paths because things may have changed */

            if (file->ComponentIndex >= 0)
                comp = &package->components[file->ComponentIndex];

            p = resolve_folder(package, comp->Directory, FALSE, FALSE, NULL);
            HeapFree(GetProcessHeap(),0,file->TargetPath);

            file->TargetPath = build_directory_name(2, p, file->FileName);
            HeapFree(GetProcessHeap(),0,p);

            if (file->Attributes & msidbFileAttributesNoncompressed)
            {
                p = resolve_folder(package, comp->Directory, TRUE, FALSE, NULL);
                file->SourcePath = build_directory_name(2, p, file->ShortName);
                HeapFree(GetProcessHeap(),0,p);
            }
            else
                file->SourcePath = build_directory_name(2, path_to_source, 
                            file->File);


            TRACE("file paths %s to %s\n",debugstr_w(file->SourcePath),
                  debugstr_w(file->TargetPath));

            /* the UI chunk */
            uirow=MSI_CreateRecord(9);
            MSI_RecordSetStringW(uirow,1,file->File);
            strcpyW(uipath,file->TargetPath);
            *(strrchrW(uipath,'\\')+1)=0;
            MSI_RecordSetStringW(uirow,9,uipath);
            MSI_RecordSetInteger(uirow,6,file->FileSize);
            ui_actiondata(package,szInstallFiles,uirow);
            msiobj_release( &uirow->hdr );
            ui_progress(package,2,file->FileSize,0,0);

            
            if (file->Attributes & msidbFileAttributesNoncompressed)
                rc = CopyFileW(file->SourcePath,file->TargetPath,FALSE);
            else
                rc = MoveFileW(file->SourcePath, file->TargetPath);

            if (!rc)
            {
                rc = GetLastError();
                ERR("Unable to move/copy file (%s -> %s) (error %d)\n",
                     debugstr_w(file->SourcePath), debugstr_w(file->TargetPath),
                      rc);
                if (rc == ERROR_ALREADY_EXISTS && file->State == 2)
                {
                    if (!CopyFileW(file->SourcePath,file->TargetPath,FALSE))
                        ERR("Unable to copy file (%s -> %s) (error %ld)\n",
                            debugstr_w(file->SourcePath), 
                            debugstr_w(file->TargetPath), GetLastError());
                    if (!(file->Attributes & msidbFileAttributesNoncompressed))
                        DeleteFileW(file->SourcePath);
                    rc = 0;
                }
                else if (rc == ERROR_FILE_NOT_FOUND)
                {
                    ERR("Source File Not Found!  Continuing\n");
                    rc = 0;
                }
                else if (file->Attributes & msidbFileAttributesVital)
                {
                    ERR("Ignoring Error and continuing (nonvital file)...\n");
                    rc = 0;
                }
            }
            else
            {
                file->State = 4;
                rc = ERROR_SUCCESS;
            }
        }
    }

    return rc;
}

inline static UINT get_file_target(MSIPACKAGE *package, LPCWSTR file_key, 
                                   LPWSTR* file_source)
{
    DWORD index;

    if (!package)
        return ERROR_INVALID_HANDLE;

    for (index = 0; index < package->loaded_files; index ++)
    {
        if (strcmpW(file_key,package->files[index].File)==0)
        {
            if (package->files[index].State >= 2)
            {
                *file_source = strdupW(package->files[index].TargetPath);
                return ERROR_SUCCESS;
            }
            else
                return ERROR_FILE_NOT_FOUND;
        }
    }

    return ERROR_FUNCTION_FAILED;
}

static UINT ACTION_DuplicateFiles(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','D','u','p','l','i','c','a','t','e','F','i','l','e','`',0};

    if (!package)
        return ERROR_INVALID_HANDLE;

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }

    while (1)
    {
        WCHAR file_key[0x100];
        WCHAR *file_source = NULL;
        WCHAR dest_name[0x100];
        LPWSTR dest_path, dest;
        WCHAR component[0x100];
        INT component_index;

        DWORD sz=0x100;

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }

        sz=0x100;
        rc = MSI_RecordGetStringW(row,2,component,&sz);
        if (rc != ERROR_SUCCESS)
        {
            ERR("Unable to get component\n");
            msiobj_release(&row->hdr);
            break;
        }

        component_index = get_loaded_component(package,component);

        if (!ACTION_VerifyComponentForAction(package, component_index, 
                                       INSTALLSTATE_LOCAL))
        {
            TRACE("Skipping copy due to disabled component\n");

            /* the action taken was the same as the current install state */        
            package->components[component_index].Action =
                package->components[component_index].Installed;

            msiobj_release(&row->hdr);
            continue;
        }

        package->components[component_index].Action = INSTALLSTATE_LOCAL;

        sz=0x100;
        rc = MSI_RecordGetStringW(row,3,file_key,&sz);
        if (rc != ERROR_SUCCESS)
        {
            ERR("Unable to get file key\n");
            msiobj_release(&row->hdr);
            break;
        }

        rc = get_file_target(package,file_key,&file_source);

        if (rc != ERROR_SUCCESS)
        {
            ERR("Original file unknown %s\n",debugstr_w(file_key));
            msiobj_release(&row->hdr);
            HeapFree(GetProcessHeap(),0,file_source);
            continue;
        }

        if (MSI_RecordIsNull(row,4))
        {
            strcpyW(dest_name,strrchrW(file_source,'\\')+1);
        }
        else
        {
            sz=0x100;
            MSI_RecordGetStringW(row,4,dest_name,&sz);
            reduce_to_longfilename(dest_name);
         }

        if (MSI_RecordIsNull(row,5))
        {
            LPWSTR p;
            dest_path = strdupW(file_source);
            p = strrchrW(dest_path,'\\');
            if (p)
                *p=0;
        }
        else
        {
            WCHAR destkey[0x100];
            sz=0x100;
            MSI_RecordGetStringW(row,5,destkey,&sz);
            sz = 0x100;
            dest_path = resolve_folder(package, destkey, FALSE,FALSE,NULL);
            if (!dest_path)
            {
                ERR("Unable to get destination folder\n");
                msiobj_release(&row->hdr);
                HeapFree(GetProcessHeap(),0,file_source);
                break;
            }
        }

        dest = build_directory_name(2, dest_path, dest_name);
           
        TRACE("Duplicating file %s to %s\n",debugstr_w(file_source),
              debugstr_w(dest)); 
        
        if (strcmpW(file_source,dest))
            rc = !CopyFileW(file_source,dest,TRUE);
        else
            rc = ERROR_SUCCESS;
        
        if (rc != ERROR_SUCCESS)
            ERR("Failed to copy file %s -> %s, last error %ld\n", debugstr_w(file_source), debugstr_w(dest_path), GetLastError());

        FIXME("We should track these duplicate files as well\n");   
 
        msiobj_release(&row->hdr);
        HeapFree(GetProcessHeap(),0,dest_path);
        HeapFree(GetProcessHeap(),0,dest);
        HeapFree(GetProcessHeap(),0,file_source);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);
    return rc;
}


/* OK this value is "interpreted" and then formatted based on the 
   first few characters */
static LPSTR parse_value(MSIPACKAGE *package, WCHAR *value, DWORD *type, 
                         DWORD *size)
{
    LPSTR data = NULL;
    if (value[0]=='#' && value[1]!='#' && value[1]!='%')
    {
        if (value[1]=='x')
        {
            LPWSTR ptr;
            CHAR byte[5];
            LPWSTR deformated = NULL;
            int count;

            deformat_string(package, &value[2], &deformated);

            /* binary value type */
            ptr = deformated;
            *type = REG_BINARY;
            if (strlenW(ptr)%2)
                *size = (strlenW(ptr)/2)+1;
            else
                *size = strlenW(ptr)/2;

            data = HeapAlloc(GetProcessHeap(),0,*size);

            byte[0] = '0'; 
            byte[1] = 'x'; 
            byte[4] = 0; 
            count = 0;
            /* if uneven pad with a zero in front */
            if (strlenW(ptr)%2)
            {
                byte[2]= '0';
                byte[3]= *ptr;
                ptr++;
                data[count] = (BYTE)strtol(byte,NULL,0);
                count ++;
                TRACE("Uneven byte count\n");
            }
            while (*ptr)
            {
                byte[2]= *ptr;
                ptr++;
                byte[3]= *ptr;
                ptr++;
                data[count] = (BYTE)strtol(byte,NULL,0);
                count ++;
            }
            HeapFree(GetProcessHeap(),0,deformated);

            TRACE("Data %li bytes(%i)\n",*size,count);
        }
        else
        {
            LPWSTR deformated;
            LPWSTR p;
            DWORD d = 0;
            deformat_string(package, &value[1], &deformated);

            *type=REG_DWORD; 
            *size = sizeof(DWORD);
            data = HeapAlloc(GetProcessHeap(),0,*size);
            p = deformated;
            if (*p == '-')
                p++;
            while (*p)
            {
                if ( (*p < '0') || (*p > '9') )
                    break;
                d *= 10;
                d += (*p - '0');
                p++;
            }
            if (deformated[0] == '-')
                d = -d;
            *(LPDWORD)data = d;
            TRACE("DWORD %li\n",*(LPDWORD)data);

            HeapFree(GetProcessHeap(),0,deformated);
        }
    }
    else
    {
        static const WCHAR szMulti[] = {'[','~',']',0};
        WCHAR *ptr;
        *type=REG_SZ;

        if (value[0]=='#')
        {
            if (value[1]=='%')
            {
                ptr = &value[2];
                *type=REG_EXPAND_SZ;
            }
            else
                ptr = &value[1];
         }
         else
            ptr=value;

        if (strstrW(value,szMulti))
            *type = REG_MULTI_SZ;

        *size = deformat_string(package, ptr,(LPWSTR*)&data);
    }
    return data;
}

static UINT ACTION_WriteRegistryValues(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','R','e','g','i','s','t','r','y','`',0 };

    if (!package)
        return ERROR_INVALID_HANDLE;

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }

    /* increment progress bar each time action data is sent */
    ui_progress(package,1,REG_PROGRESS_VALUE,1,0);

    while (1)
    {
        static const WCHAR szHCR[] = 
            {'H','K','E','Y','_','C','L','A','S','S','E','S','_',
             'R','O','O','T','\\',0};
        static const WCHAR szHCU[] =
            {'H','K','E','Y','_','C','U','R','R','E','N','T','_',
             'U','S','E','R','\\',0};
        static const WCHAR szHLM[] =
            {'H','K','E','Y','_','L','O','C','A','L','_',
             'M','A','C','H','I','N','E','\\',0};
        static const WCHAR szHU[] =
            {'H','K','E','Y','_','U','S','E','R','S','\\',0};

        LPSTR value_data = NULL;
        HKEY  root_key, hkey;
        DWORD type,size;
        LPWSTR value, key, name, component, deformated;
        LPCWSTR szRoot;
        INT component_index;
        MSIRECORD * uirow;
        LPWSTR uikey;
        INT   root;
        BOOL check_first = FALSE;

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }
        ui_progress(package,2,0,0,0);

        value = NULL;
        key = NULL;
        uikey = NULL;
        name = NULL;

        component = load_dynamic_stringW(row, 6);
        component_index = get_loaded_component(package,component);

        if (!ACTION_VerifyComponentForAction(package, component_index, 
                                       INSTALLSTATE_LOCAL))
        {
            TRACE("Skipping write due to disabled component\n");
            msiobj_release(&row->hdr);

            package->components[component_index].Action =
                package->components[component_index].Installed;

            goto next;
        }

        package->components[component_index].Action = INSTALLSTATE_LOCAL;

        name = load_dynamic_stringW(row, 4);
        if( MSI_RecordIsNull(row,5) && name )
        {
            /* null values can have special meanings */
            if (name[0]=='-' && name[1] == 0)
            {
                msiobj_release(&row->hdr);
                goto next;
            }
            else if ((name[0]=='+' && name[1] == 0) || 
                     (name[0] == '*' && name[1] == 0))
            {
                HeapFree(GetProcessHeap(),0,name);
                name = NULL;
                check_first = TRUE;
            }
        }

        root = MSI_RecordGetInteger(row,2);
        key = load_dynamic_stringW(row, 3);
      
   
        /* get the root key */
        switch (root)
        {
            case 0:  root_key = HKEY_CLASSES_ROOT; 
                     szRoot = szHCR;
                     break;
            case 1:  root_key = HKEY_CURRENT_USER;
                     szRoot = szHCU;
                     break;
            case 2:  root_key = HKEY_LOCAL_MACHINE;
                     szRoot = szHLM;
                     break;
            case 3:  root_key = HKEY_USERS; 
                     szRoot = szHU;
                     break;
            default:
                 ERR("Unknown root %i\n",root);
                 root_key=NULL;
                 szRoot = NULL;
                 break;
        }
        if (!root_key)
        {
            msiobj_release(&row->hdr);
            goto next;
        }

        deformat_string(package, key , &deformated);
        size = strlenW(deformated) + strlenW(szRoot) + 1;
        uikey = HeapAlloc(GetProcessHeap(), 0, size*sizeof(WCHAR));
        strcpyW(uikey,szRoot);
        strcatW(uikey,deformated);

        if (RegCreateKeyW( root_key, deformated, &hkey))
        {
            ERR("Could not create key %s\n",debugstr_w(deformated));
            msiobj_release(&row->hdr);
            HeapFree(GetProcessHeap(),0,deformated);
            goto next;
        }
        HeapFree(GetProcessHeap(),0,deformated);

        value = load_dynamic_stringW(row,5);
        if (value)
            value_data = parse_value(package, value, &type, &size); 
        else
        {
            static const WCHAR szEmpty[] = {0};
            value_data = (LPSTR)strdupW(szEmpty);
            size = 0;
            type = REG_SZ;
        }

        deformat_string(package, name, &deformated);

        /* get the double nulls to terminate SZ_MULTI */
        if (type == REG_MULTI_SZ)
            size +=sizeof(WCHAR);

        if (!check_first)
        {
            TRACE("Setting value %s of %s\n",debugstr_w(deformated),
                            debugstr_w(uikey));
            RegSetValueExW(hkey, deformated, 0, type, value_data, size);
        }
        else
        {
            DWORD sz = 0;
            rc = RegQueryValueExW(hkey, deformated, NULL, NULL, NULL, &sz);
            if (rc == ERROR_SUCCESS || rc == ERROR_MORE_DATA)
            {
                TRACE("value %s of %s checked already exists\n",
                                debugstr_w(deformated), debugstr_w(uikey));
            }
            else
            {
                TRACE("Checked and setting value %s of %s\n",
                                debugstr_w(deformated), debugstr_w(uikey));
                RegSetValueExW(hkey, deformated, 0, type, value_data, size);
            }
        }

        uirow = MSI_CreateRecord(3);
        MSI_RecordSetStringW(uirow,2,deformated);
        MSI_RecordSetStringW(uirow,1,uikey);

        if (type == REG_SZ)
            MSI_RecordSetStringW(uirow,3,(LPWSTR)value_data);
        else
            MSI_RecordSetStringW(uirow,3,value);

        ui_actiondata(package,szWriteRegistryValues,uirow);
        msiobj_release( &uirow->hdr );

        HeapFree(GetProcessHeap(),0,value_data);
        HeapFree(GetProcessHeap(),0,value);
        HeapFree(GetProcessHeap(),0,deformated);

        msiobj_release(&row->hdr);
        RegCloseKey(hkey);
next:
        HeapFree(GetProcessHeap(),0,uikey);
        HeapFree(GetProcessHeap(),0,key);
        HeapFree(GetProcessHeap(),0,name);
        HeapFree(GetProcessHeap(),0,component);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);
    return rc;
}

static UINT ACTION_InstallInitialize(MSIPACKAGE *package)
{
    package->script->CurrentlyScripting = TRUE;

    return ERROR_SUCCESS;
}


static UINT ACTION_InstallValidate(MSIPACKAGE *package)
{
    DWORD progress = 0;
    DWORD total = 0;
    static const WCHAR q1[]=
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','R','e','g','i','s','t','r','y','`',0};
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    int i;

    TRACE(" InstallValidate \n");

    rc = MSI_DatabaseOpenViewW(package->db, q1, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }
    while (1)
    {
        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }
        progress +=1;

        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);

    total = total + progress * REG_PROGRESS_VALUE;
    total = total + package->loaded_components * COMPONENT_PROGRESS_VALUE;
    for (i=0; i < package->loaded_files; i++)
        total += package->files[i].FileSize;
    ui_progress(package,0,total,0,0);

    for(i = 0; i < package->loaded_features; i++)
    {
        MSIFEATURE* feature = &package->features[i];
        TRACE("Feature: %s; Installed: %i; Action %i; Request %i\n",
            debugstr_w(feature->Feature), feature->Installed, feature->Action,
            feature->ActionRequest);
    }
    
    return ERROR_SUCCESS;
}

static UINT ACTION_LaunchConditions(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view = NULL;
    MSIRECORD * row = 0;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','L','a','u','n','c','h','C','o','n','d','i','t','i','o','n','`',0};
    static const WCHAR title[]=
        {'I','n','s','t','a','l','l',' ','F','a', 'i','l','e','d',0};

    TRACE("Checking launch conditions\n");

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }

    rc = ERROR_SUCCESS;
    while (rc == ERROR_SUCCESS)
    {
        LPWSTR cond = NULL; 
        LPWSTR message = NULL;

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }

        cond = load_dynamic_stringW(row,1);

        if (MSI_EvaluateConditionW(package,cond) != MSICONDITION_TRUE)
        {
            LPWSTR deformated;
            message = load_dynamic_stringW(row,2);
            deformat_string(package,message,&deformated); 
            MessageBoxW(NULL,deformated,title,MB_OK);
            HeapFree(GetProcessHeap(),0,message);
            HeapFree(GetProcessHeap(),0,deformated);
            rc = ERROR_FUNCTION_FAILED;
        }
        HeapFree(GetProcessHeap(),0,cond);
        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);
    return rc;
}

static LPWSTR resolve_keypath( MSIPACKAGE* package, INT
                            component_index)
{
    MSICOMPONENT* cmp = &package->components[component_index];

    if (cmp->KeyPath[0]==0)
    {
        LPWSTR p = resolve_folder(package,cmp->Directory,FALSE,FALSE,NULL);
        return p;
    }
    if (cmp->Attributes & msidbComponentAttributesRegistryKeyPath)
    {
        MSIRECORD * row = 0;
        UINT root,len;
        LPWSTR key,deformated,buffer,name,deformated_name;
        static const WCHAR ExecSeqQuery[] =
            {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
             '`','R','e','g','i','s','t','r','y','`',' ',
             'W','H','E','R','E',' ', '`','R','e','g','i','s','t','r','y','`',
             ' ','=',' ' ,'\'','%','s','\'',0 };
        static const WCHAR fmt[]={'%','0','2','i',':','\\','%','s','\\',0};
        static const WCHAR fmt2[]=
            {'%','0','2','i',':','\\','%','s','\\','%','s',0};

        row = MSI_QueryGetRecord(package->db, ExecSeqQuery,cmp->KeyPath);
        if (!row)
            return NULL;

        root = MSI_RecordGetInteger(row,2);
        key = load_dynamic_stringW(row, 3);
        name = load_dynamic_stringW(row, 4);
        deformat_string(package, key , &deformated);
        deformat_string(package, name, &deformated_name);

        len = strlenW(deformated) + 6;
        if (deformated_name)
            len+=strlenW(deformated_name);

        buffer = HeapAlloc(GetProcessHeap(),0, len *sizeof(WCHAR));

        if (deformated_name)
            sprintfW(buffer,fmt2,root,deformated,deformated_name);
        else
            sprintfW(buffer,fmt,root,deformated);

        HeapFree(GetProcessHeap(),0,key);
        HeapFree(GetProcessHeap(),0,deformated);
        HeapFree(GetProcessHeap(),0,name);
        HeapFree(GetProcessHeap(),0,deformated_name);
        msiobj_release(&row->hdr);

        return buffer;
    }
    else if (cmp->Attributes & msidbComponentAttributesODBCDataSource)
    {
        FIXME("UNIMPLEMENTED keypath as ODBC Source\n");
        return NULL;
    }
    else
    {
        int j;
        j = get_loaded_file(package,cmp->KeyPath);

        if (j>=0)
        {
            LPWSTR p = strdupW(package->files[j].TargetPath);
            return p;
        }
    }
    return NULL;
}

static HKEY openSharedDLLsKey()
{
    HKEY hkey=0;
    static const WCHAR path[] =
        {'S','o','f','t','w','a','r','e','\\',
         'M','i','c','r','o','s','o','f','t','\\',
         'W','i','n','d','o','w','s','\\',
         'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
         'S','h','a','r','e','d','D','L','L','s',0};

    RegCreateKeyW(HKEY_LOCAL_MACHINE,path,&hkey);
    return hkey;
}

static UINT ACTION_GetSharedDLLsCount(LPCWSTR dll)
{
    HKEY hkey;
    DWORD count=0;
    DWORD type;
    DWORD sz = sizeof(count);
    DWORD rc;
    
    hkey = openSharedDLLsKey();
    rc = RegQueryValueExW(hkey, dll, NULL, &type, (LPBYTE)&count, &sz);
    if (rc != ERROR_SUCCESS)
        count = 0;
    RegCloseKey(hkey);
    return count;
}

static UINT ACTION_WriteSharedDLLsCount(LPCWSTR path, UINT count)
{
    HKEY hkey;

    hkey = openSharedDLLsKey();
    if (count > 0)
        RegSetValueExW(hkey,path,0,REG_DWORD,
                    (LPBYTE)&count,sizeof(count));
    else
        RegDeleteValueW(hkey,path);
    RegCloseKey(hkey);
    return count;
}

/*
 * Return TRUE if the count should be written out and FALSE if not
 */
static void ACTION_RefCountComponent( MSIPACKAGE* package, UINT index)
{
    INT count = 0;
    BOOL write = FALSE;
    INT j;

    /* only refcount DLLs */
    if (package->components[index].KeyPath[0]==0 || 
        package->components[index].Attributes & 
            msidbComponentAttributesRegistryKeyPath || 
        package->components[index].Attributes & 
            msidbComponentAttributesODBCDataSource)
        write = FALSE;
    else
    {
        count = ACTION_GetSharedDLLsCount(package->components[index].
                        FullKeypath);
        write = (count > 0);

        if (package->components[index].Attributes & 
                    msidbComponentAttributesSharedDllRefCount)
            write = TRUE;
    }

    /* increment counts */
    for (j = 0; j < package->loaded_features; j++)
    {
        int i;

        if (!ACTION_VerifyFeatureForAction(package,j,INSTALLSTATE_LOCAL))
            continue;

        for (i = 0; i < package->features[j].ComponentCount; i++)
        {
            if (package->features[j].Components[i] == index)
                count++;
        }
    }
    /* decrement counts */
    for (j = 0; j < package->loaded_features; j++)
    {
        int i;
        if (!ACTION_VerifyFeatureForAction(package,j,INSTALLSTATE_ABSENT))
            continue;

        for (i = 0; i < package->features[j].ComponentCount; i++)
        {
            if (package->features[j].Components[i] == index)
                count--;
        }
    }

    /* ref count all the files in the component */
    if (write)
        for (j = 0; j < package->loaded_files; j++)
        {
            if (package->files[j].Temporary)
                continue;
            if (package->files[j].ComponentIndex == index)
                ACTION_WriteSharedDLLsCount(package->files[j].TargetPath,count);
        }
    
    /* add a count for permenent */
    if (package->components[index].Attributes &
                                msidbComponentAttributesPermanent)
        count ++;
    
    package->components[index].RefCount = count;

    if (write)
        ACTION_WriteSharedDLLsCount(package->components[index].FullKeypath,
            package->components[index].RefCount);
}

/*
 * Ok further analysis makes me think that this work is
 * actually done in the PublishComponents and PublishFeatures
 * step, and not here.  It appears like the keypath and all that is
 * resolved in this step, however actually written in the Publish steps.
 * But we will leave it here for now because it is unclear
 */
static UINT ACTION_ProcessComponents(MSIPACKAGE *package)
{
    LPWSTR productcode;
    WCHAR squished_pc[GUID_SIZE];
    WCHAR squished_cc[GUID_SIZE];
    UINT rc;
    DWORD i;
    HKEY hkey=0,hkey2=0;

    if (!package)
        return ERROR_INVALID_HANDLE;

    /* writes the Component and Features values to the registry */
    productcode = load_dynamic_property(package,szProductCode,&rc);
    if (!productcode)
        return rc;

    rc = MSIREG_OpenComponents(&hkey);
    if (rc != ERROR_SUCCESS)
        goto end;
      
    squash_guid(productcode,squished_pc);
    ui_progress(package,1,COMPONENT_PROGRESS_VALUE,1,0);
    for (i = 0; i < package->loaded_components; i++)
    {
        ui_progress(package,2,0,0,0);
        if (package->components[i].ComponentId[0]!=0)
        {
            WCHAR *keypath = NULL;
            MSIRECORD * uirow;

            squash_guid(package->components[i].ComponentId,squished_cc);
           
            keypath = resolve_keypath(package,i);
            package->components[i].FullKeypath = keypath;

            /* do the refcounting */
            ACTION_RefCountComponent( package, i);

            TRACE("Component %s (%s), Keypath=%s, RefCount=%i\n", 
                            debugstr_w(package->components[i].Component),
                            debugstr_w(squished_cc),
                            debugstr_w(package->components[i].FullKeypath), 
                            package->components[i].RefCount);
            /*
            * Write the keypath out if the component is to be registered
            * and delete the key if the component is to be deregistered
            */
            if (ACTION_VerifyComponentForAction(package, i,
                                    INSTALLSTATE_LOCAL))
            {
                rc = RegCreateKeyW(hkey,squished_cc,&hkey2);
                if (rc != ERROR_SUCCESS)
                    continue;

                if (keypath)
                {
                    RegSetValueExW(hkey2,squished_pc,0,REG_SZ,(LPVOID)keypath,
                                (strlenW(keypath)+1)*sizeof(WCHAR));

                    if (package->components[i].Attributes & 
                                msidbComponentAttributesPermanent)
                    {
                        static const WCHAR szPermKey[] =
                            { '0','0','0','0','0','0','0','0','0','0','0','0',
                              '0','0','0','0','0','0','0', '0','0','0','0','0',
                              '0','0','0','0','0','0','0','0',0};

                        RegSetValueExW(hkey2,szPermKey,0,REG_SZ,
                                        (LPVOID)keypath,
                                        (strlenW(keypath)+1)*sizeof(WCHAR));
                    }
                    
                    RegCloseKey(hkey2);
        
                    /* UI stuff */
                    uirow = MSI_CreateRecord(3);
                    MSI_RecordSetStringW(uirow,1,productcode);
                    MSI_RecordSetStringW(uirow,2,package->components[i].
                                                            ComponentId);
                    MSI_RecordSetStringW(uirow,3,keypath);
                    ui_actiondata(package,szProcessComponents,uirow);
                    msiobj_release( &uirow->hdr );
               }
            }
            else if (ACTION_VerifyComponentForAction(package, i,
                                    INSTALLSTATE_ABSENT))
            {
                DWORD res;

                rc = RegOpenKeyW(hkey,squished_cc,&hkey2);
                if (rc != ERROR_SUCCESS)
                    continue;

                RegDeleteValueW(hkey2,squished_pc);

                /* if the key is empty delete it */
                res = RegEnumKeyExW(hkey2,0,NULL,0,0,NULL,0,NULL);
                RegCloseKey(hkey2);
                if (res == ERROR_NO_MORE_ITEMS)
                    RegDeleteKeyW(hkey,squished_cc);
        
                /* UI stuff */
                uirow = MSI_CreateRecord(2);
                MSI_RecordSetStringW(uirow,1,productcode);
                MSI_RecordSetStringW(uirow,2,package->components[i].
                                ComponentId);
                ui_actiondata(package,szProcessComponents,uirow);
                msiobj_release( &uirow->hdr );
            }
        }
    } 
end:
    HeapFree(GetProcessHeap(), 0, productcode);
    RegCloseKey(hkey);
    return rc;
}

typedef struct {
    CLSID       clsid;
    LPWSTR      source;

    LPWSTR      path;
    ITypeLib    *ptLib;
} typelib_struct;

BOOL CALLBACK Typelib_EnumResNameProc( HMODULE hModule, LPCWSTR lpszType, 
                                       LPWSTR lpszName, LONG_PTR lParam)
{
    TLIBATTR *attr;
    typelib_struct *tl_struct = (typelib_struct*) lParam;
    static const WCHAR fmt[] = {'%','s','\\','%','i',0};
    int sz; 
    HRESULT res;

    if (!IS_INTRESOURCE(lpszName))
    {
        ERR("Not Int Resource Name %s\n",debugstr_w(lpszName));
        return TRUE;
    }

    sz = strlenW(tl_struct->source)+4;
    sz *= sizeof(WCHAR);

    if ((INT)lpszName == 1)
        tl_struct->path = strdupW(tl_struct->source);
    else
    {
        tl_struct->path = HeapAlloc(GetProcessHeap(),0,sz);
        sprintfW(tl_struct->path,fmt,tl_struct->source, lpszName);
    }

    TRACE("trying %s\n", debugstr_w(tl_struct->path));
    res = LoadTypeLib(tl_struct->path,&tl_struct->ptLib);
    if (!SUCCEEDED(res))
    {
        HeapFree(GetProcessHeap(),0,tl_struct->path);
        tl_struct->path = NULL;

        return TRUE;
    }

    ITypeLib_GetLibAttr(tl_struct->ptLib, &attr);
    if (IsEqualGUID(&(tl_struct->clsid),&(attr->guid)))
    {
        ITypeLib_ReleaseTLibAttr(tl_struct->ptLib, attr);
        return FALSE;
    }

    HeapFree(GetProcessHeap(),0,tl_struct->path);
    tl_struct->path = NULL;

    ITypeLib_ReleaseTLibAttr(tl_struct->ptLib, attr);
    ITypeLib_Release(tl_struct->ptLib);

    return TRUE;
}

static UINT ACTION_RegisterTypeLibraries(MSIPACKAGE *package)
{
    /* 
     * OK this is a bit confusing.. I am given a _Component key and I believe
     * that the file that is being registered as a type library is the "key file
     * of that component" which I interpret to mean "The file in the KeyPath of
     * that component".
     */
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    static const WCHAR Query[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','T','y','p','e','L','i','b','`',0};

    if (!package)
        return ERROR_INVALID_HANDLE;

    rc = MSI_DatabaseOpenViewW(package->db, Query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }

    while (1)
    {   
        WCHAR component[0x100];
        DWORD sz;
        INT index;
        LPWSTR guid;
        typelib_struct tl_struct;
        HMODULE module;
        static const WCHAR szTYPELIB[] = {'T','Y','P','E','L','I','B',0};

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }

        sz = 0x100;
        MSI_RecordGetStringW(row,3,component,&sz);

        index = get_loaded_component(package,component);
        if (index < 0)
        {
            msiobj_release(&row->hdr);
            continue;
        }

        if (!ACTION_VerifyComponentForAction(package, index,
                                INSTALLSTATE_LOCAL))
        {
            TRACE("Skipping typelib reg due to disabled component\n");
            msiobj_release(&row->hdr);

            package->components[index].Action =
                package->components[index].Installed;

            continue;
        }

        package->components[index].Action = INSTALLSTATE_LOCAL;

        index = get_loaded_file(package,package->components[index].KeyPath); 
   
        if (index < 0)
        {
            msiobj_release(&row->hdr);
            continue;
        }

        guid = load_dynamic_stringW(row,1);
        module = LoadLibraryExW(package->files[index].TargetPath, NULL,
                        LOAD_LIBRARY_AS_DATAFILE);
        if (module != NULL)
        {
            CLSIDFromString(guid, &tl_struct.clsid);
            tl_struct.source = strdupW(package->files[index].TargetPath);
            tl_struct.path = NULL;

            EnumResourceNamesW(module, szTYPELIB, Typelib_EnumResNameProc, 
                               (LONG_PTR)&tl_struct);

            if (tl_struct.path != NULL)
            {
                LPWSTR help;
                WCHAR helpid[0x100];
                HRESULT res;

                sz = 0x100;
                MSI_RecordGetStringW(row,6,helpid,&sz);

                help = resolve_folder(package,helpid,FALSE,FALSE,NULL);
                res = RegisterTypeLib(tl_struct.ptLib,tl_struct.path,help);
                HeapFree(GetProcessHeap(),0,help);

                if (!SUCCEEDED(res))
                    ERR("Failed to register type library %s\n", 
                            debugstr_w(tl_struct.path));
                else
                {
                    ui_actiondata(package,szRegisterTypeLibraries,row);
                
                    TRACE("Registered %s\n", debugstr_w(tl_struct.path));
                }

                ITypeLib_Release(tl_struct.ptLib);
                HeapFree(GetProcessHeap(),0,tl_struct.path);
            }
            else
                ERR("Failed to load type library %s\n", 
                    debugstr_w(tl_struct.source));
       
            FreeLibrary(module);
            HeapFree(GetProcessHeap(),0,tl_struct.source);
        }
        else
            ERR("Could not load file! %s\n",
                    debugstr_w(package->files[index].TargetPath));
        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);
    return rc;
}

static INT load_appid(MSIPACKAGE* package, MSIRECORD *row)
{
    DWORD index = package->loaded_appids;
    DWORD sz;
    LPWSTR buffer;

    /* fill in the data */

    package->loaded_appids++;
    if (package->loaded_appids == 1)
        package->appids = HeapAlloc(GetProcessHeap(),0,sizeof(MSIAPPID));
    else
        package->appids = HeapReAlloc(GetProcessHeap(),0,
            package->appids, package->loaded_appids * sizeof(MSIAPPID));

    memset(&package->appids[index],0,sizeof(MSIAPPID));
    
    sz = IDENTIFIER_SIZE;
    MSI_RecordGetStringW(row, 1, package->appids[index].AppID, &sz);
    TRACE("loading appid %s\n",debugstr_w(package->appids[index].AppID));

    buffer = load_dynamic_stringW(row,2);
    deformat_string(package,buffer,&package->appids[index].RemoteServerName);
    HeapFree(GetProcessHeap(),0,buffer);

    package->appids[index].LocalServer = load_dynamic_stringW(row,3);
    package->appids[index].ServiceParameters = load_dynamic_stringW(row,4);
    package->appids[index].DllSurrogate = load_dynamic_stringW(row,5);

    package->appids[index].ActivateAtStorage = !MSI_RecordIsNull(row,6);
    package->appids[index].RunAsInteractiveUser = !MSI_RecordIsNull(row,7);
    
    return index;
}

static INT load_given_appid(MSIPACKAGE *package, LPCWSTR appid)
{
    INT rc;
    MSIRECORD *row;
    INT i;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','A','p','p','I','d','`',' ','W','H','E','R','E',' ',
         '`','A','p','p','I','d','`',' ','=',' ','\'','%','s','\'',0};

    if (!appid)
        return -1;

    /* check for appids already loaded */
    for (i = 0; i < package->loaded_appids; i++)
        if (strcmpiW(package->appids[i].AppID,appid)==0)
        {
            TRACE("found appid %s at index %i\n",debugstr_w(appid),i);
            return i;
        }
    
    row = MSI_QueryGetRecord(package->db, ExecSeqQuery, appid);
    if (!row)
        return -1;

    rc = load_appid(package, row);
    msiobj_release(&row->hdr);

    return rc;
}

static INT load_given_progid(MSIPACKAGE *package, LPCWSTR progid);
static INT load_given_class(MSIPACKAGE *package, LPCWSTR classid);

static INT load_progid(MSIPACKAGE* package, MSIRECORD *row)
{
    DWORD index = package->loaded_progids;
    LPWSTR buffer;

    /* fill in the data */

    package->loaded_progids++;
    if (package->loaded_progids == 1)
        package->progids = HeapAlloc(GetProcessHeap(),0,sizeof(MSIPROGID));
    else
        package->progids = HeapReAlloc(GetProcessHeap(),0,
            package->progids , package->loaded_progids * sizeof(MSIPROGID));

    memset(&package->progids[index],0,sizeof(MSIPROGID));

    package->progids[index].ProgID = load_dynamic_stringW(row,1);
    TRACE("loading progid %s\n",debugstr_w(package->progids[index].ProgID));

    buffer = load_dynamic_stringW(row,2);
    package->progids[index].ParentIndex = load_given_progid(package,buffer);
    if (package->progids[index].ParentIndex < 0 && buffer)
        FIXME("Unknown parent ProgID %s\n",debugstr_w(buffer));
    HeapFree(GetProcessHeap(),0,buffer);

    buffer = load_dynamic_stringW(row,3);
    package->progids[index].ClassIndex = load_given_class(package,buffer);
    if (package->progids[index].ClassIndex< 0 && buffer)
        FIXME("Unknown class %s\n",debugstr_w(buffer));
    HeapFree(GetProcessHeap(),0,buffer);

    package->progids[index].Description = load_dynamic_stringW(row,4);

    if (!MSI_RecordIsNull(row,6))
    {
        INT icon_index = MSI_RecordGetInteger(row,6); 
        LPWSTR FileName = load_dynamic_stringW(row,5);
        LPWSTR FilePath;
        static const WCHAR fmt[] = {'%','s',',','%','i',0};

        build_icon_path(package,FileName,&FilePath);
       
        package->progids[index].IconPath = 
                HeapAlloc(GetProcessHeap(),0,(strlenW(FilePath)+10)*
                                sizeof(WCHAR));

        sprintfW(package->progids[index].IconPath,fmt,FilePath,icon_index);

        HeapFree(GetProcessHeap(),0,FilePath);
        HeapFree(GetProcessHeap(),0,FileName);
    }
    else
    {
        buffer = load_dynamic_stringW(row,5);
        if (buffer)
            build_icon_path(package,buffer,&(package->progids[index].IconPath));
        HeapFree(GetProcessHeap(),0,buffer);
    }

    package->progids[index].CurVerIndex = -1;

    /* if we have a parent then we may be that parents CurVer */
    if (package->progids[index].ParentIndex >= 0)
    {
        int pindex = package->progids[index].ParentIndex;
        while (package->progids[pindex].ParentIndex>= 0)
            pindex = package->progids[pindex].ParentIndex;

        FIXME("BAD BAD need to determing if we are really the CurVer\n");

        package->progids[index].CurVerIndex = pindex;
    }
    
    return index;
}

static INT load_given_progid(MSIPACKAGE *package, LPCWSTR progid)
{
    INT rc;
    MSIRECORD *row;
    INT i;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','P','r','o','g','I','d','`',' ','W','H','E','R','E',' ',
         '`','P','r','o','g','I','d','`',' ','=',' ','\'','%','s','\'',0};

    if (!progid)
        return -1;

    /* check for progids already loaded */
    for (i = 0; i < package->loaded_progids; i++)
        if (strcmpiW(package->progids[i].ProgID,progid)==0)
        {
            TRACE("found progid %s at index %i\n",debugstr_w(progid), i);
            return i;
        }
    
    row = MSI_QueryGetRecord(package->db, ExecSeqQuery, progid);
    if(!row)
        return -1;

    rc = load_progid(package, row);
    msiobj_release(&row->hdr);

    return rc;
}

static INT load_class(MSIPACKAGE* package, MSIRECORD *row)
{
    DWORD index = package->loaded_classes;
    DWORD sz,i;
    LPWSTR buffer;

    /* fill in the data */

    package->loaded_classes++;
    if (package->loaded_classes== 1)
        package->classes = HeapAlloc(GetProcessHeap(),0,sizeof(MSICLASS));
    else
        package->classes = HeapReAlloc(GetProcessHeap(),0,
            package->classes, package->loaded_classes * sizeof(MSICLASS));

    memset(&package->classes[index],0,sizeof(MSICLASS));

    sz = IDENTIFIER_SIZE;
    MSI_RecordGetStringW(row, 1, package->classes[index].CLSID, &sz);
    TRACE("loading class %s\n",debugstr_w(package->classes[index].CLSID));
    sz = IDENTIFIER_SIZE;
    MSI_RecordGetStringW(row, 2, package->classes[index].Context, &sz);
    buffer = load_dynamic_stringW(row,3);
    package->classes[index].ComponentIndex = get_loaded_component(package, 
                    buffer);
    HeapFree(GetProcessHeap(),0,buffer);

    package->classes[index].ProgIDText = load_dynamic_stringW(row,4);
    package->classes[index].ProgIDIndex = 
                load_given_progid(package, package->classes[index].ProgIDText);

    package->classes[index].Description = load_dynamic_stringW(row,5);

    buffer = load_dynamic_stringW(row,6);
    if (buffer)
        package->classes[index].AppIDIndex = 
                load_given_appid(package, buffer);
    else
        package->classes[index].AppIDIndex = -1;
    HeapFree(GetProcessHeap(),0,buffer);

    package->classes[index].FileTypeMask = load_dynamic_stringW(row,7);

    if (!MSI_RecordIsNull(row,9))
    {

        INT icon_index = MSI_RecordGetInteger(row,9); 
        LPWSTR FileName = load_dynamic_stringW(row,8);
        LPWSTR FilePath;
        static const WCHAR fmt[] = {'%','s',',','%','i',0};

        build_icon_path(package,FileName,&FilePath);
       
        package->classes[index].IconPath = 
                HeapAlloc(GetProcessHeap(),0,(strlenW(FilePath)+5)*
                                sizeof(WCHAR));

        sprintfW(package->classes[index].IconPath,fmt,FilePath,icon_index);

        HeapFree(GetProcessHeap(),0,FilePath);
        HeapFree(GetProcessHeap(),0,FileName);
    }
    else
    {
        buffer = load_dynamic_stringW(row,8);
        if (buffer)
            build_icon_path(package,buffer,&(package->classes[index].IconPath));
        HeapFree(GetProcessHeap(),0,buffer);
    }

    if (!MSI_RecordIsNull(row,10))
    {
        i = MSI_RecordGetInteger(row,10);
        if (i != MSI_NULL_INTEGER && i > 0 &&  i < 4)
        {
            static const WCHAR ole2[] = {'o','l','e','2','.','d','l','l',0};
            static const WCHAR ole32[] = {'o','l','e','3','2','.','d','l','l',0};

            switch(i)
            {
                case 1:
                    package->classes[index].DefInprocHandler = strdupW(ole2);
                    break;
                case 2:
                    package->classes[index].DefInprocHandler32 = strdupW(ole32);
                    break;
                case 3:
                    package->classes[index].DefInprocHandler = strdupW(ole2);
                    package->classes[index].DefInprocHandler32 = strdupW(ole32);
                    break;
            }
        }
        else
        {
            package->classes[index].DefInprocHandler32 = load_dynamic_stringW(
                            row, 10);
            reduce_to_longfilename(package->classes[index].DefInprocHandler32);
        }
    }
    buffer = load_dynamic_stringW(row,11);
    deformat_string(package,buffer,&package->classes[index].Argument);
    HeapFree(GetProcessHeap(),0,buffer);

    buffer = load_dynamic_stringW(row,12);
    package->classes[index].FeatureIndex = get_loaded_feature(package,buffer);
    HeapFree(GetProcessHeap(),0,buffer);

    package->classes[index].Attributes = MSI_RecordGetInteger(row,13);
    
    return index;
}

/*
 * the Class table has 3 primary keys. Generally it is only 
 * referenced through the first CLSID key. However when loading
 * all of the classes we need to make sure we do not ignore rows
 * with other Context and ComponentIndexs 
 */
static INT load_given_class(MSIPACKAGE *package, LPCWSTR classid)
{
    INT rc;
    MSIRECORD *row;
    INT i;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','C','l','a','s','s','`',' ','W','H','E','R','E',' ',
         '`','C','L','S','I','D','`',' ','=',' ','\'','%','s','\'',0};


    if (!classid)
        return -1;
    
    /* check for classes already loaded */
    for (i = 0; i < package->loaded_classes; i++)
        if (strcmpiW(package->classes[i].CLSID,classid)==0)
        {
            TRACE("found class %s at index %i\n",debugstr_w(classid), i);
            return i;
        }
    
    row = MSI_QueryGetRecord(package->db, ExecSeqQuery, classid);
    if (!row)
        return -1;

    rc = load_class(package, row);
    msiobj_release(&row->hdr);

    return rc;
}

static INT load_given_extension(MSIPACKAGE *package, LPCWSTR extension);

static INT load_mime(MSIPACKAGE* package, MSIRECORD *row)
{
    DWORD index = package->loaded_mimes;
    DWORD sz;
    LPWSTR buffer;

    /* fill in the data */

    package->loaded_mimes++;
    if (package->loaded_mimes== 1)
        package->mimes= HeapAlloc(GetProcessHeap(),0,sizeof(MSIMIME));
    else
        package->mimes= HeapReAlloc(GetProcessHeap(),0,
            package->mimes, package->loaded_mimes* 
            sizeof(MSIMIME));

    memset(&package->mimes[index],0,sizeof(MSIMIME));

    package->mimes[index].ContentType = load_dynamic_stringW(row,1); 
    TRACE("loading mime %s\n",debugstr_w(package->mimes[index].ContentType));

    buffer = load_dynamic_stringW(row,2);
    package->mimes[index].ExtensionIndex = load_given_extension(package,
                    buffer);
    HeapFree(GetProcessHeap(),0,buffer);

    sz = IDENTIFIER_SIZE;
    MSI_RecordGetStringW(row,3,package->mimes[index].CLSID,&sz);
    package->mimes[index].ClassIndex= load_given_class(package,
                    package->mimes[index].CLSID);
    
    return index;
}

static INT load_given_mime(MSIPACKAGE *package, LPCWSTR mime)
{
    INT rc;
    MSIRECORD *row;
    INT i;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','M','I','M','E','`',' ','W','H','E','R','E',' ',
         '`','C','o','n','t','e','n','t','T','y','p','e','`',' ','=',' ',
         '\'','%','s','\'',0};

    if (!mime)
        return -1;
    
    /* check for mime already loaded */
    for (i = 0; i < package->loaded_mimes; i++)
        if (strcmpiW(package->mimes[i].ContentType,mime)==0)
        {
            TRACE("found mime %s at index %i\n",debugstr_w(mime), i);
            return i;
        }
    
    row = MSI_QueryGetRecord(package->db, ExecSeqQuery, mime);
    if (!row)
        return -1;

    rc = load_mime(package, row);
    msiobj_release(&row->hdr);

    return rc;
}

static INT load_extension(MSIPACKAGE* package, MSIRECORD *row)
{
    DWORD index = package->loaded_extensions;
    DWORD sz;
    LPWSTR buffer;

    /* fill in the data */

    package->loaded_extensions++;
    if (package->loaded_extensions == 1)
        package->extensions = HeapAlloc(GetProcessHeap(),0,sizeof(MSIEXTENSION));
    else
        package->extensions = HeapReAlloc(GetProcessHeap(),0,
            package->extensions, package->loaded_extensions* 
            sizeof(MSIEXTENSION));

    memset(&package->extensions[index],0,sizeof(MSIEXTENSION));

    sz = 256;
    MSI_RecordGetStringW(row,1,package->extensions[index].Extension,&sz);
    TRACE("loading extension %s\n",
                    debugstr_w(package->extensions[index].Extension));

    buffer = load_dynamic_stringW(row,2);
    package->extensions[index].ComponentIndex = 
            get_loaded_component(package,buffer);
    HeapFree(GetProcessHeap(),0,buffer);

    package->extensions[index].ProgIDText = load_dynamic_stringW(row,3);
    package->extensions[index].ProgIDIndex = load_given_progid(package,
                    package->extensions[index].ProgIDText);

    buffer = load_dynamic_stringW(row,4);
    package->extensions[index].MIMEIndex = load_given_mime(package,buffer);
    HeapFree(GetProcessHeap(),0,buffer);

    buffer = load_dynamic_stringW(row,5);
    package->extensions[index].FeatureIndex = 
            get_loaded_feature(package,buffer);
    HeapFree(GetProcessHeap(),0,buffer);

    return index;
}

/*
 * While the extension table has 2 primary keys, this function is only looking
 * at the Extension key which is what is referenced as a forign key 
 */
static INT load_given_extension(MSIPACKAGE *package, LPCWSTR extension)
{
    INT rc;
    MSIRECORD *row;
    INT i;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','E','x','t','e','n','s','i','o','n','`',' ',
         'W','H','E','R','E',' ',
         '`','E','x','t','e','n','s','i','o','n','`',' ','=',' ',
         '\'','%','s','\'',0};

    if (!extension)
        return -1;

    /* check for extensions already loaded */
    for (i = 0; i < package->loaded_extensions; i++)
        if (strcmpiW(package->extensions[i].Extension,extension)==0)
        {
            TRACE("extension %s already loaded at %i\n",debugstr_w(extension),
                            i);
            return i;
        }
    
    row = MSI_QueryGetRecord(package->db, ExecSeqQuery, extension);
    if (!row)
        return -1;

    rc = load_extension(package, row);
    msiobj_release(&row->hdr);

    return rc;
}

static UINT iterate_load_verb(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE* package = (MSIPACKAGE*)param;
    DWORD index = package->loaded_verbs;
    LPWSTR buffer;

    /* fill in the data */

    package->loaded_verbs++;
    if (package->loaded_verbs == 1)
        package->verbs = HeapAlloc(GetProcessHeap(),0,sizeof(MSIVERB));
    else
        package->verbs = HeapReAlloc(GetProcessHeap(),0,
            package->verbs , package->loaded_verbs * sizeof(MSIVERB));

    memset(&package->verbs[index],0,sizeof(MSIVERB));

    buffer = load_dynamic_stringW(row,1);
    package->verbs[index].ExtensionIndex = load_given_extension(package,buffer);
    if (package->verbs[index].ExtensionIndex < 0 && buffer)
        ERR("Verb unable to find loaded extension %s\n", debugstr_w(buffer));
    HeapFree(GetProcessHeap(),0,buffer);

    package->verbs[index].Verb = load_dynamic_stringW(row,2);
    TRACE("loading verb %s\n",debugstr_w(package->verbs[index].Verb));
    package->verbs[index].Sequence = MSI_RecordGetInteger(row,3);

    buffer = load_dynamic_stringW(row,4);
    deformat_string(package,buffer,&package->verbs[index].Command);
    HeapFree(GetProcessHeap(),0,buffer);

    buffer = load_dynamic_stringW(row,5);
    deformat_string(package,buffer,&package->verbs[index].Argument);
    HeapFree(GetProcessHeap(),0,buffer);

    /* assosiate the verb with the correct extension */
    if (package->verbs[index].ExtensionIndex >= 0)
    {
        MSIEXTENSION* extension = &package->extensions[package->verbs[index].
                ExtensionIndex];
        int count = extension->VerbCount;

        if (count >= 99)
            FIXME("Exceeding max verb count! Increase that limit!!!\n");
        else
        {
            extension->VerbCount++;
            extension->Verbs[count] = index;
        }
    }
    
    return ERROR_SUCCESS;
}

static UINT iterate_all_classes(MSIRECORD *rec, LPVOID param)
{
    LPWSTR clsid;
    LPWSTR context;
    LPWSTR buffer;
    INT    component_index;
    MSIPACKAGE* package =(MSIPACKAGE*)param;
    INT i;
    BOOL match = FALSE;

    clsid = load_dynamic_stringW(rec,1);
    context = load_dynamic_stringW(rec,2);
    buffer = load_dynamic_stringW(rec,3);
    component_index = get_loaded_component(package,buffer);

    for (i = 0; i < package->loaded_classes; i++)
    {
        if (strcmpiW(clsid,package->classes[i].CLSID))
            continue;
        if (strcmpW(context,package->classes[i].Context))
            continue;
        if (component_index == package->classes[i].ComponentIndex)
        {
            match = TRUE;
            break;
        }
    }
    
    HeapFree(GetProcessHeap(),0,buffer);
    HeapFree(GetProcessHeap(),0,clsid);
    HeapFree(GetProcessHeap(),0,context);

    if (!match)
        load_class(package, rec);

    return ERROR_SUCCESS;
}

static VOID load_all_classes(MSIPACKAGE *package)
{
    UINT rc = ERROR_SUCCESS;
    MSIQUERY *view;

    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','C','l','a','s','s','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return;

    rc = MSI_IterateRecords(view, NULL, iterate_all_classes, package);
    msiobj_release(&view->hdr);
}

static UINT iterate_all_extensions(MSIRECORD *rec, LPVOID param)
{
    LPWSTR buffer;
    LPWSTR extension;
    INT    component_index;
    MSIPACKAGE* package =(MSIPACKAGE*)param;
    BOOL match = FALSE;
    INT i;

    extension = load_dynamic_stringW(rec,1);
    buffer = load_dynamic_stringW(rec,2);
    component_index = get_loaded_component(package,buffer);

    for (i = 0; i < package->loaded_extensions; i++)
    {
        if (strcmpiW(extension,package->extensions[i].Extension))
            continue;
        if (component_index == package->extensions[i].ComponentIndex)
        {
            match = TRUE;
            break;
        }
    }

    HeapFree(GetProcessHeap(),0,buffer);
    HeapFree(GetProcessHeap(),0,extension);

    if (!match)
        load_extension(package, rec);

    return ERROR_SUCCESS;
}

static VOID load_all_extensions(MSIPACKAGE *package)
{
    UINT rc = ERROR_SUCCESS;
    MSIQUERY *view;

    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','E','x','t','e','n','s','i','o','n','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return;

    rc = MSI_IterateRecords(view, NULL, iterate_all_extensions, package);
    msiobj_release(&view->hdr);
}

static UINT iterate_all_progids(MSIRECORD *rec, LPVOID param)
{
    LPWSTR buffer;
    MSIPACKAGE* package =(MSIPACKAGE*)param;

    buffer = load_dynamic_stringW(rec,1);
    load_given_progid(package,buffer);
    HeapFree(GetProcessHeap(),0,buffer);
    return ERROR_SUCCESS;
}

static VOID load_all_progids(MSIPACKAGE *package)
{
    UINT rc = ERROR_SUCCESS;
    MSIQUERY *view;

    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','`','P','r','o','g','I','d','`',' ',
         'F','R','O','M',' ', '`','P','r','o','g','I','d','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return;

    rc = MSI_IterateRecords(view, NULL, iterate_all_progids, package);
    msiobj_release(&view->hdr);
}

static VOID load_all_verbs(MSIPACKAGE *package)
{
    UINT rc = ERROR_SUCCESS;
    MSIQUERY *view;

    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','V','e','r','b','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return;

    rc = MSI_IterateRecords(view, NULL, iterate_load_verb, package);
    msiobj_release(&view->hdr);
}

static UINT iterate_all_mimes(MSIRECORD *rec, LPVOID param)
{
    LPWSTR buffer;
    MSIPACKAGE* package =(MSIPACKAGE*)param;

    buffer = load_dynamic_stringW(rec,1);
    load_given_mime(package,buffer);
    HeapFree(GetProcessHeap(),0,buffer);
    return ERROR_SUCCESS;
}

static VOID load_all_mimes(MSIPACKAGE *package)
{
    UINT rc = ERROR_SUCCESS;
    MSIQUERY *view;

    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ',
         '`','C','o','n','t','e','n','t','T','y','p','e','`',
         ' ','F','R','O','M',' ',
         '`','M','I','M','E','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return;

    rc = MSI_IterateRecords(view, NULL, iterate_all_mimes, package);
    msiobj_release(&view->hdr);
}

static void load_classes_and_such(MSIPACKAGE *package)
{
    TRACE("Loading all the class info and related tables\n");

    /* check if already loaded */
    if (package->classes || package->extensions || package->progids || 
        package->verbs || package->mimes)
        return;

    load_all_classes(package);
    load_all_extensions(package);
    load_all_progids(package);
    /* these loads must come after the other loads */
    load_all_verbs(package);
    load_all_mimes(package);
}

static void mark_progid_for_install(MSIPACKAGE* package, INT index)
{
    MSIPROGID* progid;
    int i;

    if (index < 0 || index >= package->loaded_progids)
        return;

    progid = &package->progids[index];

    if (progid->InstallMe == TRUE)
        return;

    progid->InstallMe = TRUE;

    /* all children if this is a parent also install */
   for (i = 0; i < package->loaded_progids; i++)
        if (package->progids[i].ParentIndex == index)
            mark_progid_for_install(package,i);
}

static void mark_mime_for_install(MSIPACKAGE* package, INT index)
{
    MSIMIME* mime;

    if (index < 0 || index >= package->loaded_mimes)
        return;

    mime = &package->mimes[index];

    if (mime->InstallMe == TRUE)
        return;

    mime->InstallMe = TRUE;
}

static UINT register_appid(MSIPACKAGE *package, int appidIndex, LPCWSTR app )
{
    static const WCHAR szAppID[] = { 'A','p','p','I','D',0 };
    HKEY hkey2,hkey3;

    if (!package)
        return ERROR_INVALID_HANDLE;

    RegCreateKeyW(HKEY_CLASSES_ROOT,szAppID,&hkey2);
    RegCreateKeyW(hkey2,package->appids[appidIndex].AppID,&hkey3);
    RegSetValueExW(hkey3,NULL,0,REG_SZ,(LPVOID)app,
                   (strlenW(app)+1)*sizeof(WCHAR));

    if (package->appids[appidIndex].RemoteServerName)
    {
        UINT size; 
        static const WCHAR szRemoteServerName[] =
             {'R','e','m','o','t','e','S','e','r','v','e','r','N','a','m','e',
              0};

        size = (strlenW(package->appids[appidIndex].RemoteServerName)+1) * 
                sizeof(WCHAR);

        RegSetValueExW(hkey3,szRemoteServerName,0,REG_SZ,
                        (LPVOID)package->appids[appidIndex].RemoteServerName,
                        size);
    }

    if (package->appids[appidIndex].LocalServer)
    {
        static const WCHAR szLocalService[] =
             {'L','o','c','a','l','S','e','r','v','i','c','e',0};
        UINT size;
        size = (strlenW(package->appids[appidIndex].LocalServer)+1) * 
                sizeof(WCHAR);

        RegSetValueExW(hkey3,szLocalService,0,REG_SZ,
                        (LPVOID)package->appids[appidIndex].LocalServer,size);
    }

    if (package->appids[appidIndex].ServiceParameters)
    {
        static const WCHAR szService[] =
             {'S','e','r','v','i','c','e',
              'P','a','r','a','m','e','t','e','r','s',0};
        UINT size;
        size = (strlenW(package->appids[appidIndex].ServiceParameters)+1) * 
                sizeof(WCHAR);
        RegSetValueExW(hkey3,szService,0,REG_SZ,
                        (LPVOID)package->appids[appidIndex].ServiceParameters,
                        size);
    }

    if (package->appids[appidIndex].DllSurrogate)
    {
        static const WCHAR szDLL[] =
             {'D','l','l','S','u','r','r','o','g','a','t','e',0};
        UINT size;
        size = (strlenW(package->appids[appidIndex].DllSurrogate)+1) * 
                sizeof(WCHAR);
        RegSetValueExW(hkey3,szDLL,0,REG_SZ,
                        (LPVOID)package->appids[appidIndex].DllSurrogate,size);
    }

    if (package->appids[appidIndex].ActivateAtStorage)
    {
        static const WCHAR szActivate[] =
             {'A','c','t','i','v','a','t','e','A','s',
              'S','t','o','r','a','g','e',0};
        static const WCHAR szY[] = {'Y',0};

        RegSetValueExW(hkey3,szActivate,0,REG_SZ,(LPVOID)szY,4);
    }

    if (package->appids[appidIndex].RunAsInteractiveUser)
    {
        static const WCHAR szRunAs[] = {'R','u','n','A','s',0};
        static const WCHAR szUser[] = 
             {'I','n','t','e','r','a','c','t','i','v','e',' ',
              'U','s','e','r',0};

        RegSetValueExW(hkey3,szRunAs,0,REG_SZ,(LPVOID)szUser,sizeof(szUser));
    }

    RegCloseKey(hkey3);
    RegCloseKey(hkey2);
    return ERROR_SUCCESS;
}

static UINT ACTION_RegisterClassInfo(MSIPACKAGE *package)
{
    /* 
     * Again I am assuming the words, "Whose key file represents" when referring
     * to a Component as to meaning that Components KeyPath file
     */
    
    UINT rc;
    MSIRECORD *uirow;
    static const WCHAR szCLSID[] = { 'C','L','S','I','D',0 };
    static const WCHAR szProgID[] = { 'P','r','o','g','I','D',0 };
    static const WCHAR szVIProgID[] = { 'V','e','r','s','i','o','n','I','n','d','e','p','e','n','d','e','n','t','P','r','o','g','I','D',0 };
    static const WCHAR szAppID[] = { 'A','p','p','I','D',0 };
    static const WCHAR szSpace[] = {' ',0};
    static const WCHAR szInprocServer32[] = {'I','n','p','r','o','c','S','e','r','v','e','r','3','2',0};
    static const WCHAR szFileType_fmt[] = {'F','i','l','e','T','y','p','e','\\','%','s','\\','%','i',0};
    HKEY hkey,hkey2,hkey3;
    BOOL install_on_demand = FALSE;
    int i;

    if (!package)
        return ERROR_INVALID_HANDLE;

    load_classes_and_such(package);
    rc = RegCreateKeyW(HKEY_CLASSES_ROOT,szCLSID,&hkey);
    if (rc != ERROR_SUCCESS)
        return ERROR_FUNCTION_FAILED;

    /* install_on_demand should be set if OLE supports install on demand OLE
     * servers. For now i am defaulting to FALSE because i do not know how to
     * check, and i am told our builtin OLE does not support it
     */
    
    for (i = 0; i < package->loaded_classes; i++)
    {
        INT index,f_index;
        DWORD size, sz;
        LPWSTR argument;

        if (package->classes[i].ComponentIndex < 0)
        {
            continue;
        }

        index = package->classes[i].ComponentIndex;
        f_index = package->classes[i].FeatureIndex;

        /* 
         * yes. MSDN says that these are based on _Feature_ not on
         * Component.  So verify the feature is to be installed
         */
        if ((!ACTION_VerifyFeatureForAction(package, f_index,
                                INSTALLSTATE_LOCAL)) &&
             !(install_on_demand && ACTION_VerifyFeatureForAction(package,
                             f_index, INSTALLSTATE_ADVERTISED)))
        {
            TRACE("Skipping class %s reg due to disabled feature %s\n", 
                            debugstr_w(package->classes[i].CLSID), 
                            debugstr_w(package->features[f_index].Feature));

            continue;
        }

        TRACE("Registering index %i  class %s\n",i,
                        debugstr_w(package->classes[i].CLSID));

        package->classes[i].Installed = TRUE;
        if (package->classes[i].ProgIDIndex >= 0)
            mark_progid_for_install(package, package->classes[i].ProgIDIndex);

        RegCreateKeyW(hkey,package->classes[i].CLSID,&hkey2);

        if (package->classes[i].Description)
            RegSetValueExW(hkey2,NULL,0,REG_SZ,(LPVOID)package->classes[i].
                            Description, (strlenW(package->classes[i].
                                     Description)+1)*sizeof(WCHAR));

        RegCreateKeyW(hkey2,package->classes[i].Context,&hkey3);
        index = get_loaded_file(package,package->components[index].KeyPath);


        /* the context server is a short path name 
         * except for if it is InprocServer32... 
         */
        if (strcmpiW(package->classes[i].Context,szInprocServer32)!=0)
        {
            sz = 0;
            sz = GetShortPathNameW(package->files[index].TargetPath, NULL, 0);
            if (sz == 0)
            {
                ERR("Unable to find short path for CLSID COM Server\n");
                argument = NULL;
            }
            else
            {
                size = sz * sizeof(WCHAR);

                if (package->classes[i].Argument)
                {
                    size += strlenW(package->classes[i].Argument) * 
                            sizeof(WCHAR);
                    size += sizeof(WCHAR);
                }

                argument = HeapAlloc(GetProcessHeap(), 0, size + sizeof(WCHAR));
                GetShortPathNameW(package->files[index].TargetPath, argument, 
                                sz);

                if (package->classes[i].Argument)
                {
                    strcatW(argument,szSpace);
                    strcatW(argument,package->classes[i].Argument);
                }
            }
        }
        else
        {
            size = lstrlenW(package->files[index].TargetPath) * sizeof(WCHAR);

            if (package->classes[i].Argument)
            {
                size += strlenW(package->classes[i].Argument) * sizeof(WCHAR);
                size += sizeof(WCHAR);
            }

            argument = HeapAlloc(GetProcessHeap(), 0, size + sizeof(WCHAR));
            strcpyW(argument, package->files[index].TargetPath);

            if (package->classes[i].Argument)
            {
                strcatW(argument,szSpace);
                strcatW(argument,package->classes[i].Argument);
            }
        }

        if (argument)
        {
            RegSetValueExW(hkey3,NULL,0,REG_SZ, (LPVOID)argument, size);
            HeapFree(GetProcessHeap(),0,argument);
        }

        RegCloseKey(hkey3);

        if (package->classes[i].ProgIDIndex >= 0 || 
            package->classes[i].ProgIDText)
        {
            LPCWSTR progid;

            if (package->classes[i].ProgIDIndex >= 0)
                progid = package->progids[
                        package->classes[i].ProgIDIndex].ProgID;
            else
                progid = package->classes[i].ProgIDText;

            RegCreateKeyW(hkey2,szProgID,&hkey3);
            RegSetValueExW(hkey3,NULL,0,REG_SZ,(LPVOID)progid,
                            (strlenW(progid)+1) *sizeof(WCHAR));
            RegCloseKey(hkey3);

            if (package->classes[i].ProgIDIndex >= 0 &&
                package->progids[package->classes[i].ProgIDIndex].ParentIndex 
                            >= 0)
            {
                progid = package->progids[package->progids[
                        package->classes[i].ProgIDIndex].ParentIndex].ProgID;

                RegCreateKeyW(hkey2,szVIProgID,&hkey3);
                RegSetValueExW(hkey3,NULL,0,REG_SZ,(LPVOID)progid,
                            (strlenW(progid)+1) *sizeof(WCHAR));
                RegCloseKey(hkey3);
            }
        }

        if (package->classes[i].AppIDIndex >= 0)
        { 
            RegSetValueExW(hkey2,szAppID,0,REG_SZ,
             (LPVOID)package->appids[package->classes[i].AppIDIndex].AppID,
             (strlenW(package->appids[package->classes[i].AppIDIndex].AppID)+1)
             *sizeof(WCHAR));

            register_appid(package,package->classes[i].AppIDIndex,
                            package->classes[i].Description);
        }

        if (package->classes[i].IconPath)
        {
            static const WCHAR szDefaultIcon[] = 
                {'D','e','f','a','u','l','t','I','c','o','n',0};

            RegCreateKeyW(hkey2,szDefaultIcon,&hkey3);

            RegSetValueExW(hkey3,NULL,0,REG_SZ,
                           (LPVOID)package->classes[i].IconPath,
                           (strlenW(package->classes[i].IconPath)+1) * 
                           sizeof(WCHAR));

            RegCloseKey(hkey3);
        }

        if (package->classes[i].DefInprocHandler)
        {
            static const WCHAR szInproc[] =
                {'I','n','p','r','o','c','H','a','n','d','l','e','r',0};

            size = (strlenW(package->classes[i].DefInprocHandler) + 1) * 
                    sizeof(WCHAR);
            RegCreateKeyW(hkey2,szInproc,&hkey3);
            RegSetValueExW(hkey3,NULL,0,REG_SZ, 
                            (LPVOID)package->classes[i].DefInprocHandler, size);
            RegCloseKey(hkey3);
        }

        if (package->classes[i].DefInprocHandler32)
        {
            static const WCHAR szInproc32[] =
                {'I','n','p','r','o','c','H','a','n','d','l','e','r','3','2',
                 0};
            size = (strlenW(package->classes[i].DefInprocHandler32) + 1) * 
                    sizeof(WCHAR);

            RegCreateKeyW(hkey2,szInproc32,&hkey3);
            RegSetValueExW(hkey3,NULL,0,REG_SZ, 
                           (LPVOID)package->classes[i].DefInprocHandler32,size);
            RegCloseKey(hkey3);
        }
        
        RegCloseKey(hkey2);

        /* if there is a FileTypeMask, register the FileType */
        if (package->classes[i].FileTypeMask)
        {
            LPWSTR ptr, ptr2;
            LPWSTR keyname;
            INT index = 0;
            ptr = package->classes[i].FileTypeMask;
            while (ptr && *ptr)
            {
                ptr2 = strchrW(ptr,';');
                if (ptr2)
                    *ptr2 = 0;
                keyname = HeapAlloc(GetProcessHeap(),0,(strlenW(szFileType_fmt)+
                                        strlenW(package->classes[i].CLSID) + 4)
                                * sizeof(WCHAR));
                sprintfW(keyname,szFileType_fmt, package->classes[i].CLSID, 
                        index);

                RegCreateKeyW(HKEY_CLASSES_ROOT,keyname,&hkey2);
                RegSetValueExW(hkey2,NULL,0,REG_SZ, (LPVOID)ptr,
                        strlenW(ptr)*sizeof(WCHAR));
                RegCloseKey(hkey2);
                HeapFree(GetProcessHeap(), 0, keyname);

                if (ptr2)
                    ptr = ptr2+1;
                else
                    ptr = NULL;

                index ++;
            }
        }
        
        uirow = MSI_CreateRecord(1);

        MSI_RecordSetStringW(uirow,1,package->classes[i].CLSID);
        ui_actiondata(package,szRegisterClassInfo,uirow);
        msiobj_release(&uirow->hdr);
    }

    RegCloseKey(hkey);
    return rc;
}

static UINT register_progid_base(MSIPACKAGE* package, MSIPROGID* progid,
                                 LPWSTR clsid)
{
    static const WCHAR szCLSID[] = { 'C','L','S','I','D',0 };
    static const WCHAR szDefaultIcon[] =
        {'D','e','f','a','u','l','t','I','c','o','n',0};
    HKEY hkey,hkey2;

    RegCreateKeyW(HKEY_CLASSES_ROOT,progid->ProgID,&hkey);

    if (progid->Description)
    {
        RegSetValueExW(hkey,NULL,0,REG_SZ,
                        (LPVOID)progid->Description, 
                        (strlenW(progid->Description)+1) *
                       sizeof(WCHAR));
    }

    if (progid->ClassIndex >= 0)
    {   
        RegCreateKeyW(hkey,szCLSID,&hkey2);
        RegSetValueExW(hkey2,NULL,0,REG_SZ,
                        (LPVOID)package->classes[progid->ClassIndex].CLSID, 
                        (strlenW(package->classes[progid->ClassIndex].CLSID)+1)
                        * sizeof(WCHAR));

        if (clsid)
            strcpyW(clsid,package->classes[progid->ClassIndex].CLSID);

        RegCloseKey(hkey2);
    }
    else
    {
        FIXME("UNHANDLED case, Parent progid but classid is NULL\n");
    }

    if (progid->IconPath)
    {
        RegCreateKeyW(hkey,szDefaultIcon,&hkey2);

        RegSetValueExW(hkey2,NULL,0,REG_SZ,(LPVOID)progid->IconPath,
                           (strlenW(progid->IconPath)+1) * sizeof(WCHAR));
        RegCloseKey(hkey2);
    }
    return ERROR_SUCCESS;
}

static UINT register_progid(MSIPACKAGE *package, MSIPROGID* progid, 
                LPWSTR clsid)
{
    UINT rc = ERROR_SUCCESS; 

    if (progid->ParentIndex < 0)
        rc = register_progid_base(package, progid, clsid);
    else
    {
        DWORD disp;
        HKEY hkey,hkey2;
        static const WCHAR szCLSID[] = { 'C','L','S','I','D',0 };
        static const WCHAR szDefaultIcon[] =
            {'D','e','f','a','u','l','t','I','c','o','n',0};
        static const WCHAR szCurVer[] =
            {'C','u','r','V','e','r',0};

        /* check if already registered */
        RegCreateKeyExW(HKEY_CLASSES_ROOT, progid->ProgID, 0, NULL, 0,
                        KEY_ALL_ACCESS, NULL, &hkey, &disp );
        if (disp == REG_OPENED_EXISTING_KEY)
        {
            TRACE("Key already registered\n");
            RegCloseKey(hkey);
            return rc;
        }

        TRACE("Registering Parent %s index %i\n",
                    debugstr_w(package->progids[progid->ParentIndex].ProgID), 
                    progid->ParentIndex);
        rc = register_progid(package,&package->progids[progid->ParentIndex],
                        clsid);

        /* clsid is same as parent */
        RegCreateKeyW(hkey,szCLSID,&hkey2);
        RegSetValueExW(hkey2,NULL,0,REG_SZ,(LPVOID)clsid, (strlenW(clsid)+1) *
                       sizeof(WCHAR));

        RegCloseKey(hkey2);


        if (progid->Description)
        {
            RegSetValueExW(hkey,NULL,0,REG_SZ,(LPVOID)progid->Description,
                           (strlenW(progid->Description)+1) * sizeof(WCHAR));
        }

        if (progid->IconPath)
        {
            RegCreateKeyW(hkey,szDefaultIcon,&hkey2);
            RegSetValueExW(hkey2,NULL,0,REG_SZ,(LPVOID)progid->IconPath,
                           (strlenW(progid->IconPath)+1) * sizeof(WCHAR));
            RegCloseKey(hkey2);
        }

        /* write out the current version */
        if (progid->CurVerIndex >= 0)
        {
            RegCreateKeyW(hkey,szCurVer,&hkey2);
            RegSetValueExW(hkey2,NULL,0,REG_SZ,
                (LPVOID)package->progids[progid->CurVerIndex].ProgID,
                (strlenW(package->progids[progid->CurVerIndex].ProgID)+1) * 
                sizeof(WCHAR));
            RegCloseKey(hkey2);
        }

        RegCloseKey(hkey);
    }
    return rc;
}

static UINT ACTION_RegisterProgIdInfo(MSIPACKAGE *package)
{
    INT i;
    MSIRECORD *uirow;

    if (!package)
        return ERROR_INVALID_HANDLE;

    load_classes_and_such(package);

    for (i = 0; i < package->loaded_progids; i++)
    {
        WCHAR clsid[0x1000];

        /* check if this progid is to be installed */

        if (!package->progids[i].InstallMe)
        {
            TRACE("progid %s not scheduled to be installed\n",
                             debugstr_w(package->progids[i].ProgID));
            continue;
        }
       
        TRACE("Registering progid %s index %i\n",
                        debugstr_w(package->progids[i].ProgID), i);

        register_progid(package,&package->progids[i],clsid);

        uirow = MSI_CreateRecord(1);
        MSI_RecordSetStringW(uirow,1,package->progids[i].ProgID);
        ui_actiondata(package,szRegisterProgIdInfo,uirow);
        msiobj_release(&uirow->hdr);
    }

    return ERROR_SUCCESS;
}

static UINT build_icon_path(MSIPACKAGE *package, LPCWSTR icon_name, 
                            LPWSTR *FilePath)
{
    LPWSTR ProductCode;
    LPWSTR SystemFolder;
    LPWSTR dest;
    UINT rc;

    static const WCHAR szInstaller[] = 
        {'M','i','c','r','o','s','o','f','t','\\',
         'I','n','s','t','a','l','l','e','r','\\',0};
    static const WCHAR szFolder[] =
        {'A','p','p','D','a','t','a','F','o','l','d','e','r',0};

    ProductCode = load_dynamic_property(package,szProductCode,&rc);
    if (!ProductCode)
        return rc;

    SystemFolder = load_dynamic_property(package,szFolder,NULL);

    dest = build_directory_name(3, SystemFolder, szInstaller, ProductCode);

    create_full_pathW(dest);

    *FilePath = build_directory_name(2, dest, icon_name);

    HeapFree(GetProcessHeap(),0,SystemFolder);
    HeapFree(GetProcessHeap(),0,ProductCode);
    HeapFree(GetProcessHeap(),0,dest);
    return ERROR_SUCCESS;
}

static UINT ACTION_CreateShortcuts(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    static const WCHAR Query[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','S','h','o','r','t','c','u','t','`',0};
    IShellLinkW *sl;
    IPersistFile *pf;
    HRESULT res;

    if (!package)
        return ERROR_INVALID_HANDLE;

    res = CoInitialize( NULL );
    if (FAILED (res))
    {
        ERR("CoInitialize failed\n");
        return ERROR_FUNCTION_FAILED;
    }

    rc = MSI_DatabaseOpenViewW(package->db, Query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }

    while (1)
    {
        LPWSTR target_file, target_folder;
        WCHAR buffer[0x100];
        DWORD sz;
        DWORD index;
        static const WCHAR szlnk[]={'.','l','n','k',0};

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }
        
        sz = 0x100;
        MSI_RecordGetStringW(row,4,buffer,&sz);

        index = get_loaded_component(package,buffer);

        if (index < 0)
        {
            msiobj_release(&row->hdr);
            continue;
        }

        if (!ACTION_VerifyComponentForAction(package, index,
                                INSTALLSTATE_LOCAL))
        {
            TRACE("Skipping shortcut creation due to disabled component\n");
            msiobj_release(&row->hdr);

            package->components[index].Action =
                package->components[index].Installed;

            continue;
        }

        package->components[index].Action = INSTALLSTATE_LOCAL;

        ui_actiondata(package,szCreateShortcuts,row);

        res = CoCreateInstance( &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                              &IID_IShellLinkW, (LPVOID *) &sl );

        if (FAILED(res))
        {
            ERR("Is IID_IShellLink\n");
            msiobj_release(&row->hdr);
            continue;
        }

        res = IShellLinkW_QueryInterface( sl, &IID_IPersistFile,(LPVOID*) &pf );
        if( FAILED( res ) )
        {
            ERR("Is IID_IPersistFile\n");
            msiobj_release(&row->hdr);
            continue;
        }

        sz = 0x100;
        MSI_RecordGetStringW(row,2,buffer,&sz);
        target_folder = resolve_folder(package, buffer,FALSE,FALSE,NULL);

        /* may be needed because of a bug somehwere else */
        create_full_pathW(target_folder);

        sz = 0x100;
        MSI_RecordGetStringW(row,3,buffer,&sz);
        reduce_to_longfilename(buffer);
        if (!strchrW(buffer,'.') || strcmpiW(strchrW(buffer,'.'),szlnk))
            strcatW(buffer,szlnk);
        target_file = build_directory_name(2, target_folder, buffer);
        HeapFree(GetProcessHeap(),0,target_folder);

        sz = 0x100;
        MSI_RecordGetStringW(row,5,buffer,&sz);
        if (strchrW(buffer,'['))
        {
            LPWSTR deformated;
            deformat_string(package,buffer,&deformated);
            IShellLinkW_SetPath(sl,deformated);
            HeapFree(GetProcessHeap(),0,deformated);
        }
        else
        {
            LPWSTR keypath;
            FIXME("poorly handled shortcut format, advertised shortcut\n");
            keypath = strdupW(package->components[index].FullKeypath);
            IShellLinkW_SetPath(sl,keypath);
            HeapFree(GetProcessHeap(),0,keypath);
        }

        if (!MSI_RecordIsNull(row,6))
        {
            LPWSTR deformated;
            sz = 0x100;
            MSI_RecordGetStringW(row,6,buffer,&sz);
            deformat_string(package,buffer,&deformated);
            IShellLinkW_SetArguments(sl,deformated);
            HeapFree(GetProcessHeap(),0,deformated);
        }

        if (!MSI_RecordIsNull(row,7))
        {
            LPWSTR deformated;
            deformated = load_dynamic_stringW(row,7);
            IShellLinkW_SetDescription(sl,deformated);
            HeapFree(GetProcessHeap(),0,deformated);
        }

        if (!MSI_RecordIsNull(row,8))
            IShellLinkW_SetHotkey(sl,MSI_RecordGetInteger(row,8));

        if (!MSI_RecordIsNull(row,9))
        {
            WCHAR *Path = NULL;
            INT index; 

            sz = 0x100;
            MSI_RecordGetStringW(row,9,buffer,&sz);

            build_icon_path(package,buffer,&Path);
            index = MSI_RecordGetInteger(row,10);

            IShellLinkW_SetIconLocation(sl,Path,index);
            HeapFree(GetProcessHeap(),0,Path);
        }

        if (!MSI_RecordIsNull(row,11))
            IShellLinkW_SetShowCmd(sl,MSI_RecordGetInteger(row,11));

        if (!MSI_RecordIsNull(row,12))
        {
            LPWSTR Path;
            sz = 0x100;
            MSI_RecordGetStringW(row,12,buffer,&sz);
            Path = resolve_folder(package, buffer, FALSE, FALSE, NULL);
            IShellLinkW_SetWorkingDirectory(sl,Path);
            HeapFree(GetProcessHeap(), 0, Path);
        }

        TRACE("Writing shortcut to %s\n",debugstr_w(target_file));
        IPersistFile_Save(pf,target_file,FALSE);
    
        HeapFree(GetProcessHeap(),0,target_file);    

        IPersistFile_Release( pf );
        IShellLinkW_Release( sl );

        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);


    CoUninitialize();

    return rc;
}


/*
 * 99% of the work done here is only done for 
 * advertised installs. However this is where the
 * Icon table is processed and written out
 * so that is what I am going to do here.
 */
static UINT ACTION_PublishProduct(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    static const WCHAR Query[]=
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','c','o','n','`',0};
    DWORD sz;
    /* for registry stuff */
    LPWSTR productcode;
    HKEY hkey=0;
    HKEY hukey=0;
    static const WCHAR szProductName[] =
        {'P','r','o','d','u','c','t','N','a','m','e',0};
    static const WCHAR szPackageCode[] =
        {'P','a','c','k','a','g','e','C','o','d','e',0};
    LPWSTR buffer;
    DWORD size;
    MSIHANDLE hDb, hSumInfo;

    if (!package)
        return ERROR_INVALID_HANDLE;

    rc = MSI_DatabaseOpenViewW(package->db, Query, &view);
    if (rc != ERROR_SUCCESS)
        goto next;

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        goto next;
    }

    while (1)
    {
        HANDLE the_file;
        WCHAR *FilePath=NULL;
        WCHAR *FileName=NULL;
        CHAR buffer[1024];

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }
    
        FileName = load_dynamic_stringW(row,1);
        if (!FileName)
        {
            ERR("Unable to get FileName\n");
            msiobj_release(&row->hdr);
            continue;
        }

        build_icon_path(package,FileName,&FilePath);

        HeapFree(GetProcessHeap(),0,FileName);

        TRACE("Creating icon file at %s\n",debugstr_w(FilePath));
        
        the_file = CreateFileW(FilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);

        if (the_file == INVALID_HANDLE_VALUE)
        {
            ERR("Unable to create file %s\n",debugstr_w(FilePath));
            msiobj_release(&row->hdr);
            HeapFree(GetProcessHeap(),0,FilePath);
            continue;
        }

        do 
        {
            DWORD write;
            sz = 1024;
            rc = MSI_RecordReadStream(row,2,buffer,&sz);
            if (rc != ERROR_SUCCESS)
            {
                ERR("Failed to get stream\n");
                CloseHandle(the_file);  
                DeleteFileW(FilePath);
                break;
            }
            WriteFile(the_file,buffer,sz,&write,NULL);
        } while (sz == 1024);

        HeapFree(GetProcessHeap(),0,FilePath);

        CloseHandle(the_file);
        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);

next:
    /* ok there is a lot more done here but i need to figure out what */
    productcode = load_dynamic_property(package,szProductCode,&rc);
    if (!productcode)
        return rc;

    rc = MSIREG_OpenProductsKey(productcode,&hkey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    rc = MSIREG_OpenUserProductsKey(productcode,&hukey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;


    buffer = load_dynamic_property(package,szProductName,NULL);
    size = strlenW(buffer)*sizeof(WCHAR);
    RegSetValueExW(hukey,szProductName,0,REG_SZ, (LPSTR)buffer,size);
    HeapFree(GetProcessHeap(),0,buffer);
    FIXME("Need to write more keys to the user registry\n");
  
    hDb= alloc_msihandle( &package->db->hdr );
    rc = MsiGetSummaryInformationW(hDb, NULL, 0, &hSumInfo); 
    MsiCloseHandle(hDb);
    if (rc == ERROR_SUCCESS)
    {
        WCHAR guidbuffer[0x200];
        size = 0x200;
        rc = MsiSummaryInfoGetPropertyW(hSumInfo, 9, NULL, NULL, NULL,
                                        guidbuffer, &size);
        if (rc == ERROR_SUCCESS)
        {
            WCHAR squashed[GUID_SIZE];
            /* for now we only care about the first guid */
            LPWSTR ptr = strchrW(guidbuffer,';');
            if (ptr) *ptr = 0;
            squash_guid(guidbuffer,squashed);
            size = strlenW(squashed)*sizeof(WCHAR);
            RegSetValueExW(hukey,szPackageCode,0,REG_SZ, (LPSTR)squashed,
                           size);
        }
        else
        {
            ERR("Unable to query Revision_Number... \n");
            rc = ERROR_SUCCESS;
        }
        MsiCloseHandle(hSumInfo);
    }
    else
    {
        ERR("Unable to open Summary Information\n");
        rc = ERROR_SUCCESS;
    }

end:

    HeapFree(GetProcessHeap(),0,productcode);    
    RegCloseKey(hkey);
    RegCloseKey(hukey);

    return rc;
}

static UINT ACTION_WriteIniValues(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    static const WCHAR ExecSeqQuery[] = 
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','n','i','F','i','l','e','`',0};
    static const WCHAR szWindowsFolder[] =
          {'W','i','n','d','o','w','s','F','o','l','d','e','r',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
    {
        TRACE("no IniFile table\n");
        return ERROR_SUCCESS;
    }

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }

    while (1)
    {
        LPWSTR component,filename,dirproperty,section,key,value,identifier;
        LPWSTR deformated_section, deformated_key, deformated_value;
        LPWSTR folder, fullname = NULL;
        MSIRECORD * uirow;
        INT component_index,action;

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }

        component = load_dynamic_stringW(row, 8);
        component_index = get_loaded_component(package,component);
        HeapFree(GetProcessHeap(),0,component);

        if (!ACTION_VerifyComponentForAction(package, component_index,
                                INSTALLSTATE_LOCAL))
        {
            TRACE("Skipping ini file due to disabled component\n");
            msiobj_release(&row->hdr);

            package->components[component_index].Action =
                package->components[component_index].Installed;

            continue;
        }

        package->components[component_index].Action = INSTALLSTATE_LOCAL;
   
        identifier = load_dynamic_stringW(row,1); 
        filename = load_dynamic_stringW(row,2);
        dirproperty = load_dynamic_stringW(row,3);
        section = load_dynamic_stringW(row,4);
        key = load_dynamic_stringW(row,5);
        value = load_dynamic_stringW(row,6);
        action = MSI_RecordGetInteger(row,7);

        deformat_string(package,section,&deformated_section);
        deformat_string(package,key,&deformated_key);
        deformat_string(package,value,&deformated_value);

        if (dirproperty)
        {
            folder = resolve_folder(package, dirproperty, FALSE, FALSE, NULL);
            if (!folder)
                folder = load_dynamic_property(package,dirproperty,NULL);
        }
        else
            folder = load_dynamic_property(package, szWindowsFolder, NULL);

        if (!folder)
        {
            ERR("Unable to resolve folder! (%s)\n",debugstr_w(dirproperty));
            goto cleanup;
        }

        fullname = build_directory_name(3, folder, filename, NULL);

        if (action == 0)
        {
            TRACE("Adding value %s to section %s in %s\n",
                debugstr_w(deformated_key), debugstr_w(deformated_section),
                debugstr_w(fullname));
            WritePrivateProfileStringW(deformated_section, deformated_key,
                                       deformated_value, fullname);
        }
        else if (action == 1)
        {
            WCHAR returned[10];
            GetPrivateProfileStringW(deformated_section, deformated_key, NULL,
                                     returned, 10, fullname);
            if (returned[0] == 0)
            {
                TRACE("Adding value %s to section %s in %s\n",
                    debugstr_w(deformated_key), debugstr_w(deformated_section),
                    debugstr_w(fullname));

                WritePrivateProfileStringW(deformated_section, deformated_key,
                                       deformated_value, fullname);
            }
        }
        else if (action == 3)
        {
            FIXME("Append to existing section not yet implemented\n");
        }
        
        uirow = MSI_CreateRecord(4);
        MSI_RecordSetStringW(uirow,1,identifier);
        MSI_RecordSetStringW(uirow,2,deformated_section);
        MSI_RecordSetStringW(uirow,3,deformated_key);
        MSI_RecordSetStringW(uirow,4,deformated_value);
        ui_actiondata(package,szWriteIniValues,uirow);
        msiobj_release( &uirow->hdr );
cleanup:
        HeapFree(GetProcessHeap(),0,identifier);
        HeapFree(GetProcessHeap(),0,fullname);
        HeapFree(GetProcessHeap(),0,filename);
        HeapFree(GetProcessHeap(),0,key);
        HeapFree(GetProcessHeap(),0,value);
        HeapFree(GetProcessHeap(),0,section);
        HeapFree(GetProcessHeap(),0,dirproperty);
        HeapFree(GetProcessHeap(),0,folder);
        HeapFree(GetProcessHeap(),0,deformated_key);
        HeapFree(GetProcessHeap(),0,deformated_value);
        HeapFree(GetProcessHeap(),0,deformated_section);
        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);
    return rc;
}

static UINT ACTION_SelfRegModules(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    static const WCHAR ExecSeqQuery[] = 
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','S','e','l','f','R','e','g','`',0};

    static const WCHAR ExeStr[] =
        {'r','e','g','s','v','r','3','2','.','e','x','e',' ','\"',0};
    static const WCHAR close[] =  {'\"',0};
    STARTUPINFOW si;
    PROCESS_INFORMATION info;
    BOOL brc;

    memset(&si,0,sizeof(STARTUPINFOW));

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
    {
        TRACE("no SelfReg table\n");
        return ERROR_SUCCESS;
    }

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        return rc;
    }

    while (1)
    {
        LPWSTR filename;
        INT index;
        DWORD len;

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }

        filename = load_dynamic_stringW(row,1);
        index = get_loaded_file(package,filename);

        if (index < 0)
        {
            ERR("Unable to find file id %s\n",debugstr_w(filename));
            HeapFree(GetProcessHeap(),0,filename);
            msiobj_release(&row->hdr);
            continue;
        }
        HeapFree(GetProcessHeap(),0,filename);

        len = strlenW(ExeStr);
        len += strlenW(package->files[index].TargetPath);
        len +=2;

        filename = HeapAlloc(GetProcessHeap(),0,len*sizeof(WCHAR));
        strcpyW(filename,ExeStr);
        strcatW(filename,package->files[index].TargetPath);
        strcatW(filename,close);

        TRACE("Registering %s\n",debugstr_w(filename));
        brc = CreateProcessW(NULL, filename, NULL, NULL, FALSE, 0, NULL,
                  c_colon, &si, &info);

        if (brc)
            msi_dialog_check_messages(info.hProcess);
 
        HeapFree(GetProcessHeap(),0,filename);
        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);
    return rc;
}

static UINT ACTION_PublishFeatures(MSIPACKAGE *package)
{
    LPWSTR productcode;
    UINT rc;
    DWORD i;
    HKEY hkey=0;
    HKEY hukey=0;
    
    if (!package)
        return ERROR_INVALID_HANDLE;

    productcode = load_dynamic_property(package,szProductCode,&rc);
    if (!productcode)
        return rc;

    rc = MSIREG_OpenFeaturesKey(productcode,&hkey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    rc = MSIREG_OpenUserFeaturesKey(productcode,&hukey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    /* here the guids are base 85 encoded */
    for (i = 0; i < package->loaded_features; i++)
    {
        LPWSTR data = NULL;
        GUID clsid;
        int j;
        INT size;
        BOOL absent = FALSE;

        if (!ACTION_VerifyFeatureForAction(package,i,INSTALLSTATE_LOCAL) &&
            !ACTION_VerifyFeatureForAction(package,i,INSTALLSTATE_SOURCE) &&
            !ACTION_VerifyFeatureForAction(package,i,INSTALLSTATE_ADVERTISED))
            absent = TRUE;

        size = package->features[i].ComponentCount*21;
        size +=1;
        if (package->features[i].Feature_Parent[0])
            size += strlenW(package->features[i].Feature_Parent)+2;

        data = HeapAlloc(GetProcessHeap(), 0, size * sizeof(WCHAR));

        data[0] = 0;
        for (j = 0; j < package->features[i].ComponentCount; j++)
        {
            WCHAR buf[21];
            memset(buf,0,sizeof(buf));
            if (package->components
                [package->features[i].Components[j]].ComponentId[0]!=0)
            {
                TRACE("From %s\n",debugstr_w(package->components
                            [package->features[i].Components[j]].ComponentId));
                CLSIDFromString(package->components
                            [package->features[i].Components[j]].ComponentId,
                            &clsid);
                encode_base85_guid(&clsid,buf);
                TRACE("to %s\n",debugstr_w(buf));
                strcatW(data,buf);
            }
        }
        if (package->features[i].Feature_Parent[0])
        {
            static const WCHAR sep[] = {'\2',0};
            strcatW(data,sep);
            strcatW(data,package->features[i].Feature_Parent);
        }

        size = (strlenW(data)+1)*sizeof(WCHAR);
        RegSetValueExW(hkey,package->features[i].Feature,0,REG_SZ,
                       (LPSTR)data,size);
        HeapFree(GetProcessHeap(),0,data);

        if (!absent)
        {
            size = strlenW(package->features[i].Feature_Parent)*sizeof(WCHAR);
            RegSetValueExW(hukey,package->features[i].Feature,0,REG_SZ,
                       (LPSTR)package->features[i].Feature_Parent,size);
        }
        else
        {
            size = (strlenW(package->features[i].Feature_Parent)+2)*
                    sizeof(WCHAR);
            data = HeapAlloc(GetProcessHeap(),0,size);
            data[0] = 0x6;
            strcpyW(&data[1],package->features[i].Feature_Parent);
            RegSetValueExW(hukey,package->features[i].Feature,0,REG_SZ,
                       (LPSTR)data,size);
            HeapFree(GetProcessHeap(),0,data);
        }
    }

end:
    RegCloseKey(hkey);
    RegCloseKey(hukey);
    HeapFree(GetProcessHeap(), 0, productcode);
    return rc;
}

static UINT ACTION_RegisterProduct(MSIPACKAGE *package)
{
    HKEY hkey=0;
    LPWSTR buffer;
    LPWSTR productcode;
    UINT rc,i;
    DWORD size;
    static WCHAR szNONE[] = {0};
    static const WCHAR szWindowsInstaler[] = 
    {'W','i','n','d','o','w','s','I','n','s','t','a','l','l','e','r',0};
    static const WCHAR szPropKeys[][80] = 
    {
{'A','R','P','A','U','T','H','O','R','I','Z','E','D','C','D','F','P','R','E','F','I','X',0},
{'A','R','P','C','O','N','T','A','C','T',0},
{'A','R','P','C','O','M','M','E','N','T','S',0},
{'P','r','o','d','u','c','t','N','a','m','e',0},
{'P','r','o','d','u','c','t','V','e','r','s','i','o','n',0},
{'A','R','P','H','E','L','P','L','I','N','K',0},
{'A','R','P','H','E','L','P','T','E','L','E','P','H','O','N','E',0},
{'A','R','P','I','N','S','T','A','L','L','L','O','C','A','T','I','O','N',0},
{'S','o','u','r','c','e','D','i','r',0},
{'M','a','n','u','f','a','c','t','u','r','e','r',0},
{'A','R','P','R','E','A','D','M','E',0},
{'A','R','P','S','I','Z','E',0},
{'A','R','P','U','R','L','I','N','F','O','A','B','O','U','T',0},
{'A','R','P','U','R','L','U','P','D','A','T','E','I','N','F','O',0},
{0},
    };

    static const WCHAR szRegKeys[][80] = 
    {
{'A','u','t','h','o','r','i','z','e','d','C','D','F','P','r','e','f','i','x',0},
{'C','o','n','t','a','c','t',0},
{'C','o','m','m','e','n','t','s',0},
{'D','i','s','p','l','a','y','N','a','m','e',0},
{'D','i','s','p','l','a','y','V','e','r','s','i','o','n',0},
{'H','e','l','p','L','i','n','k',0},
{'H','e','l','p','T','e','l','e','p','h','o','n','e',0},
{'I','n','s','t','a','l','l','L','o','c','a','t','i','o','n',0},
{'I','n','s','t','a','l','l','S','o','u','r','c','e',0},
{'P','u','b','l','i','s','h','e','r',0},
{'R','e','a','d','m','e',0},
{'S','i','z','e',0},
{'U','R','L','I','n','f','o','A','b','o','u','t',0},
{'U','R','L','U','p','d','a','t','e','I','n','f','o',0},
{0},
    };

    static const WCHAR installerPathFmt[] = {
    '%','s','\\',
    'I','n','s','t','a','l','l','e','r','\\',0};
    static const WCHAR fmt[] = {
    '%','s','\\',
    'I','n','s','t','a','l','l','e','r','\\',
    '%','x','.','m','s','i',0};
    static const WCHAR szLocalPackage[]=
         {'L','o','c','a','l','P','a','c','k','a','g','e',0};
    WCHAR windir[MAX_PATH], path[MAX_PATH], packagefile[MAX_PATH];
    INT num,start;

    if (!package)
        return ERROR_INVALID_HANDLE;

    productcode = load_dynamic_property(package,szProductCode,&rc);
    if (!productcode)
        return rc;

    rc = MSIREG_OpenUninstallKey(productcode,&hkey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    /* dump all the info i can grab */
    FIXME("Flesh out more information \n");

    i = 0;
    while (szPropKeys[i][0]!=0)
    {
        buffer = load_dynamic_property(package,szPropKeys[i],&rc);
        if (rc != ERROR_SUCCESS)
            buffer = szNONE;
        size = strlenW(buffer)*sizeof(WCHAR);
        RegSetValueExW(hkey,szRegKeys[i],0,REG_SZ,(LPSTR)buffer,size);
        i++;
    }

    rc = 0x1;
    size = sizeof(rc);
    RegSetValueExW(hkey,szWindowsInstaler,0,REG_DWORD,(LPSTR)&rc,size);
    
    /* copy the package locally */
    num = GetTickCount() & 0xffff;
    if (!num) 
        num = 1;
    start = num;
    GetWindowsDirectoryW(windir, sizeof(windir) / sizeof(windir[0]));
    snprintfW(packagefile,sizeof(packagefile)/sizeof(packagefile[0]),fmt,
     windir,num);
    do 
    {
        HANDLE handle = CreateFileW(packagefile,GENERIC_WRITE, 0, NULL,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0 );
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            break;
        }
        if (GetLastError() != ERROR_FILE_EXISTS &&
            GetLastError() != ERROR_SHARING_VIOLATION)
            break;
        if (!(++num & 0xffff)) num = 1;
        sprintfW(packagefile,fmt,num);
    } while (num != start);

    snprintfW(path,sizeof(path)/sizeof(path[0]),installerPathFmt,windir);
    create_full_pathW(path);
    TRACE("Copying to local package %s\n",debugstr_w(packagefile));
    if (!CopyFileW(package->PackagePath,packagefile,FALSE))
        ERR("Unable to copy package (%s -> %s) (error %ld)\n",
            debugstr_w(package->PackagePath), debugstr_w(packagefile),
            GetLastError());
    size = strlenW(packagefile)*sizeof(WCHAR);
    RegSetValueExW(hkey,szLocalPackage,0,REG_SZ,(LPSTR)packagefile,size);
    
end:
    HeapFree(GetProcessHeap(),0,productcode);
    RegCloseKey(hkey);

    return ERROR_SUCCESS;
}

static UINT ACTION_InstallExecute(MSIPACKAGE *package)
{
    UINT rc;

    if (!package)
        return ERROR_INVALID_HANDLE;

    rc = execute_script(package,INSTALL_SCRIPT);

    return rc;
}

static UINT ACTION_InstallFinalize(MSIPACKAGE *package)
{
    UINT rc;

    if (!package)
        return ERROR_INVALID_HANDLE;

    /* turn off scheduleing */
    package->script->CurrentlyScripting= FALSE;

    /* first do the same as an InstallExecute */
    rc = ACTION_InstallExecute(package);
    if (rc != ERROR_SUCCESS)
        return rc;

    /* then handle Commit Actions */
    rc = execute_script(package,COMMIT_SCRIPT);

    return rc;
}

static UINT ACTION_ForceReboot(MSIPACKAGE *package)
{
    static const WCHAR RunOnce[] = {
    'S','o','f','t','w','a','r','e','\\',
    'M','i','c','r','o','s','o','f','t','\\',
    'W','i','n','d','o','w','s','\\',
    'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
    'R','u','n','O','n','c','e',0};
    static const WCHAR InstallRunOnce[] = {
    'S','o','f','t','w','a','r','e','\\',
    'M','i','c','r','o','s','o','f','t','\\',
    'W','i','n','d','o','w','s','\\',
    'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
    'I','n','s','t','a','l','l','e','r','\\',
    'R','u','n','O','n','c','e','E','n','t','r','i','e','s',0};

    static const WCHAR msiexec_fmt[] = {
    '%','s',
    '\\','M','s','i','E','x','e','c','.','e','x','e',' ','/','@',' ',
    '\"','%','s','\"',0};
    static const WCHAR install_fmt[] = {
    '/','I',' ','\"','%','s','\"',' ',
    'A','F','T','E','R','R','E','B','O','O','T','=','1',' ',
    'R','U','N','O','N','C','E','E','N','T','R','Y','=','\"','%','s','\"',0};
    WCHAR buffer[256], sysdir[MAX_PATH];
    HKEY hkey,hukey;
    LPWSTR productcode;
    WCHAR  squished_pc[100];
    INT rc;
    DWORD size;
    static const WCHAR szLUS[] = {
         'L','a','s','t','U','s','e','d','S','o','u','r','c','e',0};
    static const WCHAR szSourceList[] = {
         'S','o','u','r','c','e','L','i','s','t',0};
    static const WCHAR szPackageName[] = { 
        'P','a','c','k','a','g','e','N','a','m','e',0};

    if (!package)
        return ERROR_INVALID_HANDLE;

    productcode = load_dynamic_property(package,szProductCode,&rc);
    if (!productcode)
        return rc;

    squash_guid(productcode,squished_pc);

    GetSystemDirectoryW(sysdir, sizeof(sysdir)/sizeof(sysdir[0]));
    RegCreateKeyW(HKEY_LOCAL_MACHINE,RunOnce,&hkey);
    snprintfW(buffer,sizeof(buffer)/sizeof(buffer[0]),msiexec_fmt,sysdir,
     squished_pc);

    size = strlenW(buffer)*sizeof(WCHAR);
    RegSetValueExW(hkey,squished_pc,0,REG_SZ,(LPSTR)buffer,size);
    RegCloseKey(hkey);

    TRACE("Reboot command %s\n",debugstr_w(buffer));

    RegCreateKeyW(HKEY_LOCAL_MACHINE,InstallRunOnce,&hkey);
    sprintfW(buffer,install_fmt,productcode,squished_pc);

    size = strlenW(buffer)*sizeof(WCHAR);
    RegSetValueExW(hkey,squished_pc,0,REG_SZ,(LPSTR)buffer,size);
    RegCloseKey(hkey);

    rc = MSIREG_OpenUserProductsKey(productcode,&hukey,TRUE);
    if (rc == ERROR_SUCCESS)
    {
        HKEY hukey2;
        LPWSTR buf;
        RegCreateKeyW(hukey, szSourceList, &hukey2);
        buf = load_dynamic_property(package,cszSourceDir,NULL);
        size = strlenW(buf)*sizeof(WCHAR);
        RegSetValueExW(hukey2,szLUS,0,REG_SZ,(LPSTR)buf,size);
        HeapFree(GetProcessHeap(),0,buf); 

        buf = strrchrW(package->PackagePath,'\\');
        if (buf)
        {
            buf++;
            size = strlenW(buf)*sizeof(WCHAR);
            RegSetValueExW(hukey2,szPackageName,0,REG_SZ,(LPSTR)buf,size);
        }

        RegCloseKey(hukey2);
    }
    HeapFree(GetProcessHeap(),0,productcode);

    return ERROR_INSTALL_SUSPEND;
}

UINT ACTION_ResolveSource(MSIPACKAGE* package)
{
    /*
     * we are currently doing what should be done here in the top level Install
     * however for Adminastrative and uninstalls this step will be needed
     */
    return ERROR_SUCCESS;
}

static LPWSTR create_component_advertise_string(MSIPACKAGE* package, 
                MSICOMPONENT* component, LPCWSTR feature)
{
    LPWSTR productid=NULL;
    GUID clsid;
    WCHAR productid_85[21];
    WCHAR component_85[21];
    /*
     * I have a fair bit of confusion as to when a < is used and when a > is
     * used. I do not think i have it right...
     *
     * Ok it appears that the > is used if there is a guid for the compoenent
     * and the < is used if not.
     */
    static WCHAR fmt1[] = {'%','s','%','s','<',0,0};
    static WCHAR fmt2[] = {'%','s','%','s','>','%','s',0,0};
    LPWSTR output = NULL;
    DWORD sz = 0;

    memset(productid_85,0,sizeof(productid_85));
    memset(component_85,0,sizeof(component_85));

    productid = load_dynamic_property(package,szProductCode,NULL);
    CLSIDFromString(productid, &clsid);
    
    encode_base85_guid(&clsid,productid_85);

    CLSIDFromString(component->ComponentId, &clsid);
    encode_base85_guid(&clsid,component_85);

    TRACE("Doing something with this... %s %s %s\n", 
            debugstr_w(productid_85), debugstr_w(feature),
            debugstr_w(component_85));
 
    sz = lstrlenW(productid_85) + lstrlenW(feature);
    if (component)
        sz += lstrlenW(component_85);

    sz+=3;
    sz *= sizeof(WCHAR);
           
    output = HeapAlloc(GetProcessHeap(),0,sz);
    memset(output,0,sz);

    if (component)
        sprintfW(output,fmt2,productid_85,feature,component_85);
    else
        sprintfW(output,fmt1,productid_85,feature);

    HeapFree(GetProcessHeap(),0,productid);
    
    return output;
}

static UINT register_verb(MSIPACKAGE *package, LPCWSTR progid, 
                MSICOMPONENT* component, MSIEXTENSION* extension,
                MSIVERB* verb, INT* Sequence )
{
    LPWSTR keyname;
    HKEY key;
    static const WCHAR szShell[] = {'s','h','e','l','l',0};
    static const WCHAR szCommand[] = {'c','o','m','m','a','n','d',0};
    static const WCHAR fmt[] = {'\"','%','s','\"',' ','%','s',0};
    static const WCHAR fmt2[] = {'\"','%','s','\"',0};
    LPWSTR command;
    DWORD size;
    LPWSTR advertise;

    keyname = build_directory_name(4, progid, szShell, verb->Verb, szCommand);

    TRACE("Making Key %s\n",debugstr_w(keyname));
    RegCreateKeyW(HKEY_CLASSES_ROOT, keyname, &key);
    size = strlenW(component->FullKeypath);
    if (verb->Argument)
        size += strlenW(verb->Argument);
     size += 4;

     command = HeapAlloc(GetProcessHeap(),0, size * sizeof (WCHAR));
     if (verb->Argument)
        sprintfW(command, fmt, component->FullKeypath, verb->Argument);
     else
        sprintfW(command, fmt2, component->FullKeypath);

     RegSetValueExW(key,NULL,0,REG_SZ, (LPVOID)command, (strlenW(command)+1)*
                     sizeof(WCHAR));
     HeapFree(GetProcessHeap(),0,command);

     advertise = create_component_advertise_string(package, component, 
                        package->features[extension->FeatureIndex].Feature);

     size = strlenW(advertise);

     if (verb->Argument)
        size += strlenW(verb->Argument);
     size += 4;

     command = HeapAlloc(GetProcessHeap(),0, size * sizeof (WCHAR));
     memset(command,0,size*sizeof(WCHAR));

     strcpyW(command,advertise);
     if (verb->Argument)
     {
        static const WCHAR szSpace[] = {' ',0};
         strcatW(command,szSpace);
         strcatW(command,verb->Argument);
     }

     RegSetValueExW(key, szCommand, 0, REG_MULTI_SZ, (LPBYTE)command,
                        (strlenW(command)+2)*sizeof(WCHAR));
     
     RegCloseKey(key);
     HeapFree(GetProcessHeap(),0,keyname);
     HeapFree(GetProcessHeap(),0,advertise);
     HeapFree(GetProcessHeap(),0,command);

     if (verb->Command)
     {
        keyname = build_directory_name(3, progid, szShell, verb->Verb);
        RegCreateKeyW(HKEY_CLASSES_ROOT, keyname, &key);
        RegSetValueExW(key,NULL,0,REG_SZ, (LPVOID)verb->Command,
                                    (strlenW(verb->Command)+1) *sizeof(WCHAR));
        RegCloseKey(key);
        HeapFree(GetProcessHeap(),0,keyname);
     }

     if (verb->Sequence != MSI_NULL_INTEGER)
     {
        if (*Sequence == MSI_NULL_INTEGER || verb->Sequence < *Sequence)
        {
            *Sequence = verb->Sequence;
            keyname = build_directory_name(2, progid, szShell);
            RegCreateKeyW(HKEY_CLASSES_ROOT, keyname, &key);
            RegSetValueExW(key,NULL,0,REG_SZ, (LPVOID)verb->Verb,
                            (strlenW(verb->Verb)+1) *sizeof(WCHAR));
            RegCloseKey(key);
            HeapFree(GetProcessHeap(),0,keyname);
        }
    }
    return ERROR_SUCCESS;
}

static UINT ACTION_RegisterExtensionInfo(MSIPACKAGE *package)
{
    static const WCHAR szContentType[] = 
        {'C','o','n','t','e','n','t',' ','T','y','p','e',0 };
    HKEY hkey;
    INT i;
    MSIRECORD *uirow;

    if (!package)
        return ERROR_INVALID_HANDLE;

    load_classes_and_such(package);

    for (i = 0; i < package->loaded_extensions; i++)
    {
        WCHAR extension[257];
        INT index,f_index;
     
        index = package->extensions[i].ComponentIndex;
        f_index = package->extensions[i].FeatureIndex;

        if (index < 0)
            continue;

        /* 
         * yes. MSDN says that these are based on _Feature_ not on
         * Component.  So verify the feature is to be installed
         */
        if ((!ACTION_VerifyFeatureForAction(package, f_index,
                                INSTALLSTATE_LOCAL)) &&
            (!ACTION_VerifyFeatureForAction(package, f_index,
                                INSTALLSTATE_ADVERTISED)))
        {
            TRACE("Skipping extension  %s reg due to disabled feature %s\n",
                            debugstr_w(package->extensions[i].Extension),
                            debugstr_w(package->features[f_index].Feature));

            continue;
        }

        TRACE("Registering extension %s index %i\n",
                        debugstr_w(package->extensions[i].Extension), i);

        package->extensions[i].Installed = TRUE;

        if (package->extensions[i].ProgIDIndex >= 0)
           mark_progid_for_install(package, package->extensions[i].ProgIDIndex);

        if (package->extensions[i].MIMEIndex >= 0)
           mark_mime_for_install(package, package->extensions[i].MIMEIndex);

        extension[0] = '.';
        extension[1] = 0;
        strcatW(extension,package->extensions[i].Extension);

        RegCreateKeyW(HKEY_CLASSES_ROOT,extension,&hkey);

        if (package->extensions[i].MIMEIndex >= 0)
        {
            RegSetValueExW(hkey,szContentType,0,REG_SZ,
                            (LPVOID)package->mimes[package->extensions[i].
                                MIMEIndex].ContentType,
                           (strlenW(package->mimes[package->extensions[i].
                                    MIMEIndex].ContentType)+1)*sizeof(WCHAR));
        }

        if (package->extensions[i].ProgIDIndex >= 0 || 
            package->extensions[i].ProgIDText)
        {
            static const WCHAR szSN[] = 
                {'\\','S','h','e','l','l','N','e','w',0};
            HKEY hkey2;
            LPWSTR newkey;
            LPCWSTR progid;
            INT v;
            INT Sequence = MSI_NULL_INTEGER;
            
            if (package->extensions[i].ProgIDIndex >= 0)
                progid = package->progids[package->extensions[i].
                    ProgIDIndex].ProgID;
            else
                progid = package->extensions[i].ProgIDText;

            RegSetValueExW(hkey,NULL,0,REG_SZ,(LPVOID)progid,
                           (strlenW(progid)+1)*sizeof(WCHAR));

            newkey = HeapAlloc(GetProcessHeap(),0,
                           (strlenW(progid)+strlenW(szSN)+1) * sizeof(WCHAR)); 

            strcpyW(newkey,progid);
            strcatW(newkey,szSN);
            RegCreateKeyW(hkey,newkey,&hkey2);
            RegCloseKey(hkey2);

            HeapFree(GetProcessHeap(),0,newkey);

            /* do all the verbs */
            for (v = 0; v < package->extensions[i].VerbCount; v++)
                register_verb(package, progid, 
                              &package->components[index],
                              &package->extensions[i],
                              &package->verbs[package->extensions[i].Verbs[v]], 
                              &Sequence);
        }
        
        RegCloseKey(hkey);

        uirow = MSI_CreateRecord(1);
        MSI_RecordSetStringW(uirow,1,package->extensions[i].Extension);
        ui_actiondata(package,szRegisterExtensionInfo,uirow);
        msiobj_release(&uirow->hdr);
    }

    return ERROR_SUCCESS;
}

static UINT ACTION_RegisterMIMEInfo(MSIPACKAGE *package)
{
    static const WCHAR szExten[] = 
        {'E','x','t','e','n','s','i','o','n',0 };
    HKEY hkey;
    INT i;
    MSIRECORD *uirow;

    if (!package)
        return ERROR_INVALID_HANDLE;

    load_classes_and_such(package);

    for (i = 0; i < package->loaded_mimes; i++)
    {
        WCHAR extension[257];
        LPCWSTR exten;
        LPCWSTR mime;
        static const WCHAR fmt[] = 
            {'M','I','M','E','\\','D','a','t','a','b','a','s','e','\\',
             'C','o','n','t','e','n','t',' ','T','y','p','e','\\', '%','s',0};
        LPWSTR key;

        /* 
         * check if the MIME is to be installed. Either as requesed by an
         * extension or Class
         */
        package->mimes[i].InstallMe =  ((package->mimes[i].InstallMe) ||
              (package->mimes[i].ClassIndex >= 0 &&
              package->classes[package->mimes[i].ClassIndex].Installed) ||
              (package->mimes[i].ExtensionIndex >=0 &&
              package->extensions[package->mimes[i].ExtensionIndex].Installed));

        if (!package->mimes[i].InstallMe)
        {
            TRACE("MIME %s not scheduled to be installed\n",
                             debugstr_w(package->mimes[i].ContentType));
            continue;
        }
        
        mime = package->mimes[i].ContentType;
        exten = package->extensions[package->mimes[i].ExtensionIndex].Extension;
        extension[0] = '.';
        extension[1] = 0;
        strcatW(extension,exten);

        key = HeapAlloc(GetProcessHeap(),0,(strlenW(mime)+strlenW(fmt)+1) *
                                            sizeof(WCHAR));
        sprintfW(key,fmt,mime);
        RegCreateKeyW(HKEY_CLASSES_ROOT,key,&hkey);
        RegSetValueExW(hkey,szExten,0,REG_SZ,(LPVOID)extension,
                           (strlenW(extension)+1)*sizeof(WCHAR));

        HeapFree(GetProcessHeap(),0,key);

        if (package->mimes[i].CLSID[0])
        {
            FIXME("Handle non null for field 3\n");
        }

        RegCloseKey(hkey);

        uirow = MSI_CreateRecord(2);
        MSI_RecordSetStringW(uirow,1,package->mimes[i].ContentType);
        MSI_RecordSetStringW(uirow,2,exten);
        ui_actiondata(package,szRegisterMIMEInfo,uirow);
        msiobj_release(&uirow->hdr);
    }

    return ERROR_SUCCESS;
}

static UINT ACTION_RegisterUser(MSIPACKAGE *package)
{
    static const WCHAR szProductID[]=
         {'P','r','o','d','u','c','t','I','D',0};
    HKEY hkey=0;
    LPWSTR buffer;
    LPWSTR productcode;
    LPWSTR productid;
    UINT rc,i;
    DWORD size;

    static const WCHAR szPropKeys[][80] = 
    {
        {'P','r','o','d','u','c','t','I','D',0},
        {'U','S','E','R','N','A','M','E',0},
        {'C','O','M','P','A','N','Y','N','A','M','E',0},
        {0},
    };

    static const WCHAR szRegKeys[][80] = 
    {
        {'P','r','o','d','u','c','t','I','D',0},
        {'R','e','g','O','w','n','e','r',0},
        {'R','e','g','C','o','m','p','a','n','y',0},
        {0},
    };

    if (!package)
        return ERROR_INVALID_HANDLE;

    productid = load_dynamic_property(package,szProductID,&rc);
    if (!productid)
        return ERROR_SUCCESS;

    productcode = load_dynamic_property(package,szProductCode,&rc);
    if (!productcode)
        return rc;

    rc = MSIREG_OpenUninstallKey(productcode,&hkey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    i = 0;
    while (szPropKeys[i][0]!=0)
    {
        buffer = load_dynamic_property(package,szPropKeys[i],&rc);
        if (rc == ERROR_SUCCESS)
        {
            size = strlenW(buffer)*sizeof(WCHAR);
            RegSetValueExW(hkey,szRegKeys[i],0,REG_SZ,(LPSTR)buffer,size);
        }
        else
            RegSetValueExW(hkey,szRegKeys[i],0,REG_SZ,NULL,0);
        i++;
    }

end:
    HeapFree(GetProcessHeap(),0,productcode);
    HeapFree(GetProcessHeap(),0,productid);
    RegCloseKey(hkey);

    return ERROR_SUCCESS;
}


static UINT ACTION_ExecuteAction(MSIPACKAGE *package)
{
    UINT rc;
    rc = ACTION_ProcessExecSequence(package,FALSE);
    return rc;
}


/*
 * Code based off of code located here
 * http://www.codeproject.com/gdi/fontnamefromfile.asp
 *
 * Using string index 4 (full font name) instead of 1 (family name)
 */
static LPWSTR load_ttfname_from(LPCWSTR filename)
{
    HANDLE handle;
    LPWSTR ret = NULL;
    int i;

    typedef struct _tagTT_OFFSET_TABLE{
        USHORT uMajorVersion;
        USHORT uMinorVersion;
        USHORT uNumOfTables;
        USHORT uSearchRange;
        USHORT uEntrySelector;
        USHORT uRangeShift;
    }TT_OFFSET_TABLE;

    typedef struct _tagTT_TABLE_DIRECTORY{
        char szTag[4]; /* table name */
        ULONG uCheckSum; /* Check sum */
        ULONG uOffset; /* Offset from beginning of file */
        ULONG uLength; /* length of the table in bytes */
    }TT_TABLE_DIRECTORY;

    typedef struct _tagTT_NAME_TABLE_HEADER{
    USHORT uFSelector; /* format selector. Always 0 */
    USHORT uNRCount; /* Name Records count */
    USHORT uStorageOffset; /* Offset for strings storage, 
                            * from start of the table */
    }TT_NAME_TABLE_HEADER;
   
    typedef struct _tagTT_NAME_RECORD{
        USHORT uPlatformID;
        USHORT uEncodingID;
        USHORT uLanguageID;
        USHORT uNameID;
        USHORT uStringLength;
        USHORT uStringOffset; /* from start of storage area */
    }TT_NAME_RECORD;

#define SWAPWORD(x) MAKEWORD(HIBYTE(x), LOBYTE(x))
#define SWAPLONG(x) MAKELONG(SWAPWORD(HIWORD(x)), SWAPWORD(LOWORD(x)))

    handle = CreateFileW(filename ,GENERIC_READ, 0, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, 0 );
    if (handle != INVALID_HANDLE_VALUE)
    {
        TT_TABLE_DIRECTORY tblDir;
        BOOL bFound = FALSE;
        TT_OFFSET_TABLE ttOffsetTable;

        ReadFile(handle,&ttOffsetTable, sizeof(TT_OFFSET_TABLE),NULL,NULL);
        ttOffsetTable.uNumOfTables = SWAPWORD(ttOffsetTable.uNumOfTables);
        ttOffsetTable.uMajorVersion = SWAPWORD(ttOffsetTable.uMajorVersion);
        ttOffsetTable.uMinorVersion = SWAPWORD(ttOffsetTable.uMinorVersion);
        
        if (ttOffsetTable.uMajorVersion != 1 || 
                        ttOffsetTable.uMinorVersion != 0)
            return NULL;

        for (i=0; i< ttOffsetTable.uNumOfTables; i++)
        {
            ReadFile(handle,&tblDir, sizeof(TT_TABLE_DIRECTORY),NULL,NULL);
            if (strncmp(tblDir.szTag,"name",4)==0)
            {
                bFound = TRUE;
                tblDir.uLength = SWAPLONG(tblDir.uLength);
                tblDir.uOffset = SWAPLONG(tblDir.uOffset);
                break;
            }
        }

        if (bFound)
        {
            TT_NAME_TABLE_HEADER ttNTHeader;
            TT_NAME_RECORD ttRecord;

            SetFilePointer(handle, tblDir.uOffset, NULL, FILE_BEGIN);
            ReadFile(handle,&ttNTHeader, sizeof(TT_NAME_TABLE_HEADER),
                            NULL,NULL);

            ttNTHeader.uNRCount = SWAPWORD(ttNTHeader.uNRCount);
            ttNTHeader.uStorageOffset = SWAPWORD(ttNTHeader.uStorageOffset);
            bFound = FALSE;
            for(i=0; i<ttNTHeader.uNRCount; i++)
            {
                ReadFile(handle,&ttRecord, sizeof(TT_NAME_RECORD),NULL,NULL);
                ttRecord.uNameID = SWAPWORD(ttRecord.uNameID);
                /* 4 is the Full Font Name */
                if(ttRecord.uNameID == 4)
                {
                    int nPos;
                    LPSTR buf;
                    static const LPSTR tt = " (TrueType)";

                    ttRecord.uStringLength = SWAPWORD(ttRecord.uStringLength);
                    ttRecord.uStringOffset = SWAPWORD(ttRecord.uStringOffset);
                    nPos = SetFilePointer(handle, 0, NULL, FILE_CURRENT);
                    SetFilePointer(handle, tblDir.uOffset + 
                                    ttRecord.uStringOffset + 
                                    ttNTHeader.uStorageOffset,
                                    NULL, FILE_BEGIN);
                    buf = HeapAlloc(GetProcessHeap(), 0, 
                                    ttRecord.uStringLength + 1 + strlen(tt));
                    memset(buf, 0, ttRecord.uStringLength + 1 + strlen(tt));
                    ReadFile(handle, buf, ttRecord.uStringLength, NULL, NULL);
                    if (strlen(buf) > 0)
                    {
                        strcat(buf,tt);
                        ret = strdupAtoW(buf);
                        HeapFree(GetProcessHeap(),0,buf);
                        break;
                    }

                    HeapFree(GetProcessHeap(),0,buf);
                    SetFilePointer(handle,nPos, NULL, FILE_BEGIN);
                }
            }
        }
        CloseHandle(handle);
    }
    else
        ERR("Unable to open font file %s\n", debugstr_w(filename));

    TRACE("Returning fontname %s\n",debugstr_w(ret));
    return ret;
}

static UINT ACTION_RegisterFonts(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    MSIRECORD * row = 0;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','F','o','n','t','`',0};
    static const WCHAR regfont1[] =
        {'S','o','f','t','w','a','r','e','\\',
         'M','i','c','r','o','s','o','f','t','\\',
         'W','i','n','d','o','w','s',' ','N','T','\\',
         'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
         'F','o','n','t','s',0};
    static const WCHAR regfont2[] =
        {'S','o','f','t','w','a','r','e','\\',
         'M','i','c','r','o','s','o','f','t','\\',
         'W','i','n','d','o','w','s','\\',
         'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
         'F','o','n','t','s',0};
    HKEY hkey1;
    HKEY hkey2;

    TRACE("%p\n", package);

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
    {
        TRACE("MSI_DatabaseOpenViewW failed: %d\n", rc);
        return ERROR_SUCCESS;
    }

    rc = MSI_ViewExecute(view, 0);
    if (rc != ERROR_SUCCESS)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        TRACE("MSI_ViewExecute returned %d\n", rc);
        return ERROR_SUCCESS;
    }

    RegCreateKeyW(HKEY_LOCAL_MACHINE,regfont1,&hkey1);
    RegCreateKeyW(HKEY_LOCAL_MACHINE,regfont2,&hkey2);
    
    while (1)
    {
        LPWSTR name;
        LPWSTR file;
        UINT index;
        DWORD size;

        rc = MSI_ViewFetch(view,&row);
        if (rc != ERROR_SUCCESS)
        {
            rc = ERROR_SUCCESS;
            break;
        }

        file = load_dynamic_stringW(row,1);
        index = get_loaded_file(package,file);
        if (index < 0)
        {
            ERR("Unable to load file\n");
            HeapFree(GetProcessHeap(),0,file);
            continue;
        }

        /* check to make sure that component is installed */
        if (!ACTION_VerifyComponentForAction(package, 
                package->files[index].ComponentIndex, INSTALLSTATE_LOCAL))
        {
            TRACE("Skipping: Component not scheduled for install\n");
            HeapFree(GetProcessHeap(),0,file);

            msiobj_release(&row->hdr);

            continue;
        }

        if (MSI_RecordIsNull(row,2))
            name = load_ttfname_from(package->files[index].TargetPath);
        else
            name = load_dynamic_stringW(row,2);

        if (name)
        {
            size = strlenW(package->files[index].FileName) * sizeof(WCHAR);
            RegSetValueExW(hkey1,name,0,REG_SZ,
                        (LPBYTE)package->files[index].FileName,size);
            RegSetValueExW(hkey2,name,0,REG_SZ,
                        (LPBYTE)package->files[index].FileName,size);
        }
        
        HeapFree(GetProcessHeap(),0,file);
        HeapFree(GetProcessHeap(),0,name);
        msiobj_release(&row->hdr);
    }
    MSI_ViewClose(view);
    msiobj_release(&view->hdr);

    RegCloseKey(hkey1);
    RegCloseKey(hkey2);

    TRACE("returning %d\n", rc);
    return rc;
}

static UINT ITERATE_PublishComponent(MSIRECORD *rec, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPWSTR compgroupid=NULL;
    LPWSTR feature=NULL;
    LPWSTR text = NULL;
    LPWSTR qualifier = NULL;
    LPWSTR component = NULL;
    LPWSTR advertise = NULL;
    LPWSTR output = NULL;
    HKEY hkey;
    UINT rc = ERROR_SUCCESS;
    UINT index;
    DWORD sz = 0;

    component = load_dynamic_stringW(rec,3);
    index = get_loaded_component(package,component);

    if (!ACTION_VerifyComponentForAction(package, index,
                            INSTALLSTATE_LOCAL) && 
       !ACTION_VerifyComponentForAction(package, index,
                            INSTALLSTATE_SOURCE) &&
       !ACTION_VerifyComponentForAction(package, index,
                            INSTALLSTATE_ADVERTISED))
    {
        TRACE("Skipping: Component %s not scheduled for install\n",
                        debugstr_w(component));

        HeapFree(GetProcessHeap(),0,component);
        return ERROR_SUCCESS;
    }

    compgroupid = load_dynamic_stringW(rec,1);

    rc = MSIREG_OpenUserComponentsKey(compgroupid, &hkey, TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;
    
    text = load_dynamic_stringW(rec,4);
    qualifier = load_dynamic_stringW(rec,2);
    feature = load_dynamic_stringW(rec,5);
  
    advertise = create_component_advertise_string(package, 
                    &package->components[index], feature);

    sz = strlenW(advertise);

    if (text)
        sz += lstrlenW(text);

    sz+=3;
    sz *= sizeof(WCHAR);
           
    output = HeapAlloc(GetProcessHeap(),0,sz);
    memset(output,0,sz);
    strcpyW(output,advertise);

    if (text)
        strcatW(output,text);

    sz = (lstrlenW(output)+2) * sizeof(WCHAR);
    RegSetValueExW(hkey, qualifier,0,REG_MULTI_SZ, (LPBYTE)output, sz);
    
end:
    RegCloseKey(hkey);
    HeapFree(GetProcessHeap(),0,output);
    HeapFree(GetProcessHeap(),0,compgroupid);
    HeapFree(GetProcessHeap(),0,component);
    HeapFree(GetProcessHeap(),0,feature);
    HeapFree(GetProcessHeap(),0,text);
    HeapFree(GetProcessHeap(),0,qualifier);
    
    return rc;
}

/*
 * At present I am ignorning the advertised components part of this and only
 * focusing on the qualified component sets
 */
static UINT ACTION_PublishComponents(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','P','u','b','l','i','s','h',
         'C','o','m','p','o','n','e','n','t','`',0};
    
    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_PublishComponent, package);
    msiobj_release(&view->hdr);

    return rc;
}

/* Msi functions that seem appropriate here */

/***********************************************************************
 * MsiDoActionA       (MSI.@)
 */
UINT WINAPI MsiDoActionA( MSIHANDLE hInstall, LPCSTR szAction )
{
    LPWSTR szwAction;
    UINT rc;

    TRACE(" exteral attempt at action %s\n",szAction);

    if (!szAction)
        return ERROR_FUNCTION_FAILED;
    if (hInstall == 0)
        return ERROR_FUNCTION_FAILED;

    szwAction = strdupAtoW(szAction);

    if (!szwAction)
        return ERROR_FUNCTION_FAILED; 


    rc = MsiDoActionW(hInstall, szwAction);
    HeapFree(GetProcessHeap(),0,szwAction);
    return rc;
}

/***********************************************************************
 * MsiDoActionW       (MSI.@)
 */
UINT WINAPI MsiDoActionW( MSIHANDLE hInstall, LPCWSTR szAction )
{
    MSIPACKAGE *package;
    UINT ret = ERROR_INVALID_HANDLE;

    TRACE(" external attempt at action %s \n",debugstr_w(szAction));

    package = msihandle2msiinfo(hInstall, MSIHANDLETYPE_PACKAGE);
    if( package )
    {
        ret = ACTION_PerformUIAction(package,szAction);
        msiobj_release( &package->hdr );
    }
    return ret;
}

UINT WINAPI MsiGetTargetPathA( MSIHANDLE hInstall, LPCSTR szFolder, 
                               LPSTR szPathBuf, DWORD* pcchPathBuf) 
{
    LPWSTR szwFolder;
    LPWSTR szwPathBuf;
    UINT rc;

    TRACE("getting folder %s %p %li\n",szFolder,szPathBuf, *pcchPathBuf);

    if (!szFolder)
        return ERROR_FUNCTION_FAILED;
    if (hInstall == 0)
        return ERROR_FUNCTION_FAILED;

    szwFolder = strdupAtoW(szFolder);

    if (!szwFolder)
        return ERROR_FUNCTION_FAILED; 

    szwPathBuf = HeapAlloc( GetProcessHeap(), 0 , *pcchPathBuf * sizeof(WCHAR));

    rc = MsiGetTargetPathW(hInstall, szwFolder, szwPathBuf,pcchPathBuf);

    WideCharToMultiByte( CP_ACP, 0, szwPathBuf, *pcchPathBuf, szPathBuf,
                         *pcchPathBuf, NULL, NULL );

    HeapFree(GetProcessHeap(),0,szwFolder);
    HeapFree(GetProcessHeap(),0,szwPathBuf);

    return rc;
}

UINT WINAPI MsiGetTargetPathW( MSIHANDLE hInstall, LPCWSTR szFolder, LPWSTR
                                szPathBuf, DWORD* pcchPathBuf) 
{
    LPWSTR path;
    UINT rc = ERROR_FUNCTION_FAILED;
    MSIPACKAGE *package;

    TRACE("(%s %p %li)\n",debugstr_w(szFolder),szPathBuf,*pcchPathBuf);

    package = msihandle2msiinfo(hInstall, MSIHANDLETYPE_PACKAGE);
    if (!package)
        return ERROR_INVALID_HANDLE;
    path = resolve_folder(package, szFolder, FALSE, FALSE, NULL);
    msiobj_release( &package->hdr );

    if (path && (strlenW(path) > *pcchPathBuf))
    {
        *pcchPathBuf = strlenW(path)+1;
        rc = ERROR_MORE_DATA;
    }
    else if (path)
    {
        *pcchPathBuf = strlenW(path)+1;
        strcpyW(szPathBuf,path);
        TRACE("Returning Path %s\n",debugstr_w(path));
        rc = ERROR_SUCCESS;
    }
    HeapFree(GetProcessHeap(),0,path);
    
    return rc;
}


UINT WINAPI MsiGetSourcePathA( MSIHANDLE hInstall, LPCSTR szFolder, 
                               LPSTR szPathBuf, DWORD* pcchPathBuf) 
{
    LPWSTR szwFolder;
    LPWSTR szwPathBuf;
    UINT rc;

    TRACE("getting source %s %p %li\n",szFolder,szPathBuf, *pcchPathBuf);

    if (!szFolder)
        return ERROR_FUNCTION_FAILED;
    if (hInstall == 0)
        return ERROR_FUNCTION_FAILED;

    szwFolder = strdupAtoW(szFolder);
    if (!szwFolder)
        return ERROR_FUNCTION_FAILED; 

    szwPathBuf = HeapAlloc( GetProcessHeap(), 0 , *pcchPathBuf * sizeof(WCHAR));

    rc = MsiGetSourcePathW(hInstall, szwFolder, szwPathBuf,pcchPathBuf);

    WideCharToMultiByte( CP_ACP, 0, szwPathBuf, *pcchPathBuf, szPathBuf,
                         *pcchPathBuf, NULL, NULL );

    HeapFree(GetProcessHeap(),0,szwFolder);
    HeapFree(GetProcessHeap(),0,szwPathBuf);

    return rc;
}

UINT WINAPI MsiGetSourcePathW( MSIHANDLE hInstall, LPCWSTR szFolder, LPWSTR
                                szPathBuf, DWORD* pcchPathBuf) 
{
    LPWSTR path;
    UINT rc = ERROR_FUNCTION_FAILED;
    MSIPACKAGE *package;

    TRACE("(%s %p %li)\n",debugstr_w(szFolder),szPathBuf,*pcchPathBuf);

    package = msihandle2msiinfo(hInstall, MSIHANDLETYPE_PACKAGE);
    if( !package )
        return ERROR_INVALID_HANDLE;
    path = resolve_folder(package, szFolder, TRUE, FALSE, NULL);
    msiobj_release( &package->hdr );

    if (path && strlenW(path) > *pcchPathBuf)
    {
        *pcchPathBuf = strlenW(path)+1;
        rc = ERROR_MORE_DATA;
    }
    else if (path)
    {
        *pcchPathBuf = strlenW(path)+1;
        strcpyW(szPathBuf,path);
        TRACE("Returning Path %s\n",debugstr_w(path));
        rc = ERROR_SUCCESS;
    }
    HeapFree(GetProcessHeap(),0,path);
    
    return rc;
}


/***********************************************************************
 * MsiSetTargetPathA  (MSI.@)
 */
UINT WINAPI MsiSetTargetPathA(MSIHANDLE hInstall, LPCSTR szFolder, 
                             LPCSTR szFolderPath)
{
    LPWSTR szwFolder;
    LPWSTR szwFolderPath;
    UINT rc;

    if (!szFolder)
        return ERROR_FUNCTION_FAILED;
    if (hInstall == 0)
        return ERROR_FUNCTION_FAILED;

    szwFolder = strdupAtoW(szFolder);
    if (!szwFolder)
        return ERROR_FUNCTION_FAILED; 

    szwFolderPath = strdupAtoW(szFolderPath);
    if (!szwFolderPath)
    {
        HeapFree(GetProcessHeap(),0,szwFolder);
        return ERROR_FUNCTION_FAILED; 
    }

    rc = MsiSetTargetPathW(hInstall, szwFolder, szwFolderPath);

    HeapFree(GetProcessHeap(),0,szwFolder);
    HeapFree(GetProcessHeap(),0,szwFolderPath);

    return rc;
}

UINT MSI_SetTargetPathW(MSIPACKAGE *package, LPCWSTR szFolder, 
                             LPCWSTR szFolderPath)
{
    DWORD i;
    LPWSTR path = NULL;
    LPWSTR path2 = NULL;
    MSIFOLDER *folder;

    TRACE("(%p %s %s)\n",package, debugstr_w(szFolder),debugstr_w(szFolderPath));

    if (package==NULL)
        return ERROR_INVALID_HANDLE;

    if (szFolderPath[0]==0)
        return ERROR_FUNCTION_FAILED;

    if (GetFileAttributesW(szFolderPath) == INVALID_FILE_ATTRIBUTES)
        return ERROR_FUNCTION_FAILED;

    path = resolve_folder(package,szFolder,FALSE,FALSE,&folder);

    if (!path)
        return ERROR_INVALID_PARAMETER;

    HeapFree(GetProcessHeap(),0,folder->Property);
    folder->Property = build_directory_name(2, szFolderPath, NULL);

    if (lstrcmpiW(path, folder->Property) == 0)
    {
        /*
         *  Resolved Target has not really changed, so just 
         *  set this folder and do not recalculate everything.
         */
        HeapFree(GetProcessHeap(),0,folder->ResolvedTarget);
        folder->ResolvedTarget = NULL;
        path2 = resolve_folder(package,szFolder,FALSE,TRUE,NULL);
        HeapFree(GetProcessHeap(),0,path2);
    }
    else
    {
        for (i = 0; i < package->loaded_folders; i++)
        {
            HeapFree(GetProcessHeap(),0,package->folders[i].ResolvedTarget);
            package->folders[i].ResolvedTarget=NULL;
        }

        for (i = 0; i < package->loaded_folders; i++)
        {
            path2=resolve_folder(package, package->folders[i].Directory, FALSE,
                       TRUE, NULL);
            HeapFree(GetProcessHeap(),0,path2);
        }
    }
    HeapFree(GetProcessHeap(),0,path);

    return ERROR_SUCCESS;
}

/***********************************************************************
 * MsiSetTargetPathW  (MSI.@)
 */
UINT WINAPI MsiSetTargetPathW(MSIHANDLE hInstall, LPCWSTR szFolder, 
                             LPCWSTR szFolderPath)
{
    MSIPACKAGE *package;
    UINT ret;

    TRACE("(%s %s)\n",debugstr_w(szFolder),debugstr_w(szFolderPath));

    package = msihandle2msiinfo(hInstall, MSIHANDLETYPE_PACKAGE);
    ret = MSI_SetTargetPathW( package, szFolder, szFolderPath );
    msiobj_release( &package->hdr );
    return ret;
}

/***********************************************************************
 *           MsiGetMode    (MSI.@)
 *
 * Returns an internal installer state (if it is running in a mode iRunMode)
 *
 * PARAMS
 *   hInstall    [I]  Handle to the installation
 *   hRunMode    [I]  Checking run mode
 *        MSIRUNMODE_ADMIN             Administrative mode
 *        MSIRUNMODE_ADVERTISE         Advertisement mode
 *        MSIRUNMODE_MAINTENANCE       Maintenance mode
 *        MSIRUNMODE_ROLLBACKENABLED   Rollback is enabled
 *        MSIRUNMODE_LOGENABLED        Log file is writing
 *        MSIRUNMODE_OPERATIONS        Operations in progress??
 *        MSIRUNMODE_REBOOTATEND       We need to reboot after installation completed
 *        MSIRUNMODE_REBOOTNOW         We need to reboot to continue the installation
 *        MSIRUNMODE_CABINET           Files from cabinet are installed
 *        MSIRUNMODE_SOURCESHORTNAMES  Long names in source files is suppressed
 *        MSIRUNMODE_TARGETSHORTNAMES  Long names in destination files is suppressed
 *        MSIRUNMODE_RESERVED11        Reserved
 *        MSIRUNMODE_WINDOWS9X         Running under Windows95/98
 *        MSIRUNMODE_ZAWENABLED        Demand installation is supported
 *        MSIRUNMODE_RESERVED14        Reserved
 *        MSIRUNMODE_RESERVED15        Reserved
 *        MSIRUNMODE_SCHEDULED         called from install script
 *        MSIRUNMODE_ROLLBACK          called from rollback script
 *        MSIRUNMODE_COMMIT            called from commit script
 *
 * RETURNS
 *    In the state: TRUE
 *    Not in the state: FALSE
 *
 */

BOOL WINAPI MsiGetMode(MSIHANDLE hInstall, MSIRUNMODE iRunMode)
{
    FIXME("STUB (iRunMode=%i)\n",iRunMode);
    return TRUE;
}

/***********************************************************************
 * MsiSetFeatureStateA (MSI.@)
 *
 * According to the docs, when this is called it immediately recalculates
 * all the component states as well
 */
UINT WINAPI MsiSetFeatureStateA(MSIHANDLE hInstall, LPCSTR szFeature,
                                INSTALLSTATE iState)
{
    LPWSTR szwFeature = NULL;
    UINT rc;

    szwFeature = strdupAtoW(szFeature);

    if (!szwFeature)
        return ERROR_FUNCTION_FAILED;
   
    rc = MsiSetFeatureStateW(hInstall,szwFeature, iState); 

    HeapFree(GetProcessHeap(),0,szwFeature);

    return rc;
}



UINT WINAPI MSI_SetFeatureStateW(MSIPACKAGE* package, LPCWSTR szFeature,
                                INSTALLSTATE iState)
{
    INT index, i;
    UINT rc = ERROR_SUCCESS;

    TRACE(" %s to %i\n",debugstr_w(szFeature), iState);

    index = get_loaded_feature(package,szFeature);
    if (index < 0)
        return ERROR_UNKNOWN_FEATURE;

    package->features[index].ActionRequest= iState;
    package->features[index].Action= iState;

    ACTION_UpdateComponentStates(package,szFeature);

    /* update all the features that are children of this feature */
    for (i = 0; i < package->loaded_features; i++)
    {
        if (strcmpW(szFeature, package->features[i].Feature_Parent) == 0)
            MSI_SetFeatureStateW(package, package->features[i].Feature, iState);
    }
    
    return rc;
}

/***********************************************************************
 * MsiSetFeatureStateW (MSI.@)
 */
UINT WINAPI MsiSetFeatureStateW(MSIHANDLE hInstall, LPCWSTR szFeature,
                                INSTALLSTATE iState)
{
    MSIPACKAGE* package;
    UINT rc = ERROR_SUCCESS;

    TRACE(" %s to %i\n",debugstr_w(szFeature), iState);

    package = msihandle2msiinfo(hInstall, MSIHANDLETYPE_PACKAGE);
    if (!package)
        return ERROR_INVALID_HANDLE;

    rc = MSI_SetFeatureStateW(package,szFeature,iState);

    msiobj_release( &package->hdr );
    return rc;
}

UINT WINAPI MsiGetFeatureStateA(MSIHANDLE hInstall, LPSTR szFeature,
                  INSTALLSTATE *piInstalled, INSTALLSTATE *piAction)
{
    LPWSTR szwFeature = NULL;
    UINT rc;
    
    szwFeature = strdupAtoW(szFeature);

    rc = MsiGetFeatureStateW(hInstall,szwFeature,piInstalled, piAction);

    HeapFree( GetProcessHeap(), 0 , szwFeature);

    return rc;
}

UINT MSI_GetFeatureStateW(MSIPACKAGE *package, LPWSTR szFeature,
                  INSTALLSTATE *piInstalled, INSTALLSTATE *piAction)
{
    INT index;

    index = get_loaded_feature(package,szFeature);
    if (index < 0)
        return ERROR_UNKNOWN_FEATURE;

    if (piInstalled)
        *piInstalled = package->features[index].Installed;

    if (piAction)
        *piAction = package->features[index].Action;

    TRACE("returning %i %i\n",*piInstalled,*piAction);

    return ERROR_SUCCESS;
}

UINT WINAPI MsiGetFeatureStateW(MSIHANDLE hInstall, LPWSTR szFeature,
                  INSTALLSTATE *piInstalled, INSTALLSTATE *piAction)
{
    MSIPACKAGE* package;
    UINT ret;

    TRACE("%ld %s %p %p\n", hInstall, debugstr_w(szFeature), piInstalled,
piAction);

    package = msihandle2msiinfo(hInstall, MSIHANDLETYPE_PACKAGE);
    if (!package)
        return ERROR_INVALID_HANDLE;
    ret = MSI_GetFeatureStateW(package, szFeature, piInstalled, piAction);
    msiobj_release( &package->hdr );
    return ret;
}

/***********************************************************************
 * MsiGetComponentStateA (MSI.@)
 */
UINT WINAPI MsiGetComponentStateA(MSIHANDLE hInstall, LPSTR szComponent,
                  INSTALLSTATE *piInstalled, INSTALLSTATE *piAction)
{
    LPWSTR szwComponent= NULL;
    UINT rc;
    
    szwComponent= strdupAtoW(szComponent);

    rc = MsiGetComponentStateW(hInstall,szwComponent,piInstalled, piAction);

    HeapFree( GetProcessHeap(), 0 , szwComponent);

    return rc;
}

UINT MSI_GetComponentStateW(MSIPACKAGE *package, LPWSTR szComponent,
                  INSTALLSTATE *piInstalled, INSTALLSTATE *piAction)
{
    INT index;

    TRACE("%p %s %p %p\n", package, debugstr_w(szComponent), piInstalled,
piAction);

    index = get_loaded_component(package,szComponent);
    if (index < 0)
        return ERROR_UNKNOWN_COMPONENT;

    if (piInstalled)
        *piInstalled = package->components[index].Installed;

    if (piAction)
        *piAction = package->components[index].Action;

    TRACE("states (%i, %i)\n",
(piInstalled)?*piInstalled:-1,(piAction)?*piAction:-1);

    return ERROR_SUCCESS;
}

/***********************************************************************
 * MsiGetComponentStateW (MSI.@)
 */
UINT WINAPI MsiGetComponentStateW(MSIHANDLE hInstall, LPWSTR szComponent,
                  INSTALLSTATE *piInstalled, INSTALLSTATE *piAction)
{
    MSIPACKAGE* package;
    UINT ret;

    TRACE("%ld %s %p %p\n", hInstall, debugstr_w(szComponent),
           piInstalled, piAction);

    package = msihandle2msiinfo(hInstall, MSIHANDLETYPE_PACKAGE);
    if (!package)
        return ERROR_INVALID_HANDLE;
    ret = MSI_GetComponentStateW( package, szComponent, piInstalled, piAction);
    msiobj_release( &package->hdr );
    return ret;
}
