####################################################################################################################################
# DefineTest.pm - Defines all tests that can be run
####################################################################################################################################
package pgBackRestTest::Common::DefineTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::Common::String;

use pgBackRestTest::Common::VmTest;

################################################################################################################################
# Test definition constants
#
# Documentation for these constants is in test/define.yaml.
################################################################################################################################
use constant TESTDEF_INTEGRATION                                    => 'integration';
    push @EXPORT, qw(TESTDEF_INTEGRATION);
use constant TESTDEF_PERFORMANCE                                    => 'performance';
    push @EXPORT, qw(TESTDEF_PERFORMANCE);
use constant TESTDEF_UNIT                                           => 'unit';
    push @EXPORT, qw(TESTDEF_UNIT);

use constant TESTDEF_MODULE                                         => 'module';
    push @EXPORT, qw(TESTDEF_MODULE);
use constant TESTDEF_NAME                                           => 'name';
    push @EXPORT, qw(TESTDEF_NAME);
use constant TESTDEF_TEST                                           => 'test';
    push @EXPORT, qw(TESTDEF_TEST);

use constant TESTDEF_DB                                             => 'db';
    push @EXPORT, qw(TESTDEF_DB);
use constant TESTDEF_CONTAINER                                      => 'container';
    push @EXPORT, qw(TESTDEF_CONTAINER);
use constant TESTDEF_COVERAGE                                       => 'coverage';
    push @EXPORT, qw(TESTDEF_COVERAGE);
use constant TESTDEF_EXPECT                                         => 'expect';
    push @EXPORT, qw(TESTDEF_EXPECT);
use constant TESTDEF_C                                              => 'c';
    push @EXPORT, qw(TESTDEF_C);
use constant TESTDEF_DEFINE                                         => 'define';
    push @EXPORT, qw(TESTDEF_DEFINE);
use constant TESTDEF_DEFINE_TEST                                    => 'define-test';
    push @EXPORT, qw(TESTDEF_DEFINE_TEST);
use constant TESTDEF_DEBUG_UNIT_SUPPRESS                            => 'debugUnitSuppress';
    push @EXPORT, qw(TESTDEF_DEBUG_UNIT_SUPPRESS);
use constant TESTDEF_INCLUDE                                        => 'include';
    push @EXPORT, qw(TESTDEF_INCLUDE);
use constant TESTDEF_INDIVIDUAL                                     => 'individual';
    push @EXPORT, qw(TESTDEF_INDIVIDUAL);
use constant TESTDEF_TOTAL                                          => 'total';
    push @EXPORT, qw(TESTDEF_TOTAL);
use constant TESTDEF_PERL_REQ                                       => 'perlReq';
    push @EXPORT, qw(TESTDEF_PERL_REQ);
use constant TESTDEF_VM                                             => 'vm';
    push @EXPORT, qw(TESTDEF_VM);

use constant TESTDEF_COVERAGE_FULL                                  => 'full';
    push @EXPORT, qw(TESTDEF_COVERAGE_FULL);
use constant TESTDEF_COVERAGE_PARTIAL                               => 'partial';
    push @EXPORT, qw(TESTDEF_COVERAGE_PARTIAL);
use constant TESTDEF_COVERAGE_NOCODE                                => 'noCode';
    push @EXPORT, qw(TESTDEF_COVERAGE_NOCODE);

####################################################################################################################################
# Process normalized data into a more queryable form
####################################################################################################################################
my $hTestDefHash;                                                   # An easier way to query hash version of the above
my @stryModule;                                                     # Ordered list of modules
my $hModuleTest;                                                    # Ordered list of tests for each module
my $hCoverageType;                                                  # Coverage type for each code module (full/partial)
my $hCoverageList;                                                  # Tests required for full code module coverage (if type full)

