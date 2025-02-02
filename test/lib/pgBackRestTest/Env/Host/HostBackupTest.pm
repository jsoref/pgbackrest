####################################################################################################################################
# HostBackupTest.pm - Backup host
####################################################################################################################################
package pgBackRestTest::Env::Host::HostBackupTest;
use parent 'pgBackRestTest::Env::Host::HostBaseTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl ':mode';
use File::Basename qw(dirname);
use File::stat qw{lstat};
use Storable qw(dclone);

use pgBackRest::Archive::Info;
use pgBackRest::Backup::Common;
use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Version;
use pgBackRest::Storage::Base;

use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostS3Test;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# Host defaults
####################################################################################################################################
use constant HOST_PATH_LOCK                                         => 'lock';
    push @EXPORT, qw(HOST_PATH_LOCK);
use constant HOST_PATH_LOG                                          => 'log';
    push @EXPORT, qw(HOST_PATH_LOG);
use constant HOST_PATH_REPO                                         => 'repo';

use constant HOST_PROTOCOL_TIMEOUT                                  => 10;
    push @EXPORT, qw(HOST_PROTOCOL_TIMEOUT);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParam', required => false, trace => true},
        );

    # If params are not passed
    my $oHostGroup = hostGroupGet();

    my ($strName, $strImage, $strUser);

    if (!defined($$oParam{strName}) || $$oParam{strName} eq HOST_BACKUP)
    {
        $strName = HOST_BACKUP;
        $strImage = containerRepo() . ':' . testRunGet()->vm() . '-test';
        $strUser = testRunGet()->backrestUser();
    }
    else
    {
        $strName = $$oParam{strName};
        $strImage = $$oParam{strImage};
        $strUser = testRunGet()->pgUser();
    }

    # Create the host
    my $self = $class->SUPER::new($strName, {strImage => $strImage, strUser => $strUser});
    bless $self, $class;

    # If repo is on local filesystem then set the repo-path locally
    if ($oParam->{bRepoLocal})
    {
        $self->{strRepoPath} = $self->testRunGet()->testPath() . "/$$oParam{strBackupDestination}/" . HOST_PATH_REPO;
    }
    # Else on KV store and repo will be in root
    else
    {
        $self->{strRepoPath} = '/';
    }

    # Create the repo-path if on a local filesystem
    if ($$oParam{strBackupDestination} eq $self->nameGet() && $oParam->{bRepoLocal})
    {
        storageTest()->pathCreate($self->repoPath(), {strMode => '0770'});
    }

    # Set log/lock paths
    $self->{strLogPath} = $self->testPath() . '/' . HOST_PATH_LOG;
    storageTest()->pathCreate($self->{strLogPath}, {strMode => '0770'});
    $self->{strLockPath} = $self->testPath() . '/' . HOST_PATH_LOCK;

    # Set conf file
    $self->{strBackRestConfig} =  $self->testPath() . '/' . PROJECT_CONF;

    # Set LogTest object
    $self->{oLogTest} = $$oParam{oLogTest};

    # Set synthetic
    $self->{bSynthetic} = defined($$oParam{bSynthetic}) && $$oParam{bSynthetic} ? true : false;

    # Set the backup destination
    $self->{strBackupDestination} = $$oParam{strBackupDestination};

    # Default hardlink to false
    $self->{bHardLink} = false;

    # By default there is no bogus host
    $self->{bBogusHost} = false;

    # Create a placeholder hash for file munging
    $self->{hInfoFile} = {};

    # Set whether repo should be encrypted or not
    $self->{bRepoEncrypt} = defined($$oParam{bRepoEncrypt}) ? $$oParam{bRepoEncrypt} : false;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# backupBegin
####################################################################################################################################
sub backupBegin
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupBegin', \@_,
            {name => 'strType', trace => true},
            {name => 'strComment', trace => true},
            {name => 'oParam', required => false, trace => true},
        );

    # Set defaults
    my $strTest = defined($$oParam{strTest}) ? $$oParam{strTest} : undef;
    my $fTestDelay = defined($$oParam{fTestDelay}) ? $$oParam{fTestDelay} : .2;
    my $oExpectedManifest = defined($$oParam{oExpectedManifest}) ? $$oParam{oExpectedManifest} : undef;

    $strComment =
        "${strType} backup" . (defined($strComment) ? " - ${strComment}" : '') .
        ' (' . $self->nameGet() . ' host)';

    &log(INFO, "    $strComment");

    # Execute the backup command
    my $oExecuteBackup = $self->execute(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($oExpectedManifest) ? " --no-online" : '') .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        (defined($$oParam{bStandby}) && $$oParam{bStandby} ? " --backup-standby" : '') .
        (defined($oParam->{strRepoType}) ? " --repo1-type=$oParam->{strRepoType}" : '') .
        ($strType ne 'incr' ? " --type=${strType}" : '') .
        ' --stanza=' . (defined($oParam->{strStanza}) ? $oParam->{strStanza} : $self->stanza()) . ' backup' .
        (defined($strTest) ? " --test --test-delay=${fTestDelay} --test-point=" . lc($strTest) . '=y' : ''),
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus},
         oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});

    $oExecuteBackup->begin();

    # Return at the test point if one was defined
    if (defined($strTest))
    {
        $oExecuteBackup->end($strTest);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oExecuteBackup', value => $oExecuteBackup, trace => true},
    );
}

