####################################################################################################################################
# FullCommonTest.pm - Common code for backup tests
####################################################################################################################################
package pgBackRestTest::Env::HostEnvTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRest::Archive::Common;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::LibC qw(:crypto);
use pgBackRest::Protocol::Storage::Helper;

use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostDbCommonTest;
use pgBackRestTest::Env::Host::HostDbTest;
use pgBackRestTest::Env::Host::HostDbSyntheticTest;
use pgBackRestTest::Env::Host::HostS3Test;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant ENCRYPTION_KEY_ARCHIVE                              => 'archive';
    push @EXPORT, qw(ENCRYPTION_KEY_ARCHIVE);
use constant ENCRYPTION_KEY_MANIFEST                             => 'manifest';
    push @EXPORT, qw(ENCRYPTION_KEY_MANIFEST);
use constant ENCRYPTION_KEY_BACKUPSET                            => 'backupset';
    push @EXPORT, qw(ENCRYPTION_KEY_BACKUPSET);

####################################################################################################################################
# setup
####################################################################################################################################
sub setup
{
    my $self = shift;
    my $bSynthetic = shift;
    my $oLogTest = shift;
    my $oConfigParam = shift;

    # Start S3 server first since it takes the longest
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oHostS3;

    if ($oConfigParam->{bS3})
    {
        $oHostS3 = new pgBackRestTest::Env::Host::HostS3Test();
    }

    # Get host group
    my $oHostGroup = hostGroupGet();

    # Create the backup host
    my $strBackupDestination;
    my $bHostBackup = defined($$oConfigParam{bHostBackup}) ? $$oConfigParam{bHostBackup} : false;
    my $oHostBackup = undef;

    my $bRepoEncrypt = defined($$oConfigParam{bRepoEncrypt}) ? $$oConfigParam{bRepoEncrypt} : false;

    if ($bHostBackup)
    {
        $strBackupDestination = defined($$oConfigParam{strBackupDestination}) ? $$oConfigParam{strBackupDestination} : HOST_BACKUP;

        $oHostBackup = new pgBackRestTest::Env::Host::HostBackupTest(
            {strBackupDestination => $strBackupDestination, bSynthetic => $bSynthetic, oLogTest => $oLogTest,
                bRepoLocal => !$oConfigParam->{bS3}, bRepoEncrypt => $bRepoEncrypt});
        $oHostGroup->hostAdd($oHostBackup);
    }
    else
    {
        $strBackupDestination =
            defined($$oConfigParam{strBackupDestination}) ? $$oConfigParam{strBackupDestination} : HOST_DB_MASTER;
    }

    # Create the db-master host
    my $oHostDbMaster = undef;

    if ($bSynthetic)
    {
        $oHostDbMaster = new pgBackRestTest::Env::Host::HostDbSyntheticTest(
            {strBackupDestination => $strBackupDestination, oLogTest => $oLogTest, bRepoLocal => !$oConfigParam->{bS3},
                bRepoEncrypt => $bRepoEncrypt});
    }
    else
    {
        $oHostDbMaster = new pgBackRestTest::Env::Host::HostDbTest(
            {strBackupDestination => $strBackupDestination, oLogTest => $oLogTest, bRepoLocal => !$oConfigParam->{bS3},
                bRepoEncrypt => $bRepoEncrypt});
    }

    $oHostGroup->hostAdd($oHostDbMaster);

    # Create the db-standby host
    my $oHostDbStandby = undef;

    if (defined($$oConfigParam{bStandby}) && $$oConfigParam{bStandby})
    {
        $oHostDbStandby = new pgBackRestTest::Env::Host::HostDbTest(
            {strBackupDestination => $strBackupDestination, bStandby => true, oLogTest => $oLogTest,
                bRepoLocal => !$oConfigParam->{bS3}});

        $oHostGroup->hostAdd($oHostDbStandby);
    }

    # Finalize S3 server
    #-------------------------------------------------------------------------------------------------------------------------------
    if (defined($oHostS3))
    {
        $oHostGroup->hostAdd($oHostS3, {rstryHostName => ['pgbackrest-dev.s3.amazonaws.com', 's3.amazonaws.com']});
    }

    # Create db master config
    $oHostDbMaster->configCreate({
        strBackupSource => $$oConfigParam{strBackupSource},
        bCompress => $$oConfigParam{bCompress},
        bHardlink => $bHostBackup ? undef : $$oConfigParam{bHardLink},
        bArchiveAsync => $$oConfigParam{bArchiveAsync},
        bS3 => $$oConfigParam{bS3}});

    # Create backup config if backup host exists
    if (defined($oHostBackup))
    {
        $oHostBackup->configCreate({
            bCompress => $$oConfigParam{bCompress},
            bHardlink => $$oConfigParam{bHardLink},
            bS3 => $$oConfigParam{bS3}});
    }
    # If backup host is not defined set it to db-master
    else
    {
        $oHostBackup = $strBackupDestination eq HOST_DB_MASTER ? $oHostDbMaster : $oHostDbStandby;
    }

    # Create db-standby config
    if (defined($oHostDbStandby))
    {
        $oHostDbStandby->configCreate({
            strBackupSource => $$oConfigParam{strBackupSource},
            bCompress => $$oConfigParam{bCompress},
            bHardlink => $bHostBackup ? undef : $$oConfigParam{bHardLink},
            bArchiveAsync => $$oConfigParam{bArchiveAsync}});
    }

    # Set options needed for storage helper
    $self->optionTestSet(CFGOPT_PG_PATH, $oHostDbMaster->dbBasePath());
    $self->optionTestSet(CFGOPT_REPO_PATH, $oHostBackup->repoPath());
    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());

    # Configure the repo to be encrypted if required
    if ($bRepoEncrypt)
    {
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
    }

    # Set S3 options
    if (defined($oHostS3))
    {
        $self->optionTestSet(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_S3);
        $self->optionTestSet(CFGOPT_REPO_S3_KEY, HOST_S3_ACCESS_KEY);
        $self->optionTestSet(CFGOPT_REPO_S3_KEY_SECRET, HOST_S3_ACCESS_SECRET_KEY);
        $self->optionTestSet(CFGOPT_REPO_S3_BUCKET, HOST_S3_BUCKET);
        $self->optionTestSet(CFGOPT_REPO_S3_ENDPOINT, HOST_S3_ENDPOINT);
        $self->optionTestSet(CFGOPT_REPO_S3_REGION, HOST_S3_REGION);
        $self->optionTestSet(CFGOPT_REPO_S3_HOST, $oHostS3->ipGet());
        $self->optionTestSetBool(CFGOPT_REPO_S3_VERIFY_TLS, false);
    }

    $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

    # Create S3 bucket
    if (defined($oHostS3))
    {
        storageRepo()->{oStorageC}->bucketCreate();
    }

    return $oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3;
}

