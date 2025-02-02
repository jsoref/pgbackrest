/***********************************************************************************************************************************
Test IO
***********************************************************************************************************************************/
#include <fcntl.h>

#include "common/type/json.h"

#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test functions for IoRead that are not covered by testing the IoBufferRead object
***********************************************************************************************************************************/
static bool
testIoReadOpen(void *driver)
{
    if (driver == (void *)998)
        return false;

    return true;
}

static size_t
testIoRead(void *driver, Buffer *buffer, bool block)
{
    ASSERT(driver == (void *)999);
    (void)block;

    bufCat(buffer, BUFSTRDEF("Z"));

    return 1;
}

static bool testIoReadCloseCalled = false;

static void
testIoReadClose(void *driver)
{
    ASSERT(driver == (void *)999);
    testIoReadCloseCalled = true;
}

/***********************************************************************************************************************************
Test functions for IoWrite that are not covered by testing the IoBufferWrite object
***********************************************************************************************************************************/
static bool testIoWriteOpenCalled = false;

static void
testIoWriteOpen(void *driver)
{
    ASSERT(driver == (void *)999);
    testIoWriteOpenCalled = true;
}

static void
testIoWrite(void *driver, const Buffer *buffer)
{
    ASSERT(driver == (void *)999);
    ASSERT(strEq(strNewBuf(buffer), strNew("ABC")));
}

static bool testIoWriteCloseCalled = false;

static void
testIoWriteClose(void *driver)
{
    ASSERT(driver == (void *)999);
    testIoWriteCloseCalled = true;
}

/***********************************************************************************************************************************
Test filter that counts total bytes
***********************************************************************************************************************************/
typedef struct IoTestFilterSize
{
    MemContext *memContext;
    size_t size;
} IoTestFilterSize;

static void
ioTestFilterSizeProcess(THIS_VOID, const Buffer *buffer)
{
    THIS(IoTestFilterSize);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(SIZE, this->size);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    this->size += bufUsed(buffer);

    FUNCTION_LOG_RETURN_VOID();
}

static Variant *
ioTestFilterSizeResult(THIS_VOID)
{
    THIS(IoTestFilterSize);

    return varNewUInt64(this->size);
}

