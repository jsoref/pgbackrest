/***********************************************************************************************************************************
Test Configuration Load
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/log.h"
#include "version.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Expose log internal data for unit testing/debugging
***********************************************************************************************************************************/
extern LogLevel logLevelStdOut;
extern LogLevel logLevelStdErr;
extern LogLevel logLevelFile;
extern bool logTimestamp;

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cfgLoadLogSetting()"))
    {
        cfgInit();

        TEST_RESULT_VOID(cfgLoadLogSetting(), "load log settings all defaults");

        TEST_RESULT_INT(logLevelStdOut, logLevelOff, "console logging is off");
        TEST_RESULT_INT(logLevelStdErr, logLevelOff, "stderr logging is off");
        TEST_RESULT_INT(logLevelFile, logLevelOff, "file logging is off");
        TEST_RESULT_BOOL(logTimestamp, true, "timestamp logging is on");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdLocal);

        cfgOptionValidSet(cfgOptLogLevelConsole, true);
        cfgOptionSet(cfgOptLogLevelConsole, cfgSourceParam, varNewStrZ("info"));
        cfgOptionValidSet(cfgOptLogLevelStderr, true);
        cfgOptionSet(cfgOptLogLevelStderr, cfgSourceParam, varNewStrZ("error"));
        cfgOptionValidSet(cfgOptLogLevelFile, true);
        cfgOptionSet(cfgOptLogLevelFile, cfgSourceParam, varNewStrZ("debug"));
        cfgOptionValidSet(cfgOptLogTimestamp, true);
        cfgOptionSet(cfgOptLogTimestamp, cfgSourceParam, varNewBool(false));

        TEST_RESULT_VOID(cfgLoadLogSetting(), "load log settings no defaults");
        TEST_RESULT_INT(logLevelStdOut, logLevelInfo, "console logging is info");
        TEST_RESULT_INT(logLevelStdErr, logLevelError, "stderr logging is error");
        TEST_RESULT_INT(logLevelFile, logLevelDebug, "file logging is debugging");
        TEST_RESULT_BOOL(logTimestamp, false, "timestamp logging is off");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdLocal);

        cfgOptionValidSet(cfgOptLogLevelStderr, true);
        cfgOptionSet(cfgOptLogLevelStderr, cfgSourceParam, varNewStrZ("info"));

        TEST_RESULT_VOID(cfgLoadLogSetting(), "load log settings reset stderr");

        TEST_RESULT_INT(logLevelStdErr, logLevelError, "stderr logging is error");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgLoadUpdateOption()"))
    {
        String *exe = strNew("/path/to/pgbackrest");
        String *exeOther = strNew("/other/path/to/pgbackrest");

        cfgInit();
        cfgCommandSet(cfgCmdBackup);
        cfgExeSet(exe);

        cfgOptionValidSet(cfgOptRepoHost, true);
        cfgOptionValidSet(cfgOptPgHost, true);

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "hosts are not set so don't update commands");

        cfgOptionSet(cfgOptRepoHost, cfgSourceParam, varNewStrZ("repo-host"));

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "repo remote command is updated");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptRepoHostCmd)), strPtr(exe), "    check repo1-host-cmd");

        cfgOptionSet(cfgOptRepoHostCmd, cfgSourceParam, varNewStr(exeOther));

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "repo remote command was already set");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptRepoHostCmd)), strPtr(exeOther), "    check repo1-host-cmd");

        cfgOptionSet(cfgOptRepoHost, cfgSourceParam, NULL);

        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionValidSet(cfgOptPgHostCmd, true);
        cfgOptionSet(cfgOptPgHost, cfgSourceParam, varNewStrZ("pg1-host"));

        cfgOptionValidSet(cfgOptPgHost + 1, true);
        cfgOptionSet(cfgOptPgHost + 1, cfgSourceParam, varNewStrZ("pg2-host"));
        cfgOptionValidSet(cfgOptPgHostCmd + 1, true);
        cfgOptionSet(cfgOptPgHostCmd + 1, cfgSourceParam, varNewStr(exeOther));

        cfgOptionValidSet(cfgOptPgHost + cfgOptionIndexTotal(cfgOptPgHost) - 1, true);
        cfgOptionSet(cfgOptPgHost + cfgOptionIndexTotal(cfgOptPgHost) - 1, cfgSourceParam, varNewStrZ("pgX-host"));

        TEST_RESULT_VOID(cfgLoadUpdateOption(), "pg remote command is updated");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptPgHostCmd)), strPtr(exe), "    check pg1-host-cmd");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptPgHostCmd + 1)), strPtr(exeOther), "    check pg2-host-cmd is already set");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptPgHostCmd + 2)), NULL, "    check pg3-host-cmd is not set");
        TEST_RESULT_STR(
            strPtr(cfgOptionStr(cfgOptPgHostCmd + cfgOptionIndexTotal(cfgOptPgHost) - 1)), strPtr(exe), "    check pgX-host-cmd");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();

        cfgOptionValidSet(cfgOptDbTimeout, true);
        cfgOptionSet(cfgOptDbTimeout, cfgSourceParam, varNewDbl(100));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "pg timeout set but not protocol timeout");

        cfgOptionValidSet(cfgOptProtocolTimeout, true);
        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceDefault, varNewDbl(101));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "protocol timeout > pg timeout");

        cfgOptionSet(cfgOptDbTimeout, cfgSourceParam, varNewDbl(100000));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "protocol timeout set automatically");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 100030, "    check protocol timeout");

        cfgOptionValidSet(cfgOptProtocolTimeout, true);
        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(50.5));
        TEST_ERROR(
            cfgLoadUpdateOption(), OptionInvalidValueError,
            "'50.5' is not valid for 'protocol-timeout' option\n"
                "HINT 'protocol-timeout' option (50.5) should be greater than 'db-timeout' option (100000).");

        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(45));
        cfgOptionSet(cfgOptDbTimeout, cfgSourceDefault, varNewDbl(3600));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "set default pg timeout to be less than protocol timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 45, "    check protocol timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptDbTimeout), 15, "    check db timeout");

        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(11));
        cfgOptionSet(cfgOptDbTimeout, cfgSourceDefault, varNewDbl(3600));
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "set default pg timeout to be less than test protocol timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 11, "    check protocol timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptDbTimeout), 5.5, "    check db timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdBackup);
        cfgExeSet(exe);

        cfgOptionValidSet(cfgOptPgHost, true);
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "only repo-host is valid");

        cfgOptionValidSet(cfgOptRepoHost, true);
        cfgOptionSet(cfgOptRepoHost, cfgSourceParam, varNewStrZ("repo-host"));
        cfgOptionValidSet(cfgOptPgHost + 4, true);
        cfgOptionSet(cfgOptPgHost + 4, cfgSourceParam, varNewStrZ("pg5-host"));
        TEST_ERROR(cfgLoadUpdateOption(), ConfigError, "pg and repo hosts cannot both be configured as remote");

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("help"));
        strLstAdd(argList, strNew("backup"));
        strLstAdd(argList, strNew("process-max"));

        harnessLogLevelSet(logLevelWarn);
        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "load help config -- no retention warning");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    command is help");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("expire"));

        harnessLogLevelSet(logLevelWarn);
        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option"
                " 'repo1-retention-full' to the maximum.");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        strLstAdd(argList, strNew("--repo1-retention-full=1"));

        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "load config no retention warning");
        TEST_RESULT_INT(cfgOptionInt(cfgOptRepoRetentionArchive), 1, "    repo1-retention-archive set");

        // Munge repo-type for coverage.  This will go away when there are multiple repos.
        cfgOptionSet(cfgOptRepoType, cfgSourceParam, NULL);
        TEST_RESULT_VOID(cfgLoadUpdateOption(), "load config no repo-type");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("--repo1-retention-archive-type=incr"));
        strLstAdd(argList, strNew("expire"));

        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
                "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full'"
                " to the maximum.\n"
            "P00   WARN: WAL segments will not be expired: option 'repo1-retention-archive-type=incr' but option"
                " 'repo1-retention-archive' is not set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("--repo1-retention-archive-type=diff"));
        strLstAdd(argList, strNew("expire"));

        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option"
                " 'repo1-retention-full' to the maximum.\n"
            "P00   WARN: WAL segments will not be expired: option 'repo1-retention-archive-type=diff' but neither option"
                " 'repo1-retention-archive' nor option 'repo1-retention-diff' is set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptRepoRetentionArchive), false, "    repo1-retention-archive not set");

        strLstAdd(argList, strNew("--repo1-retention-diff=2"));

        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option"
                " 'repo1-retention-full' to the maximum.");
        TEST_RESULT_INT(cfgOptionInt(cfgOptRepoRetentionArchive), 2, "    repo1-retention-archive set to retention-diff");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("--repo1-retention-archive-type=diff"));
        strLstAdd(argList, strNew("--repo1-retention-archive=3"));
        strLstAdd(argList, strNew("--repo1-retention-full=1"));
        strLstAdd(argList, strNew("expire"));

        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for retention warning");
        harnessLogResult(
            "P00   WARN: option 'repo1-retention-diff' is not set for 'repo1-retention-archive-type=diff'\n"
            "            HINT: to retain differential backups indefinitely (without warning), set option 'repo1-retention-diff'"
                " to the maximum.");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-log-timestamp"));
        strLstAdd(argList, strNew("--repo1-retention-archive-type=diff"));
        strLstAdd(argList, strNew("--repo1-retention-archive=3"));
        strLstAdd(argList, strNew("--repo1-retention-diff=2"));
        strLstAdd(argList, strNew("--repo1-retention-full=1"));
        strLstAdd(argList, strNew("expire"));

        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "load config with success");

        // -------------------------------------------------------------------------------------------------------------------------
        setenv("PGBACKREST_REPO1_S3_KEY", "mykey", true);
        setenv("PGBACKREST_REPO1_S3_KEY_SECRET", "mysecretkey", true);

        // Invalid bucket name with verification enabled fails
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-type=s3"));
        strLstAdd(argList, strNew("--repo1-s3-bucket=bogus.bucket"));
        strLstAdd(argList, strNew("--repo1-s3-region=region"));
        strLstAdd(argList, strNew("--repo1-s3-endpoint=endpoint"));
        strLstAdd(argList, strNew("--repo1-path=/repo"));
        strLstAdd(argList, strNew("archive-get"));

        TEST_ERROR(
            harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), OptionInvalidValueError,
            "'bogus.bucket' is not valid for option 'repo1-s3-bucket'"
                "\nHINT: RFC-2818 forbids dots in wildcard matches"
                "\nHINT: TLS/SSL verification cannot proceed with this bucket name"
                "\nHINT: remove dots from the bucket name");

        // Invalid bucket name with verification disabled succeeds
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-type=s3"));
        strLstAdd(argList, strNew("--repo1-s3-bucket=bogus.bucket"));
        strLstAdd(argList, strNew("--repo1-s3-region=region"));
        strLstAdd(argList, strNew("--repo1-s3-endpoint=endpoint"));
        strLstAdd(argList, strNew("--no-repo1-s3-verify-ssl"));
        strLstAdd(argList, strNew("--repo1-path=/repo"));
        strLstAdd(argList, strNew("archive-get"));

        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "invalid bucket with no verification");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptRepoS3Bucket)), "bogus.bucket", "    check bucket value");

        // Valid bucket name
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-type=s3"));
        strLstAdd(argList, strNew("--repo1-s3-bucket=cool-bucket"));
        strLstAdd(argList, strNew("--repo1-s3-region=region"));
        strLstAdd(argList, strNew("--repo1-s3-endpoint=endpoint"));
        strLstAdd(argList, strNew("--repo1-path=/repo"));
        strLstAdd(argList, strNew("archive-get"));

        TEST_RESULT_VOID(harnessCfgLoad(strLstSize(argList), strLstPtr(argList)), "valid bucket name");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptRepoS3Bucket)), "cool-bucket", "    check bucket value");

        unsetenv("PGBACKREST_REPO1_S3_KEY");
        unsetenv("PGBACKREST_REPO1_S3_KEY_SECRET");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgLoadLogFile()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=/path"));
        strLstAdd(argList, strNew("--log-path=/bogus"));
        strLstAdd(argList, strNew("--log-level-file=info"));
        strLstAdd(argList, strNew("backup"));
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for backup");
        lockRelease(true);

        // On the error case is tested here, success is tested in cfgLoad()
        TEST_RESULT_VOID(cfgLoadLogFile(), "attempt to open bogus log file");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptLogLevelFile)), "off", "log-level-file should now be off");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgLoad()"))
    {
        // Command does not have umask
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("info"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config and don't set umask");

        // Set a distinct umask value and test that the umask is reset by configLoad since default for neutral-umask=y
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=off"));
        strLstAdd(argList, strNew("archive-get"));

        umask(0111);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for neutral-umask");
        TEST_RESULT_INT(umask(0111), 0000, "    umask was reset");

        // Set a distinct umask value and test that the umask is not reset by configLoad with option --no-neutral-umask
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-neutral-umask"));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=off"));
        strLstAdd(argList, strNew("archive-get"));

        umask(0111);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for no-neutral-umask");
        TEST_RESULT_INT(umask(0), 0111, "    umask was not reset");

        // No command
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "no command");

        // Help command only
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("help"));

        ioBufferSizeSet(333);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "help command");
        TEST_RESULT_SIZE(ioBufferSize(), 333, "buffer size not updated by help command");

        // Help command for backup
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("help"));
        strLstAdd(argList, strNew("backup"));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=off"));
        strLstAdd(argList, strNew("--repo1-retention-full=2"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "help command for backup");
        TEST_RESULT_SIZE(ioBufferSize(), 4 * 1024 * 1024, "buffer size set to option default");

        // Command takes lock and opens log file
        // -------------------------------------------------------------------------------------------------------------------------
        struct stat statLog;

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=/path"));
        strLstAdd(argList, strNew("--repo1-retention-full=1"));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=warn"));
        strLstAdd(argList, strNew("backup"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "lock and open log file");
        TEST_RESULT_INT(lstat(strPtr(strNewFmt("%s/db-backup.log", testPath())), &statLog), 0, "   check log file exists");

        // Local command opens log file with special filename
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--command=backup"));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAdd(argList, strNew("--process=1"));
        strLstAdd(argList, strNew("--host-id=1"));
        strLstAdd(argList, strNew("--type=backup"));
        strLstAdd(argList, strNew("--log-level-file=warn"));
        strLstAdd(argList, strNew("local"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(
            lstat(strPtr(strNewFmt("%s/db-backup-local-001.log", testPath())), &statLog), 0, "   check log file exists");

        // Remote command opens log file with special filename
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--command=backup"));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAdd(argList, strNew("--type=backup"));
        strLstAdd(argList, strNew("--log-level-file=warn"));
        strLstAdd(argList, strNew("--process=0"));
        strLstAdd(argList, strNew("remote"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "open log file");
        TEST_RESULT_INT(
            lstat(strPtr(strNewFmt("%s/all-backup-remote-000.log", testPath())), &statLog), 0, "   check log file exists");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
