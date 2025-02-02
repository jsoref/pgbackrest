/***********************************************************************************************************************************
Test Exec Configuration
***********************************************************************************************************************************/
#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cfgExecParam()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db path", testPath()));
        strLstAddZ(argList, "--log-subprocess");
        strLstAddZ(argList, "--no-config");
        strLstAddZ(argList, "--reset-neutral-umask");
        strLstAddZ(argList, "--repo-cipher-type=aes-256-cbc");
        strLstAddZ(argList, "archive-get");

        // Set repo1-cipher-pass to make sure it is not passed on the command line
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "1234", true);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");

        TEST_RESULT_STR(
            strPtr(strLstJoin(cfgExecParam(cfgCmdLocal, NULL), "|")),
            strPtr(
                strNewFmt(
                    "--no-config|--log-subprocess|--pg1-path=\"%s/db path\"|--repo1-path=%s/repo|--stanza=test1|local", testPath(),
                    testPath())),
            "exec archive-get -> local");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        strLstAddZ(argList, "--db-include=1");
        strLstAddZ(argList, "--db-include=2");
        strLstAddZ(argList, "--recovery-option=a=b");
        strLstAddZ(argList, "--recovery-option=c=d");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        KeyValue *optionReplace = kvNew();
        kvPut(optionReplace, varNewStr(strNew("repo1-path")), varNewStr(strNew("/replace/path")));
        kvPut(optionReplace, varNewStr(strNew("stanza")), NULL);

        TEST_RESULT_STR(
            strPtr(strLstJoin(cfgExecParam(cfgCmdRestore, optionReplace), "|")),
            strPtr(
                strNewFmt(
                    "--db-include=1|--db-include=2|--pg1-path=%s/db|--recovery-option=a=b|--recovery-option=c=d"
                        "|--repo1-path=/replace/path|restore",
                    testPath())),
            "exec restore -> restore");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
