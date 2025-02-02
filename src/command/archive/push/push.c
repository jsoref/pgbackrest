/***********************************************************************************************************************************
Archive Push Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "command/archive/push/file.h"
#include "command/archive/push/protocol.h"
#include "command/command.h"
#include "command/control/common.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "config/config.h"
#include "config/exec.h"
#include "info/infoArchive.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Ready file extension constants
***********************************************************************************************************************************/
#define STATUS_EXT_READY                                            ".ready"
#define STATUS_EXT_READY_SIZE                                       (sizeof(STATUS_EXT_READY) - 1)

/***********************************************************************************************************************************
Format the warning when a file is dropped
***********************************************************************************************************************************/
static String *
archivePushDropWarning(const String *walFile, uint64_t queueMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, walFile);
        FUNCTION_TEST_PARAM(UINT64, queueMax);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        strNewFmt("dropped WAL file '%s' because archive queue exceeded %s", strPtr(walFile), strPtr(strSizeFormat(queueMax))));
}

/***********************************************************************************************************************************
Determine if the WAL process list has become large enough to drop
***********************************************************************************************************************************/
static bool
archivePushDrop(const String *walPath, const StringList *const processList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walPath);
        FUNCTION_LOG_PARAM(STRING_LIST, processList);
    FUNCTION_LOG_END();

    const uint64_t queueMax = cfgOptionUInt64(cfgOptArchivePushQueueMax);
    uint64_t queueSize = 0;
    bool result = false;

    for (unsigned int processIdx = 0; processIdx < strLstSize(processList); processIdx++)
    {
        queueSize += storageInfoNP(
            storagePg(), strNewFmt("%s/%s", strPtr(walPath), strPtr(strLstGet(processList, processIdx)))).size;

        if (queueSize > queueMax)
        {
            result = true;
            break;
        }
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Get the list of WAL files ready to be pushed according to PostgreSQL
***********************************************************************************************************************************/
static StringList *
archivePushReadyList(const String *walPath)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walPath);
    FUNCTION_LOG_END();

    ASSERT(walPath != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strLstNew();

        // Read the ready files from the archive_status directory
        StringList *readyListRaw = strLstSort(
            storageListP(
                storagePg(), strNewFmt("%s/" PG_PATH_ARCHIVE_STATUS, strPtr(walPath)),
                .expression = STRDEF("\\" STATUS_EXT_READY "$"), .errorOnMissing = true),
            sortOrderAsc);

        for (unsigned int readyIdx = 0; readyIdx < strLstSize(readyListRaw); readyIdx++)
        {
            strLstAdd(
                result,
                strSubN(strLstGet(readyListRaw, readyIdx), 0, strSize(strLstGet(readyListRaw, readyIdx)) - STATUS_EXT_READY_SIZE));
        }

        strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Determine which WAL files need to be pushed to the archive when in async mode

This is the heart of the "look ahead" functionality in async archiving.  Any files in the out directory that do not end in ok are
removed and any ok files that do not have a corresponding ready file in archive_status (meaning it has been acknowledged by
PostgreSQL) are removed.  Then all ready files that do not have a corresponding ok file (meaning it has already been processed) are
returned for processing.
***********************************************************************************************************************************/
static StringList *
archivePushProcessList(const String *walPath)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walPath);
    FUNCTION_LOG_END();

    ASSERT(walPath != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the spool out path if it does not already exist
        storagePathCreateNP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT_STR);

        // Read the status files from the spool directory, then remove any files that do not end in ok and create a list of the
        // ok files for further processing
        StringList *statusList = strLstSort(
            storageListP(storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT_STR, .errorOnMissing = true), sortOrderAsc);

        StringList *okList = strLstNew();

        for (unsigned int statusIdx = 0; statusIdx < strLstSize(statusList); statusIdx++)
        {
            const String *statusFile = strLstGet(statusList, statusIdx);

            if (strEndsWithZ(statusFile, STATUS_EXT_OK))
                strLstAdd(okList, strSubN(statusFile, 0, strSize(statusFile) - STATUS_EXT_OK_SIZE));
            else
            {
                storageRemoveP(
                    storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s", strPtr(statusFile)), .errorOnMissing = true);
            }
        }

        // Read the ready files from the archive_status directory
        StringList *readyList = archivePushReadyList(walPath);

        // Remove ok files that are not in the ready list
        StringList *okRemoveList = strLstMergeAnti(okList, readyList);

        for (unsigned int okRemoveIdx = 0; okRemoveIdx < strLstSize(okRemoveList); okRemoveIdx++)
        {
            storageRemoveP(
                storageSpoolWrite(),
                strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s" STATUS_EXT_OK, strPtr(strLstGet(okRemoveList, okRemoveIdx))),
                .errorOnMissing = true);
        }

        // Return all ready files that are not in the ok list
        result = strLstMove(strLstMergeAnti(readyList, okList), MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Check that pg_control and archive.info match and get the archive id and archive cipher passphrase (if present)

As much information as possible is collected here so that async archiving has as little work as possible to do for each file.  Sync
archiving does not benefit but it makes sense to use the same function.
***********************************************************************************************************************************/
#define FUNCTION_LOG_ARCHIVE_PUSH_CHECK_RESULT_TYPE                                                                                \
    ArchivePushCheckResult
#define FUNCTION_LOG_ARCHIVE_PUSH_CHECK_RESULT_FORMAT(value, buffer, bufferSize)                                                   \
    objToLog(&value, "ArchivePushCheckResult", buffer, bufferSize)

typedef struct ArchivePushCheckResult
{
    unsigned int pgVersion;                                         // PostgreSQL version
    uint64_t pgSystemId;                                            // PostgreSQL system id
    unsigned int pgWalSegmentSize;                                  // PostgreSQL WAL segment size
    String *archiveId;                                              // Archive id for current pg version
    String *archiveCipherPass;                                      // Archive cipher passphrase
} ArchivePushCheckResult;

static ArchivePushCheckResult
archivePushCheck(CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ArchivePushCheckResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get info from pg_control
        PgControl controlInfo = pgControlFromFile(storagePg(), cfgOptionStr(cfgOptPgPath));

        // Attempt to load the archive info file
        InfoArchive *info = infoArchiveNewLoad(storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cipherType, cipherPass);

        // Get archive id for the most recent version -- archive-push will only operate against the most recent version
        String *archiveId = infoPgArchiveId(infoArchivePg(info), infoPgDataCurrentId(infoArchivePg(info)));
        InfoPgData archiveInfo = infoPgData(infoArchivePg(info), infoPgDataCurrentId(infoArchivePg(info)));

        // Ensure that the version and system identifier match
        if (controlInfo.version != archiveInfo.version || controlInfo.systemId != archiveInfo.systemId)
        {
            THROW_FMT(
                ArchiveMismatchError,
                "PostgreSQL version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64
                "\nHINT: are you archiving to the correct stanza?",
                strPtr(pgVersionToStr(controlInfo.version)), controlInfo.systemId, strPtr(pgVersionToStr(archiveInfo.version)),
                archiveInfo.systemId);
        }

        memContextSwitch(MEM_CONTEXT_OLD());
        result.pgVersion = controlInfo.version;
        result.pgSystemId = controlInfo.systemId;
        result.pgWalSegmentSize = controlInfo.walSegmentSize;
        result.archiveId = strDup(archiveId);
        result.archiveCipherPass = strDup(infoArchiveCipherPass(info));
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(ARCHIVE_PUSH_CHECK_RESULT, result);
}

/***********************************************************************************************************************************
Push a WAL segment to the repository
***********************************************************************************************************************************/
void
cmdArchivePush(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(cfgCommand() == cfgCmdArchivePush);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure there is a parameter to retrieve the WAL segment from
        const StringList *commandParam = cfgCommandParam();

        if (strLstSize(commandParam) != 1)
            THROW(ParamRequiredError, "WAL segment to push required");

        // Test for stop file
        lockStopTest();

        // Get the segment name
        String *walFile = walPath(strLstGet(commandParam, 0), cfgOptionStr(cfgOptPgPath), STR(cfgCommandName(cfgCommand())));
        String *archiveFile = strBase(walFile);

        if (cfgOptionBool(cfgOptArchiveAsync))
        {
            bool pushed = false;                                        // Has the WAL segment been pushed yet?
            bool forked = false;                                        // Has the async process been forked yet?
            bool confessOnError = false;                                // Should we confess errors?

            // Loop and wait for the WAL segment to be pushed
            Wait *wait = waitNew((TimeMSec)(cfgOptionDbl(cfgOptArchiveTimeout) * MSEC_PER_SEC));

            do
            {
                // Check if the WAL segment has been pushed.  Errors will not be confessed on the first try to allow the async
                // process a chance to fix them.
                pushed = archiveAsyncStatus(archiveModePush, archiveFile, confessOnError);

                // If the WAL segment has not already been pushed then start the async process to push it.  There's no point in
                // forking the async process off more than once so track that as well.  Use an archive lock to prevent more than
                // one async process being launched.
                if (!pushed && !forked &&
                    lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 0, false))
                {
                    // The async process should not output on the console at all
                    KeyValue *optionReplace = kvNew();

                    kvPut(optionReplace, VARSTR(CFGOPT_LOG_LEVEL_CONSOLE_STR), VARSTRDEF("off"));
                    kvPut(optionReplace, VARSTR(CFGOPT_LOG_LEVEL_STDERR_STR), VARSTRDEF("off"));

                    // Generate command options
                    StringList *commandExec = cfgExecParam(cfgCmdArchivePushAsync, optionReplace);
                    strLstInsert(commandExec, 0, cfgExe());
                    strLstAdd(commandExec, strPath(walFile));

                    // Release the lock so the child process can acquire it
                    lockRelease(true);

                    // Fork off the async process
                    if (forkSafe() == 0)
                    {
                        // Disable logging and close log file
                        logClose();

                        // Detach from parent process
                        forkDetach();

                        // Execute the binary.  This statement will not return if it is successful.
                        THROW_ON_SYS_ERROR(
                            execvp(strPtr(cfgExe()), (char ** const)strLstPtr(commandExec)) == -1,
                            ExecuteError, "unable to execute '" CFGCMD_ARCHIVE_PUSH_ASYNC "'");
                    }

                    // Mark the async process as forked so it doesn't get forked again.  A single run of the async process should be
                    // enough to do the job, running it again won't help anything.
                    forked = true;
                }

                // Now that the async process has been launched, confess any errors that are found
                confessOnError = true;
            }
            while (!pushed && waitMore(wait));

            // If the WAL segment was not pushed then error
            if (!pushed)
            {
                THROW_FMT(
                    ArchiveTimeoutError, "unable to push WAL file '%s' to the archive asynchronously after %lg second(s)",
                    strPtr(archiveFile), cfgOptionDbl(cfgOptArchiveTimeout));
            }

            // Log success
            LOG_INFO("pushed WAL file '%s' to the archive asynchronously", strPtr(archiveFile));
        }
        else
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down
            storageRepo();

            // Get archive info
            ArchivePushCheckResult archiveInfo = archivePushCheck(
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

            // Check if the push queue has been exceeded
            if (cfgOptionTest(cfgOptArchivePushQueueMax) &&
                archivePushDrop(strPath(walFile), archivePushReadyList(strPath(walFile))))
            {
                LOG_WARN(strPtr(archivePushDropWarning(archiveFile, cfgOptionUInt64(cfgOptArchivePushQueueMax))));
            }
            // Else push the file
            else
            {
                // Push the file to the archive
                String *warning = archivePushFile(
                    walFile, archiveInfo.archiveId, archiveInfo.pgVersion, archiveInfo.pgSystemId, archiveFile,
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), archiveInfo.archiveCipherPass,
                    cfgOptionBool(cfgOptCompress), cfgOptionInt(cfgOptCompressLevel));

                // If a warning was returned then log it
                if (warning != NULL)
                    LOG_WARN(strPtr(warning));

                // Log success
                LOG_INFO("pushed WAL file '%s' to the archive", strPtr(archiveFile));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Async version of archive push that runs in parallel for performance
***********************************************************************************************************************************/
void
cmdArchivePushAsync(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(cfgCommand() == cfgCmdArchivePushAsync);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure there is a parameter with the wal path
        const StringList *commandParam = cfgCommandParam();

        if (strLstSize(commandParam) != 1)
            THROW(ParamRequiredError, "WAL path to push required");

        const String *walPath = strLstGet(commandParam, 0);

        TRY_BEGIN()
        {
            // Test for stop file
            lockStopTest();

            // Get a list of WAL files that are ready for processing
            StringList *walFileList = archivePushProcessList(walPath);

            // The archive-push-async command should not have been called unless there are WAL files to process
            if (strLstSize(walFileList) == 0)
                THROW(AssertError, "no WAL files to process");

            LOG_INFO(
                "push %u WAL file(s) to archive: %s%s", strLstSize(walFileList), strPtr(strLstGet(walFileList, 0)),
                strLstSize(walFileList) == 1 ?
                    "" : strPtr(strNewFmt("...%s", strPtr(strLstGet(walFileList, strLstSize(walFileList) - 1)))));

            // Drop files if queue max has been exceeded
            if (cfgOptionTest(cfgOptArchivePushQueueMax) && archivePushDrop(walPath, walFileList))
            {
                for (unsigned int walFileIdx = 0; walFileIdx < strLstSize(walFileList); walFileIdx++)
                {
                    const String *walFile = strLstGet(walFileList, walFileIdx);
                    const String *warning = archivePushDropWarning(walFile, cfgOptionUInt64(cfgOptArchivePushQueueMax));

                    archiveAsyncStatusOkWrite(archiveModePush, walFile, warning);
                    LOG_WARN(strPtr(warning));
                }
            }
            // Else continue processing
            else
            {
                // Get the repo storage in case it is remote and encryption settings need to be pulled down
                storageRepo();

                // Get archive info
                ArchivePushCheckResult archiveInfo = archivePushCheck(
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

                // Create the parallel executor
                ProtocolParallel *parallelExec = protocolParallelNew(
                    (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * MSEC_PER_SEC) / 2);

                for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
                    protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, processIdx));

                // Queue jobs in executor
                for (unsigned int walFileIdx = 0; walFileIdx < strLstSize(walFileList); walFileIdx++)
                {
                    protocolKeepAlive();

                    const String *walFile = strLstGet(walFileList, walFileIdx);

                    ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_PUSH_STR);
                    protocolCommandParamAdd(command, VARSTR(strNewFmt("%s/%s", strPtr(walPath), strPtr(walFile))));
                    protocolCommandParamAdd(command, VARSTR(archiveInfo.archiveId));
                    protocolCommandParamAdd(command, VARUINT(archiveInfo.pgVersion));
                    protocolCommandParamAdd(command, VARUINT64(archiveInfo.pgSystemId));
                    protocolCommandParamAdd(command, VARSTR(walFile));
                    protocolCommandParamAdd(command, VARUINT(cipherType(cfgOptionStr(cfgOptRepoCipherType))));
                    protocolCommandParamAdd(command, VARSTR(archiveInfo.archiveCipherPass));
                    protocolCommandParamAdd(command, VARBOOL(cfgOptionBool(cfgOptCompress)));
                    protocolCommandParamAdd(command, VARINT(cfgOptionInt(cfgOptCompressLevel)));

                    protocolParallelJobAdd(parallelExec, protocolParallelJobNew(VARSTR(walFile), command));
                }

                // Process jobs
                do
                {
                    unsigned int completed = protocolParallelProcess(parallelExec);

                    for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                    {
                        protocolKeepAlive();

                        // Get the job and job key
                        ProtocolParallelJob *job = protocolParallelResult(parallelExec);
                        unsigned int processId = protocolParallelJobProcessId(job);
                        const String *walFile = varStr(protocolParallelJobKey(job));

                        // The job was successful
                        if (protocolParallelJobErrorCode(job) == 0)
                        {
                            LOG_DETAIL_PID(processId, "pushed WAL file '%s' to the archive", strPtr(walFile));
                            archiveAsyncStatusOkWrite(archiveModePush, walFile, varStr(protocolParallelJobResult(job)));
                        }
                        // Else the job errored
                        else
                        {
                            LOG_WARN_PID(
                                processId,
                                "could not push WAL file '%s' to the archive (will be retried): [%d] %s", strPtr(walFile),
                                protocolParallelJobErrorCode(job), strPtr(protocolParallelJobErrorMessage(job)));

                            archiveAsyncStatusErrorWrite(
                                archiveModePush, walFile, protocolParallelJobErrorCode(job), protocolParallelJobErrorMessage(job));
                        }
                    }
                }
                while (!protocolParallelDone(parallelExec));
            }
        }
        // On any global error write a single error file to cover all unprocessed files
        CATCH_ANY()
        {
            archiveAsyncStatusErrorWrite(archiveModePush, NULL, errorCode(), STR(errorMessage()));
            RETHROW();
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