static IoFilter *
ioTestFilterSizeNew(const char *type)
{
    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoTestFilterSize")
    {
        IoTestFilterSize *driver = memNew(sizeof(IoTestFilterSize));
        driver->memContext = MEM_CONTEXT_NEW();

        this = ioFilterNewP(strNew(type), driver, NULL, .in = ioTestFilterSizeProcess, .result = ioTestFilterSizeResult);
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Test filter to multiply input to the output.  It can also flush out a variable number of bytes at the end.
***********************************************************************************************************************************/
typedef struct IoTestFilterMultiply
{
    MemContext *memContext;
    unsigned int flushTotal;
    bool writeZero;
    char flushChar;
    Buffer *multiplyBuffer;
    unsigned int multiplier;
    IoFilter *bufferFilter;
} IoTestFilterMultiply;

static void
ioTestFilterMultiplyProcess(THIS_VOID, const Buffer *input, Buffer *output)
{
    THIS(IoTestFilterMultiply);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(output != NULL && bufRemains(output) > 0);

    if (input == NULL)
    {
        // Write nothing into the output buffer to make sure the filter processing will skip the remaining filters
        if (!this->writeZero)
        {
            this->writeZero = true;
        }
        else
        {
            char flushZ[] = {this->flushChar, 0};
            bufCat(output, bufNewC(flushZ, 1));
            this->flushTotal--;
        }
    }
    else
    {
        if (this->multiplyBuffer == NULL)
        {
            this->multiplyBuffer = bufNew(bufUsed(input) * this->multiplier);
            unsigned char *inputPtr = bufPtr(input);
            unsigned char *bufferPtr = bufPtr(this->multiplyBuffer);

            for (unsigned int charIdx = 0; charIdx < bufUsed(input); charIdx++)
            {
                for (unsigned int multiplierIdx = 0; multiplierIdx < this->multiplier; multiplierIdx++)
                    bufferPtr[charIdx * this->multiplier + multiplierIdx] = inputPtr[charIdx];
            }

            bufUsedSet(this->multiplyBuffer, bufSize(this->multiplyBuffer));
        }

        ioFilterProcessInOut(this->bufferFilter, this->multiplyBuffer, output);

        if (!ioFilterInputSame(this->bufferFilter))
            this->multiplyBuffer = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

static bool
ioTestFilterMultiplyDone(const THIS_VOID)
{
    THIS(const IoTestFilterMultiply);

    return this->flushTotal == 0;
}

static bool
ioTestFilterMultiplyInputSame(const THIS_VOID)
{
    THIS(const IoTestFilterMultiply);

    return ioFilterInputSame(this->bufferFilter);
}

static IoFilter *
ioTestFilterMultiplyNew(const char *type, unsigned int multiplier, unsigned int flushTotal, char flushChar)
{
    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoTestFilterMultiply")
    {
        IoTestFilterMultiply *driver = memNew(sizeof(IoTestFilterMultiply));
        driver->memContext = MEM_CONTEXT_NEW();
        driver->bufferFilter = ioBufferNew();
        driver->multiplier = multiplier;
        driver->flushTotal = flushTotal;
        driver->flushChar = flushChar;

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ(type));
        varLstAdd(paramList, varNewUInt(multiplier));
        varLstAdd(paramList, varNewUInt(flushTotal));

        this = ioFilterNewP(
            strNew(type), driver, paramList, .done = ioTestFilterMultiplyDone, .inOut = ioTestFilterMultiplyProcess,
            .inputSame = ioTestFilterMultiplyInputSame);
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("ioBufferSize() and ioBufferSizeSet()"))
    {
        TEST_RESULT_SIZE(ioBufferSize(), 65536, "check initial buffer size");
        TEST_RESULT_VOID(ioBufferSizeSet(16384), "set buffer size");
        TEST_RESULT_SIZE(ioBufferSize(), 16384, "check buffer size");
    }

    // *****************************************************************************************************************************
    if (testBegin("IoRead, IoBufferRead, IoBuffer, IoSize, IoFilter, IoFilterGroup, and ioReadBuf()"))
    {
        IoRead *read = NULL;
        Buffer *buffer = bufNew(2);
        ioBufferSizeSet(2);

        TEST_ASSIGN(
            read, ioReadNewP((void *)998, .close = testIoReadClose, .open = testIoReadOpen, .read = testIoRead),
            "create io read object");

        TEST_RESULT_BOOL(ioReadOpen(read), false, "    open io object");

        TEST_ASSIGN(
            read, ioReadNewP((void *)999, .close = testIoReadClose, .open = testIoReadOpen, .read = testIoRead),
            "create io read object");

        TEST_RESULT_BOOL(ioReadOpen(read), true, "    open io object");
        TEST_RESULT_SIZE(ioRead(read, buffer), 2, "    read 2 bytes");
        TEST_RESULT_BOOL(ioReadEof(read), false, "    no eof");
        TEST_RESULT_VOID(ioReadClose(read), "    close io object");
        TEST_RESULT_BOOL(testIoReadCloseCalled, true, "    check io object closed");

        TEST_RESULT_VOID(ioReadFree(read), "    free read object");

        // Read a zero-length buffer to be sure it is not passed on to the filter group
        // -------------------------------------------------------------------------------------------------------------------------
        IoRead *bufferRead = NULL;
        ioBufferSizeSet(2);
        buffer = bufNew(2);
        Buffer *bufferOriginal = bufNew(0);

        TEST_ASSIGN(bufferRead, ioBufferReadNew(bufferOriginal), "create empty buffer read object");
        TEST_RESULT_BOOL(ioReadOpen(bufferRead), true, "    open");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), false, "    not eof");
        TEST_RESULT_SIZE(ioRead(bufferRead, buffer), 0, "    read 0 bytes");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), true, "    now eof");

        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(2);
        buffer = bufNew(2);
        bufferOriginal = bufNewC("123", 3);

        TEST_ASSIGN(bufferRead, ioBufferReadNew(bufferOriginal), "create buffer read object");

        TEST_RESULT_VOID(ioFilterGroupClear(ioReadFilterGroup(bufferRead)), "    clear does nothing when no filters");
        TEST_RESULT_VOID(ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioSizeNew()), "    add filter to be cleared");
        TEST_RESULT_VOID(ioFilterGroupClear(ioReadFilterGroup(bufferRead)), "    clear size filter");

        IoFilter *sizeFilter = ioSizeNew();
        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioTestFilterMultiplyNew("double", 2, 3, 'X')),
            "    add filter to filter group");
        TEST_RESULT_PTR(
            ioFilterGroupInsert(ioReadFilterGroup(bufferRead), 0, sizeFilter), bufferRead->filterGroup,
            "    add filter to filter group");
        TEST_RESULT_VOID(ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioSizeNew()), "    add filter to filter group");
        IoFilter *bufferFilter = ioBufferNew();
        TEST_RESULT_VOID(ioFilterGroupAdd(ioReadFilterGroup(bufferRead), bufferFilter), "    add filter to filter group");
        TEST_RESULT_PTR(ioFilterMove(NULL, memContextTop()), NULL, "    move NULL filter to top context");
        TEST_RESULT_STR(
            strPtr(jsonFromVar(ioFilterGroupParamAll(ioReadFilterGroup(bufferRead)), 0)),
            "[{\"size\":null},{\"double\":[\"double\",2,3]},{\"size\":null},{\"buffer\":null}]", "    check filter params");

        TEST_RESULT_BOOL(ioReadOpen(bufferRead), true, "    open");
        TEST_RESULT_INT(ioReadHandle(bufferRead), -1, "    handle invalid");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), false, "    not eof");
        TEST_RESULT_SIZE(ioRead(bufferRead, buffer), 2, "    read 2 bytes");
        TEST_RESULT_SIZE(ioRead(bufferRead, buffer), 0, "    read 0 bytes (full buffer)");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "11", "    check read");
        TEST_RESULT_STR(strPtr(ioFilterType(sizeFilter)), "size", "check filter type");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), false, "    not eof");

        TEST_RESULT_VOID(bufUsedZero(buffer), "    zero buffer");
        TEST_RESULT_SIZE(ioRead(bufferRead, buffer), 2, "    read 2 bytes");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "22", "    check read");

        TEST_ASSIGN(buffer, bufNew(3), "change output buffer size to 3");
        TEST_RESULT_SIZE(ioRead(bufferRead, buffer), 3, "    read 3 bytes");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "33X", "    check read");

        TEST_RESULT_VOID(bufUsedZero(buffer), "    zero buffer");
        TEST_RESULT_SIZE(ioRead(bufferRead, buffer), 2, "    read 2 bytes");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "XX", "    check read");
        TEST_RESULT_BOOL(ioReadEof(bufferRead), true, "    eof");
        TEST_RESULT_BOOL(ioBufferRead(ioReadDriver(bufferRead), buffer, true), 0, "    eof from driver");
        TEST_RESULT_SIZE(ioRead(bufferRead, buffer), 0, "    read 0 bytes");
        TEST_RESULT_VOID(ioReadClose(bufferRead), " close buffer read object");
        TEST_RESULT_STR(
            strPtr(jsonFromVar(ioFilterGroupResultAll(ioReadFilterGroup(bufferRead)), 0)),
            "{\"buffer\":null,\"double\":null,\"size\":[3,9]}",
            "    check filter result all");

        TEST_RESULT_PTR(ioReadFilterGroup(bufferRead), ioReadFilterGroup(bufferRead), "    check filter group");
        TEST_RESULT_UINT(
            varUInt64(varLstGet(varVarLst(ioFilterGroupResult(ioReadFilterGroup(bufferRead), ioFilterType(sizeFilter))), 0)), 3,
            "    check filter result");
        TEST_RESULT_PTR(
            ioFilterGroupResult(ioReadFilterGroup(bufferRead), strNew("double")), NULL, "    check filter result is NULL");
        TEST_RESULT_UINT(
            varUInt64(varLstGet(varVarLst(ioFilterGroupResult(ioReadFilterGroup(bufferRead), ioFilterType(sizeFilter))), 1)), 9,
            "    check filter result");

        TEST_RESULT_PTR(ioFilterDriver(bufferFilter), bufferFilter->driver, "    check filter driver");
        TEST_RESULT_PTR(ioFilterInterface(bufferFilter), &bufferFilter->interface, "    check filter interface");

        TEST_RESULT_VOID(ioFilterFree(bufferFilter), "    free buffer filter");
        TEST_RESULT_VOID(ioFilterGroupFree(ioReadFilterGroup(bufferRead)), "    free filter group object");

        // Set filter group results
        // -------------------------------------------------------------------------------------------------------------------------
        IoFilterGroup *filterGroup = ioFilterGroupNew();
        filterGroup->opened = true;
        TEST_RESULT_VOID(ioFilterGroupResultAllSet(filterGroup, NULL), "null result");
        TEST_RESULT_VOID(ioFilterGroupResultAllSet(filterGroup, jsonToVar(strNew("{\"test\":777}"))), "add result");
        filterGroup->closed = true;
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(filterGroup, strNew("test"))), 777, "    check filter result");

        // Read a zero-size buffer to ensure filters are still processed even when there is no input.  Some filters (e.g. encryption
        // and compression) will produce output even if there is no input.
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(1024);
        buffer = bufNew(1024);
        bufferOriginal = bufNew(0);

        TEST_ASSIGN(bufferRead, ioBufferReadNew(bufferOriginal), "create buffer read object");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioTestFilterMultiplyNew("double", 2, 5, 'Y')),
            "    add filter that produces output with no input");
        TEST_RESULT_BOOL(ioReadOpen(bufferRead), true, "    open read");
        TEST_RESULT_UINT(ioRead(bufferRead, buffer), 5, "    read 5 chars");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "YYYYY", "    check buffer");

        // Mixed line and buffer read
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(5);
        read = ioBufferReadNew(BUFSTRDEF("AAA123\n1234\n\n12\nBDDDEFF"));
        ioReadOpen(read);
        buffer = bufNew(3);

        // Start with a buffer read
        TEST_RESULT_INT(ioRead(read, buffer), 3, "read buffer");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "AAA", "    check buffer");

        // Do line reads of various lengths
        TEST_RESULT_STR(strPtr(ioReadLine(read)), "123", "read line");
        TEST_RESULT_STR(strPtr(ioReadLine(read)), "1234", "read line");
        TEST_RESULT_STR(strPtr(ioReadLine(read)), "", "read line");
        TEST_RESULT_STR(strPtr(ioReadLine(read)), "12", "read line");

        // Read what was left in the line buffer
        TEST_RESULT_INT(ioRead(read, buffer), 0, "read buffer");
        bufUsedSet(buffer, 2);
        TEST_RESULT_INT(ioRead(read, buffer), 1, "read buffer");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "AAB", "    check buffer");
        bufUsedSet(buffer, 0);

        // Now do a full buffer read from the input
        TEST_RESULT_INT(ioRead(read, buffer), 3, "read buffer");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "DDD", "    check buffer");

        // Read line doesn't work without a linefeed
        TEST_ERROR(ioReadLine(read), FileReadError, "unexpected eof while reading line");

        // But those bytes can be picked up by a buffer read
        buffer = bufNew(2);
        TEST_RESULT_INT(ioRead(read, buffer), 2, "read buffer");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "EF", "    check buffer");

        buffer = bufNew(1);
        TEST_RESULT_INT(ioRead(read, buffer), 1, "read buffer");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "F", "    check buffer");

        // Nothing left to read
        TEST_ERROR(ioReadLine(read), FileReadError, "unexpected eof while reading line");
        TEST_RESULT_INT(ioRead(read, buffer), 0, "read buffer");

        // Error if buffer is full and there is no linefeed
        ioBufferSizeSet(10);
        read = ioBufferReadNew(BUFSTRDEF("0123456789"));
        ioReadOpen(read);
        TEST_ERROR(ioReadLine(read), FileReadError, "unable to find line in 10 byte buffer");

        // Read IO into a buffer
        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(8);

        bufferRead = ioBufferReadNew(BUFSTRDEF("a test string"));
        ioReadOpen(bufferRead);

        TEST_RESULT_STR(strPtr(strNewBuf(ioReadBuf(bufferRead))), "a test string", "read into buffer");

        // Drain read IO
        // -------------------------------------------------------------------------------------------------------------------------
        bufferRead = ioBufferReadNew(BUFSTRDEF("a better test string"));
        ioFilterGroupAdd(ioReadFilterGroup(bufferRead), ioSizeNew());

        TEST_RESULT_BOOL(ioReadDrain(bufferRead), true, "drain read io");
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(ioReadFilterGroup(bufferRead), SIZE_FILTER_TYPE_STR)), 20, "check length");

        // Cannot open file
        TEST_ASSIGN(
            read, ioReadNewP((void *)998, .close = testIoReadClose, .open = testIoReadOpen, .read = testIoRead),
            "create io read object");
        TEST_RESULT_BOOL(ioReadDrain(read), false, "cannot open");
    }

    // *****************************************************************************************************************************
    if (testBegin("IoWrite, IoBufferWrite, IoBuffer, IoSize, IoFilter, and IoFilterGroup"))
    {
        IoWrite *write = NULL;
        ioBufferSizeSet(3);

        TEST_ASSIGN(
            write, ioWriteNewP((void *)999, .close = testIoWriteClose, .open = testIoWriteOpen, .write = testIoWrite),
            "create io write object");

        TEST_RESULT_VOID(ioWriteOpen(write), "    open io object");
        TEST_RESULT_BOOL(testIoWriteOpenCalled, true, "    check io object open");
        TEST_RESULT_VOID(ioWriteStr(write, STRDEF("ABC")), "    write 3 bytes");
        TEST_RESULT_VOID(ioWriteClose(write), "    close io object");
        TEST_RESULT_BOOL(testIoWriteCloseCalled, true, "    check io object closed");

        TEST_RESULT_VOID(ioWriteFree(write), "    free write object");

        // -------------------------------------------------------------------------------------------------------------------------
        ioBufferSizeSet(3);
        IoWrite *bufferWrite = NULL;
        Buffer *buffer = bufNew(0);

        TEST_ASSIGN(bufferWrite, ioBufferWriteNew(buffer), "create buffer write object");
        IoFilterGroup *filterGroup = ioWriteFilterGroup(bufferWrite);
        IoFilter *sizeFilter = ioSizeNew();
        TEST_RESULT_VOID(ioFilterGroupAdd(filterGroup, sizeFilter), "    add filter to filter group");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(filterGroup, ioTestFilterMultiplyNew("double", 2, 3, 'X')), "    add filter to filter group");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(filterGroup, ioTestFilterMultiplyNew("single", 1, 1, 'Y')),
            "    add filter to filter group");
        TEST_RESULT_VOID(ioFilterGroupAdd(filterGroup, ioTestFilterSizeNew("size2")), "    add filter to filter group");

        TEST_RESULT_VOID(ioWriteOpen(bufferWrite), "    open buffer write object");
        TEST_RESULT_INT(ioWriteHandle(bufferWrite), -1, "    handle invalid");
        TEST_RESULT_VOID(ioWriteLine(bufferWrite, BUFSTRDEF("AB")), "    write line");
        TEST_RESULT_VOID(ioWrite(bufferWrite, bufNew(0)), "    write 0 bytes");
        TEST_RESULT_VOID(ioWrite(bufferWrite, NULL), "    write 0 bytes");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "AABB\n\n", "    check write");

        TEST_RESULT_VOID(ioWriteStr(bufferWrite, STRDEF("Z")), "    write string");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "AABB\n\n", "    no change because output buffer is not full");
        TEST_RESULT_VOID(ioWriteStr(bufferWrite, STRDEF("12345")), "    write bytes");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "AABB\n\nZZ1122334455", "    check write");

        TEST_RESULT_VOID(ioWriteClose(bufferWrite), " close buffer write object");
        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "AABB\n\nZZ1122334455XXXY", "    check write after close");

        TEST_RESULT_PTR(ioWriteFilterGroup(bufferWrite), filterGroup, "    check filter group");
        TEST_RESULT_UINT(
            varUInt64(ioFilterGroupResult(filterGroup, ioFilterType(sizeFilter))), 9, "    check filter result");
        TEST_RESULT_UINT(varUInt64(ioFilterGroupResult(filterGroup, strNew("size2"))), 22, "    check filter result");
    }

    // *****************************************************************************************************************************
    if (testBegin("IoHandleRead, IoHandleWrite, and ioHandleWriteOneStr()"))
    {
        ioBufferSizeSet(16);

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoWrite *write = NULL;

                TEST_ASSIGN(write, ioHandleWriteNew(strNew("write test"), HARNESS_FORK_CHILD_WRITE()), "move write");
                ioWriteOpen(write);
                TEST_RESULT_INT(
                    ioWriteHandle(write), ((IoHandleWrite *)ioWriteDriver(write))->handle, "check write handle");
                TEST_RESULT_PTR(ioWriteDriver(write), write->driver, "check write driver");
                TEST_RESULT_PTR(ioWriteInterface(write), &write->interface, "check write interface");

                // Write a line to be read
                TEST_RESULT_VOID(ioWriteStrLine(write, strNew("test string 1")), "write test string");
                ioWriteFlush(write);
                ioWriteFlush(write);

                // Sleep so the other side will timeout
                const Buffer *buffer = BUFSTRDEF("12345678");
                TEST_RESULT_VOID(ioWrite(write, buffer), "write buffer");
                sleepMSec(1250);

                // Write a buffer in two parts and sleep in the middle so it will be read on the other side in two parts
                TEST_RESULT_VOID(ioWrite(write, buffer), "write buffer");
                sleepMSec(500);
                TEST_RESULT_VOID(ioWrite(write, buffer), "write buffer");
                ioWriteFlush(write);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("read test"), HARNESS_FORK_PARENT_READ_PROCESS(0), 1000);

                ioReadOpen(read);
                TEST_RESULT_INT(ioReadHandle(read), ((IoHandleRead *)ioReadDriver(read))->handle, "check handle");
                TEST_RESULT_PTR(ioReadInterface(read), &read->interface, "check interface");
                TEST_RESULT_PTR(ioReadDriver(read), read->driver, "check driver");

                // Read a string
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "test string 1", "read test string");

                // Only part of the buffer is written before timeout
                Buffer *buffer = bufNew(16);

                TEST_ERROR(ioRead(read, buffer), FileReadError, "unable to read data from read test after 1000ms");
                TEST_RESULT_UINT(bufSize(buffer), 16, "buffer is only partially read");

                // Read a buffer that is transmitted in two parts with blocking on the read side
                buffer = bufNew(16);
                bufLimitSet(buffer, 12);

                TEST_RESULT_UINT(ioRead(read, buffer), 12, "read buffer");
                bufLimitClear(buffer);
                TEST_RESULT_UINT(ioRead(read, buffer), 4, "read buffer");
                TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "1234567812345678", "check buffer");

                // Check EOF
                buffer = bufNew(16);

                TEST_RESULT_UINT(ioHandleRead(ioReadDriver(read), buffer, true), 0, "read buffer at eof");
                TEST_RESULT_UINT(ioHandleRead(ioReadDriver(read), buffer, true), 0, "read buffer at eof again");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            ioHandleWriteOneStr(999999, strNew("test")), FileWriteError,
            "unable to write to handle: [9] Bad file descriptor");

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/test.txt", testPath());
        int fileHandle = open(strPtr(fileName), O_CREAT | O_TRUNC | O_WRONLY, 0700);

        TEST_RESULT_VOID(ioHandleWriteOneStr(fileHandle, strNew("test1\ntest2")), "write string to file");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