####################################################################################################################################
# backupEnd
####################################################################################################################################
sub backupEnd
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $oExecuteBackup,
        $oParam,
        $bManifestCompare,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupEnd', \@_,
            {name => 'strType', trace => true},
            {name => 'oExecuteBackup', trace => true},
            {name => 'oParam', required => false, trace => true},
            {name => 'bManifestCompare', required => false, default => true, trace => true},
        );

    # Set defaults
    my $oExpectedManifest = defined($$oParam{oExpectedManifest}) ? dclone($$oParam{oExpectedManifest}) : undef;

    my $iExitStatus = $oExecuteBackup->end();

    return if ($oExecuteBackup->{iExpectedExitStatus} != 0);

    # If an alternate stanza was specified
    if (defined($oParam->{strStanza}))
    {
        confess &log(ASSERT,
            'if an alternate stanza is specified it must generate an error - the remaining code will not be aware of the stanza');
    }

    my $strBackup = $self->backupLast();

    # If a real backup then load the expected manifest from the actual manifest.  An expected manifest can't be generated perfectly
    # because a running database is always in flux.  Even so, it allows us test many things.
    if (!$self->synthetic())
    {
        $oExpectedManifest = iniParse(
            ${storageRepo()->get(
                storageRepo()->openRead(
                    'backup/' . $self->stanza() . "/${strBackup}/" . FILE_MANIFEST,
                    {strCipherPass => $self->cipherPassManifest()}))});
    }

    # Make sure tablespace links are correct
    if ($self->hasLink())
    {
        if ($strType eq CFGOPTVAL_BACKUP_TYPE_FULL || $self->hardLink())
        {
            my $hTablespaceManifest = storageRepo()->manifest(
                STORAGE_REPO_BACKUP . "/${strBackup}/" . MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGTBLSPC);

            # Remove . and ..
            delete($hTablespaceManifest->{'.'});
            delete($hTablespaceManifest->{'..'});

            # Iterate file links
            for my $strFile (sort(keys(%{$hTablespaceManifest})))
            {
                # Make sure the link is in the expected manifest
                my $hManifestTarget =
                    $oExpectedManifest->{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGTBLSPC . "/${strFile}"};

                if (!defined($hManifestTarget) || $hManifestTarget->{&MANIFEST_SUBKEY_TYPE} ne MANIFEST_VALUE_LINK ||
                    $hManifestTarget->{&MANIFEST_SUBKEY_TABLESPACE_ID} ne $strFile)
                {
                    confess &log(ERROR, "'${strFile}' is not in expected manifest as a link with the correct tablespace id");
                }

                # Make sure the link really is a link
                if ($hTablespaceManifest->{$strFile}{type} ne 'l')
                {
                    confess &log(ERROR, "'${strFile}' in tablespace directory is not a link");
                }

                # Make sure the link destination is correct
                my $strLinkDestination = '../../' . MANIFEST_TARGET_PGTBLSPC . "/${strFile}";

                if ($hTablespaceManifest->{$strFile}{link_destination} ne $strLinkDestination)
                {
                    confess &log(ERROR,
                        "'${strFile}' link should reference '${strLinkDestination}' but actually references " .
                        "'$hTablespaceManifest->{$strFile}{link_destination}'");
                }
            }

            # Iterate manifest targets
            for my $strTarget (sort(keys(%{$oExpectedManifest->{&MANIFEST_SECTION_BACKUP_TARGET}})))
            {
                my $hManifestTarget = $oExpectedManifest->{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget};
                my $strTablespaceId = $hManifestTarget->{&MANIFEST_SUBKEY_TABLESPACE_ID};

                # Make sure the target exists as a link on disk
                if ($hManifestTarget->{&MANIFEST_SUBKEY_TYPE} eq MANIFEST_VALUE_LINK && defined($strTablespaceId) &&
                    !defined($hTablespaceManifest->{$strTablespaceId}))
                {
                    confess &log(ERROR,
                        "target '${strTarget}' does not have a link at '" . DB_PATH_PGTBLSPC. "/${strTablespaceId}'");
                }
            }
        }
        # Else there should not be a tablespace directory at all
        elsif (storageRepo()->pathExists(STORAGE_REPO_BACKUP . "/${strBackup}/" . MANIFEST_TARGET_PGDATA . '/' . DB_PATH_PGTBLSPC))
        {
            confess &log(ERROR, 'backup must be full or hard-linked to have ' . DB_PATH_PGTBLSPC . ' directory');
        }
    }

    # Check that latest link exists unless repo links are disabled
    my $strLatestLink = storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . LINK_LATEST);
    my $bLatestLinkExists = storageRepo()->exists($strLatestLink);

    if ((!defined($oParam->{strRepoType}) || $oParam->{strRepoType} eq CFGOPTVAL_REPO_TYPE_POSIX) && $self->hasLink())
    {
        my $strLatestLinkDestination = readlink($strLatestLink);

        if ($strLatestLinkDestination ne $strBackup)
        {
            confess &log(ERROR, "'" . LINK_LATEST . "' link should be '${strBackup}' but is '${strLatestLinkDestination}");
        }
    }
    elsif ($bLatestLinkExists)
    {
        confess &log(ERROR, "'" . LINK_LATEST . "' link should not exist");
    }

    # Only do compare for synthetic backups since for real backups the expected manifest *is* the actual manifest.
    if ($self->synthetic())
    {
        # Compare only if expected to do so
        if ($bManifestCompare)
        {
            # Set backup type in the expected manifest
            ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TYPE} = $strType;

            $self->backupCompare($strBackup, $oExpectedManifest);
        }
    }

    # Add files to expect log
    if (defined($self->{oLogTest}) && (!defined($$oParam{bSupplemental}) || $$oParam{bSupplemental}))
    {
        my $oHostGroup = hostGroupGet();

        if (defined($oHostGroup->hostGet(HOST_DB_MASTER, true)))
        {
            $self->{oLogTest}->supplementalAdd($oHostGroup->hostGet(HOST_DB_MASTER)->testPath() . '/' . PROJECT_CONF);
        }

        if (defined($oHostGroup->hostGet(HOST_DB_STANDBY, true)))
        {
            $self->{oLogTest}->supplementalAdd($oHostGroup->hostGet(HOST_DB_STANDBY)->testPath() . '/' . PROJECT_CONF);
        }

        if (defined($oHostGroup->hostGet(HOST_BACKUP, true)))
        {
            $self->{oLogTest}->supplementalAdd($oHostGroup->hostGet(HOST_BACKUP)->testPath() . '/' . PROJECT_CONF);
        }

        if ($self->synthetic() && $bManifestCompare)
        {
            $self->{oLogTest}->supplementalAdd(
                storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strBackup}/" . FILE_MANIFEST), undef,
                ${storageRepo()->get(
                    storageRepo()->openRead(
                        STORAGE_REPO_BACKUP . "/${strBackup}/" . FILE_MANIFEST,
                        {strCipherPass => $self->cipherPassManifest()}))});
            $self->{oLogTest}->supplementalAdd(
                storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO), undef,
                ${storageRepo->get(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO)});
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBackup', value => $strBackup, trace => true},
    );
}

####################################################################################################################################
# backup
####################################################################################################################################
sub backup
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strComment,
        $oParam,
        $bManifestCompare,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backup', \@_,
            {name => 'strType'},
            {name => 'strComment'},
            {name => 'oParam', required => false},
            {name => 'bManifestCompare', required => false, default => true},
        );

    my $oExecuteBackup = $self->backupBegin($strType, $strComment, $oParam);
    my $strBackup = $self->backupEnd($strType, $oExecuteBackup, $oParam, $bManifestCompare);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBackup', value => $strBackup},
    );
}

