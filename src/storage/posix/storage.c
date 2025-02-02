/***********************************************************************************************************************************
Posix Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/posix/read.h"
#include "storage/posix/storage.intern.h"
#include "storage/posix/write.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_POSIX_TYPE_STR,                               STORAGE_POSIX_TYPE);

/***********************************************************************************************************************************
Define PATH_MAX if it is not defined
***********************************************************************************************************************************/
#ifndef PATH_MAX
    #define PATH_MAX                                                (4 * 1024)
#endif

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StoragePosix
{
    MemContext *memContext;                                         // Object memory context
    StorageInterface interface;                                     // Storage interface
};

/***********************************************************************************************************************************
Does a file exist? This function is only for files, not paths.
***********************************************************************************************************************************/
static bool
storagePosixExists(THIS_VOID, const String *file)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    bool result = false;

    // Attempt to stat the file to determine if it exists
    struct stat statFile;

    // Any error other than entry not found should be reported
    if (stat(strPtr(file), &statFile) == -1)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR_FMT(FileOpenError, "unable to stat '%s'", strPtr(file));
    }
    // Else found
    else
        result = !S_ISDIR(statFile.st_mode);

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
static StorageInfo
storagePosixInfo(THIS_VOID, const String *file, bool followLink)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    StorageInfo result = {0};

    // Attempt to stat the file
    struct stat statFile;

    if ((followLink ? stat(strPtr(file), &statFile) : lstat(strPtr(file), &statFile)) == -1)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_INFO, strPtr(file));
    }
    // On success load info into a structure
    else
    {
        result.exists = true;
        result.timeModified = statFile.st_mtime;

        // Get user name if it exists
        struct passwd *userData = getpwuid(statFile.st_uid);

        if (userData != NULL)
            result.user = strNew(userData->pw_name);

        // Get group name if it exists
        struct group *groupData = getgrgid(statFile.st_gid);

        if (groupData != NULL)
            result.group = strNew(groupData->gr_name);

        if (S_ISREG(statFile.st_mode))
        {
            result.type = storageTypeFile;
            result.size = (uint64_t)statFile.st_size;
        }
        else if (S_ISDIR(statFile.st_mode))
            result.type = storageTypePath;
        else if (S_ISLNK(statFile.st_mode))
        {
            result.type = storageTypeLink;

            char linkDestination[PATH_MAX];
            ssize_t linkDestinationSize = 0;

            THROW_ON_SYS_ERROR_FMT(
                (linkDestinationSize = readlink(strPtr(file), linkDestination, sizeof(linkDestination) - 1)) == -1,
                FileReadError, "unable to get destination for link '%s'", strPtr(file));

            result.linkDestination = strNewN(linkDestination, (size_t)linkDestinationSize);
        }
        else
            result.type = storageTypeSpecial;

        result.mode = statFile.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/***********************************************************************************************************************************
Info for all files/paths in a path
***********************************************************************************************************************************/
static void
storagePosixInfoListEntry(
    StoragePosix *this, const String *path, const String *name, StorageInfoListCallback callback, void *callbackData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_POSIX, this);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(FUNCTIONP, callback);
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(name != NULL);
    ASSERT(callback != NULL);

    if (!strEqZ(name, ".."))
    {
        String *pathInfo = strEqZ(name, ".") ? strDup(path) : strNewFmt("%s/%s", strPtr(path), strPtr(name));

        StorageInfo storageInfo = storagePosixInfo(this, pathInfo, false);

        if (storageInfo.exists)
        {
            storageInfo.name = name;
            callback(callbackData, &storageInfo);
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storagePosixInfoList(THIS_VOID, const String *path, StorageInfoListCallback callback, void *callbackData)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(callback != NULL);

    bool result = false;

    // Open the directory for read
    DIR *dir = opendir(strPtr(path));

    // If the directory could not be opened process errors and report missing directories
    if (dir == NULL)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR_FMT(PathOpenError, STORAGE_ERROR_LIST_INFO, strPtr(path));
    }
    else
    {
        // Directory was found
        result = true;

        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_RESET_BEGIN()
            {
                // Read the directory entries
                struct dirent *dirEntry = readdir(dir);

                while (dirEntry != NULL)
                {
                    // Get info and perform callback
                    storagePosixInfoListEntry(this, path, STR(dirEntry->d_name), callback, callbackData);

                    // Get next entry
                    dirEntry = readdir(dir);

                    // Reset the memory context occasionally so we don't use too much memory or slow down processing
                    MEM_CONTEXT_TEMP_RESET(1000);
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        FINALLY()
        {
            closedir(dir);
        }
        TRY_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
static StringList *
storagePosixList(THIS_VOID, const String *path, const String *expression)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StringList *result = NULL;
    DIR *dir = NULL;

    TRY_BEGIN()
    {
        // Open the directory for read
        dir = opendir(strPtr(path));

        // If the directory could not be opened process errors but ignore missing directories when specified
        if (!dir)
        {
            if (errno != ENOENT)
                THROW_SYS_ERROR_FMT(PathOpenError, STORAGE_ERROR_LIST, strPtr(path));
        }
        else
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Prepare regexp if an expression was passed
                RegExp *regExp = expression == NULL ? NULL : regExpNew(expression);

                // Create the string list now that we know the directory is valid
                result = strLstNew();

                // Read the directory entries
                struct dirent *dirEntry = readdir(dir);

                while (dirEntry != NULL)
                {
                    const String *entry = STR(dirEntry->d_name);

                    // Exclude current/parent directory and apply the expression if specified
                    if (!strEqZ(entry, ".") && !strEqZ(entry, "..") && (regExp == NULL || regExpMatch(regExp, entry)))
                        strLstAdd(result, entry);

                    dirEntry = readdir(dir);
                }

                // Move finished list up to the old context
                strLstMove(result, MEM_CONTEXT_OLD());
            }
            MEM_CONTEXT_TEMP_END();
        }
    }
    FINALLY()
    {
        if (dir != NULL)
            closedir(dir);
    }
    TRY_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Move a path/file
***********************************************************************************************************************************/
static bool
storagePosixMove(THIS_VOID,  StorageRead *source, StorageWrite *destination)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STORAGE_READ, source);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(source != NULL);
    ASSERT(destination != NULL);

    bool result = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *sourceFile = storageReadName(source);
        const String *destinationFile = storageWriteName(destination);
        const String *destinationPath = strPath(destinationFile);

        // Attempt to move the file
        if (rename(strPtr(sourceFile), strPtr(destinationFile)) == -1)
        {
            // Detemine which file/path is missing
            if (errno == ENOENT)
            {
                if (!storagePosixExists(this, sourceFile))
                    THROW_SYS_ERROR_FMT(FileMissingError, "unable to move missing file '%s'", strPtr(sourceFile));

                if (!storageWriteCreatePath(destination))
                {
                    THROW_SYS_ERROR_FMT(
                        PathMissingError, "unable to move '%s' to missing path '%s'", strPtr(sourceFile), strPtr(destinationPath));
                }

                storagePosixPathCreate(this, destinationPath, false, false, storageWriteModePath(destination));
                result = storagePosixMove(this, source, destination);
            }
            // Else the destination is on a different device so a copy will be needed
            else if (errno == EXDEV)
            {
                result = false;
            }
            else
                THROW_SYS_ERROR_FMT(FileMoveError, "unable to move '%s' to '%s'", strPtr(sourceFile), strPtr(destinationFile));
        }
        // Sync paths on success
        else
        {
            // Sync source path if the destination path was synced and the paths are not equal
            if (storageWriteSyncPath(destination))
            {
                String *sourcePath = strPath(sourceFile);

                if (!strEq(destinationPath, sourcePath))
                    storagePosixPathSync(this, sourcePath);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
New file read object
***********************************************************************************************************************************/
static StorageRead *
storagePosixNewRead(THIS_VOID, const String *file, bool ignoreMissing, bool compressible)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        (void)compressible;
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadPosixNew(this, file, ignoreMissing));
}

/***********************************************************************************************************************************
New file write object
***********************************************************************************************************************************/
static StorageWrite *
storagePosixNewWrite(
    THIS_VOID, const String *file, mode_t modeFile, mode_t modePath, const String *user, const String *group, time_t timeModified,
    bool createPath, bool syncFile, bool syncPath, bool atomic, bool compressible)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, group);
        FUNCTION_LOG_PARAM(INT64, timeModified);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
        (void)compressible;
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWritePosixNew(
            this, file, modeFile, modePath, user, group, timeModified, createPath, syncFile,
            this->interface.pathSync != NULL ? syncPath : false, atomic));
}

