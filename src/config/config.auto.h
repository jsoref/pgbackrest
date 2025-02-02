/***********************************************************************************************************************************
Command and Option Configuration

Automatically generated by Build.pm -- do not modify directly.
***********************************************************************************************************************************/
#ifndef CONFIG_CONFIG_AUTO_H
#define CONFIG_CONFIG_AUTO_H

/***********************************************************************************************************************************
Command constants
***********************************************************************************************************************************/
#define CFGCMD_ARCHIVE_GET                                          "archive-get"
    STRING_DECLARE(CFGCMD_ARCHIVE_GET_STR);
#define CFGCMD_ARCHIVE_GET_ASYNC                                    "archive-get-async"
    STRING_DECLARE(CFGCMD_ARCHIVE_GET_ASYNC_STR);
#define CFGCMD_ARCHIVE_PUSH                                         "archive-push"
    STRING_DECLARE(CFGCMD_ARCHIVE_PUSH_STR);
#define CFGCMD_ARCHIVE_PUSH_ASYNC                                   "archive-push-async"
    STRING_DECLARE(CFGCMD_ARCHIVE_PUSH_ASYNC_STR);
#define CFGCMD_BACKUP                                               "backup"
    STRING_DECLARE(CFGCMD_BACKUP_STR);
#define CFGCMD_CHECK                                                "check"
    STRING_DECLARE(CFGCMD_CHECK_STR);
#define CFGCMD_EXPIRE                                               "expire"
    STRING_DECLARE(CFGCMD_EXPIRE_STR);
#define CFGCMD_HELP                                                 "help"
    STRING_DECLARE(CFGCMD_HELP_STR);
#define CFGCMD_INFO                                                 "info"
    STRING_DECLARE(CFGCMD_INFO_STR);
#define CFGCMD_LOCAL                                                "local"
    STRING_DECLARE(CFGCMD_LOCAL_STR);
#define CFGCMD_LS                                                   "ls"
    STRING_DECLARE(CFGCMD_LS_STR);
#define CFGCMD_REMOTE                                               "remote"
    STRING_DECLARE(CFGCMD_REMOTE_STR);
#define CFGCMD_RESTORE                                              "restore"
    STRING_DECLARE(CFGCMD_RESTORE_STR);
#define CFGCMD_STANZA_CREATE                                        "stanza-create"
    STRING_DECLARE(CFGCMD_STANZA_CREATE_STR);
#define CFGCMD_STANZA_DELETE                                        "stanza-delete"
    STRING_DECLARE(CFGCMD_STANZA_DELETE_STR);
#define CFGCMD_STANZA_UPGRADE                                       "stanza-upgrade"
    STRING_DECLARE(CFGCMD_STANZA_UPGRADE_STR);
#define CFGCMD_START                                                "start"
    STRING_DECLARE(CFGCMD_START_STR);
#define CFGCMD_STOP                                                 "stop"
    STRING_DECLARE(CFGCMD_STOP_STR);
#define CFGCMD_VERSION                                              "version"
    STRING_DECLARE(CFGCMD_VERSION_STR);

#define CFG_COMMAND_TOTAL                                           20

/***********************************************************************************************************************************
Option constants
***********************************************************************************************************************************/
#define CFGOPT_ARCHIVE_ASYNC                                        "archive-async"
    STRING_DECLARE(CFGOPT_ARCHIVE_ASYNC_STR);
#define CFGOPT_ARCHIVE_CHECK                                        "archive-check"
    STRING_DECLARE(CFGOPT_ARCHIVE_CHECK_STR);
#define CFGOPT_ARCHIVE_COPY                                         "archive-copy"
    STRING_DECLARE(CFGOPT_ARCHIVE_COPY_STR);
#define CFGOPT_ARCHIVE_GET_QUEUE_MAX                                "archive-get-queue-max"
    STRING_DECLARE(CFGOPT_ARCHIVE_GET_QUEUE_MAX_STR);
#define CFGOPT_ARCHIVE_PUSH_QUEUE_MAX                               "archive-push-queue-max"
    STRING_DECLARE(CFGOPT_ARCHIVE_PUSH_QUEUE_MAX_STR);
#define CFGOPT_ARCHIVE_TIMEOUT                                      "archive-timeout"
    STRING_DECLARE(CFGOPT_ARCHIVE_TIMEOUT_STR);
#define CFGOPT_BACKUP_STANDBY                                       "backup-standby"
    STRING_DECLARE(CFGOPT_BACKUP_STANDBY_STR);
#define CFGOPT_BUFFER_SIZE                                          "buffer-size"
    STRING_DECLARE(CFGOPT_BUFFER_SIZE_STR);
#define CFGOPT_C                                                    "c"
    STRING_DECLARE(CFGOPT_C_STR);
