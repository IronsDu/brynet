/*
  100% free public domain implementation of the SHA-1 algorithm
  by Dominik Reichl <dominik.reichl@t-online.de>
  Web: http://www.dominik-reichl.de/

  Version 2.1 - 2012-06-19
  - Deconstructor (resetting internal variables) is now only
    implemented if SHA1_WIPE_VARIABLES is defined (which is the
    default).
  - Renamed inclusion guard to contain a GUID.
  - Demo application is now using C++/STL objects and functions.
  - Unicode build of the demo application now outputs the hashes of both
    the ANSI and Unicode representations of strings.
  - Various other demo application improvements.

  Version 2.0 - 2012-06-14
  - Added 'limits.h' include.
  - Renamed inclusion guard and macros for compliancy (names beginning
    with an underscore are reserved).

  Version 1.9 - 2011-11-10
  - Added Unicode test vectors.
  - Improved support for hashing files using the HashFile method that
    are larger than 4 GB.
  - Improved file hashing performance (by using a larger buffer).
  - Disabled unnecessary compiler warnings.
  - Internal variables are now private.

  Version 1.8 - 2009-03-16
  - Converted project files to Visual Studio 2008 format.
  - Added Unicode support for HashFile utility method.
  - Added support for hashing files using the HashFile method that are
    larger than 2 GB.
  - HashFile now returns an error code instead of copying an error
    message into the output buffer.
  - GetHash now returns an error code and validates the input parameter.
  - Added ReportHashStl STL utility method.
  - Added REPORT_HEX_SHORT reporting mode.
  - Improved Linux compatibility of test program.

  Version 1.7 - 2006-12-21
  - Fixed buffer underrun warning that appeared when compiling with
    Borland C Builder (thanks to Rex Bloom and Tim Gallagher for the
    patch).
  - Breaking change: ReportHash writes the final hash to the start
    of the buffer, i.e. it's not appending it to the string anymore.
  - Made some function parameters const.
  - Added Visual Studio 2005 project files to demo project.

  Version 1.6 - 2005-02-07 (thanks to Howard Kapustein for patches)
  - You can set the endianness in your files, no need to modify the
    header file of the CSHA1 class anymore.
  - Aligned data support.
  - Made support/compilation of the utility functions (ReportHash and
    HashFile) optional (useful when bytes count, for example in embedded
    environments).

  Version 1.5 - 2005-01-01
  - 64-bit compiler compatibility added.
  - Made variable wiping optional (define SHA1_WIPE_VARIABLES).
  - Removed unnecessary variable initializations.
  - ROL32 improvement for the Microsoft compiler (using _rotl).

  Version 1.4 - 2004-07-22
  - CSHA1 now compiles fine with GCC 3.3 under Mac OS X (thanks to Larry
    Hastings).

  Version 1.3 - 2003-08-17
  - Fixed a small memory bug and made a buffer array a class member to
    ensure correct working when using multiple CSHA1 class instances at
    one time.

  Version 1.2 - 2002-11-16
  - Borlands C++ compiler seems to have problems with string addition
    using sprintf. Fixed the bug which caused the digest report function
    not to work properly. CSHA1 is now Borland compatible.

  Version 1.1 - 2002-10-11
  - Removed two unnecessary header file includes and changed BOOL to
    bool. Fixed some minor bugs in the web page contents.

  Version 1.0 - 2002-06-20
  - First official release.

  ================ Test Vectors ================

  SHA1("abc" in ANSI) =
    A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
  SHA1("abc" in Unicode LE) =
    9F04F41A 84851416 2050E3D6 8C1A7ABB 441DC2B5

  SHA1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    in ANSI) =
    84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
  SHA1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    in Unicode LE) =
    51D7D876 9AC72C40 9C5B0E3F 69C60ADC 9A039014

  SHA1(A million repetitions of "a" in ANSI) =
    34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
  SHA1(A million repetitions of "a" in Unicode LE) =
    C4609560 A108A0C6 26AA7F2B 38A65566 739353C5
*/

#ifndef SHA1_H_A545E61D43E9404E8D736869AB3CBFE7
#define SHA1_H_A545E61D43E9404E8D736869AB3CBFE7