####################################################################################################################################
# Generate database system id for the db version
####################################################################################################################################
sub dbSysId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->dbSysId', \@_,
            {name => 'strPgVersion', trace => true},
        );

    return (1000000000000000000 + ($strPgVersion * 10));
}

####################################################################################################################################
# Get database catalog version for the db version
####################################################################################################################################
sub dbCatalogVersion
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->sysId', \@_,
            {name => 'strPgVersion', trace => true},
        );

    my $hCatalogVersion =
    {
        &PG_VERSION_83 => 200711281,
        &PG_VERSION_84 => 200904091,
        &PG_VERSION_90 => 201008051,
        &PG_VERSION_91 => 201105231,
        &PG_VERSION_92 => 201204301,
        &PG_VERSION_93 => 201306121,
        &PG_VERSION_94 => 201409291,
        &PG_VERSION_95 => 201510051,
        &PG_VERSION_96 => 201608131,
        &PG_VERSION_10 => 201707211,
        &PG_VERSION_11 => 201806231,
    };

    if (!defined($hCatalogVersion->{$strPgVersion}))
    {
        confess &log(ASSERT, "no catalog version defined for pg version ${strPgVersion}");
    }

    return $hCatalogVersion->{$strPgVersion};
}

####################################################################################################################################
# Get database control version for the db version
####################################################################################################################################
sub dbControlVersion
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->dbControlVersion', \@_,
            {name => 'strPgVersion', trace => true},
        );

    my $hControlVersion =
    {
        &PG_VERSION_83 => 833,
        &PG_VERSION_84 => 843,
        &PG_VERSION_90 => 903,
        &PG_VERSION_91 => 903,
        &PG_VERSION_92 => 922,
        &PG_VERSION_93 => 937,
        &PG_VERSION_94 => 942,
        &PG_VERSION_95 => 942,
        &PG_VERSION_96 => 960,
        &PG_VERSION_10 => 1002,
    };

    if (!defined($hControlVersion->{$strPgVersion}))
    {
        confess &log(ASSERT, "no control version defined for pg version ${strPgVersion}");
    }

    return $hControlVersion->{$strPgVersion};
}