/***********************************************************************************************************************************
Create a path
***********************************************************************************************************************************/
void
storagePosixPathCreate(THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    // Attempt to create the directory
    if (mkdir(strPtr(path), mode) == -1)
    {
        // If the parent path does not exist then create it if allowed
        if (errno == ENOENT && !noParentCreate)
        {
            storagePosixPathCreate(this, strPath(path), errorOnExists, noParentCreate, mode);
            storagePosixPathCreate(this, path, errorOnExists, noParentCreate, mode);
        }
        // Ignore path exists if allowed
        else if (errno != EEXIST || errorOnExists)
            THROW_SYS_ERROR_FMT(PathCreateError, "unable to create path '%s'", strPtr(path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Does a path exist?
***********************************************************************************************************************************/
static bool
storagePosixPathExists(THIS_VOID,  const String *path)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = false;

    // Attempt to stat the file to determine if it exists
    struct stat statPath;

    // Any error other than entry not found should be reported
    if (stat(strPtr(path), &statPath) == -1)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR_FMT(PathOpenError, "unable to stat '%s'", strPtr(path));
    }
    // Else found
    else
        result = S_ISDIR(statPath.st_mode);

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
static bool
storagePosixPathRemove(THIS_VOID, const String *path, bool recurse)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Recurse if requested
        if (recurse)
        {
            // Get a list of files in this path
            StringList *fileList = storagePosixList(this, path, NULL);

            // Only continue if the path exists
            if (fileList != NULL)
            {
                // Delete all paths and files
                for (unsigned int fileIdx = 0; fileIdx < strLstSize(fileList); fileIdx++)
                {
                    String *file = strNewFmt("%s/%s", strPtr(path), strPtr(strLstGet(fileList, fileIdx)));

                    // Rather than stat the file to discover what type it is, just try to unlink it and see what happens
                    if (unlink(strPtr(file)) == -1)
                    {
                        // These errors indicate that the entry is actually a path so we'll try to delete it that way
                        if (errno == EPERM || errno == EISDIR)              // {uncovered_branch - no EPERM on tested systems}
                            storagePosixPathRemove(this, file, true);
                        // Else error
                        else
                            THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE, strPtr(file));
                    }
                }
            }
        }

        // Delete the path
        if (rmdir(strPtr(path)) == -1)
        {
            if (errno != ENOENT)
                THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strPtr(path));

            // Path does not exist
            result = false;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Sync a path
***********************************************************************************************************************************/
void
storagePosixPathSync(THIS_VOID, const String *path)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    // Open directory and handle errors
    int handle = open(strPtr(path), O_RDONLY, 0);

    // Handle errors
    if (handle == -1)
    {
        if (errno == ENOENT)
            THROW_FMT(PathMissingError, STORAGE_ERROR_PATH_SYNC_MISSING, strPtr(path));
        else
            THROW_SYS_ERROR_FMT(PathOpenError, STORAGE_ERROR_PATH_SYNC_OPEN, strPtr(path));
    }
    else
    {
        // Attempt to sync the directory
        if (fsync(handle) == -1)
        {
            int errNo = errno;

            // Close the handle to free resources but don't check for failure
            close(handle);

            THROW_SYS_ERROR_CODE_FMT(errNo, PathSyncError, STORAGE_ERROR_PATH_SYNC, strPtr(path));
        }

        THROW_ON_SYS_ERROR_FMT(close(handle) == -1, PathCloseError, STORAGE_ERROR_PATH_SYNC_CLOSE, strPtr(path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
static void
storagePosixRemove(THIS_VOID, const String *file, bool errorOnMissing)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    // Attempt to unlink the file
    if (unlink(strPtr(file)) == -1)
    {
        if (errorOnMissing || errno != ENOENT)
            THROW_SYS_ERROR_FMT(FileRemoveError, "unable to remove '%s'", strPtr(file));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
Storage *
storagePosixNewInternal(
    const String *type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, bool pathSync)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(BOOL, pathSync);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(path != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    // Create the object
    Storage *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StoragePosix")
    {
        StoragePosix *driver = memNew(sizeof(StoragePosix));
        driver->memContext = MEM_CONTEXT_NEW();

        driver->interface = (StorageInterface)
        {
            .feature = (1 << storageFeaturePath | 1 << storageFeatureCompress), .exists = storagePosixExists,
            .info = storagePosixInfo, .infoList = storagePosixInfoList, .list = storagePosixList, .move = storagePosixMove,
            .newRead = storagePosixNewRead, .newWrite = storagePosixNewWrite, .pathCreate = storagePosixPathCreate,
            .pathExists = storagePosixPathExists, .pathRemove = storagePosixPathRemove,
            .pathSync = pathSync ? storagePosixPathSync : NULL, .remove = storagePosixRemove
        };

        this = storageNew(type, path, modeFile, modePath, write, pathExpressionFunction, driver, driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}

Storage *
storagePosixNew(
    const String *path, mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        STORAGE, storagePosixNewInternal(STORAGE_POSIX_TYPE_STR, path, modeFile, modePath, write, pathExpressionFunction, true));
}
