/***********************************************************************************************************************************
Info Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "command/info/info.h"
#include "common/debug.h"
#include "common/io/handleWrite.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "common/crypto/common.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "perl/exec.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_STATIC(CFGOPTVAL_INFO_OUTPUT_TEXT_STR,                       "text");

// Naming convention: <sectionname>_KEY_<keyname>_VAR. If the key exists in multiple sections, then <sectionname>_ is omitted.
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MIN_VAR,                          "min");
VARIANT_STRDEF_STATIC(ARCHIVE_KEY_MAX_VAR,                          "max");
VARIANT_STRDEF_STATIC(BACKREST_KEY_FORMAT_VAR,                      "format");
VARIANT_STRDEF_STATIC(BACKREST_KEY_VERSION_VAR,                     "version");
VARIANT_STRDEF_STATIC(BACKUP_KEY_BACKREST_VAR,                      "backrest");
VARIANT_STRDEF_STATIC(BACKUP_KEY_INFO_VAR,                          "info");
VARIANT_STRDEF_STATIC(BACKUP_KEY_LABEL_VAR,                         "label");
VARIANT_STRDEF_STATIC(BACKUP_KEY_PRIOR_VAR,                         "prior");
VARIANT_STRDEF_STATIC(BACKUP_KEY_REFERENCE_VAR,                     "reference");
VARIANT_STRDEF_STATIC(BACKUP_KEY_TIMESTAMP_VAR,                     "timestamp");
VARIANT_STRDEF_STATIC(BACKUP_KEY_TYPE_VAR,                          "type");
VARIANT_STRDEF_STATIC(DB_KEY_ID_VAR,                                "id");
VARIANT_STRDEF_STATIC(DB_KEY_SYSTEM_ID_VAR,                         "system-id");
VARIANT_STRDEF_STATIC(DB_KEY_VERSION_VAR,                           "version");
VARIANT_STRDEF_STATIC(INFO_KEY_REPOSITORY_VAR,                      "repository");
VARIANT_STRDEF_STATIC(KEY_ARCHIVE_VAR,                              "archive");
VARIANT_STRDEF_STATIC(KEY_DATABASE_VAR,                             "database");
VARIANT_STRDEF_STATIC(KEY_DELTA_VAR,                                "delta");
VARIANT_STRDEF_STATIC(KEY_SIZE_VAR,                                 "size");
VARIANT_STRDEF_STATIC(KEY_START_VAR,                                "start");
VARIANT_STRDEF_STATIC(KEY_STOP_VAR,                                 "stop");
VARIANT_STRDEF_STATIC(STANZA_KEY_BACKUP_VAR,                        "backup");
VARIANT_STRDEF_STATIC(STANZA_KEY_CIPHER_VAR,                        "cipher");
VARIANT_STRDEF_STATIC(STANZA_KEY_NAME_VAR,                          "name");
VARIANT_STRDEF_STATIC(STANZA_KEY_STATUS_VAR,                        "status");
VARIANT_STRDEF_STATIC(STANZA_KEY_DB_VAR,                            "db");
VARIANT_STRDEF_STATIC(STATUS_KEY_CODE_VAR,                          "code");
VARIANT_STRDEF_STATIC(STATUS_KEY_MESSAGE_VAR,                       "message");

#define INFO_STANZA_STATUS_OK                                       "ok"
#define INFO_STANZA_STATUS_ERROR                                    "error"

#define INFO_STANZA_STATUS_CODE_OK                                  0
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_OK_STR,                    "ok");
#define INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH                 1
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR,   "missing stanza path");
#define INFO_STANZA_STATUS_CODE_NO_BACKUP                           2
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR,             "no valid backups");
#define INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA                 3
STRING_STATIC(INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR,   "missing stanza data");

/***********************************************************************************************************************************
Set error status code and message for the stanza to the code and message passed.
***********************************************************************************************************************************/
static void
stanzaStatus(const int code, const String *message, Variant *stanzaInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(STRING, message);
        FUNCTION_TEST_PARAM(VARIANT, stanzaInfo);
    FUNCTION_TEST_END();

    ASSERT(code >= 0 && code <= 3);
    ASSERT(message != NULL);
    ASSERT(stanzaInfo != NULL);

    KeyValue *statusKv = kvPutKv(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR);

    kvAdd(statusKv, STATUS_KEY_CODE_VAR, VARINT(code));
    kvAdd(statusKv, STATUS_KEY_MESSAGE_VAR, VARSTR(message));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the data for the archive section of the stanza for the database info from the backup.info file.
***********************************************************************************************************************************/
static void
archiveDbList(const String *stanza, const InfoPgData *pgData, VariantList *archiveSection, const InfoArchive *info, bool currentDb)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, pgData);
        FUNCTION_TEST_PARAM(VARIANT, archiveSection);
        FUNCTION_TEST_PARAM(BOOL, currentDb);
    FUNCTION_TEST_END();

    ASSERT(stanza != NULL);
    ASSERT(pgData != NULL);
    ASSERT(archiveSection != NULL);

    // With multiple DB versions, the backup.info history-id may not be the same as archive.info history-id, so the
    // archive path must be built by retrieving the archive id given the db version and system id of the backup.info file.
    // If there is no match, an error will be thrown.
    const String *archiveId = infoArchiveIdHistoryMatch(info, pgData->id, pgData->version, pgData->systemId);

    String *archivePath = strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strPtr(stanza), strPtr(archiveId));
    String *archiveStart = NULL;
    String *archiveStop = NULL;
    Variant *archiveInfo = varNewKv(kvNew());

    // Get a list of WAL directories in the archive repo from oldest to newest, if any exist
    StringList *walDir = strLstSort(
        storageListP(storageRepo(), archivePath, .expression = WAL_SEGMENT_DIR_REGEXP_STR), sortOrderAsc);

    if (strLstSize(walDir) > 0)
    {
        // Not every WAL dir has WAL files so check each
        for (unsigned int idx = 0; idx < strLstSize(walDir); idx++)
        {
            // Get a list of all WAL in this WAL dir
            StringList *list = storageListP(
                storageRepo(), strNewFmt("%s/%s", strPtr(archivePath), strPtr(strLstGet(walDir, idx))),
                .expression = WAL_SEGMENT_FILE_REGEXP_STR);

            // If wal segments are found, get the oldest one as the archive start
            if (strLstSize(list) > 0)
            {
                // Sort the list from oldest to newest to get the oldest starting WAL archived for this DB
                list = strLstSort(list, sortOrderAsc);
                archiveStart = strSubN(strLstGet(list, 0), 0, 24);
                break;
            }
        }

        // Iterate through the directory list in the reverse so processing newest first. Cast comparison to an int for readability.
        for (unsigned int idx = strLstSize(walDir) - 1; (int)idx >= 0; idx--)
        {
            // Get a list of all WAL in this WAL dir
            StringList *list = storageListP(
                storageRepo(), strNewFmt("%s/%s", strPtr(archivePath), strPtr(strLstGet(walDir, idx))),
                .expression = WAL_SEGMENT_FILE_REGEXP_STR);

            // If wal segments are found, get the newest one as the archive stop
            if (strLstSize(list) > 0)
            {
                // Sort the list from newest to oldest to get the newest ending WAL archived for this DB
                list = strLstSort(list, sortOrderDesc);
                archiveStop = strSubN(strLstGet(list, 0), 0, 24);
                break;
            }
        }
    }

    // If there is an archive or the database is the current database then store it
    if (currentDb || archiveStart != NULL)
    {
        // Add empty database section to archiveInfo and then fill in database id from the backup.info
        KeyValue *databaseInfo = kvPutKv(varKv(archiveInfo), KEY_DATABASE_VAR);

        kvAdd(databaseInfo, DB_KEY_ID_VAR, VARUINT(pgData->id));

        kvPut(varKv(archiveInfo), DB_KEY_ID_VAR, VARSTR(archiveId));
        kvPut(varKv(archiveInfo), ARCHIVE_KEY_MIN_VAR, (archiveStart != NULL ? VARSTR(archiveStart) : (Variant *)NULL));
        kvPut(varKv(archiveInfo), ARCHIVE_KEY_MAX_VAR, (archiveStop != NULL ? VARSTR(archiveStop) : (Variant *)NULL));

        varLstAdd(archiveSection, archiveInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
For each current backup in the backup.info file of the stanza, set the data for the backup section.
***********************************************************************************************************************************/
static void
backupList(VariantList *backupSection, InfoBackup *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, backupSection);
        FUNCTION_TEST_PARAM(INFO_BACKUP, info);
    FUNCTION_TEST_END();

    ASSERT(backupSection != NULL);
    ASSERT(info != NULL);

    // For each current backup, get the label and corresponding data and build the backup section
    for (unsigned int keyIdx = 0; keyIdx < infoBackupDataTotal(info); keyIdx++)
    {
        // Get the backup data
        InfoBackupData backupData = infoBackupData(info, keyIdx);

        Variant *backupInfo = varNewKv(kvNew());

        // main keys
        kvPut(varKv(backupInfo), BACKUP_KEY_LABEL_VAR, VARSTR(backupData.backupLabel));
        kvPut(varKv(backupInfo), BACKUP_KEY_TYPE_VAR, VARSTR(backupData.backupType));
        kvPut(
            varKv(backupInfo), BACKUP_KEY_PRIOR_VAR,
            (backupData.backupPrior != NULL ? VARSTR(backupData.backupPrior) : NULL));
        kvPut(
            varKv(backupInfo), BACKUP_KEY_REFERENCE_VAR,
            (backupData.backupReference != NULL ? varNewVarLst(varLstNewStrLst(backupData.backupReference)) : NULL));

        // archive section
        KeyValue *archiveInfo = kvPutKv(varKv(backupInfo), KEY_ARCHIVE_VAR);

        kvAdd(
            archiveInfo, KEY_START_VAR,
            (backupData.backupArchiveStart != NULL ? VARSTR(backupData.backupArchiveStart) : NULL));
        kvAdd(
            archiveInfo, KEY_STOP_VAR,
            (backupData.backupArchiveStop != NULL ? VARSTR(backupData.backupArchiveStop) : NULL));

        // backrest section
        KeyValue *backrestInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_BACKREST_VAR);

        kvAdd(backrestInfo, BACKREST_KEY_FORMAT_VAR, VARUINT(backupData.backrestFormat));
        kvAdd(backrestInfo, BACKREST_KEY_VERSION_VAR, VARSTR(backupData.backrestVersion));

        // database section
        KeyValue *dbInfo = kvPutKv(varKv(backupInfo), KEY_DATABASE_VAR);

        kvAdd(dbInfo, DB_KEY_ID_VAR, VARUINT(backupData.backupPgId));

        // info section
        KeyValue *infoInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_INFO_VAR);

        kvAdd(infoInfo, KEY_SIZE_VAR, VARUINT64(backupData.backupInfoSize));
        kvAdd(infoInfo, KEY_DELTA_VAR, VARUINT64(backupData.backupInfoSizeDelta));

        // info:repository section
        KeyValue *repoInfo = kvPutKv(infoInfo, INFO_KEY_REPOSITORY_VAR);

        kvAdd(repoInfo, KEY_SIZE_VAR, VARUINT64(backupData.backupInfoRepoSize));
        kvAdd(repoInfo, KEY_DELTA_VAR, VARUINT64(backupData.backupInfoRepoSizeDelta));

        // timestamp section
        KeyValue *timeInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_TIMESTAMP_VAR);

        kvAdd(timeInfo, KEY_START_VAR, VARUINT64(backupData.backupTimestampStart));
        kvAdd(timeInfo, KEY_STOP_VAR, VARUINT64(backupData.backupTimestampStop));

        varLstAdd(backupSection, backupInfo);
    }


    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the stanza data for each stanza found in the repo.
