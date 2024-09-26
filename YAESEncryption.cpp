#include "YAESEncryption.h"
#include <iostream>
#include <sstream>
#include <iomanip>

/*
 * Static Functions
 * */
string YAESEncryption::Crypt(YAESEncryption::Aes level, YAESEncryption::Mode mode, const string &rawText,
                             const string &key, const string &iv, YAESEncryption::Padding padding)
{
    return YAESEncryption(level, mode, padding).encode(rawText, key, iv);
}

string YAESEncryption::Decrypt(YAESEncryption::Aes level, YAESEncryption::Mode mode, const string &rawText,
                               const string &key, const string &iv, YAESEncryption::Padding padding)
{
    return YAESEncryption(level, mode, padding).decode(rawText, key, iv);
}

string YAESEncryption::ExpandKey(YAESEncryption::Aes level, YAESEncryption::Mode mode, const string &key)
{
    return YAESEncryption(level, mode).expandKey(key);
}

string YAESEncryption::RemovePadding(const string &rawText, YAESEncryption::Padding padding)
{
    if (0 == rawText.size())
        return rawText;

    string ret = rawText;
    switch (padding)
    {
    case Padding::ZERO:
        // Works only if the last byte of the decoded array is not zero
        while (ret.at(ret.length() - 1) == 0x00)
            ret.erase(ret.length() - 1, 1);
        break;
    case Padding::PKCS7:
        ret.erase(ret.length() - ret.back(), ret.back());
        break;
    case Padding::ISO:
    {
        // Find the last byte which is not zero
        int marker_index = ret.length() - 1;
        for (; marker_index >= 0; --marker_index)
        {
            if (ret.at(marker_index) != 0x00)
            {
                break;
            }
        }

        // And check if it's the byte for marking padding
        if (ret.at(marker_index) == '\x80')
        {
            ret.erase(marker_index, ret.length() - 1);
        }
        break;
    }
    default:
        // do nothing
        break;
    }
    return ret;
}
/*
 * End Static function declarations
 * */

/*
 * Local Functions
 * */

