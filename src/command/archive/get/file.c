/***********************************************************************************************************************************
Archive Get File
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/get/file.h"
#include "command/archive/common.h"
#include "command/control/common.h"
#include "common/compress/gzip/common.h"
#include "common/compress/gzip/decompress.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/filter/group.h"
#include "common/log.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Check if a WAL file exists in the repository
***********************************************************************************************************************************/
#define FUNCTION_LOG_ARCHIVE_GET_CHECK_RESULT_TYPE                                                                                 \
    ArchiveGetCheckResult
#define FUNCTION_LOG_ARCHIVE_GET_CHECK_RESULT_FORMAT(value, buffer, bufferSize)                                                    \
    objToLog(&value, "ArchiveGetCheckResult", buffer, bufferSize)

typedef struct ArchiveGetCheckResult
{
    String *archiveFileActual;
    String *cipherPass;
} ArchiveGetCheckResult;

static ArchiveGetCheckResult
archiveGetCheck(const String *archiveFile, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(archiveFile != NULL);

    ArchiveGetCheckResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get pg control info
        PgControl controlInfo = pgControlFromFile(storagePg(), cfgOptionStr(cfgOptPgPath));

        // Attempt to load the archive info file
        InfoArchive *info = infoArchiveNewLoad(storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cipherType, cipherPass);

        // Loop through the pg history in case the WAL we need is not in the most recent archive id
        String *archiveId = NULL;
        const String *archiveFileActual = NULL;

        for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoArchivePg(info)); pgIdx++)
        {
            InfoPgData pgData = infoPgData(infoArchivePg(info), pgIdx);

            // Only use the archive id if it matches the current cluster
            if (pgData.systemId == controlInfo.systemId && pgData.version == controlInfo.version)
            {
                archiveId = infoPgArchiveId(infoArchivePg(info), pgIdx);

                // If a WAL segment search among the possible file names
                if (walIsSegment(archiveFile))
                {
                    String *walSegmentFile = walSegmentFind(storageRepo(), archiveId, archiveFile, 0);

                    if (walSegmentFile != NULL)
                    {
                        archiveFileActual = strNewFmt("%s/%s", strPtr(strSubN(archiveFile, 0, 16)), strPtr(walSegmentFile));
                        break;
                    }
                }
                // Else if not a WAL segment, see if it exists in the archive dir
                else if (
                    storageExistsNP(
                        storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(archiveFile))))
                {
                    archiveFileActual = archiveFile;
                    break;
                }
            }
        }

        // Error if no archive id was found -- this indicates a mismatch with the current cluster
        if (archiveId == NULL)
        {
            THROW_FMT(
                ArchiveMismatchError, "unable to retrieve the archive id for database version '%s' and system-id '%" PRIu64 "'",
                strPtr(pgVersionToStr(controlInfo.version)), controlInfo.systemId);
        }

        if (archiveFileActual != NULL)
        {
            memContextSwitch(MEM_CONTEXT_OLD());
            result.archiveFileActual = strNewFmt("%s/%s", strPtr(archiveId), strPtr(archiveFileActual));
            result.cipherPass = strDup(infoArchiveCipherPass(info));
            memContextSwitch(MEM_CONTEXT_TEMP());
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(ARCHIVE_GET_CHECK_RESULT, result);
}

/***********************************************************************************************************************************
Copy a file from the archive to the specified destination
***********************************************************************************************************************************/
int
archiveGetFile(
    const Storage *storage, const String *archiveFile, const String *walDestination, bool durable, CipherType cipherType,
    const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
        FUNCTION_LOG_PARAM(STRING, walDestination);
        FUNCTION_LOG_PARAM(BOOL, durable);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(archiveFile != NULL);
    ASSERT(walDestination != NULL);

    // By default result indicates WAL segment not found
    int result = 1;

    // Is the file compressible during the copy?
    bool compressible = true;

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure the file exists and other checks pass
        ArchiveGetCheckResult archiveGetCheckResult = archiveGetCheck(archiveFile, cipherType, cipherPass);

        if (archiveGetCheckResult.archiveFileActual != NULL)
        {
            StorageWrite *destination = storageNewWriteP(
                storage, walDestination, .noCreatePath = true, .noSyncFile = !durable, .noSyncPath = !durable,
                .noAtomic = !durable);

            // If there is a cipher then add the decrypt filter
            if (cipherType != cipherTypeNone)
            {
                ioFilterGroupAdd(
                    ioWriteFilterGroup(storageWriteIo(destination)), cipherBlockNew(cipherModeDecrypt, cipherType,
                        BUFSTR(archiveGetCheckResult.cipherPass), NULL));
                compressible = false;
            }

            // If file is compressed then add the decompression filter
            if (strEndsWithZ(archiveGetCheckResult.archiveFileActual, "." GZIP_EXT))
            {
                ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(destination)), gzipDecompressNew(false));
                compressible = false;
            }

            // Copy the file
            storageCopyNP(
                storageNewReadP(
                    storageRepo(), strNewFmt("%s/%s", STORAGE_REPO_ARCHIVE, strPtr(archiveGetCheckResult.archiveFileActual)),
                    .compressible = compressible),
                destination);

            // The WAL file was found
            result = 0;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}