#if !defined(SHA1_UTILITY_FUNCTIONS) && !defined(SHA1_NO_UTILITY_FUNCTIONS)
#define SHA1_UTILITY_FUNCTIONS
#endif

#if !defined(SHA1_STL_FUNCTIONS) && !defined(SHA1_NO_STL_FUNCTIONS)
#define SHA1_STL_FUNCTIONS
#if !defined(SHA1_UTILITY_FUNCTIONS)
#error STL functions require SHA1_UTILITY_FUNCTIONS.
#endif
#endif

#include <limits.h>
#include <memory.h>

#ifdef SHA1_UTILITY_FUNCTIONS
#include <stdio.h>
#include <string.h>
#endif

#ifdef SHA1_STL_FUNCTIONS
#include <string>
#endif

#ifdef _MSC_VER
#include <stdlib.h>
#endif

// You can define the endian mode in your files without modifying the SHA-1
// source files. Just #define SHA1_LITTLE_ENDIAN or #define SHA1_BIG_ENDIAN
// in your files, before including the SHA1.h header file. If you don't
// define anything, the class defaults to little endian.
#if !defined(SHA1_LITTLE_ENDIAN) && !defined(SHA1_BIG_ENDIAN)
#define SHA1_LITTLE_ENDIAN
#endif

// If you want variable wiping, #define SHA1_WIPE_VARIABLES, if not,
// #define SHA1_NO_WIPE_VARIABLES. If you don't define anything, it
// defaults to wiping.
#if !defined(SHA1_WIPE_VARIABLES) && !defined(SHA1_NO_WIPE_VARIABLES)
#define SHA1_WIPE_VARIABLES
#endif

#if defined(SHA1_HAS_TCHAR)
#include <tchar.h>
#else
#ifdef _MSC_VER
#include <tchar.h>
#else
#ifndef TCHAR
#define TCHAR char
#endif
#ifndef _T
#define _T(__x) (__x)
#define _tmain main
#define _tprintf printf
#define _getts gets
#define _tcslen strlen
#define _tfopen fopen
#define _tcscpy strcpy
#define _tcscat strcat
#define _sntprintf snprintf
#endif
#endif
#endif

///////////////////////////////////////////////////////////////////////////
// Define variable types

#ifndef UINT_8
#ifdef _MSC_VER// Compiling with Microsoft compiler
#define UINT_8 unsigned __int8
#else// !_MSC_VER
#define UINT_8 unsigned char
#endif// _MSC_VER
#endif

#ifndef UINT_32
#ifdef _MSC_VER// Compiling with Microsoft compiler
#define UINT_32 unsigned __int32
#else// !_MSC_VER
#if (ULONG_MAX == 0xFFFFFFFFUL)
#define UINT_32 unsigned long
#else
#define UINT_32 unsigned int
#endif
#endif// _MSC_VER
#endif// UINT_32

#ifndef INT_64
#ifdef _MSC_VER// Compiling with Microsoft compiler
#define INT_64 __int64
#else// !_MSC_VER
#define INT_64 long long
#endif// _MSC_VER
#endif// INT_64

#ifndef UINT_64
#ifdef _MSC_VER// Compiling with Microsoft compiler
#define UINT_64 unsigned __int64
#else// !_MSC_VER
#define UINT_64 unsigned long long
#endif// _MSC_VER
#endif// UINT_64

///////////////////////////////////////////////////////////////////////////
// Declare SHA-1 workspace

typedef union {
    UINT_8 c[64];
    UINT_32 l[16];
} SHA1_WORKSPACE_BLOCK;


#define SHA1_MAX_FILE_BUFFER (32 * 20 * 820)

// Rotate p_val32 by p_nBits bits to the left
#ifndef ROL32
#ifdef _MSC_VER
#define ROL32(p_val32, p_nBits) _rotl(p_val32, p_nBits)
#else
#define ROL32(p_val32, p_nBits) (((p_val32) << (p_nBits)) | ((p_val32) >> (32 - (p_nBits))))
#endif
#endif

#ifdef SHA1_LITTLE_ENDIAN
#define SHABLK0(i) (m_block->l[i] = \
                            (ROL32(m_block->l[i], 24) & 0xFF00FF00) | (ROL32(m_block->l[i], 8) & 0x00FF00FF))
#else
#define SHABLK0(i) (m_block->l[i])
#endif

