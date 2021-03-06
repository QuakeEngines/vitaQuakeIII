/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/time.h>
#include <pwd.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <vitasdk.h>
#include <sys/select.h>

void log2file(const char *format, ...) {
	__gnuc_va_list arg;
	int done;
	va_start(arg, format);
	char msg[512];
	done = vsprintf(msg, format, arg);
	va_end(arg);
	sprintf(msg, "%s\n", msg);
	FILE *log = fopen("ux0:/data/ioq3.log", "a+");
	if (log != NULL) {
		fwrite(msg, 1, strlen(msg), log);
		fclose(log);
	}
}

#ifndef RELEASE
# define printf log2file
#endif

qboolean stdinIsATTY;

// Used to determine where to store user-specific files
static char homePath[MAX_OSPATH] = {0};

// Used to store the Steam Quake 3 installation path
static char steamPath[MAX_OSPATH] = {0};

// Used to store the GOG Quake 3 installation path
static char gogPath[MAX_OSPATH] = {0};

/*
==================
Sys_DefaultHomePath
==================
*/
char *Sys_DefaultHomePath(void) {
    char *p;

    if (!*homePath && com_homepath != NULL) {
        if ((p = getenv("HOME")) != NULL) {
            Com_sprintf(homePath, sizeof(homePath), "%s%c", p, PATH_SEP);
#ifdef __APPLE__
            Q_strcat(homePath, sizeof(homePath),
                "Library/Application Support/");

            if(com_homepath->string[0])
                Q_strcat(homePath, sizeof(homePath), com_homepath->string);
            else
                Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME_MACOSX);
#else
            if (com_homepath->string[0])
                Q_strcat(homePath, sizeof(homePath), com_homepath->string);
            else
                Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME_UNIX);
#endif
        }
    }

    return homePath;
}

/*
================
Sys_SteamPath
================
*/
char *Sys_SteamPath(void) {
    // Disabled since Steam doesn't let you install Quake 3 on Mac/Linux
#if 0 //#ifdef STEAMPATH_NAME
    char *p;

    if( ( p = getenv( "HOME" ) ) != NULL )
    {
#ifdef __APPLE__
        char *steamPathEnd = "/Library/Application Support/Steam/SteamApps/common/" STEAMPATH_NAME;
#else
        char *steamPathEnd = "/.steam/steam/SteamApps/common/" STEAMPATH_NAME;
#endif
        Com_sprintf(steamPath, sizeof(steamPath), "%s%s", p, steamPathEnd);
    }
#endif

    return steamPath;
}

/*
================
Sys_GogPath
================
*/
char *Sys_GogPath(void) {
    // GOG also doesn't let you install Quake 3 on Mac/Linux
    return gogPath;
}

/*
================
Sys_Milliseconds
================
*/
int curtime;
int Sys_Milliseconds(void) {
    static uint64_t	base;

	uint64_t time = sceKernelGetProcessTimeWide() / 1000;
	
    if (!base) {
		base = time;
    }

    curtime = (int)(time - base);

    return curtime;
}

/*
==================
Sys_RandomBytes
==================
*/
qboolean Sys_RandomBytes(byte *string, int len) {
    return qfalse;
}

/*
==================
Sys_GetCurrentUser
==================
*/
char nick[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE];
char *Sys_GetCurrentUser(void) {
	SceAppUtilInitParam init_param;
	SceAppUtilBootParam boot_param;
	memset(&init_param, 0, sizeof(SceAppUtilInitParam));
	memset(&boot_param, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&init_param, &boot_param);
	sceAppUtilSystemParamGetString(SCE_SYSTEM_PARAM_ID_USERNAME, nick, SCE_SYSTEM_PARAM_USERNAME_MAXSIZE);
    return nick;
}

/*
==================
Sys_LowPhysicalMemory

TODO
==================
*/
qboolean Sys_LowPhysicalMemory(void) {
    return qfalse;
}

/*
==================
Sys_Basename
==================
*/
const char *Sys_Basename(char *path) {
    char *p = strrchr(path, '/');
    return p ? p + 1 : (char *) path;
}

