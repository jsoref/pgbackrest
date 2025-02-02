/***********************************************************************************************************************************
Archive Get Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "command/archive/get/file.h"
#include "command/archive/get/protocol.h"
#include "command/command.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/wait.h"
#include "config/config.h"
#include "config/exec.h"
#include "perl/exec.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Clean the queue and prepare a list of WAL segments that the async process should get
***********************************************************************************************************************************/
static StringList *
queueNeed(const String *walSegment, bool found, uint64_t queueSize, size_t walSegmentSize, unsigned int pgVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(BOOL, found);
        FUNCTION_LOG_PARAM(UINT64, queueSize);
        FUNCTION_LOG_PARAM(SIZE, walSegmentSize);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);

    StringList *result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Determine the first WAL segment for the async process to get.  If the WAL segment requested by
        // PostgreSQL was not found then use that.  If the segment was found but the queue is not full then
        // start with the next segment.
        const String *walSegmentFirst =
            found ? walSegmentNext(walSegment, walSegmentSize, pgVersion) : walSegment;

        // Determine how many WAL segments should be in the queue.  The queue total must be at least 2 or it doesn't make sense to
        // have async turned on at all.
        unsigned int walSegmentQueueTotal = (unsigned int)(queueSize / walSegmentSize);

        if (walSegmentQueueTotal < 2)
            walSegmentQueueTotal = 2;

        // Build the ideal queue -- the WAL segments we want in the queue after the async process has run
        StringList *idealQueue = walSegmentRange(walSegmentFirst, walSegmentSize, pgVersion, walSegmentQueueTotal);

        // Get the list of files actually in the queue
        StringList *actualQueue = strLstSort(
            storageListP(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN_STR, .errorOnMissing = true), sortOrderAsc);

        // Only preserve files that match the ideal queue. error/ok files are deleted so the async process can try again.
        RegExp *regExpPreserve = regExpNew(strNewFmt("^(%s)$", strPtr(strLstJoin(idealQueue, "|"))));

        // Build a list of WAL segments that are being kept so we can later make a list of what is needed
        StringList *keepQueue = strLstNew();

        for (unsigned int actualQueueIdx = 0; actualQueueIdx < strLstSize(actualQueue); actualQueueIdx++)
        {
            // Get file from actual queue
            const String *file = strLstGet(actualQueue, actualQueueIdx);

            // Does this match a file we want to preserve?
            if (regExpMatch(regExpPreserve, file))
                strLstAdd(keepQueue, file);

            // Else delete it
            else
                storageRemoveNP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(file)));
        }

        // Generate a list of the WAL that are needed by removing kept WAL from the ideal queue
        for (unsigned int idealQueueIdx = 0; idealQueueIdx < strLstSize(idealQueue); idealQueueIdx++)
        {
            if (!strLstExists(keepQueue, strLstGet(idealQueue, idealQueueIdx)))
                strLstAdd(result, strLstGet(idealQueue, idealQueueIdx));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Get an archive file from the repository (WAL segment, history file, etc.)
***********************************************************************************************************************************/
int
cmdArchiveGet(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Set the result assuming the archive file will not be found
    int result = 1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check the parameters
        const StringList *commandParam = cfgCommandParam();

        if (strLstSize(commandParam) != 2)
        {
            if (strLstSize(commandParam) == 0)
                THROW(ParamRequiredError, "WAL segment to get required");

            if (strLstSize(commandParam) == 1)
                THROW(ParamRequiredError, "path to copy WAL segment required");

            THROW(ParamInvalidError, "extra parameters found");
        }

        // Get the segment name
        String *walSegment = strBase(strLstGet(commandParam, 0));

        // Destination is wherever we were told to move the WAL segment
        const String *walDestination =
            walPath(strLstGet(commandParam, 1), cfgOptionStr(cfgOptPgPath), STR(cfgCommandName(cfgCommand())));

        // Async get can only be performed on WAL segments, history or other files must use synchronous mode
        if (cfgOptionBool(cfgOptArchiveAsync) && walIsSegment(walSegment))
        {
            bool found = false;                                         // Has the WAL segment been found yet?
            bool queueFull = false;                                     // Is the queue half or more full?
            bool forked = false;                                        // Has the async process been forked yet?
            bool confessOnError = false;                                // Should we confess errors?

            // Loop and wait for the WAL segment to be pushed
            Wait *wait = waitNew((TimeMSec)(cfgOptionDbl(cfgOptArchiveTimeout) * MSEC_PER_SEC));

            do
            {
                // Check for errors or missing files.  For archive-get ok indicates that the process succeeded but there is no WAL
                // file to download.
                if (archiveAsyncStatus(archiveModeGet, walSegment, confessOnError))
                {
                    storageRemoveP(
                        storageSpoolWrite(),
                        strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s" STATUS_EXT_OK, strPtr(walSegment)), .errorOnMissing = true);
                    break;
                }

                // Check if the WAL segment is already in the queue
                found = storageExistsNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment)));

                // If found then move the WAL segment to the destination directory
                if (found)
                {
                    // Source is the WAL segment in the spool queue
                    StorageRead *source = storageNewReadNP(
                        storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment)));

                    // A move will be attempted but if the spool queue and the WAL path are on different file systems then a copy
                    // will be performed instead.
                    //
                    // It looks scary that we are disabling syncs and atomicity (in case we need to copy intead of move) but this
                    // is safe because if the system crashes Postgres will not try to reuse a restored WAL segment but will instead
                    // request it again using the restore_command. In the case of a move this hardly matters since path syncs are
                    // cheap but if a copy is required we could save a lot of writes.
                    StorageWrite *destination = storageNewWriteP(
                        storageLocalWrite(), walDestination, .noCreatePath = true, .noSyncFile = true, .noSyncPath = true,
                        .noAtomic = true);

                    // Move (or copy if required) the file
                    storageMoveNP(storageSpoolWrite(), source, destination);

                    // Return success
                    result = 0;

                    // Get a list of WAL segments left in the queue
                    StringList *queue = storageListP(
                        storageSpool(), STORAGE_SPOOL_ARCHIVE_IN_STR, .expression = WAL_SEGMENT_REGEXP_STR, .errorOnMissing = true);

                    if (strLstSize(queue) > 0)
                    {
                        // Get size of the WAL segment
                        uint64_t walSegmentSize = storageInfoNP(storageLocal(), walDestination).size;

                        // Use WAL segment size to estimate queue size and determine if the async process should be launched
                        queueFull = strLstSize(queue) * walSegmentSize > cfgOptionUInt64(cfgOptArchiveGetQueueMax) / 2;
                    }
                }

                // If the WAL segment has not already been found then start the async process to get it.  There's no point in
                // forking the async process off more than once so track that as well.  Use an archive lock to prevent forking if
                // the async process was launched by another process.
                if (!forked && (!found || !queueFull)  &&
                    lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 0, false))
                {
                    // Get control info
                    PgControl pgControl = pgControlFromFile(storagePg(), cfgOptionStr(cfgOptPgPath));

                    // Create the queue
                    storagePathCreateNP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN_STR);

                    // The async process should not output on the console at all
                    KeyValue *optionReplace = kvNew();

                    kvPut(optionReplace, VARSTR(CFGOPT_LOG_LEVEL_CONSOLE_STR), VARSTRDEF("off"));
                    kvPut(optionReplace, VARSTR(CFGOPT_LOG_LEVEL_STDERR_STR), VARSTRDEF("off"));

                    // Generate command options
                    StringList *commandExec = cfgExecParam(cfgCmdArchiveGetAsync, optionReplace);
                    strLstInsert(commandExec, 0, cfgExe());

                    // Clean the current queue using the list of WAL that we ideally want in the queue.  queueNeed()
                    // will return the list of WAL needed to fill the queue and this will be passed to the async process.
                    const StringList *queue = queueNeed(
                        walSegment, found, cfgOptionUInt64(cfgOptArchiveGetQueueMax), pgControl.walSegmentSize,
                        pgControl.version);

                    for (unsigned int queueIdx = 0; queueIdx < strLstSize(queue); queueIdx++)
                        strLstAdd(commandExec, strLstGet(queue, queueIdx));

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
                            ExecuteError, "unable to execute '" CFGCMD_ARCHIVE_GET_ASYNC "'");
                    }

                    // Mark the async process as forked so it doesn't get forked again.  A single run of the async process should be
                    // enough to do the job, running it again won't help anything.
                    forked = true;
                }

                // Exit loop if WAL was found
                if (found)
                    break;

                // Now that the async process has been launched, confess any errors that are found
                confessOnError = true;
            }
            while (waitMore(wait));
        }
        // Else perform synchronous get
        else
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down
            storageRepo();

            // Get the archive file
            result = archiveGetFile(
                storageLocalWrite(), walSegment, walDestination, false, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
        }

        // Log whether or not the file was found
        if (result == 0)
            LOG_INFO("found %s in the archive", strPtr(walSegment));
        else
            LOG_INFO("unable to find %s in the archive", strPtr(walSegment));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