#define SHABLK(i) (m_block->l[i & 15] = ROL32(m_block->l[(i + 13) & 15] ^                                                       \
                                                      m_block->l[(i + 8) & 15] ^ m_block->l[(i + 2) & 15] ^ m_block->l[i & 15], \
                                              1))

// SHA-1 rounds
#define S_R0(v, w, x, y, z, i)                                            \
    {                                                                     \
        z += ((w & (x ^ y)) ^ y) + SHABLK0(i) + 0x5A827999 + ROL32(v, 5); \
        w = ROL32(w, 30);                                                 \
    }
#define S_R1(v, w, x, y, z, i)                                           \
    {                                                                    \
        z += ((w & (x ^ y)) ^ y) + SHABLK(i) + 0x5A827999 + ROL32(v, 5); \
        w = ROL32(w, 30);                                                \
    }
#define S_R2(v, w, x, y, z, i)                                   \
    {                                                            \
        z += (w ^ x ^ y) + SHABLK(i) + 0x6ED9EBA1 + ROL32(v, 5); \
        w = ROL32(w, 30);                                        \
    }
#define S_R3(v, w, x, y, z, i)                                                 \
    {                                                                          \
        z += (((w | x) & y) | (w & x)) + SHABLK(i) + 0x8F1BBCDC + ROL32(v, 5); \
        w = ROL32(w, 30);                                                      \
    }
#define S_R4(v, w, x, y, z, i)                                   \
    {                                                            \
        z += (w ^ x ^ y) + SHABLK(i) + 0xCA62C1D6 + ROL32(v, 5); \
        w = ROL32(w, 30);                                        \
    }


class CSHA1
{
public:
#ifdef SHA1_UTILITY_FUNCTIONS
    // Different formats for ReportHash(Stl)
    enum REPORT_TYPE
    {
        REPORT_HEX = 0,
        REPORT_DIGIT = 1,
        REPORT_HEX_SHORT = 2
    };
#endif

    // Constructor and destructor
    CSHA1()
    {
        (void) m_reserved0;
        (void) m_reserved1;
        m_block = (SHA1_WORKSPACE_BLOCK*) m_workspace;

        Reset();
    }

#ifdef SHA1_WIPE_VARIABLES
    ~CSHA1()
    {
        Reset();
    }
#endif

    void Reset()
    {
        // SHA1 initialization constants
        m_state[0] = 0x67452301;
        m_state[1] = 0xEFCDAB89;
        m_state[2] = 0x98BADCFE;
        m_state[3] = 0x10325476;
        m_state[4] = 0xC3D2E1F0;

        m_count[0] = 0;
        m_count[1] = 0;
    }

    // Hash in binary data and strings
    void Update(const UINT_8* pbData, UINT_32 uLen)
    {
        UINT_32 j = ((m_count[0] >> 3) & 0x3F);

        if ((m_count[0] += (uLen << 3)) < (uLen << 3))
            ++m_count[1];// Overflow

        m_count[1] += (uLen >> 29);

        UINT_32 i;
        if ((j + uLen) > 63)
        {
            i = 64 - j;
            memcpy(&m_buffer[j], pbData, i);
            Transform(m_state, m_buffer);

            for (; (i + 63) < uLen; i += 64)
                Transform(m_state, &pbData[i]);

            j = 0;
        }
        else
            i = 0;

        if ((uLen - i) != 0)
            memcpy(&m_buffer[j], &pbData[i], uLen - i);
    }

#ifdef SHA1_UTILITY_FUNCTIONS
    // Hash in file contents
    bool HashFile(const TCHAR* tszFileName)
    {
        if (tszFileName == NULL)
            return false;

        FILE* fpIn = _tfopen(tszFileName, _T("rb"));
        if (fpIn == NULL)
            return false;

        UINT_8* pbData = new UINT_8[SHA1_MAX_FILE_BUFFER];
        if (pbData == NULL)
        {
            fclose(fpIn);
            return false;
        }

        bool bSuccess = true;
        while (true)
        {
            const size_t uRead = fread(pbData, 1, SHA1_MAX_FILE_BUFFER, fpIn);

            if (uRead > 0)
                Update(pbData, static_cast<UINT_32>(uRead));

            if (uRead < SHA1_MAX_FILE_BUFFER)
            {
                if (feof(fpIn) == 0)
                    bSuccess = false;
                break;
            }
        }

        fclose(fpIn);
        delete[] pbData;
        return bSuccess;
    }
#endif