#define CFGOPT_CHECKSUM_PAGE                                        "checksum-page"
    STRING_DECLARE(CFGOPT_CHECKSUM_PAGE_STR);
#define CFGOPT_CMD_SSH                                              "cmd-ssh"
    STRING_DECLARE(CFGOPT_CMD_SSH_STR);
#define CFGOPT_COMMAND                                              "command"
    STRING_DECLARE(CFGOPT_COMMAND_STR);
#define CFGOPT_COMPRESS                                             "compress"
    STRING_DECLARE(CFGOPT_COMPRESS_STR);
#define CFGOPT_COMPRESS_LEVEL                                       "compress-level"
    STRING_DECLARE(CFGOPT_COMPRESS_LEVEL_STR);
#define CFGOPT_COMPRESS_LEVEL_NETWORK                               "compress-level-network"
    STRING_DECLARE(CFGOPT_COMPRESS_LEVEL_NETWORK_STR);
#define CFGOPT_CONFIG                                               "config"
    STRING_DECLARE(CFGOPT_CONFIG_STR);
#define CFGOPT_CONFIG_INCLUDE_PATH                                  "config-include-path"
    STRING_DECLARE(CFGOPT_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_CONFIG_PATH                                          "config-path"
    STRING_DECLARE(CFGOPT_CONFIG_PATH_STR);
#define CFGOPT_DB_INCLUDE                                           "db-include"
    STRING_DECLARE(CFGOPT_DB_INCLUDE_STR);
#define CFGOPT_DB_TIMEOUT                                           "db-timeout"
    STRING_DECLARE(CFGOPT_DB_TIMEOUT_STR);
#define CFGOPT_DELTA                                                "delta"
    STRING_DECLARE(CFGOPT_DELTA_STR);
#define CFGOPT_EXCLUDE                                              "exclude"
    STRING_DECLARE(CFGOPT_EXCLUDE_STR);
#define CFGOPT_FILTER                                               "filter"
    STRING_DECLARE(CFGOPT_FILTER_STR);
#define CFGOPT_FORCE                                                "force"
    STRING_DECLARE(CFGOPT_FORCE_STR);
#define CFGOPT_HOST_ID                                              "host-id"
    STRING_DECLARE(CFGOPT_HOST_ID_STR);
#define CFGOPT_LINK_ALL                                             "link-all"
    STRING_DECLARE(CFGOPT_LINK_ALL_STR);
#define CFGOPT_LINK_MAP                                             "link-map"
    STRING_DECLARE(CFGOPT_LINK_MAP_STR);
#define CFGOPT_LOCK_PATH                                            "lock-path"
    STRING_DECLARE(CFGOPT_LOCK_PATH_STR);
#define CFGOPT_LOG_LEVEL_CONSOLE                                    "log-level-console"
    STRING_DECLARE(CFGOPT_LOG_LEVEL_CONSOLE_STR);
#define CFGOPT_LOG_LEVEL_FILE                                       "log-level-file"
    STRING_DECLARE(CFGOPT_LOG_LEVEL_FILE_STR);
#define CFGOPT_LOG_LEVEL_STDERR                                     "log-level-stderr"
    STRING_DECLARE(CFGOPT_LOG_LEVEL_STDERR_STR);
#define CFGOPT_LOG_PATH                                             "log-path"
    STRING_DECLARE(CFGOPT_LOG_PATH_STR);
#define CFGOPT_LOG_SUBPROCESS                                       "log-subprocess"
    STRING_DECLARE(CFGOPT_LOG_SUBPROCESS_STR);
#define CFGOPT_LOG_TIMESTAMP                                        "log-timestamp"
    STRING_DECLARE(CFGOPT_LOG_TIMESTAMP_STR);
#define CFGOPT_MANIFEST_SAVE_THRESHOLD                              "manifest-save-threshold"
    STRING_DECLARE(CFGOPT_MANIFEST_SAVE_THRESHOLD_STR);
#define CFGOPT_NEUTRAL_UMASK                                        "neutral-umask"
    STRING_DECLARE(CFGOPT_NEUTRAL_UMASK_STR);
#define CFGOPT_ONLINE                                               "online"
    STRING_DECLARE(CFGOPT_ONLINE_STR);
#define CFGOPT_OUTPUT                                               "output"
    STRING_DECLARE(CFGOPT_OUTPUT_STR);
#define CFGOPT_PERL_OPTION                                          "perl-option"
    STRING_DECLARE(CFGOPT_PERL_OPTION_STR);
#define CFGOPT_PG1_HOST                                             "pg1-host"
    STRING_DECLARE(CFGOPT_PG1_HOST_STR);
#define CFGOPT_PG1_HOST_CMD                                         "pg1-host-cmd"
    STRING_DECLARE(CFGOPT_PG1_HOST_CMD_STR);
#define CFGOPT_PG1_HOST_CONFIG                                      "pg1-host-config"
    STRING_DECLARE(CFGOPT_PG1_HOST_CONFIG_STR);
#define CFGOPT_PG1_HOST_CONFIG_INCLUDE_PATH                         "pg1-host-config-include-path"
    STRING_DECLARE(CFGOPT_PG1_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_PG1_HOST_CONFIG_PATH                                 "pg1-host-config-path"
    STRING_DECLARE(CFGOPT_PG1_HOST_CONFIG_PATH_STR);
#define CFGOPT_PG1_HOST_PORT                                        "pg1-host-port"
    STRING_DECLARE(CFGOPT_PG1_HOST_PORT_STR);
#define CFGOPT_PG1_HOST_USER                                        "pg1-host-user"
    STRING_DECLARE(CFGOPT_PG1_HOST_USER_STR);
#define CFGOPT_PG1_PATH                                             "pg1-path"
    STRING_DECLARE(CFGOPT_PG1_PATH_STR);
#define CFGOPT_PG1_PORT                                             "pg1-port"
    STRING_DECLARE(CFGOPT_PG1_PORT_STR);
#define CFGOPT_PG1_SOCKET_PATH                                      "pg1-socket-path"
    STRING_DECLARE(CFGOPT_PG1_SOCKET_PATH_STR);
#define CFGOPT_PG2_HOST                                             "pg2-host"
    STRING_DECLARE(CFGOPT_PG2_HOST_STR);
#define CFGOPT_PG2_HOST_CMD                                         "pg2-host-cmd"
    STRING_DECLARE(CFGOPT_PG2_HOST_CMD_STR);
#define CFGOPT_PG2_HOST_CONFIG                                      "pg2-host-config"
    STRING_DECLARE(CFGOPT_PG2_HOST_CONFIG_STR);
#define CFGOPT_PG2_HOST_CONFIG_INCLUDE_PATH                         "pg2-host-config-include-path"
    STRING_DECLARE(CFGOPT_PG2_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_PG2_HOST_CONFIG_PATH                                 "pg2-host-config-path"
    STRING_DECLARE(CFGOPT_PG2_HOST_CONFIG_PATH_STR);
#define CFGOPT_PG2_HOST_PORT                                        "pg2-host-port"
    STRING_DECLARE(CFGOPT_PG2_HOST_PORT_STR);
#define CFGOPT_PG2_HOST_USER                                        "pg2-host-user"
    STRING_DECLARE(CFGOPT_PG2_HOST_USER_STR);
#define CFGOPT_PG2_PATH                                             "pg2-path"
    STRING_DECLARE(CFGOPT_PG2_PATH_STR);
#define CFGOPT_PG2_PORT                                             "pg2-port"
    STRING_DECLARE(CFGOPT_PG2_PORT_STR);
#define CFGOPT_PG2_SOCKET_PATH                                      "pg2-socket-path"
    STRING_DECLARE(CFGOPT_PG2_SOCKET_PATH_STR);
#define CFGOPT_PG3_HOST                                             "pg3-host"
    STRING_DECLARE(CFGOPT_PG3_HOST_STR);
#define CFGOPT_PG3_HOST_CMD                                         "pg3-host-cmd"
    STRING_DECLARE(CFGOPT_PG3_HOST_CMD_STR);
#define CFGOPT_PG3_HOST_CONFIG                                      "pg3-host-config"
    STRING_DECLARE(CFGOPT_PG3_HOST_CONFIG_STR);
#define CFGOPT_PG3_HOST_CONFIG_INCLUDE_PATH                         "pg3-host-config-include-path"
    STRING_DECLARE(CFGOPT_PG3_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_PG3_HOST_CONFIG_PATH                                 "pg3-host-config-path"
    STRING_DECLARE(CFGOPT_PG3_HOST_CONFIG_PATH_STR);
#define CFGOPT_PG3_HOST_PORT                                        "pg3-host-port"
    STRING_DECLARE(CFGOPT_PG3_HOST_PORT_STR);
#define CFGOPT_PG3_HOST_USER                                        "pg3-host-user"
    STRING_DECLARE(CFGOPT_PG3_HOST_USER_STR);
#define CFGOPT_PG3_PATH                                             "pg3-path"
    STRING_DECLARE(CFGOPT_PG3_PATH_STR);
#define CFGOPT_PG3_PORT                                             "pg3-port"
    STRING_DECLARE(CFGOPT_PG3_PORT_STR);
#define CFGOPT_PG3_SOCKET_PATH                                      "pg3-socket-path"
    STRING_DECLARE(CFGOPT_PG3_SOCKET_PATH_STR);
#define CFGOPT_PG4_HOST                                             "pg4-host"
    STRING_DECLARE(CFGOPT_PG4_HOST_STR);
#define CFGOPT_PG4_HOST_CMD                                         "pg4-host-cmd"
    STRING_DECLARE(CFGOPT_PG4_HOST_CMD_STR);
#define CFGOPT_PG4_HOST_CONFIG                                      "pg4-host-config"
    STRING_DECLARE(CFGOPT_PG4_HOST_CONFIG_STR);
#define CFGOPT_PG4_HOST_CONFIG_INCLUDE_PATH                         "pg4-host-config-include-path"
    STRING_DECLARE(CFGOPT_PG4_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_PG4_HOST_CONFIG_PATH                                 "pg4-host-config-path"
    STRING_DECLARE(CFGOPT_PG4_HOST_CONFIG_PATH_STR);
#define CFGOPT_PG4_HOST_PORT                                        "pg4-host-port"
    STRING_DECLARE(CFGOPT_PG4_HOST_PORT_STR);
#define CFGOPT_PG4_HOST_USER                                        "pg4-host-user"
    STRING_DECLARE(CFGOPT_PG4_HOST_USER_STR);
#define CFGOPT_PG4_PATH                                             "pg4-path"
    STRING_DECLARE(CFGOPT_PG4_PATH_STR);
#define CFGOPT_PG4_PORT                                             "pg4-port"
    STRING_DECLARE(CFGOPT_PG4_PORT_STR);
#define CFGOPT_PG4_SOCKET_PATH                                      "pg4-socket-path"
    STRING_DECLARE(CFGOPT_PG4_SOCKET_PATH_STR);
#define CFGOPT_PG5_HOST                                             "pg5-host"
    STRING_DECLARE(CFGOPT_PG5_HOST_STR);
#define CFGOPT_PG5_HOST_CMD                                         "pg5-host-cmd"
    STRING_DECLARE(CFGOPT_PG5_HOST_CMD_STR);
#define CFGOPT_PG5_HOST_CONFIG                                      "pg5-host-config"
    STRING_DECLARE(CFGOPT_PG5_HOST_CONFIG_STR);
#define CFGOPT_PG5_HOST_CONFIG_INCLUDE_PATH                         "pg5-host-config-include-path"
    STRING_DECLARE(CFGOPT_PG5_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_PG5_HOST_CONFIG_PATH                                 "pg5-host-config-path"
    STRING_DECLARE(CFGOPT_PG5_HOST_CONFIG_PATH_STR);
#define CFGOPT_PG5_HOST_PORT                                        "pg5-host-port"
    STRING_DECLARE(CFGOPT_PG5_HOST_PORT_STR);
#define CFGOPT_PG5_HOST_USER                                        "pg5-host-user"
    STRING_DECLARE(CFGOPT_PG5_HOST_USER_STR);
#define CFGOPT_PG5_PATH                                             "pg5-path"
    STRING_DECLARE(CFGOPT_PG5_PATH_STR);
#define CFGOPT_PG5_PORT                                             "pg5-port"
    STRING_DECLARE(CFGOPT_PG5_PORT_STR);
#define CFGOPT_PG5_SOCKET_PATH                                      "pg5-socket-path"
    STRING_DECLARE(CFGOPT_PG5_SOCKET_PATH_STR);
#define CFGOPT_PG6_HOST                                             "pg6-host"
    STRING_DECLARE(CFGOPT_PG6_HOST_STR);
#define CFGOPT_PG6_HOST_CMD                                         "pg6-host-cmd"
    STRING_DECLARE(CFGOPT_PG6_HOST_CMD_STR);
#define CFGOPT_PG6_HOST_CONFIG                                      "pg6-host-config"
    STRING_DECLARE(CFGOPT_PG6_HOST_CONFIG_STR);
#define CFGOPT_PG6_HOST_CONFIG_INCLUDE_PATH                         "pg6-host-config-include-path"
    STRING_DECLARE(CFGOPT_PG6_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_PG6_HOST_CONFIG_PATH                                 "pg6-host-config-path"
    STRING_DECLARE(CFGOPT_PG6_HOST_CONFIG_PATH_STR);
#define CFGOPT_PG6_HOST_PORT                                        "pg6-host-port"
    STRING_DECLARE(CFGOPT_PG6_HOST_PORT_STR);
#define CFGOPT_PG6_HOST_USER                                        "pg6-host-user"
    STRING_DECLARE(CFGOPT_PG6_HOST_USER_STR);
#define CFGOPT_PG6_PATH                                             "pg6-path"
    STRING_DECLARE(CFGOPT_PG6_PATH_STR);
#define CFGOPT_PG6_PORT                                             "pg6-port"
    STRING_DECLARE(CFGOPT_PG6_PORT_STR);
#define CFGOPT_PG6_SOCKET_PATH                                      "pg6-socket-path"
    STRING_DECLARE(CFGOPT_PG6_SOCKET_PATH_STR);
#define CFGOPT_PG7_HOST                                             "pg7-host"
    STRING_DECLARE(CFGOPT_PG7_HOST_STR);
#define CFGOPT_PG7_HOST_CMD                                         "pg7-host-cmd"
    STRING_DECLARE(CFGOPT_PG7_HOST_CMD_STR);
#define CFGOPT_PG7_HOST_CONFIG                                      "pg7-host-config"
    STRING_DECLARE(CFGOPT_PG7_HOST_CONFIG_STR);
#define CFGOPT_PG7_HOST_CONFIG_INCLUDE_PATH                         "pg7-host-config-include-path"
    STRING_DECLARE(CFGOPT_PG7_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_PG7_HOST_CONFIG_PATH                                 "pg7-host-config-path"
    STRING_DECLARE(CFGOPT_PG7_HOST_CONFIG_PATH_STR);
#define CFGOPT_PG7_HOST_PORT                                        "pg7-host-port"
    STRING_DECLARE(CFGOPT_PG7_HOST_PORT_STR);
#define CFGOPT_PG7_HOST_USER                                        "pg7-host-user"
    STRING_DECLARE(CFGOPT_PG7_HOST_USER_STR);
#define CFGOPT_PG7_PATH                                             "pg7-path"
    STRING_DECLARE(CFGOPT_PG7_PATH_STR);
#define CFGOPT_PG7_PORT                                             "pg7-port"
    STRING_DECLARE(CFGOPT_PG7_PORT_STR);
#define CFGOPT_PG7_SOCKET_PATH                                      "pg7-socket-path"
    STRING_DECLARE(CFGOPT_PG7_SOCKET_PATH_STR);
#define CFGOPT_PG8_HOST                                             "pg8-host"
    STRING_DECLARE(CFGOPT_PG8_HOST_STR);
#define CFGOPT_PG8_HOST_CMD                                         "pg8-host-cmd"
    STRING_DECLARE(CFGOPT_PG8_HOST_CMD_STR);
#define CFGOPT_PG8_HOST_CONFIG                                      "pg8-host-config"
    STRING_DECLARE(CFGOPT_PG8_HOST_CONFIG_STR);
#define CFGOPT_PG8_HOST_CONFIG_INCLUDE_PATH                         "pg8-host-config-include-path"
    STRING_DECLARE(CFGOPT_PG8_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_PG8_HOST_CONFIG_PATH                                 "pg8-host-config-path"
    STRING_DECLARE(CFGOPT_PG8_HOST_CONFIG_PATH_STR);
#define CFGOPT_PG8_HOST_PORT                                        "pg8-host-port"
    STRING_DECLARE(CFGOPT_PG8_HOST_PORT_STR);
#define CFGOPT_PG8_HOST_USER                                        "pg8-host-user"
    STRING_DECLARE(CFGOPT_PG8_HOST_USER_STR);
#define CFGOPT_PG8_PATH                                             "pg8-path"
    STRING_DECLARE(CFGOPT_PG8_PATH_STR);
#define CFGOPT_PG8_PORT                                             "pg8-port"
    STRING_DECLARE(CFGOPT_PG8_PORT_STR);
#define CFGOPT_PG8_SOCKET_PATH                                      "pg8-socket-path"
    STRING_DECLARE(CFGOPT_PG8_SOCKET_PATH_STR);
#define CFGOPT_PROCESS                                              "process"
    STRING_DECLARE(CFGOPT_PROCESS_STR);
#define CFGOPT_PROCESS_MAX                                          "process-max"
    STRING_DECLARE(CFGOPT_PROCESS_MAX_STR);
#define CFGOPT_PROTOCOL_TIMEOUT                                     "protocol-timeout"
    STRING_DECLARE(CFGOPT_PROTOCOL_TIMEOUT_STR);
#define CFGOPT_RECOVERY_OPTION                                      "recovery-option"
    STRING_DECLARE(CFGOPT_RECOVERY_OPTION_STR);
#define CFGOPT_REPO1_CIPHER_PASS                                    "repo1-cipher-pass"
    STRING_DECLARE(CFGOPT_REPO1_CIPHER_PASS_STR);
#define CFGOPT_REPO1_CIPHER_TYPE                                    "repo1-cipher-type"
    STRING_DECLARE(CFGOPT_REPO1_CIPHER_TYPE_STR);
#define CFGOPT_REPO1_HARDLINK                                       "repo1-hardlink"
    STRING_DECLARE(CFGOPT_REPO1_HARDLINK_STR);
#define CFGOPT_REPO1_HOST                                           "repo1-host"
    STRING_DECLARE(CFGOPT_REPO1_HOST_STR);
#define CFGOPT_REPO1_HOST_CMD                                       "repo1-host-cmd"
    STRING_DECLARE(CFGOPT_REPO1_HOST_CMD_STR);
#define CFGOPT_REPO1_HOST_CONFIG                                    "repo1-host-config"
    STRING_DECLARE(CFGOPT_REPO1_HOST_CONFIG_STR);
#define CFGOPT_REPO1_HOST_CONFIG_INCLUDE_PATH                       "repo1-host-config-include-path"
    STRING_DECLARE(CFGOPT_REPO1_HOST_CONFIG_INCLUDE_PATH_STR);
#define CFGOPT_REPO1_HOST_CONFIG_PATH                               "repo1-host-config-path"
    STRING_DECLARE(CFGOPT_REPO1_HOST_CONFIG_PATH_STR);
#define CFGOPT_REPO1_HOST_PORT                                      "repo1-host-port"
    STRING_DECLARE(CFGOPT_REPO1_HOST_PORT_STR);
#define CFGOPT_REPO1_HOST_USER                                      "repo1-host-user"
    STRING_DECLARE(CFGOPT_REPO1_HOST_USER_STR);
#define CFGOPT_REPO1_PATH                                           "repo1-path"
    STRING_DECLARE(CFGOPT_REPO1_PATH_STR);
#define CFGOPT_REPO1_RETENTION_ARCHIVE                              "repo1-retention-archive"
    STRING_DECLARE(CFGOPT_REPO1_RETENTION_ARCHIVE_STR);
#define CFGOPT_REPO1_RETENTION_ARCHIVE_TYPE                         "repo1-retention-archive-type"
    STRING_DECLARE(CFGOPT_REPO1_RETENTION_ARCHIVE_TYPE_STR);
#define CFGOPT_REPO1_RETENTION_DIFF                                 "repo1-retention-diff"
    STRING_DECLARE(CFGOPT_REPO1_RETENTION_DIFF_STR);
#define CFGOPT_REPO1_RETENTION_FULL                                 "repo1-retention-full"
    STRING_DECLARE(CFGOPT_REPO1_RETENTION_FULL_STR);
#define CFGOPT_REPO1_S3_BUCKET                                      "repo1-s3-bucket"
    STRING_DECLARE(CFGOPT_REPO1_S3_BUCKET_STR);
#define CFGOPT_REPO1_S3_CA_FILE                                     "repo1-s3-ca-file"
    STRING_DECLARE(CFGOPT_REPO1_S3_CA_FILE_STR);
#define CFGOPT_REPO1_S3_CA_PATH                                     "repo1-s3-ca-path"
    STRING_DECLARE(CFGOPT_REPO1_S3_CA_PATH_STR);
#define CFGOPT_REPO1_S3_ENDPOINT                                    "repo1-s3-endpoint"
    STRING_DECLARE(CFGOPT_REPO1_S3_ENDPOINT_STR);
#define CFGOPT_REPO1_S3_HOST                                        "repo1-s3-host"
    STRING_DECLARE(CFGOPT_REPO1_S3_HOST_STR);
#define CFGOPT_REPO1_S3_KEY                                         "repo1-s3-key"
    STRING_DECLARE(CFGOPT_REPO1_S3_KEY_STR);
#define CFGOPT_REPO1_S3_KEY_SECRET                                  "repo1-s3-key-secret"
    STRING_DECLARE(CFGOPT_REPO1_S3_KEY_SECRET_STR);
#define CFGOPT_REPO1_S3_PORT                                        "repo1-s3-port"
    STRING_DECLARE(CFGOPT_REPO1_S3_PORT_STR);
#define CFGOPT_REPO1_S3_REGION                                      "repo1-s3-region"
    STRING_DECLARE(CFGOPT_REPO1_S3_REGION_STR);
#define CFGOPT_REPO1_S3_TOKEN                                       "repo1-s3-token"
    STRING_DECLARE(CFGOPT_REPO1_S3_TOKEN_STR);
#define CFGOPT_REPO1_S3_VERIFY_TLS                                  "repo1-s3-verify-tls"
    STRING_DECLARE(CFGOPT_REPO1_S3_VERIFY_TLS_STR);
#define CFGOPT_REPO1_TYPE                                           "repo1-type"
    STRING_DECLARE(CFGOPT_REPO1_TYPE_STR);
#define CFGOPT_RESUME                                               "resume"
    STRING_DECLARE(CFGOPT_RESUME_STR);
#define CFGOPT_SET                                                  "set"
    STRING_DECLARE(CFGOPT_SET_STR);
#define CFGOPT_SORT                                                 "sort"
    STRING_DECLARE(CFGOPT_SORT_STR);
#define CFGOPT_SPOOL_PATH                                           "spool-path"
    STRING_DECLARE(CFGOPT_SPOOL_PATH_STR);
#define CFGOPT_STANZA                                               "stanza"
    STRING_DECLARE(CFGOPT_STANZA_STR);
#define CFGOPT_START_FAST                                           "start-fast"
    STRING_DECLARE(CFGOPT_START_FAST_STR);
#define CFGOPT_STOP_AUTO                                            "stop-auto"
    STRING_DECLARE(CFGOPT_STOP_AUTO_STR);
#define CFGOPT_TABLESPACE_MAP                                       "tablespace-map"
    STRING_DECLARE(CFGOPT_TABLESPACE_MAP_STR);
#define CFGOPT_TABLESPACE_MAP_ALL                                   "tablespace-map-all"
    STRING_DECLARE(CFGOPT_TABLESPACE_MAP_ALL_STR);
#define CFGOPT_TARGET                                               "target"
    STRING_DECLARE(CFGOPT_TARGET_STR);
#define CFGOPT_TARGET_ACTION                                        "target-action"
    STRING_DECLARE(CFGOPT_TARGET_ACTION_STR);
#define CFGOPT_TARGET_EXCLUSIVE                                     "target-exclusive"
    STRING_DECLARE(CFGOPT_TARGET_EXCLUSIVE_STR);
#define CFGOPT_TARGET_TIMELINE                                      "target-timeline"
    STRING_DECLARE(CFGOPT_TARGET_TIMELINE_STR);
#define CFGOPT_TEST                                                 "test"
    STRING_DECLARE(CFGOPT_TEST_STR);
#define CFGOPT_TEST_DELAY                                           "test-delay"
    STRING_DECLARE(CFGOPT_TEST_DELAY_STR);
#define CFGOPT_TEST_POINT                                           "test-point"
    STRING_DECLARE(CFGOPT_TEST_POINT_STR);
#define CFGOPT_TYPE                                                 "type"
    STRING_DECLARE(CFGOPT_TYPE_STR);

#define CFG_OPTION_TOTAL                                            167

/***********************************************************************************************************************************
Command enum
***********************************************************************************************************************************/
typedef enum
{
    cfgCmdArchiveGet,
    cfgCmdArchiveGetAsync,
    cfgCmdArchivePush,
    cfgCmdArchivePushAsync,
    cfgCmdBackup,
    cfgCmdCheck,
    cfgCmdExpire,
    cfgCmdHelp,
    cfgCmdInfo,
    cfgCmdLocal,
    cfgCmdLs,
    cfgCmdRemote,
    cfgCmdRestore,
    cfgCmdStanzaCreate,
    cfgCmdStanzaDelete,
    cfgCmdStanzaUpgrade,
    cfgCmdStart,
    cfgCmdStop,
    cfgCmdVersion,
    cfgCmdNone,
} ConfigCommand;

/***********************************************************************************************************************************
Option enum
***********************************************************************************************************************************/
typedef enum
{
    cfgOptArchiveAsync,
    cfgOptArchiveCheck,
    cfgOptArchiveCopy,
    cfgOptArchiveGetQueueMax,
    cfgOptArchivePushQueueMax,
    cfgOptArchiveTimeout,
    cfgOptBackupStandby,
    cfgOptBufferSize,
    cfgOptC,
    cfgOptChecksumPage,
    cfgOptCmdSsh,
    cfgOptCommand,
    cfgOptCompress,
    cfgOptCompressLevel,
    cfgOptCompressLevelNetwork,
    cfgOptConfig,
    cfgOptConfigIncludePath,
    cfgOptConfigPath,
    cfgOptDbInclude,
    cfgOptDbTimeout,
    cfgOptDelta,
    cfgOptExclude,
    cfgOptFilter,
    cfgOptForce,
    cfgOptHostId,
    cfgOptLinkAll,
    cfgOptLinkMap,
    cfgOptLockPath,
    cfgOptLogLevelConsole,
    cfgOptLogLevelFile,
    cfgOptLogLevelStderr,
    cfgOptLogPath,
    cfgOptLogSubprocess,
    cfgOptLogTimestamp,
    cfgOptManifestSaveThreshold,
    cfgOptNeutralUmask,
    cfgOptOnline,
    cfgOptOutput,
    cfgOptPerlOption,
    cfgOptPgHost,
    cfgOptPgHost2,
    cfgOptPgHost3,
    cfgOptPgHost4,
    cfgOptPgHost5,
    cfgOptPgHost6,
    cfgOptPgHost7,
    cfgOptPgHost8,
    cfgOptPgHostCmd,
    cfgOptPgHostCmd2,
    cfgOptPgHostCmd3,
    cfgOptPgHostCmd4,
    cfgOptPgHostCmd5,
    cfgOptPgHostCmd6,
    cfgOptPgHostCmd7,
    cfgOptPgHostCmd8,
    cfgOptPgHostConfig,
    cfgOptPgHostConfig2,
    cfgOptPgHostConfig3,
    cfgOptPgHostConfig4,
    cfgOptPgHostConfig5,
    cfgOptPgHostConfig6,
    cfgOptPgHostConfig7,
    cfgOptPgHostConfig8,
    cfgOptPgHostConfigIncludePath,
    cfgOptPgHostConfigIncludePath2,
    cfgOptPgHostConfigIncludePath3,
    cfgOptPgHostConfigIncludePath4,
    cfgOptPgHostConfigIncludePath5,
    cfgOptPgHostConfigIncludePath6,
    cfgOptPgHostConfigIncludePath7,
    cfgOptPgHostConfigIncludePath8,
    cfgOptPgHostConfigPath,
    cfgOptPgHostConfigPath2,
    cfgOptPgHostConfigPath3,
    cfgOptPgHostConfigPath4,
    cfgOptPgHostConfigPath5,
    cfgOptPgHostConfigPath6,
    cfgOptPgHostConfigPath7,
    cfgOptPgHostConfigPath8,
    cfgOptPgHostPort,
    cfgOptPgHostPort2,
    cfgOptPgHostPort3,
    cfgOptPgHostPort4,
    cfgOptPgHostPort5,
    cfgOptPgHostPort6,
    cfgOptPgHostPort7,
    cfgOptPgHostPort8,
    cfgOptPgHostUser,
    cfgOptPgHostUser2,
    cfgOptPgHostUser3,
    cfgOptPgHostUser4,
    cfgOptPgHostUser5,
    cfgOptPgHostUser6,
    cfgOptPgHostUser7,
    cfgOptPgHostUser8,
    cfgOptPgPath,
    cfgOptPgPath2,
    cfgOptPgPath3,
    cfgOptPgPath4,
    cfgOptPgPath5,
    cfgOptPgPath6,
    cfgOptPgPath7,
    cfgOptPgPath8,
    cfgOptPgPort,
    cfgOptPgPort2,
    cfgOptPgPort3,
    cfgOptPgPort4,
    cfgOptPgPort5,
    cfgOptPgPort6,
    cfgOptPgPort7,
    cfgOptPgPort8,
    cfgOptPgSocketPath,
    cfgOptPgSocketPath2,
    cfgOptPgSocketPath3,
    cfgOptPgSocketPath4,
    cfgOptPgSocketPath5,
    cfgOptPgSocketPath6,
    cfgOptPgSocketPath7,
    cfgOptPgSocketPath8,
    cfgOptProcess,
    cfgOptProcessMax,
    cfgOptProtocolTimeout,
    cfgOptRecoveryOption,
    cfgOptRepoCipherPass,
    cfgOptRepoCipherType,
    cfgOptRepoHardlink,
    cfgOptRepoHost,
    cfgOptRepoHostCmd,
    cfgOptRepoHostConfig,
    cfgOptRepoHostConfigIncludePath,
    cfgOptRepoHostConfigPath,
    cfgOptRepoHostPort,
    cfgOptRepoHostUser,
    cfgOptRepoPath,
    cfgOptRepoRetentionArchive,
    cfgOptRepoRetentionArchiveType,
    cfgOptRepoRetentionDiff,
    cfgOptRepoRetentionFull,
    cfgOptRepoS3Bucket,
    cfgOptRepoS3CaFile,
    cfgOptRepoS3CaPath,
    cfgOptRepoS3Endpoint,
    cfgOptRepoS3Host,
    cfgOptRepoS3Key,
    cfgOptRepoS3KeySecret,
    cfgOptRepoS3Port,
    cfgOptRepoS3Region,
    cfgOptRepoS3Token,
    cfgOptRepoS3VerifyTls,
    cfgOptRepoType,
    cfgOptResume,
    cfgOptSet,
    cfgOptSort,
    cfgOptSpoolPath,
    cfgOptStanza,
    cfgOptStartFast,
    cfgOptStopAuto,
    cfgOptTablespaceMap,
    cfgOptTablespaceMapAll,
    cfgOptTarget,
    cfgOptTargetAction,
    cfgOptTargetExclusive,
    cfgOptTargetTimeline,
    cfgOptTest,
    cfgOptTestDelay,
    cfgOptTestPoint,
    cfgOptType,
} ConfigOption;

#endif