####################################################################################################################################
# backupCompare
####################################################################################################################################
sub backupCompare
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackup,
        $oExpectedManifest,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupCompare', \@_,
            {name => 'strBackup', trace => true},
            {name => 'oExpectedManifest', trace => true},
        );

    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LABEL} = $strBackup;

    my $oActualManifest = new pgBackRest::Manifest(
        storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strBackup}/" . FILE_MANIFEST),
        {strCipherPass => $self->cipherPassManifest()});

    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, &MANIFEST_KEY_TIMESTAMP_START);
    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP);
    ${$oExpectedManifest}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START} =
        $oActualManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START);
    ${$oExpectedManifest}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} =
        $oActualManifest->get(INI_SECTION_BACKREST, INI_KEY_CHECKSUM);
    ${$oExpectedManifest}{&INI_SECTION_BACKREST}{&INI_KEY_FORMAT} = REPOSITORY_FORMAT + 0;

    if (defined($oExpectedManifest->{&INI_SECTION_CIPHER}) &&
        defined($oExpectedManifest->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS}) &&
        $oActualManifest->test(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS))
    {
        $oExpectedManifest->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS} =
            $oActualManifest->get(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS);
    }

    # Update the expected manifest with whether the --delta option was used or not to perform the backup.
    $oExpectedManifest->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA} =
        $oActualManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_DELTA) ? INI_TRUE : INI_FALSE;

    my $strSectionPath = $oActualManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH);

    foreach my $strFileKey ($oActualManifest->keys(MANIFEST_SECTION_TARGET_FILE))
    {
        # Determine repo size if compression or encryption is enabled
        my $bCompressed = $oExpectedManifest->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS};

        if ($bCompressed ||
            (defined($oExpectedManifest->{&INI_SECTION_CIPHER}) &&
                defined($oExpectedManifest->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS})))
        {

            my $lRepoSize =
                $oActualManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_REFERENCE) ?
                    $oActualManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_REPO_SIZE, false) :
                    (storageRepo()->info(STORAGE_REPO_BACKUP .
                        "/${strBackup}/${strFileKey}" . ($bCompressed ? '.gz' : '')))->{size};

            if (defined($lRepoSize) &&
                $lRepoSize != $oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE}{$strFileKey}{&MANIFEST_SUBKEY_SIZE})
            {
                $oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE}{$strFileKey}{&MANIFEST_SUBKEY_REPO_SIZE} = $lRepoSize;
            }
        }

        # If the backup does not have page checksums then no need to compare
        if (!$oExpectedManifest->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_CHECKSUM_PAGE})
        {
            delete($oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE}{$strFileKey}{&MANIFEST_SUBKEY_CHECKSUM_PAGE});
            delete($oExpectedManifest->{&MANIFEST_SECTION_TARGET_FILE}{$strFileKey}{&MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR});
        }
        # Else make sure things that should have checks do have checks
        elsif ($oActualManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_CHECKSUM_PAGE) !=
               isChecksumPage($strFileKey))
        {
            confess
                "check-page actual for ${strFileKey} is " .
                ($oActualManifest->test(MANIFEST_SECTION_TARGET_FILE, $strFileKey,
                    MANIFEST_SUBKEY_CHECKSUM_PAGE) ? 'set' : '[undef]') .
                ' but isChecksumPage() says it should be ' .
                (isChecksumPage($strFileKey) ? 'set' : 'undef') . '.';
        }
    }

    $self->manifestDefault($oExpectedManifest);

    my $strTestPath = $self->testPath();

    storageTest()->put("${strTestPath}/actual.manifest", iniRender($oActualManifest->{oContent}));
    storageTest()->put("${strTestPath}/expected.manifest", iniRender($oExpectedManifest));

    executeTest("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    storageTest()->remove("${strTestPath}/expected.manifest");
    storageTest()->remove("${strTestPath}/actual.manifest");

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# manifestDefault
####################################################################################################################################
sub manifestDefault
{
    my $self = shift;
    my $oExpectedManifest = shift;

    # Set defaults for subkeys that tend to repeat
    foreach my $strSection (&MANIFEST_SECTION_TARGET_FILE, &MANIFEST_SECTION_TARGET_PATH, &MANIFEST_SECTION_TARGET_LINK)
    {
        foreach my $strSubKey (&MANIFEST_SUBKEY_USER, &MANIFEST_SUBKEY_GROUP, &MANIFEST_SUBKEY_MODE, &MANIFEST_SUBKEY_MASTER)
        {
            my %oDefault;
            my $iSectionTotal = 0;

            next if !defined($oExpectedManifest->{$strSection});

            foreach my $strFile (keys(%{$oExpectedManifest->{$strSection}}))
            {
                my $strValue = $oExpectedManifest->{$strSection}{$strFile}{$strSubKey};

                if (defined($strValue))
                {
                    if (defined($oDefault{$strValue}))
                    {
                        $oDefault{$strValue}++;
                    }
                    else
                    {
                        $oDefault{$strValue} = 1;
                    }
                }

                $iSectionTotal++;
            }

            my $strMaxValue;
            my $iMaxValueTotal = 0;

            foreach my $strValue (sort(keys(%oDefault)))
            {
                if ($oDefault{$strValue} > $iMaxValueTotal)
                {
                    $iMaxValueTotal = $oDefault{$strValue};
                    $strMaxValue = $strValue;
                }
            }

            if (defined($strMaxValue) > 0 && $iMaxValueTotal > $iSectionTotal * MANIFEST_DEFAULT_MATCH_FACTOR)
            {
                if ($strSubKey eq MANIFEST_SUBKEY_MASTER)
                {
                    $oExpectedManifest->{"${strSection}:default"}{$strSubKey} = $strMaxValue ? JSON::PP::true : JSON::PP::false;
                }
                else
                {
                    $oExpectedManifest->{"${strSection}:default"}{$strSubKey} = $strMaxValue;
                }

                foreach my $strFile (keys(%{$oExpectedManifest->{$strSection}}))
                {
                    if (defined($oExpectedManifest->{$strSection}{$strFile}{$strSubKey}) &&
                        $oExpectedManifest->{$strSection}{$strFile}{$strSubKey} eq $strMaxValue)
                    {
                        delete($oExpectedManifest->{$strSection}{$strFile}{$strSubKey});
                    }
                }
            }
        }
    }
}

####################################################################################################################################
# backupLast
####################################################################################################################################
sub backupLast
{
    my $self = shift;

    my @stryBackup = storageRepo()->list(
        STORAGE_REPO_BACKUP, {strExpression => '[0-9]{8}-[0-9]{6}F(_[0-9]{8}-[0-9]{6}(D|I)){0,1}', strSortOrder => 'reverse'});

    if (!defined($stryBackup[0]))
    {
        confess 'no backup was found: ' . join(@stryBackup, ', ');
    }

    return $stryBackup[0];
}

####################################################################################################################################
# check
####################################################################################################################################
sub check
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'check ' . $self->stanza() . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($$oParam{iTimeout}) ? " --archive-timeout=$$oParam{iTimeout}" : '') .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' --stanza=' . $self->stanza() . ' check',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# expire
####################################################################################################################################
sub expire
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'oParam', required => false},
        );

    my $strComment =
        'expire' .
        (defined($$oParam{iRetentionFull}) ? " full=$$oParam{iRetentionFull}" : '') .
        (defined($$oParam{iRetentionDiff}) ? " diff=$$oParam{iRetentionDiff}" : '') .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "        ${strComment}");

    # Determine whether or not to expect an error
    my $oHostGroup = hostGroupGet();

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($$oParam{iRetentionFull}) ? " --repo1-retention-full=$$oParam{iRetentionFull}" : '') .
        (defined($$oParam{iRetentionDiff}) ? " --repo1-retention-diff=$$oParam{iRetentionDiff}" : '') .
        '  --stanza=' . $self->stanza() . ' expire',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()});
}

