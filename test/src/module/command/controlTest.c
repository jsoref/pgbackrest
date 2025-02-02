/***********************************************************************************************************************************
Test Command Control
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("lockStopFileName()"))
    {
        // Load configuration so lock path is set
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--lock-path=/path/to/lock");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR(strPtr(lockStopFileName(NULL)), "/path/to/lock/all" STOP_FILE_EXT, "stop file for all stanzas");
        TEST_RESULT_STR(strPtr(lockStopFileName(strNew("db"))), "/path/to/lock/db" STOP_FILE_EXT, "stop file for on stanza");
    }

    // *****************************************************************************************************************************
    if (testBegin("lockStopTest(), cmdStart()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--lock-path=%s", testPath()));
        strLstAddZ(argList, "start");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(lockStopTest(), "no stop files without stanza");
        TEST_RESULT_VOID(cmdStart(), "    cmdStart - no stanza, no stop files");
        harnessLogResult("P00   WARN: stop file does not exist");

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, strNew("all" STOP_FILE_EXT)), NULL), "create stop file");
        TEST_RESULT_VOID(cmdStart(), "    cmdStart - no stanza, stop file exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNew("all" STOP_FILE_EXT)), false, "    stop file removed");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--lock-path=%s", testPath()));
        strLstAddZ(argList, "start");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(lockStopTest(), "no stop files with stanza");
        TEST_RESULT_VOID(cmdStart(), "    cmdStart - stanza, no stop files");
        harnessLogResult("P00   WARN: stop file does not exist for stanza db");

        storagePutNP(storageNewWriteNP(storageTest, strNew("all" STOP_FILE_EXT)), NULL);
        TEST_ERROR(lockStopTest(), StopError, "stop file exists for all stanzas");

        storagePutNP(storageNewWriteNP(storageTest, strNew("db" STOP_FILE_EXT)), NULL);
        TEST_ERROR(lockStopTest(), StopError, "stop file exists for stanza db");

        TEST_RESULT_VOID(cmdStart(), "cmdStart - stanza, stop file exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNew("db" STOP_FILE_EXT)), false, "    stanza stop file removed");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNew("all" STOP_FILE_EXT)), true, "    all stop file not removed");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStop()"))
    {
        String *lockPath = strNewFmt("%s/lockpath", testPath());
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--lock-path=%s", strPtr(lockPath)));
        strLstAddZ(argList, "stop");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdStop(), "no stanza, create stop file");
        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoNP(storageTest, lockPath), "    get path info");
        TEST_RESULT_INT(info.mode, 0770, "    check path mode");
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, strNewFmt("%s/all" STOP_FILE_EXT, strPtr(lockPath))), true, "    all stop file created");
        TEST_ASSIGN(info, storageInfoNP(storageTest, strNewFmt("%s/all" STOP_FILE_EXT, strPtr(lockPath))), "    get file info");
        TEST_RESULT_INT(info.mode, 0640, "    check file mode");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStop(), "no stanza, stop file already exists");
        harnessLogResult("P00   WARN: stop file already exists for all stanzas");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveNP(storageTest, strNew("lockpath/all" STOP_FILE_EXT)), "remove stop file");
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chmod 444 %s", strPtr(lockPath)))), 0, "change perms");
        TEST_ERROR_FMT(
            cmdStop(), FileOpenError, "unable to stat '%s/all.stop': [13] Permission denied", strPtr(lockPath));
        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, lockPath, .recurse = true, .errorOnMissing = true), "    remove the lock path");

        // -------------------------------------------------------------------------------------------------------------------------
        String *stanzaStopFile = strNewFmt("%s/db" STOP_FILE_EXT, strPtr(lockPath));
        strLstAddZ(argList, "--stanza=db");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdStop(), "stanza, create stop file");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, stanzaStopFile), true, "    stanza stop file created");

        StringList *lockPathList = NULL;
        TEST_ASSIGN(lockPathList, storageListP(storageTest, strNew("lockpath"), .errorOnMissing = true), "    get file list");
        TEST_RESULT_INT(strLstSize(lockPathList), 1, "    only file in lock path");
        TEST_RESULT_STR(strPtr(strLstGet(lockPathList, 0)), "db" STOP_FILE_EXT, "    stanza stop exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStop(), "stanza, stop file already exists");
        harnessLogResult("P00   WARN: stop file already exists for stanza db");
        TEST_RESULT_VOID(storageRemoveNP(storageTest, stanzaStopFile), "    remove stop file");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--force");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        TEST_RESULT_VOID(cmdStop(), "stanza, create stop file, force");
        TEST_RESULT_VOID(storageRemoveNP(storageTest, stanzaStopFile), "    remove stop file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteP(storageTest, strNewFmt("%s/bad" LOCK_FILE_EXT, strPtr(lockPath)), .modeFile = 0222), NULL),
            "create a lock file that cannot be opened");
        TEST_RESULT_VOID(cmdStop(), "    stanza, create stop file but unable to open lock file");
        harnessLogResult(strPtr(strNewFmt("P00   WARN: unable to open lock file %s/bad" LOCK_FILE_EXT, strPtr(lockPath))));
        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, lockPath, .recurse = true, .errorOnMissing = true), "    remove the lock path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/empty" LOCK_FILE_EXT, strPtr(lockPath))), NULL),
            "create empty lock file");
        TEST_RESULT_VOID(cmdStop(), "    stanza, create stop file, force - empty lock file");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, stanzaStopFile), true, "    stanza stop file created");
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, strNewFmt("%s/empty" LOCK_FILE_EXT, strPtr(lockPath))), false,
            "    no other process lock, lock file was removed");

        // empty lock file with another process lock, processId == NULL
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveNP(storageTest, stanzaStopFile), "remove stop file");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/empty" LOCK_FILE_EXT, strPtr(lockPath))), NULL),
            "    create empty lock file");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioHandleReadNew(strNew("child read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("child write"), HARNESS_FORK_CHILD_WRITE());
                ioWriteOpen(write);

                int lockHandle = open(strPtr(strNewFmt("%s/empty" LOCK_FILE_EXT, strPtr(lockPath))), O_RDONLY, 0);
                TEST_RESULT_BOOL(lockHandle != -1, true, "    file handle aquired");
                TEST_RESULT_INT(flock(lockHandle, LOCK_EX | LOCK_NB), 0, "    lock the empty file");

                // Let the parent know the lock has been acquired and wait for the parent to allow lock release
                ioWriteStrLine(write, strNew(""));
                // All writes are buffered so need to flush because buffer is not full
                ioWriteFlush(write);
                // Wait for a linefeed from the parent ioWriteLine below
                ioReadLine(read);

                // Parent remove the file so just close the handle
                close(lockHandle);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("parent read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("parent write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                // Wait for the child to acquire the lock
                ioReadLine(read);

                TEST_RESULT_VOID(
                    cmdStop(),
                    "    stanza, create stop file, force - empty lock file with another process lock, processId == NULL");
                TEST_RESULT_BOOL(
                    storageExistsNP(storageTest, strNewFmt("%s/empty" LOCK_FILE_EXT, strPtr(lockPath))), false,
                    "    lock file was removed");

                // Notify the child to release the lock
                ioWriteLine(write, bufNew(0));
                ioWriteFlush(write);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // not empty lock file with another process lock, processId size trimmed to 0
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveNP(storageTest, stanzaStopFile), "remove stop file");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/empty" LOCK_FILE_EXT, strPtr(lockPath))), BUFSTRDEF(" ")),
            "    create non-empty lock file");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioHandleReadNew(strNew("child read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("child write"), HARNESS_FORK_CHILD_WRITE());
                ioWriteOpen(write);

                int lockHandle = open(strPtr(strNewFmt("%s/empty" LOCK_FILE_EXT, strPtr(lockPath))), O_RDONLY, 0);
                TEST_RESULT_BOOL(lockHandle != -1, true, "    file handle aquired");
                TEST_RESULT_INT(flock(lockHandle, LOCK_EX | LOCK_NB), 0, "    lock the non-empty file");

                // Let the parent know the lock has been acquired and wait for the parent to allow lock release
                ioWriteStrLine(write, strNew(""));
                // All writes are buffered so need to flush because buffer is not full
                ioWriteFlush(write);
                // Wait for a linefeed from the parent ioWriteLine below
                ioReadLine(read);

                // Parent remove the file so just close the handle
                close(lockHandle);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("parent read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("parent write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                // Wait for the child to acquire the lock
                ioReadLine(read);

                TEST_RESULT_VOID(
                    cmdStop(), "    stanza, create stop file, force - empty lock file with another process lock, processId size 0");
                TEST_RESULT_BOOL(
                    storageExistsNP(storageTest, strNewFmt("%s/empty" LOCK_FILE_EXT, strPtr(lockPath))), false,
                    "    lock file was removed");

                // Notify the child to release the lock
                ioWriteLine(write, bufNew(0));
                ioWriteFlush(write);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // lock file with another process lock, processId is valid
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveNP(storageTest, stanzaStopFile), "remove stop file");
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioHandleReadNew(strNew("child read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("child write"), HARNESS_FORK_CHILD_WRITE());
                ioWriteOpen(write);

                TEST_RESULT_BOOL(
                    lockAcquire(lockPath, cfgOptionStr(cfgOptStanza), 0, 30000, true), true,"    child process aquires lock");

                // Let the parent know the lock has been acquired and wait for the parent to allow lock release
                ioWriteStrLine(write, strNew(""));
                // All writes are buffered so need to flush because buffer is not full
                ioWriteFlush(write);
                // Wait for a linefeed from the parent but it will not arrive before the process is terminated
                ioReadLine(read);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("parent read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("parent write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                // Wait for the child to acquire the lock
                ioReadLine(read);

                TEST_RESULT_VOID(
                    cmdStop(),
                    "    stanza, create stop file, force - lock file with another process lock, processId is valid");

                harnessLogResult(strPtr(strNewFmt("P00   INFO: sent term signal to process %d", HARNESS_FORK_PROCESS_ID(0))));
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // lock file with another process lock, processId is invalid
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveNP(storageTest, stanzaStopFile), "remove stop file");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("%s/badpid" LOCK_FILE_EXT, strPtr(lockPath))), BUFSTRDEF("-32768")),
            "create lock file with invalid PID");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioHandleReadNew(strNew("child read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("child write"), HARNESS_FORK_CHILD_WRITE());
                ioWriteOpen(write);

                int lockHandle = open(strPtr(strNewFmt("%s/badpid" LOCK_FILE_EXT, strPtr(lockPath))), O_RDONLY, 0);
                TEST_RESULT_BOOL(lockHandle != -1, true, "    file handle aquired");
                TEST_RESULT_INT(flock(lockHandle, LOCK_EX | LOCK_NB), 0, "    lock the badpid file");

                // Let the parent know the lock has been acquired and wait for the parent to allow lock release
                ioWriteStrLine(write, strNew(""));
                // All writes are buffered so need to flush because buffer is not full
                ioWriteFlush(write);
                // Wait for a linefeed from the parent ioWriteLine below
                ioReadLine(read);

                // Remove the file and close the handle
                storageRemoveNP(storageTest, strNewFmt("%s/badpid" LOCK_FILE_EXT, strPtr(lockPath)));
                close(lockHandle);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("parent read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("parent write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                // Wait for the child to acquire the lock
                ioReadLine(read);

                TEST_RESULT_VOID(
                    cmdStop(), "    stanza, create stop file, force - lock file with another process lock, processId is invalid");
                harnessLogResult("P00   WARN: unable to send term signal to process -32768");
                TEST_RESULT_BOOL(storageExistsNP(storageTest, stanzaStopFile), true, "    stanza stop file not removed");

                // Notify the child to release the lock
                ioWriteLine(write, bufNew(0));
                ioWriteFlush(write);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
