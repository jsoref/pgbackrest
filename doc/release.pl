#!/usr/bin/perl
####################################################################################################################################
# release.pl - PgBackRest Release Manager
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

$SIG{__DIE__} = sub { Carp::confess @_ };

use Cwd qw(abs_path);
use File::Basename qw(dirname);
use Getopt::Long qw(GetOptions);
use Pod::Usage qw(pod2usage);
use Storable;

use lib dirname($0) . '/lib';
use lib dirname(dirname($0)) . '/build/lib';
use lib dirname(dirname($0)) . '/lib';
use lib dirname(dirname($0)) . '/test/lib';

use BackRestDoc::Common::Doc;
use BackRestDoc::Common::DocConfig;
use BackRestDoc::Common::DocManifest;
use BackRestDoc::Common::DocRender;
use BackRestDoc::Html::DocHtmlSite;
use BackRestDoc::Latex::DocLatex;
use BackRestDoc::Markdown::DocMarkdown;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::Storage;
use pgBackRestTest::Common::StoragePosix;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

release.pl - pgBackRest Release Manager

=head1 SYNOPSIS

release.pl [options]

 General Options:
   --help           Display usage and exit
   --version        Display pgBackRest version
   --quiet          Sets log level to ERROR
   --log-level      Log level for execution (e.g. ERROR, WARN, INFO, DEBUG)

 Release Options:
   --build          Build the cache before release (should be included in the release commit)
   --deploy         Deploy documentation to website (can be done as docs are updated)
   --no-gen         Don't auto-generate
=cut

####################################################################################################################################
# Load command line parameters and config (see usage above for details)
####################################################################################################################################
my $bHelp = false;
my $bVersion = false;
my $bQuiet = false;
my $strLogLevel = 'info';
my $bBuild = false;
my $bDeploy = false;
my $bNoGen = false;

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel,
            'build' => \$bBuild,
            'deploy' => \$bDeploy,
            'no-gen' => \$bNoGen)
    or pod2usage(2);