####################################################################################################################################
# Generate control file content
####################################################################################################################################
sub controlGenerateContent
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->controlGenerateContent', \@_,
            {name => 'strPgVersion', trace => true},
        );

    my $tControlContent = pack('Q', $self->dbSysId($strPgVersion));
    $tControlContent .= pack('L', $self->dbControlVersion($strPgVersion));
    $tControlContent .= pack('L', $self->dbCatalogVersion($strPgVersion));

    # Offset to page size by archecture bits and version
    my $rhOffsetToPageSize =
    {
        32 =>
        {
            '8.3' =>  96 - length($tControlContent),
            '8.4' => 104 - length($tControlContent),
            '9.0' => 140 - length($tControlContent),
            '9.1' => 140 - length($tControlContent),
            '9.2' => 156 - length($tControlContent),
            '9.3' => 180 - length($tControlContent),
            '9.4' => 188 - length($tControlContent),
            '9.5' => 200 - length($tControlContent),
            '9.6' => 200 - length($tControlContent),
             '10' => 200 - length($tControlContent),
             '11' => 192 - length($tControlContent),
        },

        64 =>
        {
            '8.3' => 112 - length($tControlContent),
            '8.4' => 112 - length($tControlContent),
            '9.0' => 152 - length($tControlContent),
            '9.1' => 152 - length($tControlContent),
            '9.2' => 168 - length($tControlContent),
            '9.3' => 192 - length($tControlContent),
            '9.4' => 200 - length($tControlContent),
            '9.5' => 216 - length($tControlContent),
            '9.6' => 216 - length($tControlContent),
             '10' => 216 - length($tControlContent),
             '11' => 208 - length($tControlContent),
        },
    };

    # Fill up to page size and set page size
    $tControlContent .= ('C' x $rhOffsetToPageSize->{$self->archBits()}{$strPgVersion});
    $tControlContent .= pack('L', 8192);

    # Fill up to wal segment size and set wal segment size
    $tControlContent .= ('C' x 8);
    $tControlContent .= pack('L', 16 * 1024 * 1024);

    # Pad bytes
    $tControlContent .= ('C' x (8192 - length($tControlContent)));

    return \$tControlContent;
}

####################################################################################################################################
# Generate control file and write to disk
####################################################################################################################################
sub controlGenerate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbPath,
        $strPgVersion,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->controlGenerate', \@_,
            {name => 'strDbPath', trace => true},
            {name => 'strPgVersion', trace => true},
        );

    storageTest()->put("${strDbPath}/global/pg_control", $self->controlGenerateContent($strPgVersion));
}

