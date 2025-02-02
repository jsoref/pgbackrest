/***********************************************************************************************************************************
Stanza Delete Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/control/common.h"
#include "command/stanza/delete.h"
#include "command/backup/common.h"
#include "common/debug.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoManifest.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Helper functions to assist with testing
***********************************************************************************************************************************/
static void
manifestDelete(const Storage *storageRepoWriteStanza)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, storageRepoWriteStanza);
    FUNCTION_TEST_END();

    ASSERT(storageRepoWriteStanza != NULL);

    // Get the list of backup directories from newest to oldest since don't want to invalidate a backup before
    // invalidating any backups that depend on it.
    StringList *backupList = strLstSort(
        storageListP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP), .expression = backupRegExpP(.full = true,
            .differential = true, .incremental = true)), sortOrderDesc);

    // Delete all manifest files
    for (unsigned int idx = 0; idx < strLstSize(backupList); idx++)
    {
        storageRemoveNP(
            storageRepoWriteStanza,
            strNewFmt(STORAGE_REPO_BACKUP "/%s/" INFO_MANIFEST_FILE, strPtr(strLstGet(backupList, idx))));
        storageRemoveNP(
            storageRepoWriteStanza,
            strNewFmt(STORAGE_REPO_BACKUP "/%s/" INFO_MANIFEST_FILE INFO_COPY_EXT, strPtr(strLstGet(backupList, idx))));
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
stanzaDelete(const Storage *storageRepoWriteStanza, const StringList *archiveList, const StringList *backupList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, storageRepoWriteStanza);
        FUNCTION_TEST_PARAM(STRING_LIST, archiveList);
        FUNCTION_TEST_PARAM(STRING_LIST, archiveList);
    FUNCTION_TEST_END();

    ASSERT(storageRepoWriteStanza != NULL);

    bool result = false;

    // For most drivers, NULL indicates the directory does not exist at all. For those that do not support paths (e.g. S3) an
    // empty StringList will be returned; in such a case, the directory will attempt to be deleted (this is OK).
    if (archiveList != NULL || backupList != NULL)
    {
        bool archiveNotEmpty = (archiveList != NULL && strLstSize(archiveList) > 0) ? true : false;
        bool backupNotEmpty = (backupList != NULL && strLstSize(backupList) > 0) ? true : false;

        // If something exists in either directory, then remove
        if (archiveNotEmpty || backupNotEmpty)
        {
            // If the stop file does not exist, then error. This check is required even when --force is issued.
            if (!storageExistsNP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))))
            {
                THROW_FMT(
                    FileMissingError, "stop file does not exist for stanza '%s'\n"
                    "HINT: has the pgbackrest stop command been run on this server for this stanza?",
                    strPtr(cfgOptionStr(cfgOptStanza)));
            }

            // If a force has not been issued and Postgres is running, then error
            if (!cfgOptionBool(cfgOptForce) && storageExistsNP(storagePg(), STRDEF(PG_FILE_POSTMASTERPID)))
            {
                THROW_FMT(
                    PostmasterRunningError, PG_FILE_POSTMASTERPID " exists - looks like the postmaster is running. "
                    "To delete stanza '%s', shutdown the postmaster for stanza '%s' and try again, or use --force.",
                    strPtr(cfgOptionStr(cfgOptStanza)), strPtr(cfgOptionStr(cfgOptStanza)));
            }

            // Delete the archive info files
            if (archiveNotEmpty)
            {
                storageRemoveNP(storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_STR);
                storageRemoveNP(storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_COPY_STR);
            }

            // Delete the backup info files
            if (backupNotEmpty)
            {
                storageRemoveNP(storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_STR);
                storageRemoveNP(storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_COPY_STR);
            }

            // Remove manifest files
            manifestDelete(storageRepoWriteStanza);
        }

        // Recusively remove the entire stanza repo if exists. S3 will attempt to remove even if not.
        if (archiveList != NULL)
            storagePathRemoveP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_ARCHIVE), .recurse = true);

        if (backupList != NULL)
            storagePathRemoveP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_BACKUP), .recurse = true);

        // Remove the stop file - this will not error if the stop file does not exist. If the stanza directories existed but nothing
        // was in them, then no pgbackrest commands can be in progress without the info files so a stop is technically not necessary
        storageRemoveNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza)));

        result = true;
    }
    else
        result = true;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Process stanza-delete
***********************************************************************************************************************************/
void
cmdStanzaDelete(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Verify the repo is local before proceeding
    repoIsLocalVerify();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Storage *storageRepoReadStanza = storageRepo();

        stanzaDelete(
            storageRepoWrite(),
            storageListP(storageRepoReadStanza, STRDEF(STORAGE_REPO_ARCHIVE), .nullOnMissing = true),
            storageListP(storageRepoReadStanza, STRDEF(STORAGE_REPO_BACKUP), .nullOnMissing = true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