####################################################################################################################################
# info
####################################################################################################################################
sub info
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->info', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'info' . (defined($$oParam{strStanza}) ? " $$oParam{strStanza} stanza" : ' all stanzas') . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --log-level-console=warn' .
        (defined($$oParam{strStanza}) ? " --stanza=$$oParam{strStanza}" : '') .
        (defined($$oParam{strOutput}) ? " --output=$$oParam{strOutput}" : '') . ' info',
        {strComment => $strComment, oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanzaCreate
####################################################################################################################################
sub stanzaCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stanzaCreate', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'stanza-create ' . $self->stanza() . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --stanza=' . $self->stanza() .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' stanza-create',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()});

    if (storageRepo()->exists('backup/' . $self->stanza() . qw{/} . FILE_BACKUP_INFO))
    {
        # If the info file was created, then add it to the expect log
        if (defined($self->{oLogTest}) && $self->synthetic())
        {
            $self->{oLogTest}->supplementalAdd(
                storageRepo()->pathGet('backup/' . $self->stanza() . qw{/} . FILE_BACKUP_INFO), undef,
                ${storageRepo()->get(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO)});
        }

        # Get the passphrase for accessing the manifest file
        $self->{strCipherPassManifest} =
            (new pgBackRest::Backup::Info(storageRepo()->pathGet('backup/' . $self->stanza())))->cipherPassSub();
    }

    if (storageRepo()->exists('archive/' . $self->stanza() . qw{/} . ARCHIVE_INFO_FILE))
    {
        # If the info file was created, then add it to the expect log
        if (defined($self->{oLogTest}) && $self->synthetic())
        {
            $self->{oLogTest}->supplementalAdd(
                storageRepo()->pathGet('archive/' . $self->stanza() . qw{/} . ARCHIVE_INFO_FILE), undef,
                ${storageRepo()->get(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE)});
        }

        # Get the passphrase for accessing the archived files
        $self->{strCipherPassArchive} =
            (new pgBackRest::Archive::Info(storageRepo()->pathGet('archive/' . $self->stanza())))->cipherPassSub();
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanzaUpgrade
####################################################################################################################################
sub stanzaUpgrade
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stanzaUpgrade', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'stanza-upgrade ' . $self->stanza() . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --stanza=' . $self->stanza() .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' stanza-upgrade',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()});

    # If the info file was created, then add it to the expect log
    if (defined($self->{oLogTest}) && $self->synthetic() &&
        storageRepo()->exists('backup/' . $self->stanza() . qw{/} . FILE_BACKUP_INFO))
    {
        $self->{oLogTest}->supplementalAdd(
            storageRepo()->pathGet('backup/' . $self->stanza() . qw{/} . FILE_BACKUP_INFO), undef,
            ${storageRepo()->get(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO)});
    }

    if (defined($self->{oLogTest}) && $self->synthetic() &&
        storageRepo()->exists('archive/' . $self->stanza() . qw{/} . ARCHIVE_INFO_FILE))
    {
        $self->{oLogTest}->supplementalAdd(
            storageRepo()->pathGet('archive/' . $self->stanza() . qw{/} . ARCHIVE_INFO_FILE), undef,
            ${storageRepo()->get(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE)});
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanzaDelete
####################################################################################################################################
sub stanzaDelete
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stanzaDelete', \@_,
            {name => 'strComment'},
            {name => 'oParam', required => false},
        );

    $strComment =
        'stanza-delete ' . $self->stanza() . ' - ' . $strComment .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ' --stanza=' . $self->stanza() .
        (defined($$oParam{strOptionalParam}) ? " $$oParam{strOptionalParam}" : '') .
        ' stanza-delete',
        {strComment => $strComment, iExpectedExitStatus => $$oParam{iExpectedExitStatus}, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()});

    if (defined($self->{oLogTest}) && $self->synthetic())
    {
        $self->{oLogTest}->logAdd(
            'list backup', $self->stanza() . ' must not exist for successful delete',
            join("\n", storageRepo()->list('backup')));
        $self->{oLogTest}->logAdd(
            'list archive', $self->stanza() . ' must not exist for successful delete',
            join("\n", storageRepo()->list('archive')));
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# start
####################################################################################################################################
sub start
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->start', \@_,
            {name => 'oParam', required => false},
        );

    my $strComment =
        'start' . (defined($$oParam{strStanza}) ? " $$oParam{strStanza} stanza" : ' all stanzas') .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($$oParam{strStanza}) ? " --stanza=$$oParam{strStanza}" : '') . ' start',
        {strComment => $strComment, oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});
}

####################################################################################################################################
# stop
####################################################################################################################################
sub stop
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stop', \@_,
            {name => 'oParam', required => false},
        );

    my $strComment =
        'stop' . (defined($$oParam{strStanza}) ? " $$oParam{strStanza} stanza" : ' all stanzas') .
        ' (' . $self->nameGet() . ' host)';
    &log(INFO, "    $strComment");

    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        (defined($$oParam{strStanza}) ? " --stanza=$$oParam{strStanza}" : '') .
        (defined($$oParam{bForce}) && $$oParam{bForce} ? ' --force' : '') . ' stop',
        {strComment => $strComment, oLogTest => $self->{oLogTest}, bLogOutput => $self->synthetic()});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# configCreate
####################################################################################################################################
sub configCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->stop', \@_,
            {name => 'oParam', required => false},
        );

    my %oParamHash;
    my $strStanza = $self->stanza();
    my $oHostGroup = hostGroupGet();
    my $oHostBackup = $oHostGroup->hostGet($self->backupDestination());
    my $oHostDbMaster = $oHostGroup->hostGet(HOST_DB_MASTER);
    my $oHostDbStandby = $oHostGroup->hostGet(HOST_DB_STANDBY, true);

    my $bArchiveAsync = defined($$oParam{bArchiveAsync}) ? $$oParam{bArchiveAsync} : false;

    # General options
    # ------------------------------------------------------------------------------------------------------------------------------
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE)} = lc(DETAIL);
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_LEVEL_FILE)} = lc(TRACE);
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_LEVEL_STDERR)} = lc(OFF);
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_SUBPROCESS)} = 'y';
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_TIMESTAMP)} = 'n';

    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_PATH)} = $self->logPath();
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOCK_PATH)} = $self->lockPath();

    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_PROTOCOL_TIMEOUT)} = 60;
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_DB_TIMEOUT)} = 45;

    # Set to make sure that changinf the default works and to speed compression for testing
    $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_COMPRESS_LEVEL)} = 3;

    # Only set network compress level if there is more than one host
    if ($oHostBackup != $oHostDbMaster)
    {
        $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_COMPRESS_LEVEL_NETWORK)} = 1;
    }

    if (defined($$oParam{bCompress}) && !$$oParam{bCompress})
    {
        $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_COMPRESS)} = 'n';
    }

    if ($self->isHostBackup())
    {
        if ($self->repoEncrypt())
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_CIPHER_TYPE)} =
                CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_CIPHER_PASS)} = 'x';
        }

        $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_PATH)} = $self->repoPath();

        # S3 settings
        if ($oParam->{bS3})
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_TYPE)} = CFGOPTVAL_REPO_TYPE_S3;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_S3_KEY)} = HOST_S3_ACCESS_KEY;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_S3_KEY_SECRET)} = HOST_S3_ACCESS_SECRET_KEY;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_S3_BUCKET)} = HOST_S3_BUCKET;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_S3_ENDPOINT)} = HOST_S3_ENDPOINT;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_S3_REGION)} = HOST_S3_REGION;
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{'repo1-s3-verify-ssl'} = 'n';
        }

        if (defined($$oParam{bHardlink}) && $$oParam{bHardlink})
        {
            $self->{bHardLink} = true;
            $oParamHash{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{cfgOptionName(CFGOPT_REPO_HARDLINK)} = 'y';
        }

        $oParamHash{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{cfgOptionName(CFGOPT_ARCHIVE_COPY)} = 'y';
        $oParamHash{&CFGDEF_SECTION_GLOBAL . ':' . cfgCommandName(CFGCMD_BACKUP)}{cfgOptionName(CFGOPT_START_FAST)} = 'y';
    }

    # Host specific options
    # ------------------------------------------------------------------------------------------------------------------------------

    # If this is the backup host
    if ($self->isHostBackup())
    {
        my $oHostDb1 = $oHostDbMaster;
        my $oHostDb2 = $oHostDbStandby;

        if ($self->nameTest(HOST_DB_STANDBY))
        {
            $oHostDb1 = $oHostDbStandby;
            $oHostDb2 = $oHostDbMaster;
        }

        if ($self->nameTest(HOST_BACKUP))
        {
            $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_HOST)} = $oHostDb1->nameGet();
            $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_HOST_USER)} = $oHostDb1->userGet();
            $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_HOST_CMD)} = $oHostDb1->backrestExe();
            $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_HOST_CONFIG)} = $oHostDb1->backrestConfig();

            # Port can't be configured for a synthetic host
            if (!$self->synthetic())
            {
                $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_PORT)} = $oHostDb1->pgPort();
            }
        }

        $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_PATH)} = $oHostDb1->dbBasePath();

        if (defined($oHostDb2))
        {
            # Add an invalid replica to simulate more than one replica. A warning should be thrown by dbObjectGet when a stanza is
            # created and a valid replica should be chosen.
            my $iInvalidReplica = 2;
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_HOST, $iInvalidReplica))} = BOGUS;
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_HOST_USER, $iInvalidReplica))} =
                $oHostDb2->userGet();
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CMD, $iInvalidReplica))} =
                $oHostDb2->backrestExe();
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG, $iInvalidReplica))} =
                $oHostDb2->backrestConfig();
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iInvalidReplica))} =
                $oHostDb2->dbBasePath();

            # Set a flag so we know there's a bogus host
            $self->{bBogusHost} = true;

            # Set a valid replica to the last possible index to ensure skipping indexes does not make a difference.
            my $iValidReplica = cfgOptionIndexTotal(CFGOPT_PG_PATH);
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_HOST, $iValidReplica))} = $oHostDb2->nameGet();
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_HOST_USER, $iValidReplica))} =
                $oHostDb2->userGet();
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CMD, $iValidReplica))} =
                $oHostDb2->backrestExe();
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_HOST_CONFIG, $iValidReplica))} =
                $oHostDb2->backrestConfig();
            $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iValidReplica))} = $oHostDb2->dbBasePath();

            # Only test explicit ports on the backup server.  This is so locally configured ports are also tested.
            if (!$self->synthetic() && $self->nameTest(HOST_BACKUP))
            {
                $oParamHash{$strStanza}{cfgOptionName(cfgOptionIdFromIndex(CFGOPT_PG_PORT, $iValidReplica))} = $oHostDb2->pgPort();
            }
        }
    }

    # If this is a database host
    if ($self->isHostDb())
    {
        $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_PATH)} = $self->dbBasePath();

        if (!$self->synthetic())
        {
            $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_SOCKET_PATH)} = $self->pgSocketPath();
            $oParamHash{$strStanza}{cfgOptionName(CFGOPT_PG_PORT)} = $self->pgPort();
        }

        if ($bArchiveAsync)
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL . ':' .
                cfgCommandName(CFGCMD_ARCHIVE_PUSH)}{cfgOptionName(CFGOPT_ARCHIVE_ASYNC)} = 'y';
        }

        $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_SPOOL_PATH)} = $self->spoolPath();

        # If the the backup host is remote
        if (!$self->isHostBackup())
        {
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_HOST)} = $oHostBackup->nameGet();
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_HOST_USER)} = $oHostBackup->userGet();
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_HOST_CMD)} = $oHostBackup->backrestExe();
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_REPO_HOST_CONFIG)} = $oHostBackup->backrestConfig();

            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOG_PATH)} = $self->logPath();
            $oParamHash{&CFGDEF_SECTION_GLOBAL}{cfgOptionName(CFGOPT_LOCK_PATH)} = $self->lockPath();
        }
    }

    # Write out the configuration file
    storageTest()->put($self->backrestConfig(), iniRender(\%oParamHash, true));

    # Modify the file permissions so it can be read/saved by all test users
    executeTest('sudo chmod 660 ' . $self->backrestConfig());
}

