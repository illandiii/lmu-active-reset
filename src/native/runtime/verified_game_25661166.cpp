#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <cstring>
#include "verified_game_25661166.h"
int verified_game_25661166(void) {
#ifdef LMU_PHASE_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0x66, 0x94, 0x23, 0x37, 0x81, 0x3b, 0x30, 0x1b,
                                               0x87, 0xd1, 0x2e, 0xdf, 0xa3, 0x30, 0xf3, 0x33,
                                               0x54, 0xa2, 0xa4, 0xb8, 0xcd, 0x9b, 0x04, 0x88,
                                               0x85, 0xcb, 0xf7, 0x7e, 0xb6, 0xf9, 0x40, 0xcf};
    wchar_t path[32768];
    if (!GetModuleFileNameW(NULL, path, 32768))
        return 0;
    HANDLE file =
        CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    int ok = 0;
    if (CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        unsigned char buffer[65536], digest[32];
        DWORD got = 0, size = 32;
        BOOL read_ok;
        do {
            read_ok = ReadFile(file, buffer, sizeof(buffer), &got, NULL);
            if (!read_ok || (!CryptHashData(hash, buffer, got, 0)))
                break;
        } while (got);
        if (read_ok && got == 0 && CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0) &&
            size == 32)
            ok = memcmp(digest, expected, 32) == 0;
    }
    if (hash)
        CryptDestroyHash(hash);
    if (provider)
        CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
#endif
}