sub testDefLoad
{
    my $strDefineYaml = shift;

    # Load test definitions from yaml
    require YAML::XS;
    YAML::XS->import(qw(Load));

    my $hTestDef = Load($strDefineYaml);

    # Iterate each test type
    foreach my $strModuleType (TESTDEF_UNIT, TESTDEF_INTEGRATION, TESTDEF_PERFORMANCE)
    {
        my $hModuleType = $hTestDef->{$strModuleType};

        my $bExpect = false;                                        # By default don't run expect tests
        my $bContainer = true;                                      # By default run tests in a single container
        my $bIndividual = false;                                    # By default runs are all executed in the same contanier

        if ($strModuleType eq TESTDEF_INTEGRATION)
        {
            $bExpect = true;                                        # Integration tests run expect tests
            $bContainer = false;                                    # Integration tests can run in multiple containers
            $bIndividual = true;                                    # Integration tests can change containers on each run
        }

        # Iterate each module
        foreach my $hModule (@{$hModuleType})
        {
            # Push the module onto the ordered list
            my $strModule = $hModule->{&TESTDEF_NAME};
            push(@stryModule, $strModule);

            # Iterate each test
            my @stryModuleTest;

            foreach my $hModuleTest (@{$hModule->{&TESTDEF_TEST}})
            {
                # Push the test on the order list
                my $strTest = $hModuleTest->{&TESTDEF_NAME};
                push(@stryModuleTest, $strTest);

                # Resolve variables that can be set in the module or the test
                foreach my $strVar (
                    TESTDEF_DEFINE, TESTDEF_DEFINE_TEST, TESTDEF_DEBUG_UNIT_SUPPRESS, TESTDEF_DB, TESTDEF_PERL_REQ, TESTDEF_VM)
                {
                    $hTestDefHash->{$strModule}{$strTest}{$strVar} = coalesce(
                        $hModuleTest->{$strVar}, $hModule->{$strVar}, $strVar eq TESTDEF_VM ? undef : false);

                    # Make false = 0 for debugging
                    if ($strVar ne TESTDEF_VM && $hTestDefHash->{$strModule}{$strTest}{$strVar} eq '')
                    {
                        $hTestDefHash->{$strModule}{$strTest}{$strVar} = false;
                    }
                }

                # Set module type variables
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_C} =
                    $strModuleType eq TESTDEF_UNIT && $strTest !~ /perl$/ ? true : false;
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_INTEGRATION} = $strModuleType eq TESTDEF_INTEGRATION ? true : false;
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_EXPECT} = $bExpect;
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_CONTAINER} = $bContainer;
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_INDIVIDUAL} = $bIndividual;

                # Set test count
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_TOTAL} = $hModuleTest->{&TESTDEF_TOTAL};

                # If this is a C test then add the test module to coverage
                if ($hModuleTest->{&TESTDEF_C})
                {
                    my $strTestFile = "module/${strModule}/${strTest}Test";

                    $hModuleTest->{&TESTDEF_COVERAGE}{$strTestFile} = TESTDEF_COVERAGE_FULL;
                }

                # Concatenate coverage for modules and tests
                foreach my $hCoverage ($hModule->{&TESTDEF_COVERAGE}, $hModuleTest->{&TESTDEF_COVERAGE})
                {
                    foreach my $strCodeModule (sort(keys(%{$hCoverage})))
                    {
                        if (defined($hTestDefHash->{$strModule}{$strTest}{&TESTDEF_COVERAGE}{$strCodeModule}))
                        {
                            confess &log(ASSERT,
                                "${strCodeModule} is defined for coverage in both module ${strModule} and test ${strTest}");
                        }

                        $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_COVERAGE}{$strCodeModule} = $hCoverage->{$strCodeModule};

                        # Build coverage type hash and make sure coverage type does not change
                        if (!defined($hCoverageType->{$strCodeModule}))
                        {
                            $hCoverageType->{$strCodeModule} = $hCoverage->{$strCodeModule};
                        }
                        elsif ($hCoverageType->{$strCodeModule} ne $hCoverage->{$strCodeModule})
                        {
                            confess &log(ASSERT, "cannot mix coverage types for ${strCodeModule}");
                        }

                        # Add to coverage list
                        push(@{$hCoverageList->{$strCodeModule}}, {strModule=> $strModule, strTest => $strTest});
                    }
                }

                # Set include list
                $hTestDefHash->{$strModule}{$strTest}{&TESTDEF_INCLUDE} = $hModuleTest->{&TESTDEF_INCLUDE};
            }

            $hModuleTest->{$strModule} = \@stryModuleTest;
        }
    }
}

push @EXPORT, qw(testDefLoad);

####################################################################################################################################
# testDefModuleList
####################################################################################################################################
sub testDefModuleList
{
    return @stryModule;
}

push @EXPORT, qw(testDefModuleList);

####################################################################################################################################
# testDefModule
####################################################################################################################################
sub testDefModule
{
    my $strModule = shift;

    if (!defined($hTestDefHash->{$strModule}))
    {
        confess &log(ASSERT, "unable to find module ${strModule}");
    }

    return $hTestDefHash->{$strModule};
}

push @EXPORT, qw(testDefModule);

####################################################################################################################################
# testDefModuleTestList
####################################################################################################################################
sub testDefModuleTestList
{
    my $strModule = shift;

    if (!defined($hModuleTest->{$strModule}))
    {
        confess &log(ASSERT, "unable to find module ${strModule}");
    }

    return @{$hModuleTest->{$strModule}};
}

push @EXPORT, qw(testDefModuleTestList);

####################################################################################################################################
# testDefModuleTest
####################################################################################################################################
sub testDefModuleTest
{
    my $strModule = shift;
    my $strModuleTest = shift;

    if (!defined($hTestDefHash->{$strModule}{$strModuleTest}))
    {
        confess &log(ASSERT, "unable to find module ${strModule}, test ${strModuleTest}");
    }

    return $hTestDefHash->{$strModule}{$strModuleTest};
}

push @EXPORT, qw(testDefModuleTest);

####################################################################################################################################
# testDefCoverageType
####################################################################################################################################
sub testDefCoverageType
{
    return $hCoverageType;
}

push @EXPORT, qw(testDefCoverageType);

####################################################################################################################################
# testDefCoverageList
####################################################################################################################################
sub testDefCoverageList
{
    return $hCoverageList;
}

push @EXPORT, qw(testDefCoverageList);

1;