####################################################################################################################################
# configUpdate - update configuration with new options
####################################################################################################################################
sub configUpdate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $hParam,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->configUpdate', \@_,
            {name => 'hParam'},
        );

    # Load db config file
    my $oConfig = iniParse(${storageTest()->get($self->backrestConfig())}, {bRelaxed => true});

    # Load params
    foreach my $strSection (keys(%{$hParam}))
    {
        foreach my $strKey (keys(%{$hParam->{$strSection}}))
        {
            $oConfig->{$strSection}{$strKey} = $hParam->{$strSection}{$strKey};
        }
    }

    # Modify the file permissions so it can be saved by all test users
    executeTest(
        'sudo chmod 660 ' . $self->backrestConfig() . ' && sudo chmod 770 ' . dirname($self->backrestConfig()));

    storageTest()->put($self->backrestConfig(), iniRender($oConfig, true));

    # Fix permissions back to original
    executeTest(
        'sudo chmod 660 ' . $self->backrestConfig() . ' && sudo chmod 770 ' . dirname($self->backrestConfig()) .
        ' && sudo chown ' . $self->userGet() . ' ' . $self->backrestConfig());

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# manifestMunge
#
# Allows for munging of the manifest while making it appear to be valid.  This is used to create various error conditions that
# should be caught by the unit tests.
####################################################################################################################################
sub manifestMunge
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackup,
        $hParam,
        $bCache,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestMunge', \@_,
            {name => 'strBackup'},
            {name => '$hParam'},
            {name => 'bCache', default => true},
        );

    $self->infoMunge(storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strBackup}/" . FILE_MANIFEST), $hParam, $bCache, true);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# manifestRestore
####################################################################################################################################
sub manifestRestore
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackup,
        $bSave,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manifestRestore', \@_,
            {name => 'strBackup'},
            {name => 'bSave', default => true},
        );

    $self->infoRestore(storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strBackup}/" . FILE_MANIFEST), $bSave);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# infoMunge
#
# With the file name specified (e.g. /repo/archive/db/archive.info) copy the current values from the file into the global hash and
# update the file with the new values passed. Later, using infoRestore, the global variable will be used to restore the file to its
# original state.
####################################################################################################################################
sub infoMunge
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $hParam,
        $bCache,
        $bManifest,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoMunge', \@_,
            {name => 'strFileName'},
            {name => 'hParam'},
            {name => 'bCache', default => true},
            {name => 'bManifest', default => false},
        );

    # If the original file content does not exist then load it
    if (!defined($self->{hInfoFile}{$strFileName}))
    {
        $self->{hInfoFile}{$strFileName} = new pgBackRest::Common::Ini($strFileName, {oStorage => storageRepo(),
        strCipherPass => !$bManifest ? storageRepo()->cipherPassUser() : $self->cipherPassManifest()});
    }

    # Make a copy of the original file contents
    my $oMungeIni = new pgBackRest::Common::Ini(
        $strFileName,
        {bLoad => false, strContent => iniRender($self->{hInfoFile}{$strFileName}->{oContent}), oStorage => storageRepo(),
        strCipherPass => !$bManifest ? storageRepo()->cipherPassUser() : $self->cipherPassManifest()});

    # Load params
    foreach my $strSection (keys(%{$hParam}))
    {
        foreach my $strKey (keys(%{$hParam->{$strSection}}))
        {
            if (ref($hParam->{$strSection}{$strKey}) eq 'HASH')
            {
                foreach my $strSubKey (keys(%{$hParam->{$strSection}{$strKey}}))
                {
                    # Munge the copy with the new parameter values
                    if (defined($hParam->{$strSection}{$strKey}{$strSubKey}))
                    {
                        $oMungeIni->set($strSection, $strKey, $strSubKey, $hParam->{$strSection}{$strKey}{$strSubKey});
                    }
                    else
                    {
                        $oMungeIni->remove($strSection, $strKey, $strSubKey);
                    }
                }
            }
            else
            {
                # Munge the copy with the new parameter values
                if (defined($hParam->{$strSection}{$strKey}))
                {
                    $oMungeIni->set($strSection, $strKey, undef, $hParam->{$strSection}{$strKey});
                }
                else
                {
                    $oMungeIni->remove($strSection, $strKey);
                }
            }
        }
    }

    # Modify the file/directory permissions so it can be saved
    if ($self->isFS())
    {
        executeTest("sudo rm -f ${strFileName}* && sudo chmod 770 " . dirname($strFileName));
    }

    # Save the munged data to the file
    $oMungeIni->save();

    # Fix permissions
    if ($self->isFS())
    {
        executeTest(
            "sudo chmod 640 ${strFileName}* && sudo chmod 750 " . dirname($strFileName) .
            ' && sudo chown ' . $self->userGet() . " ${strFileName}*");
    }

    # Clear the cache is requested
    if (!$bCache)
    {
        delete($self->{hInfoFile}{$strFileName});
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# infoRestore
#
# With the file name specified (e.g. /repo/archive/db/archive.info) use the original file contents in the global hash to restore the
# file to its original state after modifying the values with infoMunge.
####################################################################################################################################
sub infoRestore
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,
        $bSave,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoRestore', \@_,
            {name => 'strFileName'},
            {name => 'bSave', default => true},
        );

    # If the original file content exists in the global hash, then save it to the file
    if (defined($self->{hInfoFile}{$strFileName}))
    {
        if ($bSave)
        {
            # Modify the file/directory permissions so it can be saved
            if ($self->isFS())
            {
                executeTest("sudo rm -f ${strFileName}* && sudo chmod 770 " . dirname($strFileName));
            }

            # Save the munged data to the file
            $self->{hInfoFile}{$strFileName}->{bModified} = true;
            $self->{hInfoFile}{$strFileName}->save();

            # Fix permissions
            if ($self->isFS())
            {
                executeTest(
                    "sudo chmod 640 ${strFileName} && sudo chmod 750 " . dirname($strFileName) .
                    ' && sudo chown ' . $self->userGet() . " ${strFileName}*");
            }
        }
    }
    else
    {
        confess &log(ASSERT, "There is no original data cached for $strFileName. infoMunge must be called first.");
    }

    # Remove the element from the hash
    delete($self->{hInfoFile}{$strFileName});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# configRecovery