namespace
{

quint8 xTime(quint8 x)
{
    return ((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

quint8 multiply(quint8 x, quint8 y)
{
    return (((y & 1) * x) ^ ((y >> 1 & 1) * xTime(x)) ^ ((y >> 2 & 1) * xTime(xTime(x))) ^ ((y >> 3 & 1) * xTime(xTime(xTime(x)))) ^ ((y >> 4 & 1) * xTime(xTime(xTime(xTime(x))))));
}

std::string toHex(const std::string &s, bool upper_case = false)
{
    std::ostringstream result;

    for (std::size_t i = 0; i < s.length(); ++i)
    {
        result << std::hex << std::setfill('0') << std::setw(2)
               << (upper_case ? std::uppercase : std::nouppercase) << (int)s[i];
    }

    return result.str();
}

std::string right(std::string const &source, std::size_t const length)
{
    if (length >= source.size())
    {
        return source;
    }
    return source.substr(source.size() - length);
}

std::string left(std::string const &source, std::size_t const length)
{
    if (length >= source.size())
    {
        return source;
    }
    return source.substr(0, length);
}
}

YAESEncryption::YAESEncryption(YAESEncryption::Aes level, YAESEncryption::Mode mode,
                               YAESEncryption::Padding padding)
    : m_nb(4),
      m_blocklen(16),
      m_level(level),
      m_mode(mode),
      m_padding(padding),
      m_aesNIAvailable(false),
      m_state(nullptr)
{
    switch (level)
    {
    case AES_128:
    {
        AES128 aes;
        m_nk = aes.nk;
        m_keyLen = aes.keylen;
        m_nr = aes.nr;
        m_expandedKey = aes.expandedKey;
    }
        break;
    case AES_192:
    {
        AES192 aes;
        m_nk = aes.nk;
        m_keyLen = aes.keylen;
        m_nr = aes.nr;
        m_expandedKey = aes.expandedKey;
    }
        break;
    case AES_256:
    {
        AES256 aes;
        m_nk = aes.nk;
        m_keyLen = aes.keylen;
        m_nr = aes.nr;
        m_expandedKey = aes.expandedKey;
    }
        break;
    default:
    {
        AES128 aes;
        m_nk = aes.nk;
        m_keyLen = aes.keylen;
        m_nr = aes.nr;
        m_expandedKey = aes.expandedKey;
    }
        break;
    }
}

string YAESEncryption::getPadding(int currSize, int alignment)
{
    int size = (alignment - currSize % alignment) % alignment;
    switch (m_padding)
    {
    case Padding::ZERO:
        return string(size, 0x00);
        break;
    case Padding::PKCS7:
        if (size == 0)
            size = alignment;
        return string(size, size);
        break;
    case Padding::ISO:
        if (size > 0)
            return '\x80' + string(size - 1, 0x00);
        break;
    default:
        return string(size, 0x00);
        break;
    }
    return string();
}


string YAESEncryption::expandKey(const string &key)
{
    int i, k;
    quint8 tempa[4]; // Used for the column/row operations
    string roundKey(key); // The first round key is the key itself.

    // All other round keys are found from the previous round keys.
    //i == Nk
    for(i = m_nk; i < m_nb * (m_nr + 1); i++)
    {
        tempa[0] = (quint8) roundKey.at((i-1) * 4 + 0);
        tempa[1] = (quint8) roundKey.at((i-1) * 4 + 1);
        tempa[2] = (quint8) roundKey.at((i-1) * 4 + 2);
        tempa[3] = (quint8) roundKey.at((i-1) * 4 + 3);

        if (i % m_nk == 0)
        {
            // This function shifts the 4 bytes in a word to the left once.
            // [a0,a1,a2,a3] becomes [a1,a2,a3,a0]

            // Function RotWord()
            k = tempa[0];
            tempa[0] = tempa[1];
            tempa[1] = tempa[2];
            tempa[2] = tempa[3];
            tempa[3] = k;

            // Function Subword()
            tempa[0] = getSBoxValue(tempa[0]);
            tempa[1] = getSBoxValue(tempa[1]);
            tempa[2] = getSBoxValue(tempa[2]);
            tempa[3] = getSBoxValue(tempa[3]);

            tempa[0] =  tempa[0] ^ Rcon[i/m_nk];
        }

        if (m_level == AES_256 && i % m_nk == 4)
        {
            // Function Subword()
            tempa[0] = getSBoxValue(tempa[0]);
            tempa[1] = getSBoxValue(tempa[1]);
            tempa[2] = getSBoxValue(tempa[2]);
            tempa[3] = getSBoxValue(tempa[3]);
        }
        roundKey.insert(i * 4 + 0, 1, (quint8) roundKey.at((i - m_nk) * 4 + 0) ^ tempa[0]);
        roundKey.insert(i * 4 + 1, 1, (quint8) roundKey.at((i - m_nk) * 4 + 1) ^ tempa[1]);
        roundKey.insert(i * 4 + 2, 1, (quint8) roundKey.at((i - m_nk) * 4 + 2) ^ tempa[2]);
        roundKey.insert(i * 4 + 3, 1, (quint8) roundKey.at((i - m_nk) * 4 + 3) ^ tempa[3]);
    }
    return roundKey;
}



// This function adds the round key to state.
// The round key is added to the state by an XOR function.
void YAESEncryption::addRoundKey(const quint8 round, const string &expKey)
{
    string::iterator it = m_state->begin();
    for(std::size_t i=0; i < 16 && i < m_state->size(); ++i)
        it[i] = (quint8) it[i] ^ (quint8) expKey.at(round * m_nb * 4 + (i/4) * m_nb + (i%4));
}

// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
void YAESEncryption::subBytes()
{
    string::iterator it = m_state->begin();
    for(int i = 0; i < 16; i++)
        it[i] = getSBoxValue((quint8) it[i]);
}

// The ShiftRows() function shifts the rows in the state to the left.
// Each row is shifted with different offset.
// Offset = Row number. So the first row is not shifted.
void YAESEncryption::shiftRows()
{
    string::iterator it = m_state->begin();
    quint8 temp;
    //Keep in mind that QByteArray is column-driven!!

    //Shift 1 to left
    temp   = (quint8)it[1];
    it[1]  = (quint8)it[5];
    it[5]  = (quint8)it[9];
    it[9]  = (quint8)it[13];
    it[13] = (quint8)temp;

    //Shift 2 to left
    temp   = (quint8)it[2];
    it[2]  = (quint8)it[10];
    it[10] = (quint8)temp;
    temp   = (quint8)it[6];
    it[6]  = (quint8)it[14];
    it[14] = (quint8)temp;

    //Shift 3 to left
    temp   = (quint8)it[3];
    it[3]  = (quint8)it[15];
    it[15] = (quint8)it[11];
    it[11] = (quint8)it[7];
    it[7]  = (quint8)temp;
}

// MixColumns function mixes the columns of the state matrix
//optimized!!
void YAESEncryption::mixColumns()
{
    string::iterator it = m_state->begin();
    quint8 tmp, tm, t;

    for(int i = 0; i < 16; i += 4){
        t       = (quint8)it[i];
        tmp     =  (quint8)it[i] ^ (quint8)it[i+1] ^ (quint8)it[i+2] ^ (quint8)it[i+3] ;

        tm      = xTime( (quint8)it[i] ^ (quint8)it[i+1] );
        it[i]   = (quint8)it[i] ^ (quint8)tm ^ (quint8)tmp;

        tm      = xTime( (quint8)it[i+1] ^ (quint8)it[i+2]);
        it[i+1] = (quint8)it[i+1] ^ (quint8)tm ^ (quint8)tmp;

        tm      = xTime( (quint8)it[i+2] ^ (quint8)it[i+3]);
        it[i+2] =(quint8)it[i+2] ^ (quint8)tm ^ (quint8)tmp;

        tm      = xTime((quint8)it[i+3] ^ (quint8)t);
        it[i+3] =(quint8)it[i+3] ^ (quint8)tm ^ (quint8)tmp;
    }
}

// MixColumns function mixes the columns of the state matrix.
// The method used to multiply may be difficult to understand for the inexperienced.
// Please use the references to gain more information.
void YAESEncryption::invMixColumns()
{
    string::iterator it = m_state->begin();
    quint8 a,b,c,d;
    for(int i = 0; i < 16; i+=4){
        a = (quint8) it[i];
        b = (quint8) it[i+1];
        c = (quint8) it[i+2];
        d = (quint8) it[i+3];

        it[i]   = (quint8) (multiply(a, 0x0e) ^ multiply(b, 0x0b) ^ multiply(c, 0x0d) ^ multiply(d, 0x09));
        it[i+1] = (quint8) (multiply(a, 0x09) ^ multiply(b, 0x0e) ^ multiply(c, 0x0b) ^ multiply(d, 0x0d));
        it[i+2] = (quint8) (multiply(a, 0x0d) ^ multiply(b, 0x09) ^ multiply(c, 0x0e) ^ multiply(d, 0x0b));
        it[i+3] = (quint8) (multiply(a, 0x0b) ^ multiply(b, 0x0d) ^ multiply(c, 0x09) ^ multiply(d, 0x0e));
    }
}

// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
void YAESEncryption::invSubBytes()
{
    string::iterator it = m_state->begin();
    for(int i = 0; i < 16; ++i)
        it[i] = getSBoxInvert((quint8) it[i]);
}

void YAESEncryption::invShiftRows()
{
    string::iterator it = m_state->begin();
    uint8_t temp;

    //Keep in mind that QByteArray is column-driven!!

    //Shift 1 to right
    temp   = (quint8)it[13];
    it[13] = (quint8)it[9];
    it[9]  = (quint8)it[5];
    it[5]  = (quint8)it[1];
    it[1]  = (quint8)temp;

    //Shift 2
    temp   = (quint8)it[10];
    it[10] = (quint8)it[2];
    it[2]  = (quint8)temp;
    temp   = (quint8)it[14];
    it[14] = (quint8)it[6];
    it[6]  = (quint8)temp;

    //Shift 3
    temp   = (quint8)it[7];
    it[7]  = (quint8)it[11];
    it[11] = (quint8)it[15];
    it[15] = (quint8)it[3];
    it[3]  = (quint8)temp;
}

string YAESEncryption::byteXor(const string &a, const string &b)
{
    string::const_iterator it_a = a.begin();
    string::const_iterator it_b = b.begin();
    string ret;

    for(std::size_t i = 0; i < std::min(a.size(), b.size()); i++)
        ret.insert(i, 1, it_a[i] ^ it_b[i]);

    return ret;
}

// Cipher is the main function that encrypts the PlainText.
string YAESEncryption::cipher(const string &expKey, const string &in)
{

    //m_state is the input buffer...
    string output(in);
    m_state = &output;

    // Add the First round key to the state before starting the rounds.
    addRoundKey(0, expKey);

    // There will be Nr rounds.
    // The first Nr-1 rounds are identical.
    // These Nr-1 rounds are executed in the loop below.
    for(quint8 round = 1; round < m_nr; ++round){
        subBytes();
        shiftRows();
        mixColumns();
        addRoundKey(round, expKey);
    }

    // The last round is given below.
    // The MixColumns function is not here in the last round.
    subBytes();
    shiftRows();
    addRoundKey(m_nr, expKey);

    return output;
}

string YAESEncryption::invCipher(const string &expKey, const string &in)
{
    //m_state is the input buffer.... handle it!
    string output(in);
    m_state = &output;

    // Add the First round key to the state before starting the rounds.
    addRoundKey(m_nr, expKey);

    // There will be Nr rounds.
    // The first Nr-1 rounds are identical.
    // These Nr-1 rounds are executed in the loop below.
    for(quint8 round=m_nr-1; round>0 ; round--){
        invShiftRows();
        invSubBytes();
        addRoundKey(round, expKey);
        invMixColumns();
    }

    // The last round is given below.
    // The MixColumns function is not here in the last round.
    invShiftRows();
    invSubBytes();
    addRoundKey(0, expKey);

    return output;
}

string YAESEncryption::printArray(uchar* arr, int size)
{
    string print("");
    for(int i=0; i<size; i++)
        print.append(1, arr[i]);

    return toHex(print);
}

string YAESEncryption::encode(const string &rawText, const string &key, const string &iv)
{
    if (m_mode >= CBC && ((0 == iv.size()) || iv.size() != m_blocklen))
        return string();

    string expandedKey = expandKey(key);
    string alignedText(rawText);

    //Fill array with padding
    alignedText.append(getPadding(static_cast<int>(rawText.size()), m_blocklen));

    switch(m_mode)
    {
    case ECB: {
        string ret;
        for(std::size_t i=0; i < alignedText.size(); i+= m_blocklen)
            ret.append(cipher(expandedKey, alignedText.substr(i, m_blocklen)));
        return ret;
    }
        break;
    case CBC: {
        string ret;
        string ivTemp = iv;
        for(std::size_t i=0; i < alignedText.size(); i+= m_blocklen) {
            alignedText.replace(i, m_blocklen, byteXor(alignedText.substr(i, m_blocklen),ivTemp));
            ret.append(cipher(expandedKey, alignedText.substr(i, m_blocklen)));
            ivTemp = ret.substr(i, m_blocklen);
        }
        return ret;
    }
        break;
    case CFB: {
        string ret;
        ret.append(byteXor(left(alignedText, m_blocklen), cipher(expandedKey, iv)));
        for(std::size_t i=0; i < alignedText.size(); i+= m_blocklen) {
            if (i+m_blocklen < alignedText.size())
                ret.append(byteXor(alignedText.substr(i+m_blocklen, m_blocklen),
                                   cipher(expandedKey, ret.substr(i, m_blocklen))));
        }
        return ret;
    }
        break;
    case OFB: {
        string ret;
        string ofbTemp;
        ofbTemp.append(cipher(expandedKey, iv));
        for (std::size_t i=m_blocklen; i < alignedText.size(); i += m_blocklen){
            ofbTemp.append(cipher(expandedKey, right(ofbTemp, m_blocklen)));
        }
        ret.append(byteXor(alignedText, ofbTemp));
        return ret;
    }
        break;
    default: break;
    }
    return string();
}

string YAESEncryption::decode(const string &rawText, const string &key, const string &iv)
{
    if (m_mode >= CBC && ((0 == iv.size()) || iv.size() != m_blocklen))
        return string();

    string ret;
    string expandedKey = expandKey(key);

    switch(m_mode)
    {
    case ECB:
        for(std::size_t i=0; i < rawText.size(); i+= m_blocklen)
            ret.append(invCipher(expandedKey, rawText.substr(i, m_blocklen)));
        break;
    case CBC: {
        string ivTemp(iv);
        for(std::size_t i=0; i < rawText.size(); i+= m_blocklen){
            ret.append(invCipher(expandedKey, rawText.substr(i, m_blocklen)));
            ret.replace(i, m_blocklen, byteXor(ret.substr(i, m_blocklen),ivTemp));
            ivTemp = rawText.substr(i, m_blocklen);
        }
    }
        break;
    case CFB: {
        ret.append(byteXor(rawText.substr(0, m_blocklen), cipher(expandedKey, iv)));
        for(std::size_t i=0; i < rawText.size(); i+= m_blocklen){
            if (i+m_blocklen < rawText.size()) {
                ret.append(byteXor(rawText.substr(i+m_blocklen, m_blocklen),
                                   cipher(expandedKey, rawText.substr(i, m_blocklen))));
            }
        }
    }
        break;
    case OFB: {
        string ofbTemp;
        ofbTemp.append(cipher(expandedKey, iv));
        for (std::size_t i=m_blocklen; i < rawText.size(); i += m_blocklen){
            ofbTemp.append(cipher(expandedKey, right(ofbTemp, m_blocklen)));
        }
        ret.append(byteXor(rawText, ofbTemp));
    }
        break;
    default:
        //do nothing
        break;
    }
    return ret;
}

string YAESEncryption::removePadding(const string &rawText)
{
    return RemovePadding(rawText, (Padding) m_padding);
}
