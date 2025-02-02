/***********************************************************************************************************************************
Archive Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/ini.h"
#include "common/memContext.h"
#include "common/object.h"
#include "info/infoArchive.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(INFO_ARCHIVE_PATH_FILE_STR,                           INFO_ARCHIVE_PATH_FILE);
STRING_EXTERN(INFO_ARCHIVE_PATH_FILE_COPY_STR,                      INFO_ARCHIVE_PATH_FILE_COPY);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoArchive
{
    MemContext *memContext;                                         // Mem context
    InfoPg *infoPg;                                                 // Contents of the DB data
    String *archiveId;                                              // Archive id for the current PG version
};

OBJECT_DEFINE_FREE(INFO_ARCHIVE);

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static InfoArchive *
infoArchiveNewInternal(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    InfoArchive *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoArchive")
    {
        // Create object
        this = memNew(sizeof(InfoArchive));
        this->memContext = MEM_CONTEXT_NEW();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, this);
}

/***********************************************************************************************************************************
Create new object without loading it from a file
***********************************************************************************************************************************/
InfoArchive *
infoArchiveNew(unsigned int pgVersion, uint64_t pgSystemId, CipherType cipherType, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
    FUNCTION_LOG_END();

    ASSERT(pgVersion > 0 && pgSystemId > 0);

    InfoArchive *this = infoArchiveNewInternal();

    // Initialize the pg data
    this->infoPg = infoPgNew(cipherType, cipherPassSub);
    infoArchivePgSet(this, pgVersion, pgSystemId);

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, this);
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
InfoArchive *
infoArchiveNewLoad(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    InfoArchive *this = infoArchiveNewInternal();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Catch file missing error and add archive-specific hints before rethrowing
        TRY_BEGIN()
        {
            this->infoPg = infoPgNewLoad(storage, fileName, infoPgArchive, cipherType, cipherPass, NULL);
        }
        CATCH_ANY()
        {
            THROWP_FMT(
                errorType(),
                "%s\n"
                "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
                "HINT: is archive_command configured correctly in postgresql.conf?\n"
                "HINT: has a stanza-create been performed?\n"
                "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
                errorMessage());
        }
        TRY_END();

        // Store the archiveId for the current PG db-version db-id
        this->archiveId = infoPgArchiveId(this->infoPg, infoPgDataCurrentId(this->infoPg));
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, this);
}

/***********************************************************************************************************************************
Set the infoPg data
***********************************************************************************************************************************/
InfoArchive *
infoArchivePgSet(InfoArchive *this, unsigned int pgVersion, uint64_t pgSystemId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    this->infoPg = infoPgSet(this->infoPg, infoPgArchive, pgVersion, pgSystemId, 0, 0);
    this->archiveId = infoPgArchiveId(this->infoPg, infoPgDataCurrentId(this->infoPg));

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
void
infoArchiveSave(
    InfoArchive *this, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        infoPgSave(infoArchivePg(this), iniNew(), storage, fileName, infoPgArchive, cipherType, cipherPass);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Given a backrest history id and postgres systemId and version, return the archiveId of the best match
***********************************************************************************************************************************/
const String *
infoArchiveIdHistoryMatch(
    const InfoArchive *this, const unsigned int historyId, const unsigned int pgVersion, const uint64_t pgSystemId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_LOG_PARAM(UINT, historyId);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    String *archiveId = NULL;
    InfoPg *infoPg = infoArchivePg(this);

    // Search the history list, from newest to oldest
    for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoPg); pgIdx++)
    {
        InfoPgData pgDataArchive = infoPgData(infoPg, pgIdx);

        // If there is an exact match with the history, system and version then get the archiveId and stop
        if (historyId == pgDataArchive.id && pgSystemId == pgDataArchive.systemId && pgVersion == pgDataArchive.version)
        {
            archiveId = infoPgArchiveId(infoPg, pgIdx);
            break;
        }
    }

    // If there was not an exact match, then search for the first matching database system-id and version
    if (archiveId == NULL)
    {
        for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoPg); pgIdx++)
        {
            InfoPgData pgDataArchive = infoPgData(infoPg, pgIdx);

            if (pgSystemId == pgDataArchive.systemId && pgVersion == pgDataArchive.version)
            {
                archiveId = infoPgArchiveId(infoPg, pgIdx);
                break;
            }
        }
    }

    // If the archive id has not been found, then error
    if (archiveId == NULL)
    {
        THROW_FMT(
            ArchiveMismatchError,
            "unable to retrieve the archive id for database version '%s' and system-id '%" PRIu64 "'",
            strPtr(pgVersionToStr(pgVersion)), pgSystemId);
    }

    FUNCTION_LOG_RETURN(STRING, archiveId);
}

/***********************************************************************************************************************************
Get the current archive id
***********************************************************************************************************************************/
const String *
infoArchiveId(const InfoArchive *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_ARCHIVE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->archiveId);
}

/***********************************************************************************************************************************
Return the cipher passphrase
***********************************************************************************************************************************/
const String *
infoArchiveCipherPass(const InfoArchive *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_ARCHIVE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(infoPgCipherPass(this->infoPg));
}

/***********************************************************************************************************************************
Get PostgreSQL info
***********************************************************************************************************************************/
InfoPg *
infoArchivePg(const InfoArchive *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_ARCHIVE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->infoPg);
}