####################################################################################################################################
sub configRecovery
{
    my $self = shift;
    my $oHostBackup = shift;
    my $oRecoveryHashRef = shift;

    # Get stanza
    my $strStanza = $self->stanza();

    # Load db config file
    my $oConfig = iniParse(${storageTest->get($self->backrestConfig())}, {bRelaxed => true});

    # Rewrite recovery options
    my @stryRecoveryOption;

    foreach my $strOption (sort(keys(%$oRecoveryHashRef)))
    {
        push (@stryRecoveryOption, "${strOption}=${$oRecoveryHashRef}{$strOption}");
    }

    if (@stryRecoveryOption)
    {
        $oConfig->{$strStanza}{cfgOptionName(CFGOPT_RECOVERY_OPTION)} = \@stryRecoveryOption;
    }

    # Save db config file
    storageTest()->put($self->backrestConfig(), iniRender($oConfig, true));
}

####################################################################################################################################
# configRemap
####################################################################################################################################
sub configRemap
{
    my $self = shift;
    my $oRemapHashRef = shift;
    my $oManifestRef = shift;

    # Get stanza name
    my $strStanza = $self->stanza();

    # Load db config file
    my $oConfig = iniParse(${storageTest()->get($self->backrestConfig())}, {bRelaxed => true});

    # Load backup config file
    my $oRemoteConfig;
    my $oHostBackup =
        !$self->standby() && !$self->nameTest($self->backupDestination()) ?
            hostGroupGet()->hostGet($self->backupDestination()) : undef;

    if (defined($oHostBackup))
    {
        $oRemoteConfig = iniParse(${storageTest()->get($oHostBackup->backrestConfig())}, {bRelaxed => true});
    }

    # Rewrite recovery section
    delete($oConfig->{"${strStanza}:restore"}{cfgOptionName(CFGOPT_TABLESPACE_MAP)});
    my @stryTablespaceMap;

    foreach my $strRemap (sort(keys(%$oRemapHashRef)))
    {
        my $strRemapPath = ${$oRemapHashRef}{$strRemap};

        if ($strRemap eq MANIFEST_TARGET_PGDATA)
        {
            $oConfig->{$strStanza}{cfgOptionName(CFGOPT_PG_PATH)} = $strRemapPath;

            ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;

            if (defined($oHostBackup))
            {
                $oRemoteConfig->{$strStanza}{cfgOptionName(CFGOPT_PG_PATH)} = $strRemapPath;
            }
        }
        else
        {
            my $strTablespaceOid = (split('\/', $strRemap))[1];
            push (@stryTablespaceMap, "${strTablespaceOid}=${strRemapPath}");

            ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strRemap}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;
            ${$oManifestRef}{&MANIFEST_SECTION_TARGET_LINK}{MANIFEST_TARGET_PGDATA . "/${strRemap}"}{destination} = $strRemapPath;
        }
    }

    if (@stryTablespaceMap)
    {
        $oConfig->{"${strStanza}:restore"}{cfgOptionName(CFGOPT_TABLESPACE_MAP)} = \@stryTablespaceMap;
    }

    # Save db config file
    storageTest()->put($self->backrestConfig(), iniRender($oConfig, true));

    # Save backup config file (but not if this is the standby which is not the source of backups)
    if (defined($oHostBackup))
    {
        # Modify the file permissions so it can be read/saved by all test users
        executeTest(
            'sudo chmod 660 ' . $oHostBackup->backrestConfig() . ' && sudo chmod 770 ' . dirname($oHostBackup->backrestConfig()));

        storageTest()->put($oHostBackup->backrestConfig(), iniRender($oRemoteConfig, true));

        # Fix permissions
        executeTest(
            'sudo chmod 660 ' . $oHostBackup->backrestConfig() . ' && sudo chmod 770 ' . dirname($oHostBackup->backrestConfig()) .
            ' && sudo chown ' . $oHostBackup->userGet() . ' ' . $oHostBackup->backrestConfig());
    }
}

