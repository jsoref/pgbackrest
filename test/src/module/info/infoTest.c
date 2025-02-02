/***********************************************************************************************************************************
Test Info Handler
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    String *cipherPass = strNew("123xyz");
    String *fileName = strNewFmt("%s/test.ini", testPath());
    Info *info = NULL;

    // *****************************************************************************************************************************
    if (testBegin("infoNew()"))
    {
        TEST_ASSIGN(info, infoNew(cipherTypeAes256Cbc, cipherPass), "infoNew(cipher)");
        TEST_RESULT_PTR(infoCipherPass(info), cipherPass, "    cipherPass is set");

        TEST_ASSIGN(info, infoNew(cipherTypeNone, NULL), "infoNew(NULL)");
        TEST_RESULT_PTR(infoCipherPass(info), NULL, "    cipherPass is NULL");

        TEST_ERROR(
            infoNew(cipherTypeNone, strNew("")), AssertError,
            "assertion '!((cipherType == cipherTypeNone && cipherPassSub != NULL) || (cipherType != cipherTypeNone && "
            "(cipherPassSub == NULL || strSize(cipherPassSub) == 0)))' failed");
        TEST_ERROR(
            infoNew(cipherTypeAes256Cbc, strNew("")), AssertError,
            "assertion '!((cipherType == cipherTypeNone && cipherPassSub != NULL) || (cipherType != cipherTypeNone && "
            "(cipherPassSub == NULL || strSize(cipherPassSub) == 0)))' failed");
        TEST_ERROR(
            infoNew(cipherTypeAes256Cbc, NULL), AssertError,
            "assertion '!((cipherType == cipherTypeNone && cipherPassSub != NULL) || (cipherType != cipherTypeNone && "
            "(cipherPassSub == NULL || strSize(cipherPassSub) == 0)))' failed");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoNewLoad(), infoFileName(), infoIni()"))
    {
        // Initialize test variables
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = NULL;
        String *fileNameCopy = strNewFmt("%s/test.ini.copy", testPath());

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"1efa53e0611604ad7d833c5547eb60ff716e758c\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
        );

        // Info files missing and at least one is required
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            infoNewLoad(storageLocal(), fileName, cipherTypeNone, NULL, NULL), FileMissingError,
            "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING,
            testPath(), testPath(), strPtr(strNewFmt("%s/test.ini", testPath())),
            strPtr(strNewFmt("%s/test.ini.copy", testPath())));

        // Only copy exists and one is required
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), BUFSTR(content)), "put info.copy to file");

        TEST_ASSIGN(info, infoNewLoad(storageLocal(), fileName, cipherTypeNone, NULL, NULL), "load copy file");

        TEST_RESULT_PTR(infoCipherPass(info), NULL, "    cipherPass is not set");

        // Remove the copy and store only the main info file and encrypt it. One is required.
        //--------------------------------------------------------------------------------------------------------------------------
        StorageWrite *infoWrite = storageNewWriteNP(storageLocalWrite(), fileName);

        ioFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(infoWrite)), cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc,
            BUFSTRDEF("12345678"), NULL));

        storageRemoveNP(storageLocalWrite(), fileNameCopy);
        storagePutNP(
            infoWrite,
            BUFSTRDEF(
                "[backrest]\n"
                "backrest-checksum=\"9d2f6dce339751e1a056187fad67d2834b3d4ab3\"\n"
                "backrest-format=5\n"
                "backrest-version=\"2.04\"\n"
                "\n"
                "[cipher]\n"
                "cipher-pass=\"ABCDEFGH\"\n"
                "\n"
                "[db]\n"
                "db-id=1\n"
                "db-system-id=6569239123849665679\n"
                "db-version=\"9.4\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"));

        // Only main info exists and is required
        Ini *ini = NULL;
        TEST_ASSIGN(info, infoNewLoad(storageLocal(), fileName, cipherTypeAes256Cbc, strNew("12345678"), &ini), "load file");

        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("cipher"), strNew("cipher-pass"))), "\"ABCDEFGH\"", "    check ini");
        TEST_RESULT_STR(strPtr(infoCipherPass(info)), "ABCDEFGH", "    cipherPass is set");

        // Invalid format
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storageLocalWrite(), fileName);

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"14617b089cb5c9b3224e739bb794e865b9bcdf4b\"\n"
            "backrest-format=4\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
            "\"db-version\":\"9.4\"}\n"
        );

        // Only main file exists but the backrest-format is invalid
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), BUFSTR(content)), "put invalid br format to file");

        TEST_ERROR_FMT(
            infoNewLoad(storageLocal(), fileName, cipherTypeNone, NULL, NULL), FormatError,
            "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
            "FormatError: invalid format in '%s/test.ini', expected 5 but found 4\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING,
            testPath(), testPath(), testPath(), strPtr(strNewFmt("%s/test.ini.copy", testPath())));

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"14617b089cb5c9b3224e739bb794e865b9bcdf4b\"\n"
            "backrest-format=4\n"
            "backrest-version=\"2.05\"\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
            "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), fileNameCopy), BUFSTR(content)), "put invalid info to copy file");

        TEST_ERROR(
            infoNewLoad(storageLocal(), fileName, cipherTypeNone, NULL, NULL), FileOpenError,
            strPtr(
                strNewFmt(
                    "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
                    "FormatError: invalid format in '%s/test.ini', expected 5 but found 4\n"
                    "ChecksumError: invalid checksum in '%s/test.ini.copy', expected 'af92308095d6141bcda6b2df6d574f98d1115163'"
                        " but found '14617b089cb5c9b3224e739bb794e865b9bcdf4b'",
                testPath(), testPath(), testPath(), testPath())));

        // Invalid checksum
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storageLocalWrite(), fileName);
        storageRemoveNP(storageLocalWrite(), fileNameCopy);

        // change the checksum
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"4306ec205f71417c301e403c4714090e61c8a999\"\n"
            "backrest-format=5\n"
            "backrest-version=\"1.23\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6455618988686438683\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6455618988686438683,\"db-version\":\"9.6\"}\n"
            "2={\"db-id\":6457457208275135411,\"db-version\":\"9.6\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), BUFSTR(content)), "put invalid checksum to copy");

        // Empty checksum for main file
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\n"
            "backrest-format=5\n"
            "backrest-version=\"1.23\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6455618988686438683\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6455618988686438683,\"db-version\":\"9.6\"}\n"
            "2={\"db-id\":6457457208275135411,\"db-version\":\"9.6\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), BUFSTR(content)), "put empty checksum to file");

        // Copy file error
        TEST_ERROR(
            infoNewLoad(storageLocal(), fileName, cipherTypeNone, NULL, NULL), ChecksumError,
            strPtr(
                strNewFmt(
                    "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
                    "ChecksumError: invalid checksum in '%s/test.ini', expected '4306ec205f71417c301e403c4714090e61c8a736' but"
                        " no checksum found\n"
                    "ChecksumError: invalid checksum in '%s/test.ini.copy', expected '4306ec205f71417c301e403c4714090e61c8a736'"
                        " but found '4306ec205f71417c301e403c4714090e61c8a999'",
                testPath(), testPath(), testPath(), testPath())));

        // Encryption error
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storageLocalWrite(), fileName);
        TEST_ERROR_FMT(
            infoNewLoad(storageLocal(), fileName, cipherTypeAes256Cbc, strNew("12345678"), NULL), CryptoError,
            "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
                "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
                "CryptoError: '%s/test.ini.copy' cipher header invalid\n"
                "HINT: Is or was the repo encrypted?",
            testPath(), testPath(), strPtr(strNewFmt("%s/test.ini", testPath())), testPath());

        storageRemoveNP(storageLocalWrite(), fileNameCopy);

        // infoFree()
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoFree(info), "infoFree() - free info memory context");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoSave()"))
    {
        Ini *ini = iniNew();
        iniSet(ini, strNew("section1"), strNew("key1"), strNew("value1"));
        Info *info = infoNew(cipherTypeNone, NULL);
        TEST_RESULT_VOID(infoSave(info, ini, storageTest, fileName, cipherTypeNone, NULL), "save info");

        ini = NULL;
        TEST_RESULT_VOID(infoNewLoad(storageTest, fileName, cipherTypeNone, NULL, &ini), "    reload info");
        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("section1"), strNew("key1"))), "value1", "    check ini");

        TEST_RESULT_BOOL(storageExistsNP(storageTest, fileName), true, "check main exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(fileName))), true, "check copy exists");

        TEST_ERROR(
            infoSave(info, ini, storageTest, fileName, cipherTypeAes256Cbc, cipherPass), AssertError,
            "assertion '!((cipherType != cipherTypeNone && this->cipherPass == NULL) || "
            "(cipherType == cipherTypeNone && this->cipherPass != NULL))' failed");

        // Add encryption
        // -------------------------------------------------------------------------------------------------------------------------
        ini = iniNew();
        iniSet(ini, strNew("section1"), strNew("key1"), strNew("value4"));
        info = infoNew(cipherTypeAes256Cbc, strNew("/hall-pass"));
        TEST_RESULT_VOID(infoSave(info, ini, storageTest, fileName, cipherTypeAes256Cbc, cipherPass), "save encrypted info");

        ini = NULL;
        TEST_RESULT_VOID(infoNewLoad(storageTest, fileName, cipherTypeAes256Cbc, cipherPass, &ini), "    reload info");
        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("section1"), strNew("key1"))), "value4", "    check ini");
        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("cipher"), strNew("cipher-pass"))), "\"/hall-pass\"", "    check cipher-pass");

        TEST_ERROR(
            infoSave(info, ini, storageTest, fileName, cipherTypeNone, NULL), AssertError,
            "assertion '!((cipherType != cipherTypeNone && this->cipherPass == NULL) || "
            "(cipherType == cipherTypeNone && this->cipherPass != NULL))' failed");
    }
}
