/***********************************************************************************************************************************
Helper macros for LibC.xs
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Package Names
***********************************************************************************************************************************/
#define PACKAGE_NAME                                                "pgBackRest"
#define PACKAGE_NAME_LIBC                                           PACKAGE_NAME "::LibC"

/***********************************************************************************************************************************
Load C error into ERROR_SV_error

#define FUNCTION_NAME_ERROR                                         "libcExceptionNew"

#define ERROR_SV()                                                                                                                 \
    SV *ERROR_SV_error;                                                                                                            \
                                                                                                                                   \
    {                                                                                                                              \
        // Push parameters onto the Perl stack                                                                                     \
        ENTER;                                                                                                                     \
        SAVETMPS;                                                                                                                  \
        PUSHMARK(SP);                                                                                                              \
        EXTEND(SP, 2);                                                                                                             \
        PUSHs(sv_2mortal(newSViv(errorCode())));                                                                                   \
        PUSHs(sv_2mortal(newSVpv(errorMessage(), 0)));                                                                             \
        PUTBACK;                                                                                                                   \
                                                                                                                                   \
        // Call error function                                                                                                     \
        int count = call_pv(PACKAGE_NAME_LIBC "::" FUNCTION_NAME_ERROR, G_SCALAR);                                                 \
        SPAGAIN;                                                                                                                   \
                                                                                                                                   \
        // Check that correct number of parameters was returned                                                                    \
        if (count != 1)                                                                                                            \
            croak("expected 1 return value from " FUNCTION_NAME_ERROR "()");                                                       \
                                                                                                                                   \
        // Make a copy of the error that can be returned                                                                           \
        ERROR_SV_error = newSVsv(POPs);                                                                                            \
                                                                                                                                   \
        // Clean up the stack                                                                                                      \
        PUTBACK;                                                                                                                   \
        FREETMPS;                                                                                                                  \
        LEAVE;                                                                                                                     \
    }

This turned out to be a dead end because Perl 5.10 does not support croak_sv(), but this code has been kept for example purposes.
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Error handling macros that throw a Perl error when a C error is caught
***********************************************************************************************************************************/
#define ERROR_XS_BEGIN()                                                                                                           \
    TRY_BEGIN()

#define ERROR_XS()                                                                                                                 \
    croak("PGBRCLIB:%d:%s:%d:%s", errorCode(), errorFileName(), errorFileLine(), errorMessage());

#define ERROR_XS_END()                                                                                                             \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        ERROR_XS();                                                                                                                \
    }                                                                                                                              \
    TRY_END();

/***********************************************************************************************************************************
Core context handling macros, only intended to be called from other macros
***********************************************************************************************************************************/
#define MEM_CONTEXT_XS_OLD()                                                                                                       \
    MEM_CONTEXT_XS_memContextOld

#define MEM_CONTEXT_XS_CORE_BEGIN(memContext)                                                                                      \
    /* Switch to the new memory context */                                                                                         \
    MemContext *MEM_CONTEXT_XS_OLD() = memContextSwitch(memContext);                                                               \
                                                                                                                                   \
    /* Store any errors to be croaked to Perl at the end */                                                                        \
    bool MEM_CONTEXT_XS_croak = false;                                                                                             \
                                                                                                                                   \
    /* Try the statement block */                                                                                                  \
    TRY_BEGIN()

#define MEM_CONTEXT_XS_CORE_END()                                                                                                  \
    /* Set error to be croak to Perl later */                                                                                      \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        MEM_CONTEXT_XS_croak = true;                                                                                               \
    }                                                                                                                              \
    /* Free the context on error */                                                                                                \
    FINALLY()                                                                                                                      \
    {                                                                                                                              \
        memContextSwitch(MEM_CONTEXT_XS_OLD());                                                                                    \
    }                                                                                                                              \
    TRY_END();