    // Finalize hash; call it before using ReportHash(Stl)
    void Final()
    {
        UINT_32 i;

        UINT_8 pbFinalCount[8];
        for (i = 0; i < 8; ++i)
            pbFinalCount[i] = static_cast<UINT_8>((m_count[((i >= 4) ? 0 : 1)] >>
                                                   ((3 - (i & 3)) * 8)) &
                                                  0xFF);// Endian independent

        Update((UINT_8*) "\200", 1);

        while ((m_count[0] & 504) != 448)
            Update((UINT_8*) "\0", 1);

        Update(pbFinalCount, 8);// Cause a Transform()

        for (i = 0; i < 20; ++i)
            m_digest[i] = static_cast<UINT_8>((m_state[i >> 2] >> ((3 -
                                                                    (i & 3)) *
                                                                   8)) &
                                              0xFF);

            // Wipe variables for security reasons
#ifdef SHA1_WIPE_VARIABLES
        memset(m_buffer, 0, 64);
        memset(m_state, 0, 20);
        memset(m_count, 0, 8);
        memset(pbFinalCount, 0, 8);
        Transform(m_state, m_buffer);
#endif
    }

#ifdef SHA1_UTILITY_FUNCTIONS
    bool ReportHash(TCHAR* tszReport, REPORT_TYPE rtReportType = REPORT_HEX) const
    {
        if (tszReport == NULL)
            return false;

        TCHAR tszTemp[16];

        if ((rtReportType == REPORT_HEX) || (rtReportType == REPORT_HEX_SHORT))
        {
            _sntprintf(tszTemp, 15, _T("%02X"), m_digest[0]);
            _tcscpy(tszReport, tszTemp);

            const TCHAR* lpFmt = ((rtReportType == REPORT_HEX) ? _T(" %02X") : _T("%02X"));
            for (size_t i = 1; i < 20; ++i)
            {
                _sntprintf(tszTemp, 15, lpFmt, m_digest[i]);
                _tcscat(tszReport, tszTemp);
            }
        }
        else if (rtReportType == REPORT_DIGIT)
        {
            _sntprintf(tszTemp, 15, _T("%u"), m_digest[0]);
            _tcscpy(tszReport, tszTemp);

            for (size_t i = 1; i < 20; ++i)
            {
                _sntprintf(tszTemp, 15, _T(" %u"), m_digest[i]);
                _tcscat(tszReport, tszTemp);
            }
        }
        else
            return false;

        return true;
    }
#endif

#ifdef SHA1_STL_FUNCTIONS
    bool ReportHashStl(std::basic_string<TCHAR>& strOut, REPORT_TYPE rtReportType =
                                                                 REPORT_HEX) const
    {
        TCHAR tszOut[84];
        const bool bResult = ReportHash(tszOut, rtReportType);
        if (bResult)
            strOut = tszOut;
        return bResult;
    }
#endif

