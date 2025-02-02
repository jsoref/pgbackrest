/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPq.h"
#include "postgres/interface.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    String *stanza = strNew("db");
    String *fileName = strNew("test.info");
    String *backupStanzaPath = strNewFmt("repo/backup/%s", strPtr(stanza));
    String *backupInfoFileName = strNewFmt("%s/backup.info", strPtr(backupStanzaPath));
    String *archiveStanzaPath = strNewFmt("repo/archive/%s", strPtr(stanza));
    String *archiveInfoFileName = strNewFmt("%s/archive.info", strPtr(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "pgbackrest");
    strLstAddZ(argListBase, "--no-online");
    strLstAdd(argListBase, strNewFmt("--stanza=%s", strPtr(stanza)));
    strLstAdd(argListBase, strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaCreate(), infoValidate()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-host=/repo/not/local");
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdStanzaCreate(), HostInvalidError, "stanza-create command must be run on the repository host");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create the stop file
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_ERROR_FMT(cmdStanzaCreate(), StopError, "stop file exists for stanza %s", strPtr(stanza));
        TEST_RESULT_VOID(
            storageRemoveNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), "    remove the stop file");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - no files exist");

        String *contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "put archive info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        String *contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - files already exist and both are valid");
        harnessLogResult("P00   INFO: stanza 'db' already exists and is valid");

        // Remove backup.info
        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName, .errorOnMissing = true), "backup.info removed");
        TEST_RESULT_VOID(cmdStanzaCreate(), "    stanza create - success with archive.info files and only backup.info.copy");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))),
            true, "backup.info recreated from backup.info.copy");

        // Remove archive.info
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true), "archive.info removed");
        TEST_RESULT_VOID(cmdStanzaCreate(), "    stanza create - success with backup.info files and only archive.info.copy");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName))))),
            true, "archive.info recreated from archive.info.copy");

        // Remove info files
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true), "archive.info removed");
        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName, .errorOnMissing = true), "backup.info removed");
        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - success with copy files only");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)))))),
            true, "info files recreated from copy files");

        // Remove copy files
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)), .errorOnMissing = true),
            "archive.info.copy removed");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)), .errorOnMissing = true),
            "backup.info.copy removed");
        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - success with info files only");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)))))),
            true, "info files recreated from info files");

        // Errors
        //--------------------------------------------------------------------------------------------------------------------------
        // Archive files removed - backup.info and backup.info.copy exist
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)), .errorOnMissing = true),
            "archive.info.copy removed");
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true), "archive.info removed");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "backup.info exists but archive.info is missing\n"
            "HINT: this may be a symptom of repository corruption!");

        // Archive files removed - backup.info.copy exists
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)), .errorOnMissing = true),
            "backup.info.copy removed");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "backup.info exists but archive.info is missing\n"
            "HINT: this may be a symptom of repository corruption!");

        // Archive files removed - backup.info exists
        TEST_RESULT_VOID(
            storageMoveNP(storageTest,
                storageNewReadNP(storageTest, backupInfoFileName),
                storageNewWriteNP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)))),
            "backup.info moved to backup.info.copy");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "backup.info exists but archive.info is missing\n"
            "HINT: this may be a symptom of repository corruption!");

        // Backup files removed - archive.info file exists
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)), .errorOnMissing = true),
            "backup.info.copy removed");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "archive.info exists but backup.info is missing\n"
            "HINT: this may be a symptom of repository corruption!");

        // Backup files removed - archive.info.copy file exists
        TEST_RESULT_VOID(
            storageMoveNP(storageTest,
                storageNewReadNP(storageTest, archiveInfoFileName),
                storageNewWriteNP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)))),
                "archive.info moved to archive.info.copy");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "archive.info exists but backup.info is missing\n"
            "HINT: this may be a symptom of repository corruption!");

        // Backup files removed - archive.info and archive.info.copy exist
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "archive.info exists but backup.info is missing\n"
            "HINT: this may be a symptom of repository corruption!");

        // infoValidate()
        //--------------------------------------------------------------------------------------------------------------------------
        // Create a corrupted backup file - db id
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put back info to file - bad db-id");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        // Create a corrupted backup file - system id
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put back info to file - bad system-id");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        // Create a corrupted backup file - system id and version
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put back info to file - bad system-id and version");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        // Create a corrupted backup file - version
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put back info to file - bad version");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
        // Copy files may or may not exist - remove
        storageRemoveNP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)));
        storageRemoveNP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)));

        // Create an archive.info file that matches the backup.info file but does not match the current database version
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info file");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup and archive info files already exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");

        // Create archive.info and backup.info files that match but do not match the current database system-id
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put back info to file");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup and archive info files already exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");

        // Remove the info files and add sub directory to backup
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true), "archive.info removed");
        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName, .errorOnMissing = true), "backup.info removed");

        TEST_RESULT_VOID(
            storagePathCreateNP(storageTest, strNewFmt("%s/backup.history", strPtr(backupStanzaPath))),
            "create directory in backup");
        TEST_ERROR_FMT(cmdStanzaCreate(), PathNotEmptyError, "backup directory not empty");

        // File in archive, directory in backup
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("%s/somefile", strPtr(archiveStanzaPath))), BUFSTRDEF("some content")),
            "create file in archive");
        TEST_ERROR_FMT(cmdStanzaCreate(), PathNotEmptyError, "backup directory and/or archive directory not empty");

        // File in archive, backup empty
        TEST_RESULT_VOID(
            storagePathRemoveNP(storageTest, strNewFmt("%s/backup.history", strPtr(backupStanzaPath))), "remove backup subdir");
        TEST_ERROR_FMT(cmdStanzaCreate(), PathNotEmptyError, "archive directory not empty");

        // Repeat last test using --force (deprecated)
        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--force");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        TEST_ERROR_FMT(cmdStanzaCreate(), PathNotEmptyError, "archive directory not empty");
        harnessLogResult("P00   WARN: option --force is no longer supported");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgValidate(), online=y"))
    {
        String *pg1 = strNew("pg1");
        String *pg1Path = strNewFmt("%s/%s", testPath(), strPtr(pg1));

        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pg1Path)));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // pgControl and database match
        //--------------------------------------------------------------------------------------------------------------------------
        // Create pg_control
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", strPtr(pg1Path), false),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - db online");
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, strNewFmt("repo/archive/%s/archive.info", strPtr(stanza))), true, "    stanza created");

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", strPtr(pg1Path), false),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - db online");
        harnessLogResult("P00   INFO: stanza 'db' is already up to date");

        // Version mismatch
        //--------------------------------------------------------------------------------------------------------------------------
        // Create pg_control with different version
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_91, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", strPtr(pg1Path), false),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR_FMT(
            pgValidate(), DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s'"
            " read from '%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters",
            strPtr(pgVersionToStr(PG_VERSION_92)), strPtr(pg1Path), strPtr(pgVersionToStr(PG_VERSION_91)), strPtr(pg1Path),
            strPtr(pg1Path));

        // Path mismatch
        //--------------------------------------------------------------------------------------------------------------------------
        // Create pg_control
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", strPtr(strNewFmt("%s/pg2", testPath())), false),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR_FMT(
            pgValidate(), DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s'"
            " read from '%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL
            "'\nHINT: the pg1-path and pg1-port settings likely reference different clusters",
            strPtr(pgVersionToStr(PG_VERSION_92)), strPtr(strNewFmt("%s/pg2", testPath())), strPtr(pgVersionToStr(PG_VERSION_92)),
            strPtr(pg1Path), strPtr(pg1Path));

        // Primary at pg2
        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--pg2-path=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--pg2-port=5434");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control for master
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        // Create pg_control for standby
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, testPath())),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_94, .systemId = 6569239123849665700}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", testPath(), true),
            HRNPQ_MACRO_OPEN_92(2, "dbname='postgres' port=5434", strPtr(pg1Path), false),
            HRNPQ_MACRO_CLOSE(2),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        PgControl pgControl = {0};
        TEST_ASSIGN(pgControl, pgValidate(), "validate master on pg2");
        TEST_RESULT_UINT(pgControl.version, PG_VERSION_92, "    version set");
        TEST_RESULT_UINT(pgControl.systemId, 6569239123849665699, "    systemId set");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaCreate() - encryption"))
    {
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "12345678", true);
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - encryption");

        InfoArchive *infoArchive = NULL;
        TEST_ASSIGN(
            infoArchive, infoArchiveNewLoad(storageTest, archiveInfoFileName, cipherTypeAes256Cbc, strNew("12345678")),
            "  load archive info");
        TEST_RESULT_PTR_NE(infoArchiveCipherPass(infoArchive), NULL, "  cipher sub set");

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup, infoBackupNewLoad(storageTest, backupInfoFileName, cipherTypeAes256Cbc, strNew("12345678")),
            "  load backup info");
        TEST_RESULT_PTR_NE(infoBackupCipherPass(infoBackup), NULL, "  cipher sub set");

        TEST_RESULT_BOOL(
            strEq(infoArchiveCipherPass(infoArchive), infoBackupCipherPass(infoBackup)), false,
            "  cipher sub different for archive and backup");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaUpgrade()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-host=/repo/not/local");
        strLstAddZ(argList, "stanza-upgrade");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdStanzaUpgrade(), HostInvalidError, "stanza-upgrade command must be run on the repository host");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "stanza-upgrade");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create the stop file
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_ERROR_FMT(cmdStanzaUpgrade(), StopError, "stop file exists for stanza %s", strPtr(stanza));
        TEST_RESULT_VOID(
            storageRemoveNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), "    remove the stop file");

        //--------------------------------------------------------------------------------------------------------------------------
        // Load Parameters
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "stanza-upgrade");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - files already exist and both are valid");
        harnessLogResult("P00   INFO: stanza 'db' is already up to date");

        // Remove the copy files
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)), .errorOnMissing = true);
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)), .errorOnMissing = true);

        // backup info up to date but archive info db-id mismatch
        //--------------------------------------------------------------------------------------------------------------------------
        String *contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_ERROR_FMT(
            cmdStanzaUpgrade(), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        // backup info up to date but archive info version is not
        //--------------------------------------------------------------------------------------------------------------------------
        String *contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - version");
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "    put archive info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        // archive info up to date but backup info version is not
        //--------------------------------------------------------------------------------------------------------------------------
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - version");
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "    put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");

        // backup info up to date but archive info system-id is not
        //--------------------------------------------------------------------------------------------------------------------------
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - system-id");
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "    put archive info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        // archive info up to date but backup info system-id is not
        //--------------------------------------------------------------------------------------------------------------------------
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - system-id");
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "    put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaDelete(), stanzaDelete(), manifestDelete()"))
    {
        // Load Parameters
        StringList *argListCmd = strLstNew();
        strLstAddZ(argListCmd, "pgbackrest");
        strLstAdd(argListCmd, strNewFmt("--repo1-path=%s/repo", testPath()));

        StringList *argList = strLstDup(argListCmd);
        strLstAddZ(argList, "--repo1-host=/repo/not/local");
        strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
        strLstAddZ(argList,"stanza-delete");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdStanzaDelete(), HostInvalidError, "stanza-delete command must be run on the repository host");

        //--------------------------------------------------------------------------------------------------------------------------
        String *stanzaOther = strNew("otherstanza");

        // Load Parameters
        argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanzaOther)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanzaOther)));
        strLstAddZ(argList, "--no-online");
        strLstAddZ(argList,"stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control for stanza-create
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanzaOther))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "create a stanza that will not be deleted");

        argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
        strLstAddZ(argList,"stanza-delete");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // stanza already deleted
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - success on stanza does not exist");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, NULL), true, "    archiveList=NULL, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), NULL), true, "    archiveList=0, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, strLstNew()), true, "    archiveList=NULL, backupList=0");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), strLstNew()), true, "    archiveList=0, backupList=0");

        // create and delete a stanza
        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
        strLstAddZ(argList, "--no-online");
        strLstAddZ(argList,"stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control for stanza-create
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "create a stanza to be deleted");
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, strNewFmt("repo/archive/%s/archive.info", strPtr(stanza))), true, "    stanza created");

        argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
        strLstAddZ(argList,"stanza-delete");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdStanzaDelete(), FileMissingError, "stop file does not exist for stanza 'db'\n"
            "HINT: has the pgbackrest stop command been run on this server for this stanza?");

        // Create the stop file
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/archive/%s", strPtr(stanza))), false, "    stanza deleted");
        TEST_RESULT_BOOL(
            storageExistsNP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))), false, "    stop file removed");

        // Create stanza with directories only
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePathCreateNP(storageTest, strNewFmt("repo/archive/%s/9.6-1/1234567812345678", strPtr(stanza))),
            "create archive sub directory");
        TEST_RESULT_VOID(
            storagePathCreateNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strPtr(stanza))),
            "create backup sub directory");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - sub directories only");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/archive/%s", strPtr(stanza))), false, "    stanza archive deleted");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/backup/%s", strPtr(stanza))), false, "    stanza backup deleted");

        // Create stanza archive only
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/archive/%s/archive.info", strPtr(stanza))), BUFSTRDEF("")),
                "create archive.info");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - archive only");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/archive/%s", strPtr(stanza))), false, "    stanza deleted");

        // Create stanza backup only
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/backup.info", strPtr(stanza))), BUFSTRDEF("")),
                "create backup.info");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - backup only");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/backup/%s", strPtr(stanza))), false, "    stanza deleted");

        // Create a backup file that matches the regex for a backup directory
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strPtr(stanza))), BUFSTRDEF("")),
                "backup file that looks like a directory");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_ERROR_FMT(
            cmdStanzaDelete(), FileRemoveError,
            "unable to remove '%s/repo/backup/%s/20190708-154306F/backup.manifest': [20] Not a directory", testPath(),
            strPtr(stanza));
        TEST_RESULT_VOID(
            storageRemoveNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strPtr(stanza))), "remove backup directory");

        // Create backup manifest
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F/backup.manifest", strPtr(stanza))),
                BUFSTRDEF("")), "create backup.manifest only");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191726I/backup.manifest.copy",
                strPtr(stanza))), BUFSTRDEF("")), "create backup.manifest.copy only");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest",
                strPtr(stanza))), BUFSTRDEF("")), "create backup.manifest");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest.copy",
                strPtr(stanza))), BUFSTRDEF("")), "create backup.manifest.copy");
        TEST_RESULT_VOID(manifestDelete(storageRepoWrite()), "delete manifests");
        TEST_RESULT_BOOL(
            (storageExistsNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F/backup.manifest", strPtr(stanza))) &&
            storageExistsNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191726I/backup.manifest.copy",
            strPtr(stanza))) &&
            storageExistsNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest",
            strPtr(stanza))) &&
            storageExistsNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest.copy",
            strPtr(stanza)))), false, "    all manifests deleted");

        // Create only stanza paths
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete");
        TEST_RESULT_VOID(
            storagePathCreateNP(storageTest, strNewFmt("repo/archive/%s", strPtr(stanza))), "create empty stanza archive path");
        TEST_RESULT_VOID(
            storagePathCreateNP(storageTest, strNewFmt("repo/backup/%s", strPtr(stanza))), "create empty stanza backup path");
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - empty directories");

        // Create postmaster.pid
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/backup.info", strPtr(stanza))), BUFSTRDEF("")),
                "create backup.info");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/" PG_FILE_POSTMASTERPID, strPtr(stanza))), BUFSTRDEF("")),
            "create postmaster file");
        TEST_ERROR_FMT(
            cmdStanzaDelete(), PostmasterRunningError, PG_FILE_POSTMASTERPID " exists - looks like the postmaster is running. "
            "To delete stanza 'db', shutdown the postmaster for stanza 'db' and try again, or use --force.");

        // Force deletion
        strLstAddZ(argList,"--force");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete --force");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/backup/%s", strPtr(stanza))), false, "    stanza deleted");

        // Ensure other stanza never deleted
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, strNewFmt("repo/archive/%s/archive.info", strPtr(stanzaOther))), true,
            "otherstanza exists");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
