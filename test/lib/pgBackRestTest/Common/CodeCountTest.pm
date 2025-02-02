####################################################################################################################################
# Classify files and generate code totals
####################################################################################################################################
package pgBackRestTest::Common::CodeCountTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;

use pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# Scan all files and assign types
####################################################################################################################################
sub codeCountScan
{
    my $oStorage = shift;
    my $strBasePath = shift;

    # Load YAML
    require YAML::XS;
    YAML::XS->import(qw(Load Dump));

    my $hCodeCount = {};

    # Build manifest of all files
    my $hManifest = $oStorage->manifest($strBasePath);
    my $strYamlDetail = undef;

    foreach my $strFile (sort(keys(%{$hManifest})))
    {
        # Only interested in files
        next if $hManifest->{$strFile}{type} ne 'f';

        # Only exclude these directories/files entirely
        next if ($strFile =~ '^\.' ||
                 $strFile =~ '\.md$' ||
                 $strFile =~ '\.log$' ||
                 $strFile eq 'LICENSE' ||
                 $strFile =~ '^doc/example/' ||
                 $strFile =~ '^doc/output/' ||
                 $strFile =~ '^doc/resource/fake\-cert' ||
                 $strFile =~ '\.png$' ||
                 $strFile =~ '\.eps$' ||
                 $strFile =~ '\.cache$' ||
                 $strFile =~ '^doc/site/' ||
                 $strFile eq 'libc/typemap' ||
                 $strFile eq 'test/Vagrantfile' ||
                 $strFile =~ '^test/certificate/' ||
                 $strFile =~ '^test/code-count/' ||
                 $strFile =~ '^test/coverage/' ||
                 $strFile =~ '^test/data/' ||
                 $strFile =~ '^test/docker/' ||
                 $strFile =~ '^test/expect/' ||
                 $strFile =~ '^test/lint/' ||
                 $strFile =~ '^test/package/' ||
                 $strFile =~ '^test/patch/' ||
                 $strFile =~ '^test/profile/' ||
                 $strFile eq 'test/scratch.txt' ||
                 $strFile eq 'test/src/valgrind.suppress' ||
                 $strFile eq 'test/src/lcov.conf');

        # Classify the souce file
        my $strClass = 'test/harness';

        if ($strFile =~ '^doc/xml/' || $strFile eq 'doc/manifest.xml')
        {
            $strClass = 'doc/source';
        }
        elsif ($strFile =~ '^doc/' || $strFile =~ '^doc/resource/')
        {
            $strClass = 'doc/core';
        }
        elsif ($strFile =~ '^build/' || $strFile =~ '^libc/build/' || $strFile eq 'libc/Makefile.PL' ||
               $strFile eq 'src/Makefile.in' || $strFile eq 'src/configure' || $strFile eq 'src/configure.ac')
        {
            $strClass = 'build';
        }
        elsif ($strFile =~ '^test/lib/pgBackRestTest/Module/' || $strFile =~ '^test/src/module/' ||
               $strFile =~ '^libc/t/')
        {
            $strClass = 'test/module';
        }
        elsif ($strFile =~ '^src/' || $strFile =~ '^lib/' || $strFile =~ '^libc/')
        {
            if ($strFile =~ '\.auto\..$' || $strFile =~ 'Auto\.pm$')
            {
                $strClass = 'core/auto';
            }
            else
            {
                $strClass = 'core';
            }
        }

        # Force unrecognized file types
        my $strForceLang = undef;
        my $strType = undef;

        if ($strFile =~ '\.xs$')
        {
            $strType = 'xs';
            $strForceLang = 'XS';
        }
        elsif ($strFile =~ '\.h$' || $strFile =~ '\.h\.in$' || $strFile =~ '\.xsh$')
        {
            $strType = 'c/h';
            $strForceLang = 'C/C++ Header';
        }
        elsif ($strFile =~ '\.c$')
        {
            $strType = 'c';
            $strForceLang = 'C';
        }
        elsif ($strFile =~ '\.t$' || $strFile =~ '\.pl$' || $strFile =~ '\.pm$' || $strFile =~ '\.PL$')
        {
            $strType = 'perl';
            $strForceLang = 'Perl';
        }
        elsif ($strFile =~ '\.yaml$')
        {
            $strType = 'yaml';
            $strForceLang = 'YAML';
        }
        elsif ($strFile =~ 'Makefile\.in$' || $strFile =~ '^src\/configure')
        {
            $strType = 'make';
            $strForceLang = 'make';
        }
        elsif ($strFile =~ '\.xml$')
        {
            $strType = 'xml';
            $strForceLang = 'XML';
        }
        elsif ($strFile =~ '\.css$')
        {
            $strType = 'css';
            $strForceLang = 'CSS';
        }
        elsif ($strFile =~ '\.dtd$')
        {
            $strType = 'dtd';
            $strForceLang = 'DTD';
        }
        elsif ($strFile =~ '\.tex$')
        {
            $strType = 'latex';
            $strForceLang = 'Latex';
        }
        else
        {
            confess &log(ERROR, "unable to map type for ${strFile}");
        }

        # Load the file counts
        my $strYaml = executeTest(
            "cloc --yaml ${strBasePath}/${strFile}" .
            " --read-lang-def=${strBasePath}/test/code-count/code.def" .
            " --force-lang='${strForceLang}'");

        # Error if the file was ignored
        if ($strYaml =~ '1 file ignored')
        {
            confess &log(ERROR, "file type for ${strBasePath}/${strFile} not recognized:\n${strYaml}");
        }

        # Clean out extraneous keys
        my $hFileCount = Load($strYaml);
        delete($hFileCount->{header});
        delete($hFileCount->{SUM});

        # There should only be one key - the file type
        if (keys(%{$hFileCount}) != 1)
        {
            confess &log(ERROR, "more than one file type in ${strBasePath}/${strFile}:\n" . Dump($hFileCount));
        }

        # Translate type
        my ($strTypeRaw) = keys(%{$hFileCount});
        $hFileCount = $hFileCount->{$strTypeRaw};

        # Append to yaml file
        $strYamlDetail .=
            (defined($strYamlDetail) ? "\n" : '') .
            "${strFile}:\n" .
            "  class: ${strClass}\n" .
            "  type: ${strType}\n";

        # Summarize
        $hCodeCount->{$strClass}{$strType}{code} += $hFileCount->{code};
        $hCodeCount->{$strClass}{$strType}{comment} += $hFileCount->{comment};

        $hCodeCount->{$strClass}{total}{code} += $hFileCount->{code};
        $hCodeCount->{$strClass}{total}{comment} += $hFileCount->{comment};

        $hCodeCount->{total}{code} += $hFileCount->{code};
        $hCodeCount->{total}{comment} += $hFileCount->{comment};
    }

    # Save the file
    $oStorage->put(
        'test/code-count/file-type.yaml',
        "# File types for source files in the project\n" . $strYamlDetail);

    # Reload the file to make sure there aren't any formatting issues
    Load(${$oStorage->get('test/code-count/file-type.yaml')});

    # Display code count summary
    &log(INFO, "code count summary:\n" . Dump($hCodeCount));
}

push @EXPORT, qw(codeCountScan);

1;