####################################################################################################################################
# restore
####################################################################################################################################
sub restore
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strComment,
        $strBackup,
        $rhExpectedManifest,
        $rhRemapHash,
        $bDelta,
        $bForce,
        $strType,
        $strTarget,
        $bTargetExclusive,
        $strTargetAction,
        $strTargetTimeline,
        $rhRecoveryHash,
        $iExpectedExitStatus,
        $strOptionalParam,
        $bTablespace,
        $strUser,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->restore', \@_,
            {name => 'strComment', required => false},
            {name => 'strBackup'},
            {name => 'rhExpectedManifest', optional => true},
            {name => 'rhRemapHash', optional => true},
            {name => 'bDelta', optional => true, default => false},
            {name => 'bForce', optional => true, default => false},
            {name => 'strType', optional => true},
            {name => 'strTarget', optional => true},
            {name => 'bTargetExclusive', optional => true, default => false},
            {name => 'strTargetAction', optional => true},
            {name => 'strTargetTimeline', optional => true},
            {name => 'rhRecoveryHash', optional => true},
            {name => 'iExpectedExitStatus', optional => true},
            {name => 'strOptionalParam', optional => true},
            {name => 'bTablespace', optional => true},
            {name => 'strUser', optional => true},
        );

    # Build link map options
    my $strLinkMap;

    foreach my $strTarget (sort(keys(%{$self->{hLinkRemap}})))
    {
        $strLinkMap .= " --link-map=\"${strTarget}=${$self->{hLinkRemap}}{$strTarget}\"";
    }

    $strComment = 'restore' .
                  ($bDelta ? ' delta' : '') .
                  ($bForce ? ', force' : '') .
                  ($strBackup ne cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_SET) ? ", backup '${strBackup}'" : '') .
                  # This does not output 'default' for synthetic tests to make expect logs match up (may change later)
                  ($strType ? ", type '${strType}'" : (defined($rhExpectedManifest) ? '' : ", type 'default'")) .
                  ($strTarget ? ", target '${strTarget}'" : '') .
                  ($strTargetTimeline ? ", timeline '${strTargetTimeline}'" : '') .
                  ($bTargetExclusive ? ', exclusive' : '') .
                  (defined($strTargetAction) && $strTargetAction ne cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_TARGET_ACTION)
                      ? ', ' . cfgOptionName(CFGOPT_TARGET_ACTION) . "=${strTargetAction}" : '') .
                  (defined($rhRemapHash) ? ', remap' : '') .
                  (defined($iExpectedExitStatus) ? ", expect exit ${iExpectedExitStatus}" : '') .
                  (defined($strComment) ? " - ${strComment}" : '') .
                  ' (' . $self->nameGet() . ' host)';
    &log(INFO, "        ${strComment}");

    # Get the backup host
    my $oHostGroup = hostGroupGet();
    my $oHostBackup = defined($oHostGroup->hostGet(HOST_BACKUP, true)) ? $oHostGroup->hostGet(HOST_BACKUP) : $self;

    # Load the expected manifest if it was not defined
    my $oExpectedManifest = undef;

    if (!defined($rhExpectedManifest))
    {
        # Load the manifest
        my $oExpectedManifest = new pgBackRest::Manifest(
            storageRepo()->pathGet(
                STORAGE_REPO_BACKUP . qw{/} . ($strBackup eq 'latest' ? $oHostBackup->backupLast() : $strBackup) . qw{/} .
                    FILE_MANIFEST),
            {strCipherPass => $oHostBackup->cipherPassManifest()});

        $rhExpectedManifest = $oExpectedManifest->{oContent};

        # Remap links in the expected manifest
        foreach my $strTarget (sort(keys(%{$self->{hLinkRemap}})))
        {
            my $strDestination = ${$self->{hLinkRemap}}{$strTarget};
            my $strTarget = 'pg_data/' . $strTarget;
            my $strTargetPath = $strDestination;

            # If this link is to a file then the specified path must be split into file and path parts
            if ($oExpectedManifest->isTargetFile($strTarget))
            {
                $strTargetPath = dirname($strTargetPath);

                # Error when the path is not deep enough to be valid
                if (!defined($strTargetPath))
                {
                    confess &log(ERROR, "${strDestination} is not long enough to be target for ${strTarget}");
                }

                # Set the file part
                $oExpectedManifest->set(
                    MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_FILE,
                    substr($strDestination, length($strTargetPath) + 1));

                # Set the link target
                $oExpectedManifest->set(
                    MANIFEST_SECTION_TARGET_LINK, $strTarget, MANIFEST_SUBKEY_DESTINATION, $strDestination);
            }
            else
            {
                # Set the link target
                $oExpectedManifest->set(MANIFEST_SECTION_TARGET_LINK, $strTarget, MANIFEST_SUBKEY_DESTINATION, $strTargetPath);
            }

            # Set the target path
            $oExpectedManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTarget, MANIFEST_SUBKEY_PATH, $strTargetPath);
        }
    }

    # Get the backup host
    if (defined($rhRemapHash))
    {
        $self->configRemap($rhRemapHash, $rhExpectedManifest);
    }

    if (defined($rhRecoveryHash))
    {
        $self->configRecovery($oHostBackup, $rhRecoveryHash);
    }

    # Create the restore command
    $self->executeSimple(
        $self->backrestExe() .
        ' --config=' . $self->backrestConfig() .
        ($bDelta ? ' --delta' : '') .
        ($bForce ? ' --force' : '') .
        ($strBackup ne cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_SET) ? " --set=${strBackup}" : '') .
        (defined($strOptionalParam) ? " ${strOptionalParam} " : '') .
        (defined($strType) && $strType ne CFGOPTVAL_RESTORE_TYPE_DEFAULT ? " --type=${strType}" : '') .
        (defined($strTarget) ? " --target=\"${strTarget}\"" : '') .
        (defined($strTargetTimeline) ? " --target-timeline=\"${strTargetTimeline}\"" : '') .
        ($bTargetExclusive ? ' --target-exclusive' : '') .
        (defined($strLinkMap) ? $strLinkMap : '') .
        ($self->synthetic() ? '' : ' --link-all') .
        (defined($strTargetAction) && $strTargetAction ne cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_TARGET_ACTION)
            ? ' --' . cfgOptionName(CFGOPT_TARGET_ACTION) . "=${strTargetAction}" : '') .
        ' --stanza=' . $self->stanza() . ' restore',
        {strComment => $strComment, iExpectedExitStatus => $iExpectedExitStatus, oLogTest => $self->{oLogTest},
         bLogOutput => $self->synthetic()},
        $strUser);

    if (!defined($iExpectedExitStatus))
    {
        $self->restoreCompare($strBackup, dclone($rhExpectedManifest), $bTablespace);

        if (defined($self->{oLogTest}))
        {
            $self->{oLogTest}->supplementalAdd(
                $rhExpectedManifest->{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} .
                "/recovery.conf");
        }
    }
}