***********************************************************************************************************************************/
static VariantList *
stanzaInfoList(const String *stanza, StringList *stanzaList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(STRING_LIST, stanzaList);
    FUNCTION_TEST_END();

    ASSERT(stanzaList != NULL);

    VariantList *result = varLstNew();
    bool stanzaFound = false;

    // Sort the list
    stanzaList = strLstSort(stanzaList, sortOrderAsc);

    for (unsigned int idx = 0; idx < strLstSize(stanzaList); idx++)
    {
        String *stanzaListName = strLstGet(stanzaList, idx);

        // If a specific stanza has been requested and this is not it, then continue to the next in the list else indicate found
        if (stanza != NULL)
        {
            if (!strEq(stanza, stanzaListName))
                continue;
            else
                stanzaFound = true;
        }

        // Create the stanzaInfo and section variables
        Variant *stanzaInfo = varNewKv(kvNew());
        VariantList *dbSection = varLstNew();
        VariantList *backupSection = varLstNew();
        VariantList *archiveSection = varLstNew();
        InfoBackup *info = NULL;

        // Catch certain errors
        TRY_BEGIN()
        {
            // Attempt to load the backup info file
            info = infoBackupNewLoad(
                storageRepo(), strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strPtr(stanzaListName), INFO_BACKUP_FILE),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
        }
        CATCH(FileMissingError)
        {
            // If there is no backup.info then set the status to indicate missing
            stanzaStatus(
                INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA, INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_DATA_STR, stanzaInfo);
        }
        CATCH(CryptoError)
        {
            // If a reason for the error is due to a an ecryption error, add a hint
            THROW_FMT(
                CryptoError,
                "%s\n"
                "HINT: use option --stanza if encryption settings are different for the stanza than the global settings",
                errorMessage());
        }
        TRY_END();

        // Set the stanza name and cipher
        kvPut(varKv(stanzaInfo), STANZA_KEY_NAME_VAR, VARSTR(stanzaListName));
        kvPut(varKv(stanzaInfo), STANZA_KEY_CIPHER_VAR, VARSTR(CIPHER_TYPE_NONE_STR));

        // If the backup.info file exists, get the database history information (newest to oldest) and corresponding archive
        if (info != NULL)
        {
            // Determine if encryption is enabled by checking for a cipher passphrase.  This is not ideal since it does not tell us
            // what type of encryption is in use, but to figure that out we need a way to query the (possibly) remote repo to find
            // out.  No such mechanism exists so this will have to do for now.  Probably the easiest thing to do is store the
            // cipher type in the info file.
            if (infoPgCipherPass(infoBackupPg(info)) != NULL)
                kvPut(varKv(stanzaInfo), STANZA_KEY_CIPHER_VAR, VARSTR(CIPHER_TYPE_AES_256_CBC_STR));

            for (unsigned int pgIdx = infoPgDataTotal(infoBackupPg(info)) - 1; (int)pgIdx >= 0; pgIdx--)
            {
                InfoPgData pgData = infoPgData(infoBackupPg(info), pgIdx);
                Variant *pgInfo = varNewKv(kvNew());

                kvPut(varKv(pgInfo), DB_KEY_ID_VAR, VARUINT(pgData.id));
                kvPut(varKv(pgInfo), DB_KEY_SYSTEM_ID_VAR, VARUINT64(pgData.systemId));
                kvPut(varKv(pgInfo), DB_KEY_VERSION_VAR, VARSTR(pgVersionToStr(pgData.version)));

                varLstAdd(dbSection, pgInfo);

                // Get the archive info for the DB from the archive.info file
                InfoArchive *info = infoArchiveNewLoad(
                    storageRepo(), strNewFmt(STORAGE_PATH_ARCHIVE "/%s/%s", strPtr(stanzaListName), INFO_ARCHIVE_FILE),
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
                archiveDbList(stanzaListName, &pgData, archiveSection, info, (pgIdx == 0 ? true : false));
            }

            // Get data for all existing backups for this stanza
            backupList(backupSection, info);
        }

        // Add the database history, backup and archive sections to the stanza info
        kvPut(varKv(stanzaInfo), STANZA_KEY_DB_VAR, varNewVarLst(dbSection));
        kvPut(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR, varNewVarLst(backupSection));
        kvPut(varKv(stanzaInfo), KEY_ARCHIVE_VAR, varNewVarLst(archiveSection));

        // If a status has not already been set and there are no backups then set status to no backup
        if (kvGet(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR) == NULL &&
            varLstSize(kvGetList(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR)) == 0)
        {
            stanzaStatus(INFO_STANZA_STATUS_CODE_NO_BACKUP, INFO_STANZA_STATUS_MESSAGE_NO_BACKUP_STR, stanzaInfo);
        }

        // If a status has still not been set then set it to OK
        if (kvGet(varKv(stanzaInfo), STANZA_KEY_STATUS_VAR) == NULL)
            stanzaStatus(INFO_STANZA_STATUS_CODE_OK, INFO_STANZA_STATUS_MESSAGE_OK_STR, stanzaInfo);

        varLstAdd(result, stanzaInfo);
    }

    // If looking for a specific stanza and it was not found, set minimum info and the status
    if (stanza != NULL && !stanzaFound)
    {
        Variant *stanzaInfo = varNewKv(kvNew());

        kvPut(varKv(stanzaInfo), STANZA_KEY_NAME_VAR, VARSTR(stanza));

        kvPut(varKv(stanzaInfo), STANZA_KEY_DB_VAR, varNewVarLst(varLstNew()));
        kvPut(varKv(stanzaInfo), STANZA_KEY_BACKUP_VAR, varNewVarLst(varLstNew()));

        stanzaStatus(INFO_STANZA_STATUS_CODE_MISSING_STANZA_PATH, INFO_STANZA_STATUS_MESSAGE_MISSING_STANZA_PATH_STR, stanzaInfo);
        varLstAdd(result, stanzaInfo);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Format the text output for each database of the stanza.
***********************************************************************************************************************************/
static void
formatTextDb(const KeyValue *stanzaInfo, String *resultStr)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, stanzaInfo);
        FUNCTION_TEST_PARAM(STRING, resultStr);
    FUNCTION_TEST_END();

    ASSERT(stanzaInfo != NULL);

    VariantList *dbSection = kvGetList(stanzaInfo, STANZA_KEY_DB_VAR);
    VariantList *archiveSection = kvGetList(stanzaInfo, KEY_ARCHIVE_VAR);
    VariantList *backupSection = kvGetList(stanzaInfo, STANZA_KEY_BACKUP_VAR);

    // For each database (working from oldest to newest) find the corresponding archive and backup info
    for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbSection); dbIdx++)
    {
        KeyValue *pgInfo = varKv(varLstGet(dbSection, dbIdx));
        unsigned int dbId = varUInt(kvGet(pgInfo, DB_KEY_ID_VAR));

        // List is ordered so 0 is always the current DB index
        if (dbIdx == varLstSize(dbSection) - 1)
            strCat(resultStr, "\n    db (current)");

        // Get the min/max archive information for the database
        String *archiveResult = strNew("");

        for (unsigned int archiveIdx = 0; archiveIdx < varLstSize(archiveSection); archiveIdx++)
        {
            KeyValue *archiveInfo = varKv(varLstGet(archiveSection, archiveIdx));
            KeyValue *archiveDbInfo = varKv(kvGet(archiveInfo, KEY_DATABASE_VAR));
            unsigned int archiveDbId = varUInt(kvGet(archiveDbInfo, DB_KEY_ID_VAR));

            if (archiveDbId == dbId)
            {
                strCatFmt(
                    archiveResult, "\n        wal archive min/max (%s): ",
                    strPtr(varStr(kvGet(archiveInfo, DB_KEY_ID_VAR))));

                // Get the archive min/max if there are any archives for the database
                if (kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR) != NULL)
                {
                    strCatFmt(
                        archiveResult, "%s/%s\n", strPtr(varStr(kvGet(archiveInfo, ARCHIVE_KEY_MIN_VAR))),
                        strPtr(varStr(kvGet(archiveInfo, ARCHIVE_KEY_MAX_VAR))));
                }
                else
                    strCat(archiveResult, "none present\n");
            }
        }

        // Get the information for each current backup
        String *backupResult = strNew("");

        for (unsigned int backupIdx = 0; backupIdx < varLstSize(backupSection); backupIdx++)
        {
            KeyValue *backupInfo = varKv(varLstGet(backupSection, backupIdx));
            KeyValue *backupDbInfo = varKv(kvGet(backupInfo, KEY_DATABASE_VAR));
            unsigned int backupDbId = varUInt(kvGet(backupDbInfo, DB_KEY_ID_VAR));

            if (backupDbId == dbId)
            {
                strCatFmt(
                    backupResult, "\n        %s backup: %s\n", strPtr(varStr(kvGet(backupInfo, BACKUP_KEY_TYPE_VAR))),
                    strPtr(varStr(kvGet(backupInfo, BACKUP_KEY_LABEL_VAR))));

                KeyValue *timestampInfo = varKv(kvGet(backupInfo, BACKUP_KEY_TIMESTAMP_VAR));

                // Get and format the backup start/stop time
                static char timeBufferStart[20];
                static char timeBufferStop[20];
                time_t timeStart = (time_t)varUInt64(kvGet(timestampInfo, KEY_START_VAR));
                time_t timeStop = (time_t)varUInt64(kvGet(timestampInfo, KEY_STOP_VAR));

                strftime(timeBufferStart, 20, "%Y-%m-%d %H:%M:%S", localtime(&timeStart));
                strftime(timeBufferStop, 20, "%Y-%m-%d %H:%M:%S", localtime(&timeStop));

                strCatFmt(
                    backupResult, "            timestamp start/stop: %s / %s\n", timeBufferStart, timeBufferStop);
                strCat(backupResult, "            wal start/stop: ");

                KeyValue *archiveDBackupInfo = varKv(kvGet(backupInfo, KEY_ARCHIVE_VAR));

                if (kvGet(archiveDBackupInfo, KEY_START_VAR) != NULL &&
                    kvGet(archiveDBackupInfo, KEY_STOP_VAR) != NULL)
                {
                    strCatFmt(backupResult, "%s / %s\n", strPtr(varStr(kvGet(archiveDBackupInfo, KEY_START_VAR))),
                        strPtr(varStr(kvGet(archiveDBackupInfo, KEY_STOP_VAR))));
                }
                else
                    strCat(backupResult, "n/a\n");

                KeyValue *info = varKv(kvGet(backupInfo, BACKUP_KEY_INFO_VAR));

                strCatFmt(
                    backupResult, "            database size: %s, backup size: %s\n",
                    strPtr(strSizeFormat(varUInt64Force(kvGet(info, KEY_SIZE_VAR)))),
                    strPtr(strSizeFormat(varUInt64Force(kvGet(info, KEY_DELTA_VAR)))));

                KeyValue *repoInfo = varKv(kvGet(info, INFO_KEY_REPOSITORY_VAR));

                strCatFmt(
                    backupResult, "            repository size: %s, repository backup size: %s\n",
                    strPtr(strSizeFormat(varUInt64Force(kvGet(repoInfo, KEY_SIZE_VAR)))),
                    strPtr(strSizeFormat(varUInt64Force(kvGet(repoInfo, KEY_DELTA_VAR)))));

                if (kvGet(backupInfo, BACKUP_KEY_REFERENCE_VAR) != NULL)
                {
                    StringList *referenceList = strLstNewVarLst(varVarLst(kvGet(backupInfo, BACKUP_KEY_REFERENCE_VAR)));
                    strCatFmt(backupResult, "            backup reference list: %s\n", strPtr(strLstJoin(referenceList, ", ")));
                }
            }
        }

        // If there is data to display, then display it.
        if (strSize(archiveResult) > 0 || strSize(backupResult) > 0)
        {
            if (dbIdx != varLstSize(dbSection) - 1)
                strCat(resultStr, "\n    db (prior)");

            if (strSize(archiveResult) > 0)
                strCat(resultStr, strPtr(archiveResult));

            if (strSize(backupResult) > 0)
                strCat(resultStr, strPtr(backupResult));
        }
    }
    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Render the information for the stanza based on the command parameters.
