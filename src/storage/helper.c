/***********************************************************************************************************************************
Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "config/define.h"
#include "config/config.h"
#include "protocol/helper.h"
#include "storage/cifs/storage.h"
#include "storage/posix/storage.h"
#include "storage/remote/storage.h"
#include "storage/s3/storage.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Storage path constants
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_SPOOL_ARCHIVE_IN_STR,                         STORAGE_SPOOL_ARCHIVE_IN);
STRING_EXTERN(STORAGE_SPOOL_ARCHIVE_OUT_STR,                        STORAGE_SPOOL_ARCHIVE_OUT);

STRING_EXTERN(STORAGE_PATH_ARCHIVE_STR,                             STORAGE_PATH_ARCHIVE);
STRING_EXTERN(STORAGE_PATH_BACKUP_STR,                              STORAGE_PATH_BACKUP);

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct
{
    MemContext *memContext;                                         // Mem context for storage helper

    Storage *storageLocal;                                          // Local read-only storage
    Storage *storageLocalWrite;                                     // Local write storage
    Storage **storagePg;                                            // PostgreSQL read-only storage
    Storage **storagePgWrite;                                       // PostgreSQL write storage
    Storage *storageRepo;                                           // Repository read-only storage
    Storage *storageRepoWrite;                                      // Repository write storage
    Storage *storageSpool;                                          // Spool read-only storage
    Storage *storageSpoolWrite;                                     // Spool write storage

    String *stanza;                                                 // Stanza for storage
    bool stanzaInit;                                                // Has the stanza been initialized?
    RegExp *walRegExp;                                              // Regular expression for identifying wal files
} storageHelper;

/***********************************************************************************************************************************
Create the storage helper memory context
***********************************************************************************************************************************/
static void
storageHelperInit(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            storageHelper.memContext = memContextNew("storageHelper");
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Initialize the stanza and error if it changes
***********************************************************************************************************************************/
static void
storageHelperStanzaInit(const bool stanzaRequired)
{
    FUNCTION_TEST_VOID();

    // If the stanza is NULL and the storage has not already been initialized then initialize the stanza
    if (!storageHelper.stanzaInit)
    {
        if (stanzaRequired && cfgOptionStr(cfgOptStanza) == NULL)
            THROW(AssertError, "stanza cannot be NULL for this storage object");

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.stanza = strDup(cfgOptionStr(cfgOptStanza));
            storageHelper.stanzaInit = true;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get a local storage object
***********************************************************************************************************************************/
const Storage *
storageLocal(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageLocal == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageLocal =  storagePosixNew(
                FSLASH_STR, STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageLocal);
}

/***********************************************************************************************************************************
Get a writable local storage object

This should be used very sparingly.  If writes are not needed then always use storageLocal() or a specific storage object instead.
***********************************************************************************************************************************/
const Storage *
storageLocalWrite(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageLocalWrite == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageLocalWrite = storagePosixNew(
                FSLASH_STR, STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageLocalWrite);
}
/***********************************************************************************************************************************
Get pg storage for the specified host id
***********************************************************************************************************************************/
static Storage *
storagePgGet(unsigned int hostId, bool write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, hostId);
        FUNCTION_TEST_PARAM(BOOL, write);
    FUNCTION_TEST_END();

    Storage *result = NULL;

    // Use remote storage
    if (!pgIsLocal(hostId))
    {
        result = storageRemoteNew(
            STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write, NULL,
            protocolRemoteGet(protocolStorageTypePg, hostId), cfgOptionUInt(cfgOptCompressLevelNetwork));
    }
    // Use Posix storage
    else
    {
        result = storagePosixNew(
            cfgOptionStr(cfgOptPgPath + hostId - 1), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write, NULL);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get ready-only PostgreSQL storage for a specific host id
***********************************************************************************************************************************/
const Storage *
storagePgId(unsigned int hostId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, hostId);
    FUNCTION_TEST_END();

    if (storageHelper.storagePg == NULL || storageHelper.storagePg[hostId - 1] == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            if (storageHelper.storagePg == NULL)
                storageHelper.storagePg = memNew(sizeof(Storage *) * cfgDefOptionIndexTotal(cfgDefOptPgPath));

            storageHelper.storagePg[hostId - 1] = storagePgGet(hostId, false);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storagePg[hostId - 1]);
}

/***********************************************************************************************************************************
Get ready-only PostgreSQL storage for the host-id or the default of 1
***********************************************************************************************************************************/
const Storage *
storagePg(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(storagePgId(cfgOptionTest(cfgOptHostId) ? cfgOptionUInt(cfgOptHostId) : 1));
}

/***********************************************************************************************************************************
Get write PostgreSQL storage for a specific host id
***********************************************************************************************************************************/
const Storage *
storagePgIdWrite(unsigned int hostId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, hostId);
    FUNCTION_TEST_END();

    if (storageHelper.storagePgWrite == NULL || storageHelper.storagePgWrite[hostId - 1] == NULL)
    {
        storageHelperInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            if (storageHelper.storagePgWrite == NULL)
                storageHelper.storagePgWrite = memNew(sizeof(Storage *) * cfgDefOptionIndexTotal(cfgDefOptPgPath));

            storageHelper.storagePgWrite[hostId - 1] = storagePgGet(hostId, true);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storagePgWrite[hostId - 1]);
}

/***********************************************************************************************************************************
Get write PostgreSQL storage for the host-id or the default of 1
***********************************************************************************************************************************/
const Storage *
storagePgWrite(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(storagePgIdWrite(cfgOptionTest(cfgOptHostId) ? cfgOptionUInt(cfgOptHostId) : 1));
}

/***********************************************************************************************************************************
Create the WAL regular expression
***********************************************************************************************************************************/
static void
storageHelperRepoInit(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.walRegExp == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            storageHelper.walRegExp = regExpNew(STRDEF("^[0-F]{24}"));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Construct a repo path from an expression and path
***********************************************************************************************************************************/
static String *
storageRepoPathExpression(const String *expression, const String *path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, path);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);

    String *result = NULL;

    if (strEqZ(expression, STORAGE_REPO_ARCHIVE))
    {
        // Contruct the base path
        if (storageHelper.stanza != NULL)
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s", strPtr(storageHelper.stanza));
        else
            result = strNew(STORAGE_PATH_ARCHIVE);

        // If a subpath should be appended, determine if it is WAL path, else just append the subpath
        if (path != NULL)
        {
            StringList *pathSplit = strLstNewSplitZ(path, "/");
            String *file = strLstSize(pathSplit) == 2 ? strLstGet(pathSplit, 1) : NULL;

            if (file != NULL && regExpMatch(storageHelper.walRegExp, file))
                strCatFmt(result, "/%s/%s/%s", strPtr(strLstGet(pathSplit, 0)), strPtr(strSubN(file, 0, 16)), strPtr(file));
            else
                strCatFmt(result, "/%s", strPtr(path));
        }
    }
    else if (strEqZ(expression, STORAGE_REPO_BACKUP))
    {
        // Contruct the base path
        if (storageHelper.stanza != NULL)
            result = strNewFmt(STORAGE_PATH_BACKUP "/%s", strPtr(storageHelper.stanza));
        else
            result = strNew(STORAGE_PATH_BACKUP);

        // Append subpath if provided
        if (path != NULL)
            strCatFmt(result, "/%s", strPtr(path));
    }
    else
        THROW_FMT(AssertError, "invalid expression '%s'", strPtr(expression));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get the repo storage
***********************************************************************************************************************************/
static Storage *
storageRepoGet(const String *type, bool write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
        FUNCTION_TEST_PARAM(BOOL, write);
    FUNCTION_TEST_END();

    ASSERT(type != NULL);

    Storage *result = NULL;

    // Use remote storage
    if (!repoIsLocal())
    {
        result = storageRemoteNew(
            STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write, storageRepoPathExpression,
            protocolRemoteGet(protocolStorageTypeRepo, 1), cfgOptionUInt(cfgOptCompressLevelNetwork));
    }
    // Use CIFS storage
    else if (strEqZ(type, STORAGE_TYPE_CIFS))
    {
        result = storageCifsNew(
            cfgOptionStr(cfgOptRepoPath), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write, storageRepoPathExpression);
    }
    // Use Posix storage
    else if (strEqZ(type, STORAGE_TYPE_POSIX))
    {
        result = storagePosixNew(
            cfgOptionStr(cfgOptRepoPath), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write, storageRepoPathExpression);
    }
    // Use S3 storage
    else if (strEqZ(type, STORAGE_TYPE_S3))
    {
        // Set the default port
        unsigned int port = cfgOptionUInt(cfgOptRepoS3Port);

        // Extract port from the endpoint and host if it is present
        const String *endPoint = cfgOptionHostPort(cfgOptRepoS3Endpoint, &port);
        const String *host = cfgOptionHostPort(cfgOptRepoS3Host, &port);

        // If the port option was set explicitly then use it in preference to appended ports
        if (cfgOptionSource(cfgOptRepoS3Port) != cfgSourceDefault)
            port = cfgOptionUInt(cfgOptRepoS3Port);

        result = storageS3New(
            cfgOptionStr(cfgOptRepoPath), write, storageRepoPathExpression, cfgOptionStr(cfgOptRepoS3Bucket), endPoint,
            cfgOptionStr(cfgOptRepoS3Region), cfgOptionStr(cfgOptRepoS3Key), cfgOptionStr(cfgOptRepoS3KeySecret),
            cfgOptionTest(cfgOptRepoS3Token) ? cfgOptionStr(cfgOptRepoS3Token) : NULL, STORAGE_S3_PARTSIZE_MIN,
            STORAGE_S3_DELETE_MAX, host, port, STORAGE_S3_TIMEOUT_DEFAULT, cfgOptionBool(cfgOptRepoS3VerifyTls),
            cfgOptionTest(cfgOptRepoS3CaFile) ? cfgOptionStr(cfgOptRepoS3CaFile) : NULL,
            cfgOptionTest(cfgOptRepoS3CaPath) ? cfgOptionStr(cfgOptRepoS3CaPath) : NULL);
    }
    else
        THROW_FMT(AssertError, "invalid storage type '%s'", strPtr(type));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get a read-only repository storage object
***********************************************************************************************************************************/
const Storage *
storageRepo(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageRepo == NULL)
    {
        storageHelperInit();
        storageHelperStanzaInit(false);
        storageHelperRepoInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageRepo = storageRepoGet(cfgOptionStr(cfgOptRepoType), false);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageRepo);
}

/***********************************************************************************************************************************
Get a writable repository storage object
***********************************************************************************************************************************/
const Storage *
storageRepoWrite(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageRepoWrite == NULL)
    {
        storageHelperInit();
        storageHelperStanzaInit(false);
        storageHelperRepoInit();

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageRepoWrite = storageRepoGet(cfgOptionStr(cfgOptRepoType), true);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageRepoWrite);
}

/***********************************************************************************************************************************
Get a spool storage object
***********************************************************************************************************************************/
static String *
storageSpoolPathExpression(const String *expression, const String *path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, path);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);
    ASSERT(storageHelper.stanza != NULL);

    String *result = NULL;

    if (strEqZ(expression, STORAGE_SPOOL_ARCHIVE_IN))
    {
        if (path == NULL)
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/in", strPtr(storageHelper.stanza));
        else
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/in/%s", strPtr(storageHelper.stanza), strPtr(path));
    }
    else if (strEqZ(expression, STORAGE_SPOOL_ARCHIVE_OUT))
    {
        if (path == NULL)
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/out", strPtr(storageHelper.stanza));
        else
            result = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/out/%s", strPtr(storageHelper.stanza), strPtr(path));
    }
    else
        THROW_FMT(AssertError, "invalid expression '%s'", strPtr(expression));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get a read-only spool storage object