/***********************************************************************************************************************************
Simplifies creation of the memory context in contructors and includes error handling
***********************************************************************************************************************************/
#define MEM_CONTEXT_XS_NEW_BEGIN(contextName)                                                                                      \
{                                                                                                                                  \
    /* Attempt to create the memory context */                                                                                     \
    MemContext *MEM_CONTEXT_XS_memContext = NULL;                                                                                  \
                                                                                                                                   \
    TRY_BEGIN()                                                                                                                    \
    {                                                                                                                              \
        MEM_CONTEXT_XS_memContext = memContextNew(contextName);                                                                    \
    }                                                                                                                              \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        ERROR_XS()                                                                                                                 \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
                                                                                                                                   \
    MEM_CONTEXT_XS_CORE_BEGIN(MEM_CONTEXT_XS_memContext)

#define MEM_CONTEXT_XS_NEW_END()                                                                                                   \
    MEM_CONTEXT_XS_CORE_END();                                                                                                     \
                                                                                                                                   \
    /* Free context and croak on error */                                                                                          \
    if (MEM_CONTEXT_XS_croak)                                                                                                      \
    {                                                                                                                              \
        memContextFree(MEM_CONTEXT_XS_memContext);                                                                                 \
                                                                                                                                   \
        ERROR_XS()                                                                                                                 \
    }                                                                                                                              \
}

#define MEM_CONTEXT_XS()                                                                                                           \
    MEM_CONTEXT_XS_memContext

/***********************************************************************************************************************************
Simplifies switching the memory context in functions and includes error handling
***********************************************************************************************************************************/
#define MEM_CONTEXT_XS_BEGIN(memContext)                                                                                           \
{                                                                                                                                  \
    MEM_CONTEXT_XS_CORE_BEGIN(memContext)

#define MEM_CONTEXT_XS_END()                                                                                                       \
    MEM_CONTEXT_XS_CORE_END();                                                                                                     \
                                                                                                                                   \
    /* Croak on error */                                                                                                           \
    if (MEM_CONTEXT_XS_croak)                                                                                                      \
    {                                                                                                                              \
        ERROR_XS()                                                                                                                 \
    }                                                                                                                              \
}

/***********************************************************************************************************************************
Simplifies switching to a temp memory context in functions and includes error handling
***********************************************************************************************************************************/
#define MEM_CONTEXT_XS_TEMP()                                                                                                      \
    MEM_CONTEXT_XS_TEMP_memContext

#define MEM_CONTEXT_XS_TEMP_BEGIN()                                                                                                \
{                                                                                                                                  \
    MemContext *MEM_CONTEXT_XS_TEMP() = memContextNew("temporary");                                                                \
                                                                                                                                   \
    MEM_CONTEXT_XS_CORE_BEGIN(MEM_CONTEXT_XS_TEMP())

#define MEM_CONTEXT_XS_TEMP_END()                                                                                                  \
    /* Set error to be croak to Perl later */                                                                                      \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        MEM_CONTEXT_XS_croak = true;                                                                                               \
    }                                                                                                                              \
    /* Free the context on error */                                                                                                \
    FINALLY()                                                                                                                      \
    {                                                                                                                              \
            memContextSwitch(MEM_CONTEXT_XS_OLD());                                                                                \
            memContextFree(MEM_CONTEXT_XS_TEMP());                                                                                 \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
                                                                                                                                   \
    /* Croak on error */                                                                                                           \
    if (MEM_CONTEXT_XS_croak)                                                                                                      \
    {                                                                                                                              \
        ERROR_XS()                                                                                                                 \
    }                                                                                                                              \
}

/***********************************************************************************************************************************
Free memory context in destructors
***********************************************************************************************************************************/
#define MEM_CONTEXT_XS_DESTROY(memContext)                                                                                         \
    memContextFree(memContext)

/***********************************************************************************************************************************
Create new string from an SV
***********************************************************************************************************************************/
#define STR_NEW_SV(param)                                                                                                          \
    (SvOK(param) ? strNewN(SvPV_nolen(param), SvCUR(param)) : NULL)

/***********************************************************************************************************************************
Create const buffer from an SV
***********************************************************************************************************************************/
#define BUF_CONST_SV(param)                                                                                                        \
    (SvOK(param) ? BUF(SvPV_nolen(param), SvCUR(param)) : NULL)
