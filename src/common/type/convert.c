/***********************************************************************************************************************************
Convert Base Data Types
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/type/convert.h"

/***********************************************************************************************************************************
Check results of strto*() function for:
    * leading/trailing spaces
    * invalid characters
    * blank string
    * error in errno
***********************************************************************************************************************************/
static void
cvtZToIntValid(int errNo, int base, const char *value, const char *endPtr, const char *type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, errNo);
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(STRINGZ, endPtr);
        FUNCTION_TEST_PARAM(STRINGZ, type);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);
    ASSERT(endPtr != NULL);

    if (errNo != 0 || *value == '\0' || isspace(*value) || *endPtr != '\0')
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to %s", base, value, type);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Convert zero-terminated string to int64 and validate result
***********************************************************************************************************************************/
static int64_t
cvtZToInt64Internal(const char *value, const char *type, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(STRINGZ, type);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);
    ASSERT(type != NULL);

    // Convert from string
    errno = 0;
    char *endPtr = NULL;
    int64_t result = strtoll(value, &endPtr, base);

    // Validate the result
    cvtZToIntValid(errno, base, value, endPtr, type);

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert zero-terminated string to uint64 and validate result
***********************************************************************************************************************************/
static uint64_t
cvtZToUInt64Internal(const char *value, const char *type, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(STRINGZ, type);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);
    ASSERT(type != NULL);

    // Convert from string
    errno = 0;
    char *endPtr = NULL;
    uint64_t result = strtoull(value, &endPtr, base);

    // Validate the result
    cvtZToIntValid(errno, base, value, endPtr, type);

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert boolean to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtBoolToZ(bool value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%s", cvtBoolToConstZ(value));

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

// Since booleans only have two possible values we can return a const with the value.  This is useful when a boolean needs to be
// output as part of a large string.
const char *
cvtBoolToConstZ(bool value)
{
    return value ? "true" : "false";
}

/***********************************************************************************************************************************
Convert char to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtCharToZ(char value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%c", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert double to zero-terminated string and vice versa
***********************************************************************************************************************************/
size_t
cvtDoubleToZ(double value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DOUBLE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Convert to a string
    size_t result = (size_t)snprintf(buffer, bufferSize, "%lf", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    // Any formatted double should be at least 8 characters, i.e. 0.000000
    ASSERT(strlen(buffer) >= 8);
    // Any formatted double should have a decimal point
    ASSERT(strchr(buffer, '.') != NULL);

    // Strip off any final 0s and the decimal point if there are no non-zero digits after it
    char *end = buffer + strlen(buffer) - 1;

    while (*end == '0' || *end == '.')
    {
        // It should not be possible to go past the beginning because format "%lf" will always write a decimal point
        ASSERT(end > buffer);

        end--;

        if (*(end + 1) == '.')
            break;
    }

    // Zero terminate the string
    end[1] = 0;

    // Return string length
    FUNCTION_TEST_RETURN((size_t)(end - buffer + 1));
}

double
cvtZToDouble(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    double result = 0;
    sscanf(value, "%lf", &result);

    if (result == 0 && strcmp(value, "0") != 0)
        THROW_FMT(FormatError, "unable to convert string '%s' to double", value);

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert int to zero-terminated string and vice versa
***********************************************************************************************************************************/
size_t
cvtIntToZ(int value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%d", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

int
cvtZToIntBase(const char *value, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    int64_t result = cvtZToInt64Internal(value, "int", base);

    if (result > INT_MAX || result < INT_MIN)
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to int", base, value);

    FUNCTION_TEST_RETURN((int)result);
}

int
cvtZToInt(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(cvtZToIntBase(value, 10));
}

/***********************************************************************************************************************************
Convert int64 to zero-terminated string and vice versa
***********************************************************************************************************************************/
size_t
cvtInt64ToZ(int64_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%" PRId64, value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

int64_t
cvtZToInt64Base(const char *value, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(cvtZToInt64Internal(value, "int64", base));
}

int64_t
cvtZToInt64(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(cvtZToInt64Base(value, 10));
}

/***********************************************************************************************************************************
Convert mode to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtModeToZ(mode_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MODE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%04o", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

mode_t
cvtZToMode(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(cvtZToUIntBase(value, 8));
}

/***********************************************************************************************************************************
Convert size to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtSizeToZ(size_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%zu", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert ssize to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtSSizeToZ(ssize_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SSIZE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%zd", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert uint to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtUIntToZ(unsigned int value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%u", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

unsigned int
cvtZToUIntBase(const char *value, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    uint64_t result = cvtZToUInt64Internal(value, "unsigned int", base);

    // Don't allow negative numbers even though strtoull() does and check max value
    if (*value == '-' || result > UINT_MAX)
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to unsigned int", base, value);

    FUNCTION_TEST_RETURN((unsigned int)result);
}

unsigned int
cvtZToUInt(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(cvtZToUIntBase(value, 10));
}

/***********************************************************************************************************************************
Convert uint64 to zero-terminated string and visa versa
***********************************************************************************************************************************/
size_t
cvtUInt64ToZ(uint64_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%" PRIu64, value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(result);
}

uint64_t
cvtZToUInt64Base(const char *value, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    uint64_t result = cvtZToUInt64Internal(value, "uint64", base);

    // Don't allow negative numbers even though strtoull() does
    if (*value == '-')
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to uint64", base, value);

    FUNCTION_TEST_RETURN(result);
}

uint64_t
cvtZToUInt64(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(cvtZToUInt64Base(value, 10));
}
