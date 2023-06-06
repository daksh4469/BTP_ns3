#include "ns3/animation-interface.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wave-module.h"
#include <random>

// SHA256 from: http://www.zedwood.com/article/cpp-sha256-function
#define SHA2_SHFR(x, n) (x >> n)
#define SHA2_ROTR(x, n) ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define SHA2_ROTL(x, n) ((x << n) | (x >> ((sizeof(x) << 3) - n)))
#define SHA2_CH(x, y, z) ((x & y) ^ (~x & z))
#define SHA2_MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define SHA256_F1(x) (SHA2_ROTR(x, 2) ^ SHA2_ROTR(x, 13) ^ SHA2_ROTR(x, 22))
#define SHA256_F2(x) (SHA2_ROTR(x, 6) ^ SHA2_ROTR(x, 11) ^ SHA2_ROTR(x, 25))
#define SHA256_F3(x) (SHA2_ROTR(x, 7) ^ SHA2_ROTR(x, 18) ^ SHA2_SHFR(x, 3))
#define SHA256_F4(x) (SHA2_ROTR(x, 17) ^ SHA2_ROTR(x, 19) ^ SHA2_SHFR(x, 10))
#define SHA2_UNPACK32(x, str)                                                                      \
    {                                                                                              \
        *((str) + 3) = (uint8)((x));                                                               \
        *((str) + 2) = (uint8)((x) >> 8);                                                          \
        *((str) + 1) = (uint8)((x) >> 16);                                                         \
        *((str) + 0) = (uint8)((x) >> 24);                                                         \
    }
#define SHA2_PACK32(str, x)                                                                        \
    {                                                                                              \
        *(x) = ((uint32) * ((str) + 3)) | ((uint32) * ((str) + 2) << 8) |                          \
               ((uint32) * ((str) + 1) << 16) | ((uint32) * ((str) + 0) << 24);                    \
    }

using namespace ns3;

class SerialNumberTag : public Tag
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("SerialNumberTag")
                           .SetParent<Tag>()
                           .AddConstructor<SerialNumberTag>();
    return tid;
  }

  virtual TypeId GetInstanceTypeId() const
  {
    return GetTypeId();
  }

  virtual uint32_t GetSerializedSize() const
  {
    return sizeof(uint32_t);
  }

  virtual void Serialize(TagBuffer buffer) const
  {
    buffer.WriteU32(m_serialNumber);
  }

  virtual void Deserialize(TagBuffer buffer)
  {
    m_serialNumber = buffer.ReadU32();
  }

  virtual void Print (std::ostream &os) const{
     os << "SerialNumber : " << m_serialNumber;
  }

  void SetSerialNumber(uint32_t serialNumber)
  {
    m_serialNumber = serialNumber;
  }

  uint32_t GetSerialNumber() const
  {
    return m_serialNumber;
  }

private:
  uint32_t m_serialNumber;
};

class SenderIdTag : public Tag
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("SenderIdTag")
                           .SetParent<Tag>()
                           .AddConstructor<SenderIdTag>();
    return tid;
  }

  virtual TypeId GetInstanceTypeId() const
  {
    return GetTypeId();
  }

  virtual uint32_t GetSerializedSize() const
  {
    return sizeof(uint32_t);
  }

  virtual void Serialize(TagBuffer buffer) const
  {
    buffer.WriteU32(m_senderId);
  }

  virtual void Deserialize(TagBuffer buffer)
  {
    m_senderId = buffer.ReadU32();
  }

  virtual void Print (std::ostream &os) const{
     os << "Sender ID : " << m_senderId;
  }

  void SetSenderId(uint32_t serialNumber)
  {
    m_senderId = serialNumber;
  }

  uint32_t GetSenderId() const
  {
    return m_senderId;
  }

private:
  uint32_t m_senderId;
};

class SHA256
{
  protected:
    typedef unsigned char uint8;
    typedef unsigned int uint32;
    typedef unsigned long long uint64;

    const static uint32 sha256_k[];
    static const unsigned int SHA224_256_BLOCK_SIZE = (512 / 8);

  public:
    void init();
    void update(const unsigned char* message, unsigned int len);
    void final(unsigned char* digest);
    static const unsigned int DIGEST_SIZE = (256 / 8);

