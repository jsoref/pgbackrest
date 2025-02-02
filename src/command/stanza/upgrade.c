/***********************************************************************************************************************************
Stanza Update Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/control/common.h"
#include "command/stanza/common.h"
#include "command/stanza/upgrade.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Process stanza-upgrade
***********************************************************************************************************************************/
void
cmdStanzaUpgrade(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Verify the repo is local and that a stop was not issued before proceeding
    repoIsLocalVerify();
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Storage *storageRepoReadStanza = storageRepo();
        const Storage *storageRepoWriteStanza = storageRepoWrite();
        bool infoArchiveUpgrade = false;
        bool infoBackupUpgrade = false;

        // Get the version and system information - validating it if the database is online
        PgControl pgControl = pgValidate();

        // Load the info files (errors if missing)
        InfoArchive *infoArchive = infoArchiveNewLoad(
            storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));

        InfoBackup *infoBackup = infoBackupNewLoad(
            storageRepoReadStanza, INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

        // Since the file save of archive.info and backup.info are not atomic, then check and update each separately.
        // Update archive
        if (pgControl.version != archiveInfo.version || pgControl.systemId != archiveInfo.systemId)
        {
            infoArchivePgSet(infoArchive, pgControl.version, pgControl.systemId);
            infoArchiveUpgrade = true;
        }

        // Update backup
        if (pgControl.version != backupInfo.version || pgControl.systemId != backupInfo.systemId)
        {
            infoBackupPgSet(
                infoBackup, pgControl.version, pgControl.systemId, pgControl.controlVersion, pgControl.catalogVersion);
            infoBackupUpgrade = true;
        }

        // Get the backup and archive info pg data and throw an error if the ids do not match before saving (even if only one
        // needed to be updated)
        backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));
        archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));
        infoValidate(&archiveInfo, &backupInfo);

        // Save archive info
        if (infoArchiveUpgrade)
        {
            infoArchiveSave(
                infoArchive, storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
        }

        // Save backup info
        if (infoBackupUpgrade)
        {
            infoBackupSave(
                infoBackup, storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
        }

        if (!(infoArchiveUpgrade || infoBackupUpgrade))
            LOG_INFO("stanza '%s' is already up to date", strPtr(cfgOptionStr(cfgOptStanza)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