***********************************************************************************************************************************/
const Storage *
storageSpool(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageSpool == NULL)
    {
        storageHelperInit();
        storageHelperStanzaInit(true);

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageSpool = storagePosixNew(
                cfgOptionStr(cfgOptSpoolPath), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false,
                storageSpoolPathExpression);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageSpool);
}

/***********************************************************************************************************************************
Get a writable spool storage object
***********************************************************************************************************************************/
const Storage *
storageSpoolWrite(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.storageSpoolWrite == NULL)
    {
        storageHelperInit();
        storageHelperStanzaInit(true);

        MEM_CONTEXT_BEGIN(storageHelper.memContext)
        {
            storageHelper.storageSpoolWrite = storagePosixNew(
                cfgOptionStr(cfgOptSpoolPath), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true,
                storageSpoolPathExpression);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(storageHelper.storageSpoolWrite);
}

/***********************************************************************************************************************************
Free all storage helper objects.

This should be done on any config load to ensure that stanza changes are honored.  Currently this is only done in testing, but in
the future it will likely be done in production as well.
***********************************************************************************************************************************/
void
storageHelperFree(void)
{
    FUNCTION_TEST_VOID();

    if (storageHelper.memContext != NULL)
        memContextFree(storageHelper.memContext);

    memset(&storageHelper, 0, sizeof(storageHelper));

    FUNCTION_TEST_RETURN_VOID();
}
