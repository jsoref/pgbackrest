/***********************************************************************************************************************************
Backup Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/file.h"
#include "command/backup/protocol.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_BACKUP_FILE_STR,                     PROTOCOL_COMMAND_BACKUP_FILE);

/***********************************************************************************************************************************
Process protocol requests
***********************************************************************************************************************************/
bool
backupProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_BACKUP_FILE_STR))
        {
            // Backup the file
            BackupFileResult result = backupFile(
                varStr(varLstGet(paramList, 0)), varBoolForce(varLstGet(paramList, 1)), varUInt64(varLstGet(paramList, 2)),
                varStr(varLstGet(paramList, 3)), varBoolForce(varLstGet(paramList, 4)),
                varUInt64(varLstGet(paramList, 5)) << 32 | varUInt64(varLstGet(paramList, 6)), varStr(varLstGet(paramList, 7)),
                varBoolForce(varLstGet(paramList, 8)), varBoolForce(varLstGet(paramList, 9)),
                varUIntForce(varLstGet(paramList, 10)), varStr(varLstGet(paramList, 11)), varBoolForce(varLstGet(paramList, 12)),
                varLstSize(paramList) == 14 ? cipherTypeAes256Cbc : cipherTypeNone,
                varLstSize(paramList) == 14 ? varStr(varLstGet(paramList, 13)) : NULL);

            // Return backup result
            VariantList *resultList = varLstNew();
            varLstAdd(resultList, varNewUInt(result.backupCopyResult));
            varLstAdd(resultList, varNewUInt64(result.copySize));
            varLstAdd(resultList, varNewUInt64(result.repoSize));
            varLstAdd(resultList, varNewStr(result.copyChecksum));
            varLstAdd(resultList, result.pageChecksumResult != NULL ? varNewKv(result.pageChecksumResult) : NULL);

            protocolServerResponse(server, varNewVarLst(resultList));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
