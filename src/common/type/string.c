/***********************************************************************************************************************************
String Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/macro.h"
#include "common/memContext.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constant strings that are generally useful
***********************************************************************************************************************************/
STRING_EXTERN(BRACKETL_STR,                                         "[");
STRING_EXTERN(BRACKETR_STR,                                         "]");
STRING_EXTERN(CR_STR,                                               "\r");
STRING_EXTERN(EMPTY_STR,                                            "");
STRING_EXTERN(EQ_STR,                                               "=");
STRING_EXTERN(FALSE_STR,                                            "false");
STRING_EXTERN(FSLASH_STR,                                           "/");
STRING_EXTERN(LF_STR,                                               "\n");
STRING_EXTERN(N_STR,                                                "n");
STRING_EXTERN(NULL_STR,                                             "null");
STRING_EXTERN(TRUE_STR,                                             "true");
STRING_EXTERN(Y_STR,                                                "y");
STRING_EXTERN(ZERO_STR,                                             "0");

/***********************************************************************************************************************************
Maximum size of a string
***********************************************************************************************************************************/
#define STRING_SIZE_MAX                                            1073741824

#define CHECK_SIZE(size)                                                                                                           \
    do                                                                                                                             \
    {                                                                                                                              \
        if ((size) > STRING_SIZE_MAX)                                                                                              \
            THROW(AssertError, "string size must be <= " STRINGIFY(STRING_SIZE_MAX) " bytes");                                     \
    }                                                                                                                              \
    while(0)

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct String
{
    STRING_COMMON                                                   // Shared in common with constant strings
    MemContext *memContext;                                         // Required for dynamically allocated strings
};

/***********************************************************************************************************************************
Create a new string from a zero-terminated string
***********************************************************************************************************************************/
String *
strNew(const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, string);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);

    // Check size
    size_t stringSize = strlen(string);
    CHECK_SIZE(stringSize);

    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->size = (unsigned int)stringSize;

    // Allocate and assign string
    this->buffer = memNewRaw(this->size + 1);
    strcpy(this->buffer, string);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create a new string from a buffer

