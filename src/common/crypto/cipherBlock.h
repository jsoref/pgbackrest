/***********************************************************************************************************************************
Block Cipher Header
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_CIPHERBLOCK_H
#define COMMON_CRYPTO_CIPHERBLOCK_H

#include "common/io/filter/filter.h"
#include "common/crypto/common.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CIPHER_BLOCK_FILTER_TYPE                                   "cipherBlock"
    STRING_DECLARE(CIPHER_BLOCK_FILTER_TYPE_STR);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
IoFilter *cipherBlockNew(CipherMode mode, CipherType cipherType, const Buffer *pass, const String *digestName);
IoFilter *cipherBlockNewVar(const VariantList *paramList);

#endif
