/***********************************************************************************************************************************
Common Functions and Definitions for Backup and Expire Commands
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_COMMON_H
#define COMMAND_BACKUP_COMMON_H

#include <stdbool.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Backup type enum and contants
***********************************************************************************************************************************/
typedef enum
{
    backupTypeFull,
    backupTypeDiff,
    backupTypeIncr,
} BackupType;

#define BACKUP_TYPE_FULL                                            "full"
    STRING_DECLARE(BACKUP_TYPE_FULL_STR);
#define BACKUP_TYPE_DIFF                                            "diff"
    STRING_DECLARE(BACKUP_TYPE_DIFF_STR);
#define BACKUP_TYPE_INCR                                            "incr"
    STRING_DECLARE(BACKUP_TYPE_INCR_STR);

/***********************************************************************************************************************************
Returns an anchored regex string for filtering backups based on the type (at least one type is required to be true)
***********************************************************************************************************************************/
typedef struct BackupRegExpParam
{
    bool full;
    bool differential;
    bool incremental;
} BackupRegExpParam;

#define backupRegExpP(...)                                                                                                         \
    backupRegExp((BackupRegExpParam){__VA_ARGS__})

String *backupRegExp(BackupRegExpParam param);

/***********************************************************************************************************************************
Convert text backup type to an enum and back
***********************************************************************************************************************************/
BackupType backupType(const String *type);
const String *backupTypeStr(BackupType type);

#endif