If the buffer has a NULL character this may not work as expected.  All the data will be copied but only the data before the NULL
character will be used as a string.
***********************************************************************************************************************************/
String *
strNewBuf(const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Check size
    CHECK_SIZE(bufUsed(buffer));

    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->size = (unsigned int)bufUsed(buffer);

    // Allocate and assign string
    this->buffer = memNewRaw(this->size + 1);
    memcpy(this->buffer, (char *)bufPtr(buffer), this->size);
    this->buffer[this->size] = 0;

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create a new string from a format string with parameters (i.e. sprintf)
***********************************************************************************************************************************/
String *
strNewFmt(const char *format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    ASSERT(format != NULL);

    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    size_t formatSize = (size_t)vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Check size
    CHECK_SIZE(formatSize);

    // Allocate and assign string
    this->size = (unsigned int)formatSize;
    this->buffer = memNewRaw(this->size + 1);
    va_start(argumentList, format);
    vsnprintf(this->buffer, this->size + 1, format, argumentList);
    va_end(argumentList);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create a new string from a string with a specific length

The string may or may not be zero-terminated but we'll use that nomeclature since we're not concerned about the end of the string.
***********************************************************************************************************************************/
String *
strNewN(const char *string, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(CHARDATA, string);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);

    // Check size
    CHECK_SIZE(size);

    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->size = (unsigned int)size;

    // Allocate and assign string
    this->buffer = memNewRaw(this->size + 1);
    strncpy(this->buffer, string, this->size);
    this->buffer[this->size] = 0;

    // Return buffer
    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Return the file part of a string (i.e. everything after the last / or the entire string if there is no /)
***********************************************************************************************************************************/
String *
strBase(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const char *end = this->buffer + this->size;

    while (end > this->buffer && *(end - 1) != '/')
        end--;

    FUNCTION_TEST_RETURN(strNew(end));
}

/***********************************************************************************************************************************
Does the string begin with the specified string?
***********************************************************************************************************************************/
bool
strBeginsWith(const String *this, const String *beginsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, beginsWith);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(beginsWith != NULL);

    FUNCTION_TEST_RETURN(strBeginsWithZ(this, strPtr(beginsWith)));
}

bool
strBeginsWithZ(const String *this, const char *beginsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, beginsWith);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(beginsWith != NULL);

    bool result = false;
    unsigned int beginsWithSize = (unsigned int)strlen(beginsWith);

    if (this->size >= beginsWithSize)
        result = strncmp(strPtr(this), beginsWith, beginsWithSize) == 0;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Resize the string to allow the requested number of characters to be appended
***********************************************************************************************************************************/
static void
strResize(String *this, size_t requested)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(SIZE, requested);
    FUNCTION_TEST_END();

    if (requested > this->extra)
    {
        // Check size
        CHECK_SIZE((size_t)this->size + requested);

        // Calculate new extra needs to satisfy request and leave extra space for new growth
        this->extra = (unsigned int)requested + ((this->size + (unsigned int)requested) / 2);

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->buffer = memGrowRaw(this->buffer, this->size + this->extra + 1);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Append a string
***********************************************************************************************************************************/
String *
strCat(String *this, const char *cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, cat);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(cat != NULL);

    // Determine length of string to append
    size_t sizeGrow = strlen(cat);

    // Ensure there is enough space to grow the string
    strResize(this, sizeGrow);

    // Append the string
    strcpy(this->buffer + this->size, cat);
    this->size += (unsigned int)sizeGrow;
    this->extra -= (unsigned int)sizeGrow;

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Append a character
***********************************************************************************************************************************/
String *
strCatChr(String *this, char cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, cat);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(cat != 0);

    // Ensure there is enough space to grow the string
    strResize(this, 1);

    // Append the character
    this->buffer[this->size++] = cat;
    this->buffer[this->size] = 0;
    this->extra--;

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Append a formatted string
***********************************************************************************************************************************/
String *
strCatFmt(String *this, const char *format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(format != NULL);

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    size_t sizeGrow = (size_t)vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Ensure there is enough space to grow the string
    strResize(this, sizeGrow);

    // Append the formatted string
    va_start(argumentList, format);
    vsnprintf(this->buffer + this->size, sizeGrow + 1, format, argumentList);
    va_end(argumentList);

    this->size += (unsigned int)sizeGrow;
    this->extra -= (unsigned int)sizeGrow;

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
C-style string compare
***********************************************************************************************************************************/
int
strCmp(const String *this, const String *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, compare);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(compare != NULL);

    FUNCTION_TEST_RETURN(strcmp(strPtr(this), strPtr(compare)));
}

int
strCmpZ(const String *this, const char *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, compare);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(compare != NULL);

    FUNCTION_TEST_RETURN(strcmp(strPtr(this), compare));
}

/***********************************************************************************************************************************
Duplicate a string from an existing string
***********************************************************************************************************************************/
String *
strDup(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
        result = strNew(strPtr(this));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Is the string empty?
***********************************************************************************************************************************/
bool
strEmpty(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(strSize(this) == 0);
}

/***********************************************************************************************************************************
Does the string end with the specified string?
***********************************************************************************************************************************/
bool
strEndsWith(const String *this, const String *endsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, endsWith);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(endsWith != NULL);

    FUNCTION_TEST_RETURN(strEndsWithZ(this, strPtr(endsWith)));
}

bool
strEndsWithZ(const String *this, const char *endsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, endsWith);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(endsWith != NULL);

    bool result = false;
    unsigned int endsWithSize = (unsigned int)strlen(endsWith);

    if (this->size >= endsWithSize)
        result = strcmp(strPtr(this) + (this->size - endsWithSize), endsWith) == 0;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Are two strings equal?

There are two separate implementations because string objects can get the size very efficiently whereas the zero-terminated strings
would need a call to strlen().
***********************************************************************************************************************************/
bool
strEq(const String *this, const String *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, compare);
    FUNCTION_TEST_END();

    bool result = false;

    if (this != NULL && compare != NULL)
    {
        if (this->size == compare->size)
            result = strcmp(strPtr(this), strPtr(compare)) == 0;
    }
    else
        result = this == NULL && compare == NULL;

    FUNCTION_TEST_RETURN(result);
}

bool
strEqZ(const String *this, const char *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, compare);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(compare != NULL);

    FUNCTION_TEST_RETURN(strcmp(strPtr(this), compare) == 0);
}

/***********************************************************************************************************************************
Upper-case the first letter
***********************************************************************************************************************************/
String *
strFirstUpper(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->size > 0)
        this->buffer[0] = (char)toupper(this->buffer[0]);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Lower-case the first letter
***********************************************************************************************************************************/
String *
strFirstLower(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->size > 0)
        this->buffer[0] = (char)tolower(this->buffer[0]);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Upper-case entire string
***********************************************************************************************************************************/
String *
strUpper(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->size > 0)
        for (unsigned int idx = 0; idx <= this->size; idx++)
            this->buffer[idx] = (char)toupper(this->buffer[idx]);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Upper-case entire string
***********************************************************************************************************************************/
String *
strLower(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->size > 0)
        for (unsigned int idx = 0; idx <= this->size; idx++)
            this->buffer[idx] = (char)tolower(this->buffer[idx]);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Return the path part of a string (i.e. everything before the last / or "" if there is no /)
***********************************************************************************************************************************/
String *
strPath(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const char *end = this->buffer + this->size;

    while (end > this->buffer && *(end - 1) != '/')
        end--;

    FUNCTION_TEST_RETURN(
        strNewN(
            this->buffer,
            end - this->buffer <= 1 ? (size_t)(end - this->buffer) : (size_t)(end - this->buffer - 1)));
}

/***********************************************************************************************************************************
Return string ptr
***********************************************************************************************************************************/
const char *
strPtr(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    char *result = NULL;

    if (this != NULL)
        result = this->buffer;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Quote a string
***********************************************************************************************************************************/
String *
strQuote(const String *this, const String *quote)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, quote);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(quote != NULL);

    FUNCTION_TEST_RETURN(strQuoteZ(this, strPtr(quote)));
}