####################################################################################################################################
# walSegment
#
# Generate name of WAL segment from component parts.
####################################################################################################################################
sub walSegment
{
    my $self = shift;
    my $iTimeline = shift;
    my $iMajor = shift;
    my $iMinor = shift;

    return uc(sprintf('%08x%08x%08x', $iTimeline, $iMajor, $iMinor));
}

####################################################################################################################################
# Generate WAL file content
####################################################################################################################################
sub walGenerateContent
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
        $iSourceNo,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->walGenerateContent', \@_,
            {name => 'strPgVersion', trace => true},
            {name => 'iSourceNo', optional => true, default => 1, trace => true},
        );

    # Get WAL magic for the PG version
    my $hWalMagic =
    {
        &PG_VERSION_83 => hex('0xD062'),
        &PG_VERSION_84 => hex('0xD063'),
        &PG_VERSION_90 => hex('0xD064'),
        &PG_VERSION_91 => hex('0xD066'),
        &PG_VERSION_92 => hex('0xD071'),
        &PG_VERSION_93 => hex('0xD075'),
        &PG_VERSION_94 => hex('0xD07E'),
        &PG_VERSION_95 => hex('0xD087'),
        &PG_VERSION_96 => hex('0xD093'),
        &PG_VERSION_10 => hex('0xD097'),
    };

    my $tWalContent = pack('S', $hWalMagic->{$strPgVersion});

    # Indicate that the long header is present
    $tWalContent .= pack('S', 2);

    # Add junk (H for header) for the bytes that won't be read by the tests
    my $iOffset = 12 + ($strPgVersion >= PG_VERSION_93 ? testRunGet()->archBits() / 8 : 0);
    $tWalContent .= ('H' x $iOffset);

    # Add the system identifier
    $tWalContent .= pack('Q', $self->dbSysId($strPgVersion));

    # Add the source number to produce WAL segments with different checksums
    $tWalContent .= pack('S', $iSourceNo);

    # Pad out to the required size (B for body)
    $tWalContent .= ('B' x (PG_WAL_SEGMENT_SIZE - length($tWalContent)));

    return \$tWalContent;
}

####################################################################################################################################
# Generate WAL file content checksum
####################################################################################################################################
sub walGenerateContentChecksum
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPgVersion,
        $hParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->walGenerateContent', \@_,
            {name => 'strPgVersion', trace => true},
            {name => 'hParam', required => false, trace => true},
        );

    return cryptoHashOne('sha1', ${$self->walGenerateContent($strPgVersion, $hParam)});
}

####################################################################################################################################
# walGenerate
#
# Generate a WAL segment and ready file for testing.
####################################################################################################################################
sub walGenerate
{
    my $self = shift;
    my $strWalPath = shift;
    my $strPgVersion = shift;
    my $iSourceNo = shift;
    my $strWalSegment = shift;
    my $bPartial = shift;
    my $bChecksum = shift;
    my $bReady = shift;

    my $rtWalContent = $self->walGenerateContent($strPgVersion, {iSourceNo => $iSourceNo});
    my $strWalFile =
        "${strWalPath}/${strWalSegment}" . ($bChecksum ? '-' . cryptoHashOne('sha1', $rtWalContent) : '') .
            (defined($bPartial) && $bPartial ? '.partial' : '');

    # Put the WAL segment and the ready file
    storageTest()->put($strWalFile, $rtWalContent);

    if (!defined($bReady) || $bReady)
    {
        storageTest()->put("${strWalPath}/archive_status/${strWalSegment}.ready");
    }

    return $strWalFile;
}

####################################################################################################################################
# walRemove
#
# Remove WAL file and ready file.
####################################################################################################################################
sub walRemove
{
    my $self = shift;
    my $strWalPath = shift;
    my $strWalFile = shift;

    storageTest()->remove("$self->{strWalPath}/${strWalFile}");
    storageTest()->remove("$self->{strWalPath}/archive_status/${strWalFile}.ready");
}

1;
