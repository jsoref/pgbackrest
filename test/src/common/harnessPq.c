/***********************************************************************************************************************************
Pq Test Harness
***********************************************************************************************************************************/
#ifndef HARNESS_PQ_REAL

#include <string.h>

#include <libpq-fe.h>

#include "common/type/json.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

#include "common/harnessPq.h"

/***********************************************************************************************************************************
Script that defines how shim functions operate
***********************************************************************************************************************************/
HarnessPq *harnessPqScript;
unsigned int harnessPqScriptIdx;

// If there is a script failure change the behavior of cleanup functions to return immediately so the real error will be reported
// rather than a bogus scripting error during cleanup
bool harnessPqScriptFail;

/***********************************************************************************************************************************
Set pq script
***********************************************************************************************************************************/
void
harnessPqScriptSet(HarnessPq *harnessPqScriptParam)
{
    if (harnessPqScript != NULL)
        THROW(AssertError, "previous pq script has not yet completed");

    if (harnessPqScriptParam[0].function == NULL)
        THROW(AssertError, "pq script must have entries");

    harnessPqScript = harnessPqScriptParam;
    harnessPqScriptIdx = 0;
}

/***********************************************************************************************************************************
Run pq script
***********************************************************************************************************************************/
static HarnessPq *
harnessPqScriptRun(const char *function, const VariantList *param, HarnessPq *parent)
{
    // Convert params to json for comparison and reporting
    String *paramStr = param ? jsonFromVar(varNewVarLst(param), 0) : strNew("");

    // Ensure script has not ended
    if (harnessPqScript == NULL)
    {
        harnessPqScriptFail = true;
        THROW_FMT(AssertError, "pq script ended before %s (%s)", function, strPtr(paramStr));
    }

    // Get current script item
    HarnessPq *result = &harnessPqScript[harnessPqScriptIdx];

    // Check that expected function was called
    if (strcmp(result->function, function) != 0)
    {
        harnessPqScriptFail = true;

        THROW_FMT(
            AssertError, "pq script [%u] expected function %s (%s) but got %s (%s)", harnessPqScriptIdx, result->function,
            result->param == NULL ? "" : result->param, function, strPtr(paramStr));
    }

    // Check that parameters match
    if ((param != NULL && result->param == NULL) || (param == NULL && result->param != NULL) ||
        (param != NULL && result->param != NULL && !strEqZ(paramStr, result->param)))
    {
        harnessPqScriptFail = true;

        THROW_FMT(
            AssertError, "pq script [%u] function '%s', expects param '%s' but got '%s'", harnessPqScriptIdx, result->function,
            result->param ? result->param : "NULL", param ? strPtr(paramStr) : "NULL");
    }

    // Make sure the session matches with the parent as a sanity check
    if (parent != NULL && result->session != parent->session)
    {
        THROW_FMT(
            AssertError, "pq script [%u] function '%s', expects session '%u' but got '%u'", harnessPqScriptIdx, result->function,
            result->session, parent->session);
    }

    // Sleep if requested
    if (result->sleep > 0)
        sleepMSec(result->sleep);

    harnessPqScriptIdx++;

    if (harnessPqScript[harnessPqScriptIdx].function == NULL)
        harnessPqScript = NULL;

    return result;
}

/***********************************************************************************************************************************
Shim for PQconnectdb()
***********************************************************************************************************************************/
PGconn *PQconnectdb(const char *conninfo)
{
    return (PGconn *)harnessPqScriptRun(HRNPQ_CONNECTDB, varLstAdd(varLstNew(), varNewStrZ(conninfo)), NULL);
}