    // Get the raw message digest (20 bytes)
    bool GetHash(UINT_8* pbDest20) const
    {
        if (pbDest20 == NULL)
            return false;
        memcpy(pbDest20, m_digest, 20);
        return true;
    }

private:
    // Private SHA-1 transformation
    void Transform(UINT_32* pState, const UINT_8* pBuffer)
    {
        UINT_32 a = pState[0], b = pState[1], c = pState[2], d = pState[3], e = pState[4];

        memcpy(m_block, pBuffer, 64);

        // 4 rounds of 20 operations each, loop unrolled
        S_R0(a, b, c, d, e, 0);
        S_R0(e, a, b, c, d, 1);
        S_R0(d, e, a, b, c, 2);
        S_R0(c, d, e, a, b, 3);
        S_R0(b, c, d, e, a, 4);
        S_R0(a, b, c, d, e, 5);
        S_R0(e, a, b, c, d, 6);
        S_R0(d, e, a, b, c, 7);
        S_R0(c, d, e, a, b, 8);
        S_R0(b, c, d, e, a, 9);
        S_R0(a, b, c, d, e, 10);
        S_R0(e, a, b, c, d, 11);
        S_R0(d, e, a, b, c, 12);
        S_R0(c, d, e, a, b, 13);
        S_R0(b, c, d, e, a, 14);
        S_R0(a, b, c, d, e, 15);
        S_R1(e, a, b, c, d, 16);
        S_R1(d, e, a, b, c, 17);
        S_R1(c, d, e, a, b, 18);
        S_R1(b, c, d, e, a, 19);
        S_R2(a, b, c, d, e, 20);
        S_R2(e, a, b, c, d, 21);
        S_R2(d, e, a, b, c, 22);
        S_R2(c, d, e, a, b, 23);
        S_R2(b, c, d, e, a, 24);
        S_R2(a, b, c, d, e, 25);
        S_R2(e, a, b, c, d, 26);
        S_R2(d, e, a, b, c, 27);
        S_R2(c, d, e, a, b, 28);
        S_R2(b, c, d, e, a, 29);
        S_R2(a, b, c, d, e, 30);
        S_R2(e, a, b, c, d, 31);
        S_R2(d, e, a, b, c, 32);
        S_R2(c, d, e, a, b, 33);
        S_R2(b, c, d, e, a, 34);
        S_R2(a, b, c, d, e, 35);
        S_R2(e, a, b, c, d, 36);
        S_R2(d, e, a, b, c, 37);
        S_R2(c, d, e, a, b, 38);
        S_R2(b, c, d, e, a, 39);
        S_R3(a, b, c, d, e, 40);
        S_R3(e, a, b, c, d, 41);
        S_R3(d, e, a, b, c, 42);
        S_R3(c, d, e, a, b, 43);
        S_R3(b, c, d, e, a, 44);
        S_R3(a, b, c, d, e, 45);
        S_R3(e, a, b, c, d, 46);
        S_R3(d, e, a, b, c, 47);
        S_R3(c, d, e, a, b, 48);
        S_R3(b, c, d, e, a, 49);
        S_R3(a, b, c, d, e, 50);
        S_R3(e, a, b, c, d, 51);
        S_R3(d, e, a, b, c, 52);
        S_R3(c, d, e, a, b, 53);
        S_R3(b, c, d, e, a, 54);
        S_R3(a, b, c, d, e, 55);
        S_R3(e, a, b, c, d, 56);
        S_R3(d, e, a, b, c, 57);
        S_R3(c, d, e, a, b, 58);
        S_R3(b, c, d, e, a, 59);
        S_R4(a, b, c, d, e, 60);
        S_R4(e, a, b, c, d, 61);
        S_R4(d, e, a, b, c, 62);
        S_R4(c, d, e, a, b, 63);
        S_R4(b, c, d, e, a, 64);
        S_R4(a, b, c, d, e, 65);
        S_R4(e, a, b, c, d, 66);
        S_R4(d, e, a, b, c, 67);
        S_R4(c, d, e, a, b, 68);
        S_R4(b, c, d, e, a, 69);
        S_R4(a, b, c, d, e, 70);
        S_R4(e, a, b, c, d, 71);
        S_R4(d, e, a, b, c, 72);
        S_R4(c, d, e, a, b, 73);
        S_R4(b, c, d, e, a, 74);
        S_R4(a, b, c, d, e, 75);
        S_R4(e, a, b, c, d, 76);
        S_R4(d, e, a, b, c, 77);
        S_R4(c, d, e, a, b, 78);
        S_R4(b, c, d, e, a, 79);

        // Add the working vars back into state
        pState[0] += a;
        pState[1] += b;
        pState[2] += c;
        pState[3] += d;
        pState[4] += e;

        // Wipe variables
#ifdef SHA1_WIPE_VARIABLES
        a = b = c = d = e = 0;
#endif
    }

    // Member variables
    UINT_32 m_state[5];
    UINT_32 m_count[2];
    UINT_32 m_reserved0[1];// Memory alignment padding
    UINT_8 m_buffer[64];
    UINT_8 m_digest[20];
    UINT_32 m_reserved1[3];// Memory alignment padding

    UINT_8 m_workspace[64];
    SHA1_WORKSPACE_BLOCK* m_block;// SHA1 pointer to the byte array above
};

#endif// SHA1_H_A545E61D43E9404E8D736869AB3CBFE7