  protected:
    void transform(const unsigned char* message, unsigned int block_nb);
    unsigned int m_tot_len;
    unsigned int m_len;
    unsigned char m_block[2 * SHA224_256_BLOCK_SIZE];
    uint32 m_h[8];
};

const unsigned int SHA256::sha256_k[64] = // UL = uint32
    {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
     0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
     0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
     0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
     0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
     0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
     0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
     0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
     0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
     0xc67178f2};

void
SHA256::transform(const unsigned char* message, unsigned int block_nb)
{
    uint32 w[64];
    uint32 wv[8];
    uint32 t1, t2;
    const unsigned char* sub_block;
    int i;
    int j;
    for (i = 0; i < (int)block_nb; i++)
    {
        sub_block = message + (i << 6);
        for (j = 0; j < 16; j++)
        {
            SHA2_PACK32(&sub_block[j << 2], &w[j]);
        }
        for (j = 16; j < 64; j++)
        {
            w[j] = SHA256_F4(w[j - 2]) + w[j - 7] + SHA256_F3(w[j - 15]) + w[j - 16];
        }
        for (j = 0; j < 8; j++)
        {
            wv[j] = m_h[j];
        }
        for (j = 0; j < 64; j++)
        {
            t1 = wv[7] + SHA256_F2(wv[4]) + SHA2_CH(wv[4], wv[5], wv[6]) + sha256_k[j] + w[j];
            t2 = SHA256_F1(wv[0]) + SHA2_MAJ(wv[0], wv[1], wv[2]);
            wv[7] = wv[6];
            wv[6] = wv[5];
            wv[5] = wv[4];
            wv[4] = wv[3] + t1;
            wv[3] = wv[2];
            wv[2] = wv[1];
            wv[1] = wv[0];
            wv[0] = t1 + t2;
        }
        for (j = 0; j < 8; j++)
        {
            m_h[j] += wv[j];
        }
    }
}

void
SHA256::init()
{
    m_h[0] = 0x6a09e667;
    m_h[1] = 0xbb67ae85;
    m_h[2] = 0x3c6ef372;
    m_h[3] = 0xa54ff53a;
    m_h[4] = 0x510e527f;
    m_h[5] = 0x9b05688c;
    m_h[6] = 0x1f83d9ab;
    m_h[7] = 0x5be0cd19;
    m_len = 0;
    m_tot_len = 0;
}

void
SHA256::update(const unsigned char* message, unsigned int len)
{
    unsigned int block_nb;
    unsigned int new_len, rem_len, tmp_len;
    const unsigned char* shifted_message;
    tmp_len = SHA224_256_BLOCK_SIZE - m_len;
    rem_len = len < tmp_len ? len : tmp_len;
    memcpy(&m_block[m_len], message, rem_len);
    if (m_len + len < SHA224_256_BLOCK_SIZE)
    {
        m_len += len;
        return;
    }
    new_len = len - rem_len;
    block_nb = new_len / SHA224_256_BLOCK_SIZE;
    shifted_message = message + rem_len;
    transform(m_block, 1);
    transform(shifted_message, block_nb);
    rem_len = new_len % SHA224_256_BLOCK_SIZE;
    memcpy(m_block, &shifted_message[block_nb << 6], rem_len);
    m_len = rem_len;
    m_tot_len += (block_nb + 1) << 6;
}

void
SHA256::final(unsigned char* digest)
{
    unsigned int block_nb;
    unsigned int pm_len;
    unsigned int len_b;
    int i;
    block_nb = (1 + ((SHA224_256_BLOCK_SIZE - 9) < (m_len % SHA224_256_BLOCK_SIZE)));
    len_b = (m_tot_len + m_len) << 3;
    pm_len = block_nb << 6;
    memset(m_block + m_len, 0, pm_len - m_len);
    m_block[m_len] = 0x80;
    SHA2_UNPACK32(len_b, m_block + pm_len - 4);
    transform(m_block, block_nb);
    for (i = 0; i < 8; i++)
    {
        SHA2_UNPACK32(m_h[i], &digest[i << 2]);
    }
}

uint8_t*
sha256(uint8_t* input, size_t len)
{
    unsigned char* digest = new unsigned char[SHA256::DIGEST_SIZE];
    memset(digest, 0, SHA256::DIGEST_SIZE);

    SHA256 ctx = SHA256();
    ctx.init();
    ctx.update((unsigned char*)input, len);
    ctx.final(digest);

    return (uint8_t*)digest;
}