String *
strQuoteZ(const String *this, const char *quote)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, quote);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(quote != NULL);

    FUNCTION_TEST_RETURN(strNewFmt("%s%s%s", quote, strPtr(this), quote));
}

/***********************************************************************************************************************************
Replace a character with another character
***********************************************************************************************************************************/
String *
strReplaceChr(String *this, char find, char replace)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, find);
        FUNCTION_TEST_PARAM(CHAR, replace);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    for (unsigned int stringIdx = 0; stringIdx < this->size; stringIdx++)
    {
        if (this->buffer[stringIdx] == find)
            this->buffer[stringIdx] = replace;
    }

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Return a substring given only the start position
***********************************************************************************************************************************/
String *
strSub(const String *this, size_t start)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(SIZE, start);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(start < this->size);

    FUNCTION_TEST_RETURN(strSubN(this, start, this->size - start));
}

/***********************************************************************************************************************************
Return a substring given the start position and size
***********************************************************************************************************************************/
String *
strSubN(const String *this, size_t start, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(SIZE, start);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(start < this->size);
    ASSERT(start + size <= this->size);

    FUNCTION_TEST_RETURN(strNewN(this->buffer + start, size));
}

/***********************************************************************************************************************************
Return string size
***********************************************************************************************************************************/
size_t
strSize(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->size);
}

/***********************************************************************************************************************************
Trim whitespace from the beginnning and end of a string
***********************************************************************************************************************************/
String *
strTrim(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Nothing to trim if size is zero
    if (this->size > 0)
    {
        // Find the beginning of the string skipping all whitespace
        char *begin = this->buffer;

        while (*begin != 0 && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n'))
            begin++;

        // Find the end of the string skipping all whitespace
        char *end = this->buffer + (this->size - 1);

        while (end > begin && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
            end--;

        // Is there anything to trim?
        size_t newSize = (size_t)(end - begin + 1);

        if (begin != this->buffer || newSize < strSize(this))
        {
            // Calculate new size
            this->size = (unsigned int)newSize;

            // Move the substr to the beginning of the buffer
            memmove(this->buffer, begin, this->size);
            this->buffer[this->size] = 0;
            this->extra = 0;

            MEM_CONTEXT_BEGIN(this->memContext)
            {
                // Resize the buffer
                this->buffer = memGrowRaw(this->buffer, this->size + 1);
            }
            MEM_CONTEXT_END();
        }
    }

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Return the index to the location of the the first occurrence of a character within a string, else -1
***********************************************************************************************************************************/
int
strChr(const String *this, char chr)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, chr);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    int result = -1;

    if (this->size > 0)
    {
        const char *ptr = strchr(this->buffer, chr);

        if (ptr != NULL)
            result = (int)(ptr - this->buffer);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Truncate the end of a string from the index provided to the current end (e.g. 123KB pass index of K returns 123)
***********************************************************************************************************************************/
String *
strTrunc(String *this, int idx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(idx >= 0 && (size_t)idx <= this->size);

    if (this->size > 0)
    {
        // Reset the size to end at the index
        this->size = (unsigned int)(idx);
        this->buffer[this->size] = 0;
        this->extra = 0;

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            // Resize the buffer
            this->buffer = memGrowRaw(this->buffer, this->size + 1);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Convert an object to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t strObjToLog(const void *object, StrObjToLogFormat formatFunc, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = (size_t)snprintf(buffer, bufferSize, "%s", object == NULL ? strPtr(NULL_STR) : strPtr(formatFunc(object)));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
strToLog(const String *this)
{
    return this == NULL ? strDup(NULL_STR) : strNewFmt("{\"%s\"}", strPtr(this));
}

/***********************************************************************************************************************************
Format sizes (file, buffer, etc.) in human-readable form
***********************************************************************************************************************************/
String *
strSizeFormat(const uint64_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, size);

    FUNCTION_TEST_END();

    String *result = NULL;

    if (size < 1024)
        result = strNewFmt("%" PRIu64 "B", size);
    else if (size < (1024 * 1024))
    {
        if ((uint64_t)((double)size / 102.4) % 10 != 0)
            result = strNewFmt("%.1lfKB", (double)size / 1024);
        else
            result = strNewFmt("%" PRIu64 "KB", size / 1024);
    }
    else if (size < (1024 * 1024 * 1024))
    {
        if ((uint64_t)((double)size / (1024 * 102.4)) % 10 != 0)
            result = strNewFmt("%.1lfMB", (double)size / (1024 * 1024));
        else
            result = strNewFmt("%" PRIu64 "MB", size / (1024 * 1024));
    }
    else
    {
        if ((uint64_t)((double)size / (1024 * 1024 * 102.4)) % 10 != 0)
            result = strNewFmt("%.1lfGB", (double)size / (1024 * 1024 * 1024));
        else
            result = strNewFmt("%" PRIu64 "GB", size / (1024 * 1024 * 1024));
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Free the string
***********************************************************************************************************************************/
void
strFree(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    if (this != NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            memFree(this->buffer);
            memFree(this);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}