***********************************************************************************************************************************/
static String *
infoRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get stanza if specified
        const String *stanza = cfgOptionTest(cfgOptStanza) ? cfgOptionStr(cfgOptStanza) : NULL;

        // Get a list of stanzas in the backup directory.
        StringList *stanzaList = storageListNP(storageRepo(), STORAGE_PATH_BACKUP_STR);
        VariantList *infoList = varLstNew();
        String *resultStr = strNew("");

        // If the backup storage exists, then search for and process any stanzas
        if (strLstSize(stanzaList) > 0)
            infoList = stanzaInfoList(stanza, stanzaList);

        // Format text output
        if (strEq(cfgOptionStr(cfgOptOutput), CFGOPTVAL_INFO_OUTPUT_TEXT_STR))
        {
            // Process any stanza directories
            if  (varLstSize(infoList) > 0)
            {
                for (unsigned int stanzaIdx = 0; stanzaIdx < varLstSize(infoList); stanzaIdx++)
                {
                    KeyValue *stanzaInfo = varKv(varLstGet(infoList, stanzaIdx));

                    // Add a carriage return between stanzas
                    if (stanzaIdx > 0)
                        strCatFmt(resultStr, "\n");

                    // Stanza name and status
                    strCatFmt(
                        resultStr, "stanza: %s\n    status: ", strPtr(varStr(kvGet(stanzaInfo, STANZA_KEY_NAME_VAR))));

                    // If an error has occurred, provide the information that is available and move onto next stanza
                    KeyValue *stanzaStatus = varKv(kvGet(stanzaInfo, STANZA_KEY_STATUS_VAR));
                    int statusCode = varInt(kvGet(stanzaStatus, STATUS_KEY_CODE_VAR));

                    if (statusCode != INFO_STANZA_STATUS_CODE_OK)
                    {
                        strCatFmt(
                            resultStr, "%s (%s)\n", INFO_STANZA_STATUS_ERROR,
                            strPtr(varStr(kvGet(stanzaStatus, STATUS_KEY_MESSAGE_VAR))));

                        if (statusCode == INFO_STANZA_STATUS_CODE_MISSING_STANZA_DATA ||
                            statusCode == INFO_STANZA_STATUS_CODE_NO_BACKUP)
                        {
                            strCatFmt(
                                resultStr, "    cipher: %s\n", strPtr(varStr(kvGet(stanzaInfo, STANZA_KEY_CIPHER_VAR))));

                            // If there is a backup.info file but no backups, then process the archive info
                            if (statusCode == INFO_STANZA_STATUS_CODE_NO_BACKUP)
                                formatTextDb(stanzaInfo, resultStr);
                        }

                        continue;
                    }
                    else
                        strCatFmt(resultStr, "%s\n", INFO_STANZA_STATUS_OK);

                    // Cipher
                    strCatFmt(resultStr, "    cipher: %s\n",
                        strPtr(varStr(kvGet(stanzaInfo, STANZA_KEY_CIPHER_VAR))));

                    formatTextDb(stanzaInfo, resultStr);
                }
            }
            else
                resultStr = strNew("No stanzas exist in the repository.\n");
        }
        // Format json output
        else
            resultStr = jsonFromVar(varNewVarLst(infoList), 4);

        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(resultStr);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Render info and output to stdout
***********************************************************************************************************************************/
void
cmdInfo(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioHandleWriteOneStr(STDOUT_FILENO, infoRender());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