/*
==================
Sys_Dirname
==================
*/
const char *Sys_Dirname(char *path) {
    static const char dot[] = ".";
    char *last_slash;

    /* Find last '/'.  */
    last_slash = path != NULL ? strrchr(path, '/') : NULL;

    if (last_slash != NULL && last_slash != path && last_slash[1] == '\0') {
        /* Determine whether all remaining characters are slashes.  */
        char *runp;

        for (runp = last_slash; runp != path; --runp)
            if (runp[-1] != '/')
                break;

        /* The '/' is the last character, we have to look further.  */
        if (runp != path)
            last_slash = memrchr(path, '/', runp - path);
    }

    if (last_slash != NULL) {
        /* Determine whether all remaining characters are slashes.  */
        char *runp;

        for (runp = last_slash; runp != path; --runp)
            if (runp[-1] != '/')
                break;

        /* Terminate the path.  */
        if (runp == path) {
            /* The last slash is the first character in the string.  We have to
            return "/".  As a special case we have to return "//" if there
            are exactly two slashes at the beginning of the string.  See
            XBD 4.10 Path Name Resolution for more information.  */
            if (last_slash == path + 1)
                ++last_slash;
            else
                last_slash = path + 1;
        } else
            last_slash = runp;

        last_slash[0] = '\0';
    } else
        /* This assignment is ill-designed but the XPG specs require to
        return a string containing "." in any case no directory part is
        found and so a static and constant string is required.  */
        path = (char *) dot;

    return path;
}

/*
==============
Sys_FOpen
==============
*/
FILE *Sys_FOpen(const char *ospath, const char *mode) {
    struct stat buf;

    // check if path exists and is a directory
    if (!stat(ospath, &buf) && S_ISDIR(buf.st_mode))
        return NULL;

    return fopen(ospath, mode);
}

/*
==================
Sys_Mkdir
==================
*/
qboolean Sys_Mkdir(const char *path) {

    int result = sceIoMkdir(path, 0777);
    //printf("Sys_Mkdir: 0x%08X\n", result);
    if (result != 0 && result != 0x80010011
            && result != 0x8001000D)
        return errno == EEXIST;

    return qtrue;
}

/*
==================
Sys_Mkfifo
==================
*/
FILE *Sys_Mkfifo(const char *ospath) {
    return NULL;
}

/*
==================
Sys_Cwd
==================
*/
char *Sys_Cwd(void) {
    return DEFAULT_BASEDIR;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==================
Sys_ListFilteredFiles
==================
*/
void Sys_ListFilteredFiles(const char *basedir, char *subdirs, char *filter, char **list, int *numfiles) {
    char search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
    char filename[MAX_OSPATH];
    DIR *fdir;
    struct dirent *d;
    struct stat st;

    if (*numfiles >= MAX_FOUND_FILES - 1) {
        return;
    }

    if (strlen(subdirs)) {
        Com_sprintf(search, sizeof(search), "%s/%s", basedir, subdirs);
    } else {
        Com_sprintf(search, sizeof(search), "%s", basedir);
    }

    if ((fdir = opendir(search)) == NULL) {
        return;
    }

    while ((d = readdir(fdir)) != NULL) {
        Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
        if (stat(filename, &st) == -1)
            continue;

        if (st.st_mode & S_IFDIR) {
            if (Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, "..")) {
                if (strlen(subdirs)) {
                    Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
                } else {
                    Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
                }
                Sys_ListFilteredFiles(basedir, newsubdirs, filter, list, numfiles);
            }
        }
        if (*numfiles >= MAX_FOUND_FILES - 1) {
            break;
        }
        Com_sprintf(filename, sizeof(filename), "%s/%s", subdirs, d->d_name);
        if (!Com_FilterPath(filter, filename, qfalse))
            continue;
        list[*numfiles] = CopyString(filename);
        (*numfiles)++;
    }

    closedir(fdir);
}

