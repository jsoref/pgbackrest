/***********************************************************************************************************************************
Database Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "config/config.h"
#include "db/helper.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Get specified cluster
***********************************************************************************************************************************/
static Db *
dbGetId(unsigned int pgId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgId);
    FUNCTION_LOG_END();

    ASSERT(pgId > 0);

    Db *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *applicationName = strNewFmt(PROJECT_NAME " [%s]", cfgCommandName(cfgCommand()));

        if (pgIsLocal(pgId))
        {
            result = dbNew(
                pgClientNew(
                    cfgOptionStr(cfgOptPgSocketPath + pgId - 1), cfgOptionUInt(cfgOptPgPort + pgId - 1), PG_DB_POSTGRES_STR, NULL,
                    (TimeMSec)(cfgOptionDbl(cfgOptDbTimeout) * MSEC_PER_SEC)),
                NULL, applicationName);
        }
        else
            result = dbNew(NULL, protocolRemoteGet(protocolStorageTypePg, pgId), applicationName);

        dbMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(DB, result);
}

/***********************************************************************************************************************************
Get primary cluster or primary and standby cluster
***********************************************************************************************************************************/
DbGetResult
dbGet(bool primaryOnly, bool primaryRequired)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, primaryOnly);
        FUNCTION_LOG_PARAM(BOOL, primaryRequired);
    FUNCTION_LOG_END();

    DbGetResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Loop through to look for primary and standby (if required)
        for (unsigned int pgIdx = 0; pgIdx < cfgOptionIndexTotal(cfgOptPgPath); pgIdx++)
        {
            if (cfgOptionTest(cfgOptPgHost + pgIdx) || cfgOptionTest(cfgOptPgPath + pgIdx))
            {
                Db *db = NULL;
                bool standby = false;

                TRY_BEGIN()
                {
                    db = dbGetId(pgIdx + 1);
                    dbOpen(db);
                    standby = dbIsStandby(db);
                }
                CATCH_ANY()
                {
                    dbFree(db);
                    db = NULL;

                    LOG_WARN("unable to check pg-%u: [%s] %s", pgIdx + 1, errorTypeName(errorType()), errorMessage());
                }
                TRY_END();

                // Was the connection successful
                if (db != NULL)
                {
                    // Is this cluster a standby
                    if (standby)
                    {
                        // If a standby has not already been found then assign it
                        if (result.standbyId == 0 && !primaryOnly)
                        {
                            result.standbyId = pgIdx + 1;
                            result.standby = db;
                        }
                        // Else close the connection since we don't need it
                        else
                            dbFree(db);
                    }
                    // Else is a primary
                    else
                    {
                        // Error if more than one primary was found
                        if (result.primaryId != 0)
                            THROW(DbConnectError, "more than one primary cluster found");

                        result.primaryId = pgIdx + 1;
                        result.primary = db;
                    }
                }
            }
        }

        // Error if no primary was found
        if (result.primaryId == 0 && primaryRequired)
            THROW(DbConnectError, "unable to find primary cluster - cannot proceed");

        dbMove(result.primary, MEM_CONTEXT_OLD());
        dbMove(result.standby, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(DB_GET_RESULT, result);
}