/***********************************************************************************************************************************
Shim for PQstatus()
***********************************************************************************************************************************/
ConnStatusType PQstatus(const PGconn *conn)
{
    return (ConnStatusType)harnessPqScriptRun(HRNPQ_STATUS, NULL, (HarnessPq *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQerrorMessage()
***********************************************************************************************************************************/
char *PQerrorMessage(const PGconn *conn)
{
    return (char *)harnessPqScriptRun(HRNPQ_ERRORMESSAGE, NULL, (HarnessPq *)conn)->resultZ;
}

/***********************************************************************************************************************************
Shim for PQsetNoticeProcessor()
***********************************************************************************************************************************/
PQnoticeProcessor
PQsetNoticeProcessor(PGconn *conn, PQnoticeProcessor proc, void *arg)
{
    (void)conn;

    // Call the processor that was passed so we have coverage
    proc(arg, "test notice");
    return NULL;
}

/***********************************************************************************************************************************
Shim for PQsendQuery()
***********************************************************************************************************************************/
int
PQsendQuery(PGconn *conn, const char *query)
{
    return harnessPqScriptRun(HRNPQ_SENDQUERY, varLstAdd(varLstNew(), varNewStrZ(query)), (HarnessPq *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQconsumeInput()
***********************************************************************************************************************************/
int
PQconsumeInput(PGconn *conn)
{
    return harnessPqScriptRun(HRNPQ_CONSUMEINPUT, NULL, (HarnessPq *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQisBusy()
***********************************************************************************************************************************/
int
PQisBusy(PGconn *conn)
{
    return harnessPqScriptRun(HRNPQ_ISBUSY, NULL, (HarnessPq *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQgetCancel()
***********************************************************************************************************************************/
PGcancel *
PQgetCancel(PGconn *conn)
{
    return (PGcancel *)harnessPqScriptRun(HRNPQ_GETCANCEL, NULL, (HarnessPq *)conn);
}

/***********************************************************************************************************************************
Shim for PQcancel()
***********************************************************************************************************************************/
int
PQcancel(PGcancel *cancel, char *errbuf, int errbufsize)
{
    HarnessPq *harnessPq = harnessPqScriptRun(HRNPQ_CANCEL, NULL, (HarnessPq *)cancel);

    if (!harnessPq->resultInt)
    {
        strncpy(errbuf, harnessPq->resultZ, (size_t)errbufsize);
        errbuf[errbufsize - 1] = '\0';
    }

    return harnessPq->resultInt;
}

/***********************************************************************************************************************************
Shim for PQfreeCancel()
***********************************************************************************************************************************/
void
PQfreeCancel(PGcancel *cancel)
{
    harnessPqScriptRun(HRNPQ_FREECANCEL, NULL, (HarnessPq *)cancel);
}

/***********************************************************************************************************************************
Shim for PQgetResult()
***********************************************************************************************************************************/
PGresult *
PQgetResult(PGconn *conn)
{
    if (!harnessPqScriptFail)
    {
        HarnessPq *harnessPq = harnessPqScriptRun(HRNPQ_GETRESULT, NULL, (HarnessPq *)conn);
        return harnessPq->resultNull ? NULL : (PGresult *)harnessPq;
    }

    return NULL;
}

/***********************************************************************************************************************************
Shim for PQresultStatus()
***********************************************************************************************************************************/
ExecStatusType
PQresultStatus(const PGresult *res)
{
    return (ExecStatusType)harnessPqScriptRun(HRNPQ_RESULTSTATUS, NULL, (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQresultErrorMessage()
***********************************************************************************************************************************/
char *
PQresultErrorMessage(const PGresult *res)
{
    return (char *)harnessPqScriptRun(HRNPQ_RESULTERRORMESSAGE, NULL, (HarnessPq *)res)->resultZ;
}

/***********************************************************************************************************************************
Shim for PQntuples()
***********************************************************************************************************************************/
int
PQntuples(const PGresult *res)
{
    return harnessPqScriptRun(HRNPQ_NTUPLES, NULL, (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQnfields()
***********************************************************************************************************************************/
int
PQnfields(const PGresult *res)
{
    return harnessPqScriptRun(HRNPQ_NFIELDS, NULL, (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQgetisnull()
***********************************************************************************************************************************/
int
PQgetisnull(const PGresult *res, int tup_num, int field_num)
{
    return harnessPqScriptRun(
        HRNPQ_GETISNULL, varLstAdd(varLstAdd(varLstNew(), varNewInt(tup_num)), varNewInt(field_num)), (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQftype()
***********************************************************************************************************************************/
Oid
PQftype(const PGresult *res, int field_num)
{
    return (Oid)harnessPqScriptRun(HRNPQ_FTYPE, varLstAdd(varLstNew(), varNewInt(field_num)), (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQgetvalue()
***********************************************************************************************************************************/
char *
PQgetvalue(const PGresult *res, int tup_num, int field_num)
{
    return (char *)harnessPqScriptRun(
        HRNPQ_GETVALUE, varLstAdd(varLstAdd(varLstNew(), varNewInt(tup_num)), varNewInt(field_num)), (HarnessPq *)res)->resultZ;
}

/***********************************************************************************************************************************
Shim for PQclear()
***********************************************************************************************************************************/
void
PQclear(PGresult *res)
{
    if (!harnessPqScriptFail)
        harnessPqScriptRun(HRNPQ_CLEAR, NULL, (HarnessPq *)res);
}

/***********************************************************************************************************************************
Shim for PQfinish()
***********************************************************************************************************************************/
void PQfinish(PGconn *conn)
{
    if (!harnessPqScriptFail)
        harnessPqScriptRun(HRNPQ_FINISH, NULL, (HarnessPq *)conn);
}

#endif // HARNESS_PQ_REAL