/*
==================
Sys_ListFiles
==================
*/
char **Sys_ListFiles(const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs) {
    struct dirent *d;
    DIR *fdir;
    qboolean dironly = wantsubs;
    char search[MAX_OSPATH];
    int nfiles;
    char **listCopy;
    char *list[MAX_FOUND_FILES];
    int i;
    struct stat st;

    int extLen;

    if (filter) {

        nfiles = 0;
        Sys_ListFilteredFiles(directory, "", filter, list, &nfiles);

        list[nfiles] = NULL;
        *numfiles = nfiles;

        if (!nfiles)
            return NULL;

        listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
        for (i = 0; i < nfiles; i++) {
            listCopy[i] = list[i];
        }
        listCopy[i] = NULL;

        return listCopy;
    }

    if (!extension)
        extension = "";

    if (extension[0] == '/' && extension[1] == 0) {
        extension = "";
        dironly = qtrue;
    }

    extLen = strlen(extension);

    // search
    nfiles = 0;

    if ((fdir = opendir(directory)) == NULL) {
        *numfiles = 0;
        return NULL;
    }

    while ((d = readdir(fdir)) != NULL) {
        Com_sprintf(search, sizeof(search), "%s/%s", directory, d->d_name);
        if (stat(search, &st) == -1)
            continue;
        if ((dironly && !(st.st_mode & S_IFDIR)) ||
            (!dironly && (st.st_mode & S_IFDIR)))
            continue;

        if (*extension) {
            if (strlen(d->d_name) < extLen ||
                Q_stricmp(
                        d->d_name + strlen(d->d_name) - extLen,
                        extension)) {
                continue; // didn't match
            }
        }

        if (nfiles == MAX_FOUND_FILES - 1)
            break;
        list[nfiles] = CopyString(d->d_name);
        nfiles++;
    }

    list[nfiles] = NULL;

    closedir(fdir);

    // return a copy of the list
    *numfiles = nfiles;

    if (!nfiles) {
        return NULL;
    }

    listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
    for (i = 0; i < nfiles; i++) {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;

    return listCopy;
}

/*
==================
Sys_FreeFileList
==================
*/
void Sys_FreeFileList(char **list) {
    int i;

    if (!list) {
        return;
    }

    for (i = 0; list[i]; i++) {
        Z_Free(list[i]);
    }

    Z_Free(list);
}

/*
==================
Sys_Sleep

Block execution for msec or until input is received.
==================
*/
void Sys_Sleep(int msec) {
    if (msec == 0)
        return;

    if (stdinIsATTY) {
        fd_set fdset;

        FD_ZERO(&fdset);
        FD_SET(STDIN_FILENO, &fdset);
        if (msec < 0) {
            select(STDIN_FILENO + 1, &fdset, NULL, NULL, NULL);
        } else {
            struct timeval timeout;

            timeout.tv_sec = msec / 1000;
            timeout.tv_usec = (msec % 1000) * 1000;
            select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout);
        }
    } else {
        // With nothing to select() on, we can't wait indefinitely
        if (msec < 0)
            msec = 10;

        sceKernelDelayThread(msec * 1000);

    }
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog(const char *error) {
    log2file(error);
}

/*
==============
Sys_GLimpSafeInit

Unix specific "safe" GL implementation initialisation
==============
*/
void Sys_GLimpSafeInit(void) {
    // NOP
}

/*
==============
Sys_GLimpInit

Unix specific GL implementation initialisation
==============
*/
void Sys_GLimpInit(void) {
    // NOP
}

void Sys_SetFloatEnv(void) {
    // rounding toward nearest
    // fesetround(FE_TONEAREST);
    // TODO: PSP2 ?
}

/*
==============
Sys_PlatformInit

Unix specific initialisation
==============
*/
void Sys_PlatformInit(void) {
    const char *term = getenv("TERM");

    signal(SIGHUP, Sys_SigHandler);
    signal(SIGQUIT, Sys_SigHandler);
    signal(SIGTRAP, Sys_SigHandler);
    signal(SIGABRT, Sys_SigHandler);
    signal(SIGBUS, Sys_SigHandler);

    Sys_SetFloatEnv();

    stdinIsATTY = isatty(STDIN_FILENO) &&
                  !(term && (!strcmp(term, "raw") || !strcmp(term, "dumb")));
}

/*
==============
Sys_PlatformExit

Unix specific deinitialisation
==============
*/
void Sys_PlatformExit(void) {
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/

void Sys_SetEnv(const char *name, const char *value) {
    if (value && *value)
        setenv(name, value, 1);
    else
        unsetenv(name);
}

/*
==============
Sys_PID
==============
*/
int Sys_PID(void) {
    return getpid();
}

/*
==============
Sys_PIDIsRunning
==============
*/
qboolean Sys_PIDIsRunning(int pid) {
    return kill(pid, 0) == 0;
}

/*
=================
Sys_DllExtension

Check if filename should be allowed to be loaded as a DLL.
=================
*/
qboolean Sys_DllExtension(const char *name) {
    const char *p;
    char c = 0;

    if (COM_CompareExtension(name, DLL_EXT)) {
        return qtrue;
    }

    // Check for format of filename.so.1.2.3
    p = strstr(name, DLL_EXT ".");

    if (p) {
        p += strlen(DLL_EXT);

        // Check if .so is only followed for periods and numbers.
        while (*p) {
            c = *p;

            if (!isdigit(c) && c != '.') {
                return qfalse;
            }

            p++;
        }

        // Don't allow filename to end in a period. file.so., file.so.0., etc
        if (c != '.') {
            return qtrue;
        }
    }

    return qfalse;
}