Async version of archive get that runs in parallel for performance
***********************************************************************************************************************************/
void
cmdArchiveGetAsync(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            // Check the parameters
            const StringList *walSegmentList = cfgCommandParam();

            if (strLstSize(walSegmentList) < 1)
                THROW(ParamInvalidError, "at least one wal segment is required");

            LOG_INFO(
                "get %u WAL file(s) from archive: %s%s", strLstSize(walSegmentList), strPtr(strLstGet(walSegmentList, 0)),
                strLstSize(walSegmentList) == 1 ?
                    "" : strPtr(strNewFmt("...%s", strPtr(strLstGet(walSegmentList, strLstSize(walSegmentList) - 1)))));

            // Create the parallel executor
            ProtocolParallel *parallelExec = protocolParallelNew(
                (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * MSEC_PER_SEC) / 2);

            for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
                protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, processIdx));

            // Queue jobs in executor
            for (unsigned int walSegmentIdx = 0; walSegmentIdx < strLstSize(walSegmentList); walSegmentIdx++)
            {
                const String *walSegment = strLstGet(walSegmentList, walSegmentIdx);

                ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_GET_STR);
                protocolCommandParamAdd(command, VARSTR(walSegment));

                protocolParallelJobAdd(parallelExec, protocolParallelJobNew(VARSTR(walSegment), command));
            }

            // Process jobs
            do
            {
                unsigned int completed = protocolParallelProcess(parallelExec);

                for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                {
                    // Get the job and job key
                    ProtocolParallelJob *job = protocolParallelResult(parallelExec);
                    unsigned int processId = protocolParallelJobProcessId(job);
                    const String *walSegment = varStr(protocolParallelJobKey(job));

                    // The job was successful
                    if (protocolParallelJobErrorCode(job) == 0)
                    {
                        // Get the archive file
                        if (varIntForce(protocolParallelJobResult(job)) == 0)
                        {
                            LOG_DETAIL_PID(processId, "found %s in the archive", strPtr(walSegment));
                        }
                        // If it does not exist write an ok file to indicate that it was checked
                        else
                        {
                            LOG_DETAIL_PID(processId, "unable to find %s in the archive", strPtr(walSegment));
                            archiveAsyncStatusOkWrite(archiveModeGet, walSegment, NULL);
                        }
                    }
                    // Else the job errored
                    else
                    {
                        LOG_WARN_PID(
                            processId,
                            "could not get %s from the archive (will be retried): [%d] %s", strPtr(walSegment),
                            protocolParallelJobErrorCode(job), strPtr(protocolParallelJobErrorMessage(job)));

                        archiveAsyncStatusErrorWrite(
                            archiveModeGet, walSegment, protocolParallelJobErrorCode(job), protocolParallelJobErrorMessage(job));
                    }
                }
            }
            while (!protocolParallelDone(parallelExec));
        }
        // On any global error write a single error file to cover all unprocessed files
        CATCH_ANY()
        {
            archiveAsyncStatusErrorWrite(archiveModeGet, NULL, errorCode(), STR(errorMessage()));
            RETHROW();
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