uint8_t*
convertInt(uint32_t val)
{
    uint8_t* arr = new uint8_t[4];
    arr[0] = (val >> 24) & 0xFF;
    arr[1] = (val >> 16) & 0xFF;
    arr[2] = (val >> 8) & 0xFF;
    arr[3] = val & 0xFF;
    return arr;
}

uint8_t* convertInt64(int64_t value)
{
    uint8_t* array = new uint8_t[8];
    for (int i = 0; i < 8; ++i) {
        array[i] = static_cast<uint8_t>((value >> (56 - (8 * i))) & 0xFF);
    }
    return array;
}

int64_t ByteArrayToInt64(const uint8_t* byteArray)
{
    int64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<int64_t>(byteArray[i]) << (56 - (8 * i));
    }
    return value;
}


uint32_t
generateRandom()
{
    uint32_t min = 0;
    uint32_t max = INT_MAX;
    // Create a random device as the seed
    std::random_device rd;

    // Create a random engine
    std::mt19937 mt(rd());

    // Create a distribution for the specified range
    std::uniform_int_distribution<int> dist(min, max);

    // Generate a random integer
    return dist(mt);
}

uint8_t*
generateRandomBytes()
{
    uint8_t* randomBytes = new uint8_t[32];

    // Create a random device as the seed
    std::random_device rd;

    // Create a random engine
    std::mt19937 mt(rd());

    // Create a distribution for the byte range
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    // Generate random bytes
    for (uint32_t i = 0; i < 32; i++)
    {
        randomBytes[i] = dist(mt);
    }

    return randomBytes;
}

uint8_t*
concat(uint8_t* arr1, size_t size1, uint8_t* arr2, size_t size2)
{
    uint8_t* arr = new uint8_t[size1 + size2];
    std::copy(arr1, arr1 + size1, arr);
    std::copy(arr2, arr2 + size2, arr + size1);
    return arr;
}

uint8_t*
xor32(uint8_t* arr1, uint8_t* arr2)
{
    uint8_t* arr = new uint8_t[32];
    for (size_t i = 0; i < 32; ++i)
    {
        arr[i] = arr1[i] ^ arr2[i];
    }
    return arr;
}

void
print32(uint8_t* arr, size_t n)
{
    for (uint32_t i = 0; i < n/4; ++i)
    {
        std::cout << *(uint32_t*)(arr + 4*i);
    }
    std::cout<<"\n";
}

uint32_t ConvertByteArrayToInteger(const uint8_t* byteArray)
{
    uint32_t result = 0;
    result |= static_cast<uint32_t>(byteArray[0]) << 24;
    result |= static_cast<uint32_t>(byteArray[1]) << 16;
    result |= static_cast<uint32_t>(byteArray[2]) << 8;
    result |= static_cast<uint32_t>(byteArray[3]);
    return result;
}


uint8_t* concat3(uint8_t* a1, size_t sz1, uint8_t* a2, size_t sz2, uint8_t* a3, size_t sz3) {
    uint8_t* arr = new uint8_t[sz1 + sz2 + sz3];
    std::copy(a1, a1 + sz1, arr);
    std::copy(a2, a2 + sz2, arr + sz1);
    std::copy(a3, a3 + sz3, arr + sz1 + sz2);
    return arr;

}

uint8_t* concat4(uint8_t* a1, size_t sz1, uint8_t* a2, size_t sz2, uint8_t* a3, size_t sz3, uint8_t* a4, size_t sz4) {
    uint8_t* arr = new uint8_t[sz1 + sz2 + sz3 + sz4];
    std::copy(a1, a1 + sz1, arr);
    std::copy(a2, a2 + sz2, arr + sz1);
    std::copy(a3, a3 + sz3, arr + sz1 + sz2);
    std::copy(a4, a4 + sz4, arr + sz1 + sz2 + sz3);
    return arr;

}

uint32_t modulo(uint8_t* arr, size_t sz, uint32_t l) {
  uint32_t result = 0;
    for (size_t i = 0; i < sz; i++) {
        result = (result * 256 + arr[i]) % l;
    }
    return result;
}