####################################################################################################################################
# Run in eval block to catch errors
####################################################################################################################################
eval
{
    # Display version and exit if requested
    if ($bHelp || $bVersion)
    {
        print PROJECT_NAME . ' ' . PROJECT_VERSION . " Release Manager\n";

        if ($bHelp)
        {
            print "\n";
            pod2usage();
        }

        exit 0;
    }

    # If neither build nor deploy is requested then error
    if (!$bBuild && !$bDeploy)
    {
        confess &log(ERROR, 'neither --build nor --deploy requested, nothing to do');
    }

    # Set console log level
    if ($bQuiet)
    {
        $strLogLevel = 'error';
    }

    logLevelSet(undef, uc($strLogLevel), OFF);

    # Set the paths
    my $strDocPath = dirname(abs_path($0));
    my $strDocHtml = "${strDocPath}/output/html";
    my $strDocExe = "${strDocPath}/doc.pl";
    my $strTestExe = dirname($strDocPath) . "/test/test.pl";

    my $oStorageDoc = new pgBackRestTest::Common::Storage(
        $strDocPath, new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));

    # Determine if this is a dev release
    my $bDev = PROJECT_VERSION =~ /dev$/;
    my $strVersion = $bDev ? 'dev' : PROJECT_VERSION;

    if ($bBuild)
    {
        if (!$bNoGen)
        {
            # Update git history
            my $strGitCommand =
                'git -C ' . $strDocPath .
                ' log --pretty=format:\'{^^^^commit^^^^:^^^^%H^^^^,^^^^date^^^^:^^^^%ci^^^^,^^^^subject^^^^:^^^^%s^^^^,^^^^body^^^^:^^^^%b^^^^},\'';
            my $strGitLog = qx($strGitCommand);
            $strGitLog =~ s/\^\^\^\^\}\,\n/\#\#\#\#/mg;
            $strGitLog =~ s/\\/\\\\/g;
            $strGitLog =~ s/\n/\\n/mg;
            $strGitLog =~ s/\r/\\r/mg;
            $strGitLog =~ s/\t/\\t/mg;
            $strGitLog =~ s/\"/\\\"/g;
            $strGitLog =~ s/\^\^\^\^/\"/g;
            $strGitLog =~ s/\#\#\#\#/\"\}\,\n/mg;
            $strGitLog = '[' . substr($strGitLog, 0, length($strGitLog) - 1) . ']';
            my @hyGitLog = @{(JSON::PP->new()->allow_nonref())->decode($strGitLog)};

            # Load prior history
            my @hyGitLogPrior = @{(JSON::PP->new()->allow_nonref())->decode(
                ${$oStorageDoc->get("${strDocPath}/resource/git-history.cache")})};

            # Add new commits
            for (my $iGitLogIdx = @hyGitLog - 1; $iGitLogIdx >= 0; $iGitLogIdx--)
            {
                my $rhGitLog = $hyGitLog[$iGitLogIdx];
                my $bFound = false;

                foreach my $rhGitLogPrior (@hyGitLogPrior)
                {
                    if ($rhGitLog->{commit} eq $rhGitLogPrior->{commit})
                    {
                        $bFound = true;
                    }
                }

                next if $bFound;

                $rhGitLog->{body} = trim($rhGitLog->{body});

                if ($rhGitLog->{body} eq '')
                {
                    delete($rhGitLog->{body});
                }

                unshift(@hyGitLogPrior, $rhGitLog);
            }

            # Write git log
            $strGitLog = undef;

            foreach my $rhGitLog (@hyGitLogPrior)
            {
                $strGitLog .=
                    (defined($strGitLog) ? ",\n" : '') .
                    "    {\n" .
                    '        "commit": ' . trim((JSON::PP->new()->allow_nonref()->pretty())->encode($rhGitLog->{commit})) . ",\n" .
                    '        "date": ' . trim((JSON::PP->new()->allow_nonref()->pretty())->encode($rhGitLog->{date})) . ",\n" .
                    '        "subject": ' . trim((JSON::PP->new()->allow_nonref()->pretty())->encode($rhGitLog->{subject}));

                # Skip the body if it is empty or a release (since we already have the release note content)
                if ($rhGitLog->{subject} !~ /^v[0-9]{1,2}\.[0-9]{1,2}\: /g && defined($rhGitLog->{body}))
                {
                    $strGitLog .=
                        ",\n" .
                        '        "body": ' . trim((JSON::PP->new()->allow_nonref()->pretty())->encode($rhGitLog->{body}));
                }

                $strGitLog .=
                    "\n" .
                    "    }";
            }

            $oStorageDoc->put("${strDocPath}/resource/git-history.cache", "[\n${strGitLog}\n]\n");

            # Generate coverage summmary
            &log(INFO, "Generate Coverage Summary");
            executeTest(
                "${strTestExe} --no-package --no-valgrind --no-optimize --vm-max=3 --coverage-summary",
                {bShowOutputAsync => true});
        }

        # Remove permanent cache file
        $oStorageDoc->remove("${strDocPath}/resource/exe.cache", {bIgnoreMissing => true});

        # Remove all docker containers to get consistent IP address assignments
        executeTest('docker rm -f $(docker ps -a -q)', {bSuppressError => true});

        # Generate deployment docs for RHEL/Centos 7
        &log(INFO, "Generate RHEL/CentOS 7 documentation");

        executeTest("${strDocExe} --deploy --key-var=os-type=centos7 --out=pdf", {bShowOutputAsync => true});
        executeTest("${strDocExe} --deploy --cache-only --key-var=os-type=centos7 --out=pdf");

        # Generate deployment docs for RHEL/Centos 6
        &log(INFO, "Generate RHEL/CentOS 6 documentation");

        executeTest("${strDocExe} --deploy --key-var=os-type=centos6 --out=pdf", {bShowOutputAsync => true});
        executeTest("${strDocExe} --deploy --cache-only --key-var=os-type=centos6 --out=pdf");

        # Generate deployment docs for Debian
        &log(INFO, "Generate Debian/Ubuntu documentation");

        executeTest("${strDocExe} --deploy", {bShowOutputAsync => true});

        # Generate a full copy of the docs for review
        &log(INFO, "Generate full documentation for review");

        executeTest("${strDocExe} --deploy --cache-only --key-var=os-type=centos7 --out=html --var=project-url-root=index.html");
        $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-centos7.html");
        executeTest(
            "${strDocExe} --deploy --out-preserve --cache-only --key-var=os-type=centos6 --out=html" .
                " --var=project-url-root=index.html");
        $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-centos6.html");

        executeTest("${strDocExe} --deploy --out-preserve --cache-only --out=man --out=html --var=project-url-root=index.html");
    }

    if ($bDeploy)
    {
        my $strDeployPath = "${strDocPath}/site";

        # Generate docs for the website history
        &log(INFO, 'Generate website ' . ($bDev ? 'dev' : 'history') . ' documentation');

        my $strDocExeVersion =
            ${strDocExe} . ($bDev ? ' --dev' : ' --deploy --cache-only') . ' --var=project-url-root=index.html --out=html';

        executeTest("${strDocExeVersion} --key-var=os-type=centos7");
        $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-centos7.html");
        executeTest("${strDocExeVersion} --out-preserve --key-var=os-type=centos6");
        $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-centos6.html");

        $oStorageDoc->remove("$strDocHtml/release.html");
        executeTest("${strDocExeVersion} --out-preserve --exclude=release");

        # Deploy to repository
        &log(INFO, '...Deploy to repository');
        executeTest("rm -rf ${strDeployPath}/prior/${strVersion}");
        executeTest("mkdir ${strDeployPath}/prior/${strVersion}");
        executeTest("cp ${strDocHtml}/* ${strDeployPath}/prior/${strVersion}");

        # Generate docs for the main website
        if (!$bDev)
        {
            &log(INFO, "Generate website documentation");

            executeTest("${strDocExe} --deploy --cache-only --key-var=os-type=centos7 --out=html");
            $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-centos7.html");
            executeTest("${strDocExe} --deploy --out-preserve --cache-only --key-var=os-type=centos6 --out=html");
            $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-centos6.html");
            executeTest("${strDocExe} --deploy --out-preserve --cache-only --out=html");

            # Deploy to repository
            &log(INFO, '...Deploy to repository');
            executeTest("rm -rf ${strDeployPath}/dev");
            executeTest("find ${strDeployPath} -maxdepth 1 -type f -exec rm {} +");
            executeTest("cp ${strDocHtml}/* ${strDeployPath}");
            executeTest("cp ${strDocPath}/../README.md ${strDeployPath}");
            executeTest("cp ${strDocPath}/../LICENSE ${strDeployPath}");
        }

        # Update permissions
        executeTest("find ${strDeployPath} -type d -exec chmod 750 {} +");
        executeTest("find ${strDeployPath} -type f -exec chmod 640 {} +");
    }

    # Exit with success
    exit 0;
}

####################################################################################################################################
# Check for errors
####################################################################################################################################
or do
{
    # If a backrest exception then return the code
    exit $EVAL_ERROR->code() if (isException(\$EVAL_ERROR));

    # Else output the unhandled error
    print $EVAL_ERROR;
    exit ERROR_UNHANDLED;
};

# It shouldn't be possible to get here
&log(ASSERT, 'execution reached invalid location in ' . __FILE__ . ', line ' . __LINE__);
exit ERROR_ASSERT;