####################################################################################################################################
# restoreCompare
####################################################################################################################################
sub restoreCompare
{
    my $self = shift;
    my $strBackup = shift;
    my $oExpectedManifestRef = shift;
    my $bTablespace = shift;

    my $strTestPath = $self->testPath();

    # Get the backup host
    my $oHostGroup = hostGroupGet();
    my $oHostBackup = defined($oHostGroup->hostGet(HOST_BACKUP, true)) ? $oHostGroup->hostGet(HOST_BACKUP) : $self;

    # Load the last manifest if it exists
    my $oLastManifest = undef;

    if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR}))
    {
        my $oExpectedManifest =
            new pgBackRest::Manifest(
                storageRepo()->pathGet(
                    STORAGE_REPO_BACKUP . qw{/} . ($strBackup eq 'latest' ? $oHostBackup->backupLast() : $strBackup) .
                        '/'. FILE_MANIFEST),
            {strCipherPass => $oHostBackup->cipherPassManifest()});

        # Get the --delta option from the backup manifest so the actual manifest can be built the same way for comparison
        $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA} =
            $oExpectedManifest->get(MANIFEST_SECTION_BACKUP_OPTION, &MANIFEST_KEY_DELTA);

        $oLastManifest =
            new pgBackRest::Manifest(
                storageRepo()->pathGet(
                    STORAGE_REPO_BACKUP . qw{/} .
                        ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR} . qw{/} . FILE_MANIFEST),
            {strCipherPass => $oHostBackup->cipherPassManifest()});
    }

    # Generate the tablespace map for real backups
    my $oTablespaceMap = undef;

    if (!$self->synthetic())
    {
        # Tablespace_map file is not restored in versions >= 9.5 because it interferes with internal remapping features.
        if (${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= PG_VERSION_95)
        {
            delete(${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/tablespace_map'});
        }

        foreach my $strTarget (keys(%{${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}}))
        {
            if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID}))
            {
                my $iTablespaceId =
                    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID};

                $$oTablespaceMap{$iTablespaceId} =
                    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_NAME};
            }
        }
    }

    # Generate the actual manifest
    my $strDbClusterPath =
        ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH};

    if (defined($bTablespace) && !$bTablespace)
    {
        foreach my $strTarget (keys(%{${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_TARGET}}))
        {
            if ($$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TYPE} eq
                MANIFEST_VALUE_LINK &&
                defined($$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_ID}))
            {
                my $strRemapPath;
                my $iTablespaceName =
                    $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_TABLESPACE_NAME};

                $strRemapPath = "../../tablespace/${iTablespaceName}";

                $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_TARGET}{$strTarget}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;
                $$oExpectedManifestRef{&MANIFEST_SECTION_TARGET_LINK}{MANIFEST_TARGET_PGDATA . "/${strTarget}"}
                    {&MANIFEST_SUBKEY_DESTINATION} = $strRemapPath;
            }
        }
    }

    my $oActualManifest = new pgBackRest::Manifest(
        "${strTestPath}/" . FILE_MANIFEST,
        {bLoad => false, strDbVersion => $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION},
            iDbCatalogVersion => $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG},
            oStorage => storageTest()});

    # Build the actual manifest using the delta setting that was actually used by the latest backup if one exists
    $oActualManifest->build(storageTest(), $strDbClusterPath, $oLastManifest, false,
        $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA}, $oTablespaceMap);
    $oActualManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_DELTA, undef,
        $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA});

    my $strSectionPath = $oActualManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH);

    foreach my $strName ($oActualManifest->keys(MANIFEST_SECTION_TARGET_FILE))
    {
        # If synthetic match checksum errors since they can't be verified here
        if ($self->synthetic)
        {
            my $bChecksumPage = $oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE};

            if (defined($bChecksumPage))
            {
                $oActualManifest->boolSet(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE, $bChecksumPage);

                if (!$bChecksumPage &&
                    defined($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR}))
                {
                    $oActualManifest->set(
                        MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR,
                        $oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR});
                }
            }
        }
        # Else if page checksums are enabled make sure the correct files are being checksummed
        else
        {
            if ($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_CHECKSUM_PAGE})
            {
                if (defined($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE}) !=
                    isChecksumPage($strName))
                {
                    confess
                        "check-page actual for ${strName} is " .
                        ($oActualManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName,
                            MANIFEST_SUBKEY_CHECKSUM_PAGE) ? 'set' : '[undef]') .
                        ' but isChecksumPage() says it should be ' .
                        (isChecksumPage($strName) ? 'set' : '[undef]') . '.';
                }

                # Because the page checksum flag is copied to incr and diff from the previous backup but further processing is not
                # done, they can't be expected to match so delete them.
                delete($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM_PAGE});
                $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM_PAGE);
            }
        }

        if (!$self->synthetic())
        {
            $oActualManifest->set(
                MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE,
                ${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{size});
        }

        # Remove repo-size from the manifest.  ??? This could be improved to get actual sizes from the backup.
        $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REPO_SIZE);
        delete($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_REPO_SIZE});

        if ($oActualManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_SIZE) != 0)
        {
            my $oStat = lstat($oActualManifest->dbPathGet($strSectionPath, $strName));

            # When performing a selective restore, the files for the database(s) that are not restored are still copied but as empty
            # sparse files (blocks == 0). If the file is not a sparse file or is a link, then get the actual checksum for comparison
            if ($oStat->blocks > 0 || S_ISLNK($oStat->mode))
            {
                my ($strHash) = storageTest()->hashSize($oActualManifest->dbPathGet($strSectionPath, $strName));

                $oActualManifest->set(
                    MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM, $strHash);

                # If the delta option was set, it is possible that the checksum on the file changed from the last manifest. If so,
                # then the file was expected to be copied by the backup and therefore the reference would have been removed.
                if ($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA})
                {
                    # If the actual checksum and last manifest checksum don't match, remove the reference
                    if (defined($oLastManifest) &&
                        $oLastManifest->test(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM) &&
                        $strHash ne $oLastManifest->get(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM))
                    {
                        $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE);
                    }
                }
            }
            else
            {
                # If there is a sparse file, remove the checksum and reference since they may or may not match. In this case, it is
                # not important to check them since it is known that the file was intentionally not restored.
                $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_CHECKSUM);
                delete(${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_CHECKSUM});

                $oActualManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strName, MANIFEST_SUBKEY_REFERENCE);
                delete(${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_FILE}{$strName}{&MANIFEST_SUBKEY_REFERENCE});
            }
        }
    }

    # If the link section is empty then delete it and the default section
    if (keys(%{${$oExpectedManifestRef}{&MANIFEST_SECTION_TARGET_LINK}}) == 0)
    {
        delete($$oExpectedManifestRef{&MANIFEST_SECTION_TARGET_LINK});
        delete($$oExpectedManifestRef{&MANIFEST_SECTION_TARGET_LINK . ':default'});
    }

    # Set actual to expected for settings that always change from backup to backup
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_CHECK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_COPY});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BACKUP_STANDBY, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_BACKUP_STANDBY});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BUFFER_SIZE, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_BUFFER_SIZE});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS_LEVEL, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS_LEVEL});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS_LEVEL_NETWORK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS_LEVEL_NETWORK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_HARDLINK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ONLINE});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_PROCESS_MAX, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_PROCESS_MAX});
    $oActualManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_DELTA, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_DELTA});

    $oActualManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION});
    $oActualManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CONTROL});
    $oActualManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG});
    $oActualManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_SYSTEM_ID});
    $oActualManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_ID, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_ID});

    $oActualManifest->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef,
                          ${$oExpectedManifestRef}{&INI_SECTION_BACKREST}{&INI_KEY_VERSION});

    # Copy passphrase if one exists
    if (defined($oExpectedManifestRef->{&INI_SECTION_CIPHER}) &&
        defined($oExpectedManifestRef->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS}))
    {
        $oActualManifest->set(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS, undef,
                              $oExpectedManifestRef->{&INI_SECTION_CIPHER}{&INI_KEY_CIPHER_PASS});
    }

    # This option won't be set in the actual manifest
    delete($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_CHECKSUM_PAGE});

    if ($self->synthetic())
    {
        $oActualManifest->remove(MANIFEST_SECTION_BACKUP);
        delete($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP});
    }
    else
    {
        $oActualManifest->set(
            INI_SECTION_BACKREST, INI_KEY_CHECKSUM, undef, $oExpectedManifestRef->{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LABEL});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP});
        $oActualManifest->set(
            MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef,
            $oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TYPE});

        $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_START, undef,
                              ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LSN_START});
        $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_STOP, undef,
                              ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LSN_STOP});

        if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_START}))
        {
            $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef,
                                  ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_START});
        }

        if (${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_STOP})
        {
            $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef,
                                  ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_STOP});
        }
    }

    # Check that archive status exists in the manifest for an online backup
    my $strArchiveStatusPath = MANIFEST_TARGET_PGDATA . qw{/} . $oActualManifest->walPath() . qw{/} . DB_PATH_ARCHIVESTATUS;

    if ($oExpectedManifestRef->{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ONLINE} &&
        !defined($oExpectedManifestRef->{&MANIFEST_SECTION_TARGET_PATH}{$strArchiveStatusPath}))
    {
        confess &log(ERROR, "${strArchiveStatusPath} expected for online backup", ERROR_ASSERT);
    }

    # Delete the list of DBs
    delete($$oExpectedManifestRef{&MANIFEST_SECTION_DB});

    $self->manifestDefault($oExpectedManifestRef);

    storageTest()->put("${strTestPath}/actual.manifest", iniRender($oActualManifest->{oContent}));
    storageTest()->put("${strTestPath}/expected.manifest", iniRender($oExpectedManifestRef));

    executeTest("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    storageTest()->remove("${strTestPath}/expected.manifest");
    storageTest()->remove("${strTestPath}/actual.manifest");
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub backrestConfig {return shift->{strBackRestConfig}}
sub backupDestination {return shift->{strBackupDestination}}
sub backrestExe {return testRunGet()->backrestExe()}
sub bogusHost {return shift->{bBogusHost}}
sub hardLink {return shift->{bHardLink}}
sub hasLink {storageRepo()->capability(STORAGE_CAPABILITY_LINK)}
sub isFS {storageRepo()->type() ne STORAGE_S3}
sub isHostBackup {my $self = shift; return $self->backupDestination() eq $self->nameGet()}
sub isHostDbMaster {return shift->nameGet() eq HOST_DB_MASTER}
sub isHostDbStandby {return shift->nameGet() eq HOST_DB_STANDBY}
sub isHostDb {my $self = shift; return $self->isHostDbMaster() || $self->isHostDbStandby()}
sub lockPath {return shift->{strLockPath}}
sub logPath {return shift->{strLogPath}}
sub repoPath {return shift->{strRepoPath}}
sub repoEncrypt {return shift->{bRepoEncrypt}}
sub stanza {return testRunGet()->stanza()}
sub synthetic {return shift->{bSynthetic}}
sub cipherPassManifest {return shift->{strCipherPassManifest}}
sub cipherPassArchive {return shift->{strCipherPassArchive}}

1;
