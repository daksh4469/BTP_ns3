#include "./utility.h"

#include "ns3/animation-interface.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wave-module.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <random>

using namespace ns3;

NodeContainer vehicles;
NodeContainer rsus;
NodeContainer tas;
double tim = 0.0;

Ipv4InterfaceContainer vehicleInterfaces;
Ipv4InterfaceContainer rsuInterfaces;
Ipv4InterfaceContainer taInterfaces;

const uint32_t numVehicle = 50;
const uint32_t numRSU = 25;

uint32_t l = 1 << 6;
EC_GROUP* curve_group = EC_GROUP_new_by_curve_name(NID_secp224r1);
int cnt1, cnt2, cnt3, cnt4, cnt5, cnt6, cnt8, cnt9, cnt10, cnt11 = 0;
uint8_t* k;
std::set<uint32_t> blockChain;

static void
setSerialNumber(Ptr<Packet> packet, uint32_t num)
{
    SerialNumberTag serialTag;
    serialTag.SetSerialNumber(num);
    packet->AddPacketTag(serialTag);
}

static void
setSenderId(Ptr<Packet> packet, uint32_t num)
{
    SenderIdTag IdTag;
    IdTag.SetSenderId(num);
    packet->AddPacketTag(IdTag);
}

static uint32_t
getSerialNumber(Ptr<Packet> packet)
{
    SerialNumberTag serialTag;
    packet->PeekPacketTag(serialTag);
    return serialTag.GetSerialNumber();
}

static uint32_t
getSenderId(Ptr<Packet> packet)
{
    SenderIdTag senderTag;
    packet->PeekPacketTag(senderTag);
    return senderTag.GetSenderId();
}

int
getRSU(uint32_t vehicleId)
{
    Vector position = vehicles.Get(vehicleId)->GetObject<MobilityModel>()->GetPosition();
    double x = position.x;
    double y = position.y;
    int x1 = x / 250;
    int y1 = y / 250;
    if (x - (x1 * 250) > 125.0)
        x1++;
    if (y - (y1 * 250) > 125.0)
        y1++;
    return x1 * 5 + y1;
}

uint8_t*
getBufFromPoint(EC_POINT* P, size_t& size)
{
    size = EC_POINT_point2oct(curve_group, P, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);
    uint8_t* PBuf = (uint8_t*)OPENSSL_malloc(size);
    EC_POINT_point2oct(curve_group, P, POINT_CONVERSION_UNCOMPRESSED, PBuf, size, NULL);
    return PBuf;
}

EC_POINT*
getPointFromBuf(const uint8_t* buffer, size_t size)
{
    EC_POINT* point = EC_POINT_new(curve_group);
    EC_POINT_oct2point(curve_group, point, buffer, size, NULL);
    return point;
}

struct VehicleInfo
{
    VehicleInfo()
    {
    }

    void setParams(uint8_t* id, uint8_t* rpw, uint8_t* r)
    {
        this->id = id;
        this->rpw = rpw;
        this->r = r;
    }

    uint8_t* id;
    uint8_t* rpw;
    uint8_t* r;
};

class VANETApp : public Application
{
  public:
    VANETApp()
    {
        this->m_port = 80;
    }

    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::VANETApp").AddConstructor<VANETApp>().SetParent<Application>();
        return tid;
    }

    TypeId GetInstanceTypeId() const
    {
        return this->GetTypeId();
    }

    void SendPacket(Ptr<Packet> packet, InetSocketAddress addr)
    {
        if (m_socket == nullptr)
        {
            std::cout << "Socket Not Initialized\n";
            return;
        }
        if (m_socket->SendTo(packet, 0, addr) == -1)
        {
            std::cout << "Failed send";
        }
    }

    void receiveData(Ptr<Socket> socket, uint8_t* buf, uint32_t size)
    {
        Ptr<Packet> packet;
        while ((packet = socket->Recv()))
        {
            packet->CopyData(buf, size);
            std::cout << "Received data is :"
                      << "\t";
            print32(buf, size);
            std::cout << "\n";
        }
    }

  private:
    virtual void HandleRead(Ptr<Socket> socket){};

  protected:
    void StartApplication()
    {
        std::cout << "Application started\n";
        m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(local) == -1)
        {
            std::cout << "Failed to bind socket";
        }
        m_socket->SetRecvCallback(MakeCallback(&VANETApp::HandleRead, this));
    }

    void StopApplication()
    {
        std::cout << "Remaining data\n";
        Ptr<Packet> packet;
        uint8_t buf[100] = {0};
        while ((packet = m_socket->Recv()))

        {
            packet->CopyData(buf, sizeof(buf));
            std::cout << "Received data is :"
                      << "\t";
            print32(buf, 64);
            std::cout << "\n";
        }
        m_socket->Close();
    }

    uint16_t m_port;
    Ptr<Socket> m_socket;
};

class RSUApp : public VANETApp
{
  public:
    RSUApp()
        : VANETApp()
    {
    }

    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("RSUApp").SetParent<Application>().AddConstructor<RSUApp>();
        return tid;
    }

    TypeId GetInstanceTypeId() const
    {
        return this->GetTypeId();
    }

    void SetRSU_SID(uint32_t i)
    {
        this->rsu_id = i;
        this->id = new uint8_t[32];
        this->sj = new uint8_t[32];
    }

    void HandleRead(Ptr<Socket> socket)
    {
        uint8_t readBuf[256] = {0};
        Ptr<Packet> packet = socket->Recv();
        packet->CopyData(readBuf, 256);

        switch (getSerialNumber(packet))
        {
        case 1: {
            std::cout << "RSU Registeration completed : " << this->rsu_id << std::endl;
            std::copy(readBuf, readBuf + 32, id);
            std::copy(readBuf + 32, readBuf + 64, sj);
            // std::copy(readBuf + 64, readBuf + 96, k);
            BIGNUM* privKey = NULL;
            BN_bin2bn(sj, 32, privKey);
            this->P = EC_POINT_new(curve_group);
            EC_POINT_mul(curve_group, this->P, privKey, NULL, NULL, NULL);
            registered = true;
        }
        break;

        case 3: {
            std::cout << "Received login and auth message from vehicle " << getSenderId(packet)
                      << "\n";
            uint32_t PiLen = packet->GetSize() - 72;
            uint8_t* M1 = new uint8_t[32];
            uint8_t* M2 = new uint8_t[32];
            uint8_t* Pi = new uint8_t[PiLen];
            uint8_t* t1 = new uint8_t[8];
            std::copy(readBuf, readBuf + 32, M1);
            std::copy(readBuf + 32, readBuf + 64, M2);
            std::copy(readBuf + 64, readBuf + 64 + PiLen, Pi);
            std::copy(readBuf + (packet->GetSize() - 8), readBuf + packet->GetSize(), t1);

            if ((Simulator::Now().GetNanoSeconds() - ConvertByteArray64ToInteger(t1)) < 7e9)
            {
                EC_POINT* Pij = EC_POINT_new(curve_group);
                BIGNUM* sjMul = NULL;
                BN_bin2bn(this->sj, 32, sjMul);

                EC_POINT* PiPoint = getPointFromBuf(Pi, PiLen);
                EC_POINT_mul(curve_group, Pij, NULL, PiPoint, sjMul, NULL);

                size_t PijLen = 0;
                uint8_t* PijBuf = getBufFromPoint(Pij, PijLen);

                uint8_t* rid =
                    xor32(M1,
                          sha256(concat(concat(this->id, 32, PijBuf, PijLen), 32 + PijLen, t1, 8),
                                 40 + PijLen));
                if ((blockChain.find(ConvertByteArrayToInteger(
                         sha256(concat(rid, 32, k, 32), 64))) != blockChain.end()))
                {
                    if (byteArrayEqual(M2,
                                       sha256(concat3(rid, 32, PijBuf, PijLen, t1, 8), 40 + PijLen),
                                       32))
                    {
                        uint8_t* bj = generateRandomBytes();
                        uint8_t* nj = generateRandomBytes();
                        uint8_t* t2 = convertInt64(Simulator::Now().GetNanoSeconds());

                        EC_POINT* Qij = EC_POINT_new(curve_group);
                        EC_POINT* Qj = EC_POINT_new(curve_group);

                        BIGNUM* BjMul = NULL;
                        BN_bin2bn(bj, 32, BjMul);

                        EC_POINT_mul(curve_group, Qij, NULL, PiPoint, BjMul, NULL);
                        EC_POINT_mul(curve_group, Qj, BjMul, NULL, NULL, NULL);

                        uint8_t* tid = xor32(rid, sha256(concat3(nj, 32, this->id, 32, k, 32), 96));

                        size_t QijLen = 0;
                        uint8_t* QijBuf = getBufFromPoint(Qij, QijLen);
                        size_t QjLen = 0;
                        uint8_t* QjBuf = getBufFromPoint(Qj, QjLen);

                        uint8_t* M3 =
                            xor32(tid, sha256(concat(QijBuf, QijLen, rid, 32), 32 + QijLen));
                        uint8_t* SKij =
                            sha256(concat3(QijBuf, QijLen, tid, 32, rid, 32), 64 + QijLen);
                        uint8_t* M4 = sha256(concat(SKij, 32, t2, 8), 40);

                        Ptr<Packet> pkt =
                            Create<Packet>(concat4(M3, 32, M4, 32, QjBuf, QjLen, t2, 8),
                                           72 + QjLen);
                        setSerialNumber(pkt, 3);
                        Simulator::Schedule(
                            Seconds(0.2),
                            &RSUApp::SendPacket,
                            this,
                            pkt,
                            InetSocketAddress(vehicleInterfaces.GetAddress(getSenderId(packet)),
                                              80));
                        cnt5++;
                    }
                }
            }
        }
        break;
        }
    }

    bool registered = false;
    uint32_t rsu_id;
    EC_POINT* P;
    uint8_t* id;

  private:
    uint8_t* sj;
};

class VehicleApp : public VANETApp
{
  public:
    static TypeId GetTypeId(void)
    {
        static TypeId tid =
            TypeId("VehicleApp").SetParent<Application>().AddConstructor<VehicleApp>();
        return tid;
    }

    TypeId GetInstanceTypeId() const
    {
        return this->GetTypeId();
    }

    void SetVehicleID(uint32_t id)
    {
        this->v_id = id;
    }

    Time tstart;

    void RegisterWithTA()
    {
        this->id = generateRandomBytes();
        this->pw = generateRandomBytes();
        this->x = generateRandomBytes();
        this->hpw = sha256(concat(this->id, 32, this->pw, 32), 64);
        this->rpw = xor32(this->hpw, sha256(this->x, 32));
        Ptr<Packet> packet = Create<Packet>(concat(this->id, 32, this->rpw, 32), 64);
        setSerialNumber(packet, 2);
        std::cout << "Registration started on vehicle : " << v_id << "at time " << Simulator::Now()
                  << std::endl;
        setSenderId(packet, v_id);
        // this->SendPacket(packet, InetSocketAddress(taInterfaces.GetAddress(0), 80));
        Simulator::ScheduleWithContext(this->GetNode()->GetId(),
                                       Seconds(0),
                                       &VehicleApp::SendPacket,
                                       this,
                                       packet,
                                       InetSocketAddress(taInterfaces.GetAddress(0), 80));
        cnt1++;
    }

    void LoginandAuth()
    {
        uint8_t* hpw = sha256(concat(this->id, 32, this->pw, 32), 64);
        uint8_t* r = xor32(this->A, hpw);
        uint8_t* x = xor32(this->B, sha256(r, 32));
        uint8_t* rpw = xor32(hpw, sha256(x, 32));
        this->rid = sha256(concat(concat(this->id, 32, rpw, 32), 64, r, 32), 96);
        if (ConvertByteArrayToInteger(this->C) ==
            ConvertByteArrayToInteger(
                sha256(convertInt(modulo(sha256(concat(rid, 32, x, 32), 64), 32, l)), 4)))
        {
            this->b = generateRandomBytes();
            int destRSU = getRSU(v_id);
            Ptr<RSUApp> rsuApp = DynamicCast<RSUApp>(rsus.Get(destRSU)->GetApplication(0));
            EC_POINT* destPubKey = rsuApp->P;
            BIGNUM* mul = BN_new();
            BN_CTX* ctx = BN_CTX_new();

            BIGNUM* bNum = BN_bin2bn(this->b, 32, NULL);
            BIGNUM* rNum = BN_bin2bn(r, 32, NULL);
            BN_mul(mul, bNum, rNum, ctx);

            EC_POINT* Pij = EC_POINT_new(curve_group);
            EC_POINT_mul(curve_group, Pij, NULL, destPubKey, mul, NULL);
            EC_POINT* Pi = EC_POINT_new(curve_group);
            EC_POINT_mul(curve_group, Pi, mul, NULL, NULL, NULL);

            size_t PijLen = 0;
            uint8_t* PijBuf = getBufFromPoint(Pij, PijLen);

            size_t PiLen = 0;
            uint8_t* PiBuf = getBufFromPoint(Pi, PiLen);

            uint8_t* t1 = convertInt64(Simulator::Now().GetNanoSeconds());
            uint8_t* m1 =
                xor32(rid,
                      sha256(concat(concat(rsuApp->id, 32, PijBuf, PijLen), 32 + PijLen, t1, 8),
                             40 + PijLen));
            uint8_t* m2 =
                sha256(concat(concat(rid, 32, PijBuf, PijLen), 32 + PijLen, t1, 8), 40 + PijLen);
            Ptr<Packet> pkt = Create<Packet>(
                concat(concat(concat(m1, 32, m2, 32), 64, PiBuf, PiLen), 64 + PiLen, t1, 8),
                72 + PiLen);
            setSerialNumber(pkt, 3);
            setSenderId(pkt, v_id);

            std::cout << "Login and Auth message sent from vehicle " << v_id << " to RSU "
                      << destRSU << "at time " << Simulator::Now() << "\n";

            // this->SendPacket(pkt,InetSocketAddress(rsuInterfaces.GetAddress(destRSU),80));
            tstart = Simulator::Now();
            Simulator::ScheduleWithContext(
                this->GetNode()->GetId(),
                Seconds(0.2),
                &VehicleApp::SendPacket,
                this,
                pkt,
                InetSocketAddress(rsuInterfaces.GetAddress(destRSU), 80));
            cnt2++;
        }
        else
        {
            std::cout << "Check failed\n";
        }
    }

    bool reg = false;
    bool loggedIn = false;

  private:
    uint32_t v_id;
    uint8_t* id;
    uint8_t* pw;
    uint8_t* hpw;
    uint8_t* x;
    uint8_t* rpw;
    uint8_t *A, *B, *C, *b, *r;
    uint8_t* rid;

    void HandleRead(Ptr<Socket> socket)
    {
        uint8_t readBuf[256] = {0};
        Ptr<Packet> packet = socket->Recv();
        packet->CopyData(readBuf, 256);

        switch (getSerialNumber(packet))
        {
        case 2: {
            if (this->reg)
                break;
            this->r = new uint8_t[32];
            std::copy(readBuf, readBuf + 32, this->r);
            std::cout << "Registration completed for vehicle " << v_id << "at time"
                      << Simulator::Now() << "\n";
            uint8_t* rid = sha256(concat3(this->id, 32, this->rpw, 32, r, 32), 96);
            this->A = xor32(this->r, this->hpw);
            this->B = xor32(this->x, sha256(r, 32));
            this->C =
                sha256(convertInt(modulo(sha256(concat(rid, 32, this->x, 32), 64), 32, l)), 4);
            cnt3++;
            this->reg = true;
        }
        break;

        case 3: {
            if (this->loggedIn)
                break;
            uint32_t QjLen = packet->GetSize() - 72;
            uint8_t* M3 = new uint8_t[32];
            uint8_t* M4 = new uint8_t[32];
            uint8_t* QjBuf = new uint8_t[QjLen];
            uint8_t* t2 = new uint8_t[8];

            std::copy(readBuf, readBuf + 32, M3);
            std::copy(readBuf + 32, readBuf + 64, M4);
            std::copy(readBuf + 64, readBuf + 64 + QjLen, QjBuf);
            std::copy(readBuf + (packet->GetSize() - 8), readBuf + packet->GetSize(), t2);

            EC_POINT* QjPoint = getPointFromBuf(QjBuf, QjLen);
            BIGNUM* mul = BN_new();
            BN_CTX* ctx = BN_CTX_new();

            BIGNUM* bNum = BN_bin2bn(this->b, 32, NULL);
            BIGNUM* rNum = BN_bin2bn(this->r, 32, NULL);
            BN_mul(mul, bNum, rNum, ctx);

            EC_POINT* Qij = EC_POINT_new(curve_group);
            EC_POINT_mul(curve_group, Qij, NULL, QjPoint, mul, NULL);

            size_t QijLen = 0;
            uint8_t* QijBuf = getBufFromPoint(Qij, QijLen);

            uint8_t* tid = xor32(M3, sha256(concat(QijBuf, QijLen, rid, 32), 32 + QijLen));
            uint8_t* SKij = sha256(concat3(QijBuf, QijLen, tid, 32, rid, 32), 64 + QijLen);
            tim += Simulator::Now().GetSeconds() - tstart.GetSeconds();

            if (byteArrayEqual(M4, sha256(concat(SKij, 32, t2, 8), 40), 32))
            {
                std::cout << "Auth success on vehicle " << v_id << "at time" << Simulator::Now()
                          << "\n";
                cnt10++;
                this->loggedIn = true;
            }
            else
            {
                std::cout << "Auth Failed on vehicle " << v_id << "\n";
            }
        }
        break;

        default:
            break;
        }
    }
};

struct RSUParams
{
    void setParams(uint8_t* hVR, uint8_t* x)
    {
        this->hVR = hVR;
        this->randomSK = x;
    }

    uint8_t *hVR, *randomSK;
};

struct vehicleAuthInfo
{
    uint8_t *avt, *x2, *tc, *nu, *nt, *tu;
};

class TAApp : public VANETApp
{
  public:
    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("TAApp").SetParent<Application>().AddConstructor<TAApp>();
        return tid;
    }

    TypeId GetInstanceTypeId() const
    {
        return this->GetTypeId();
    }

    void StartApplication()
    {
        VANETApp::StartApplication();
        sTA = generateRandomBytes();
        idTA = generateRandomBytes();
    }

    void systemSetup()
    {
        for (uint32_t i = 0; i < numRSU; ++i)
        {
            std::cout << "RSU message sent" << i << std::endl;
            uint8_t* id = generateRandomBytes();
            uint8_t* sj = sha256(concat(id, 32, sTA, 32), 64);
            k = sha256(concat(idTA, 32, sTA, 32), 64);
            Ptr<Packet> packet = Create<Packet>(concat3(id, 32, sj, 32, k, 32), 96);
            setSerialNumber(packet, 1);
            // this->SendPacket(packet, InetSocketAddress(rsuInterfaces.GetAddress(i), 80));
            Simulator::ScheduleWithContext(this->GetNode()->GetId(),
                                           Seconds(0.2),
                                           &TAApp::SendPacket,
                                           this,
                                           packet,
                                           InetSocketAddress(taInterfaces.GetAddress(i + 1), 80));
        }
    }

  private:
    VehicleInfo vehicleParams[numVehicle];
    uint8_t* sTA;
    uint8_t* idTA;

    void HandleRead(Ptr<Socket> socket)
    {
        uint8_t readBuf[256] = {0};
        Ptr<Packet> packet = socket->Recv();
        packet->CopyData(readBuf, 256);

        switch (getSerialNumber(packet))
        {
        case 1: {
            break;
        }

        case 2: {
            uint8_t* r = generateRandomBytes();
            uint8_t* id = new uint8_t[32];
            uint8_t* rpw = new uint8_t[32];
            std::copy(readBuf, readBuf + 32, id);
            std::copy(readBuf + 32, readBuf + 64, rpw);
            uint8_t* rid = sha256(concat3(id, 32, rpw, 32, r, 32), 96);
            uint32_t senderId = getSenderId(packet);
            std::cout << "Received msg from vehicle: " << senderId << std::endl;
            vehicleParams[senderId].setParams(id, rpw, r);
            blockChain.insert(ConvertByteArrayToInteger(sha256(concat(rid, 32, k, 32), 64)));
            Ptr<Packet> pkt = Create<Packet>(concat(r, 32, rid, 32), 64);
            setSerialNumber(pkt, 2);

            // this->SendPacket(pkt,InetSocketAddress(vehicleInterfaces.GetAddress(senderId), 80));
            Simulator::ScheduleWithContext(
                this->GetNode()->GetId(),
                Seconds(0),
                &TAApp::SendPacket,
                this,
                pkt,
                InetSocketAddress(vehicleInterfaces.GetAddress(senderId), 80));
        }

        break;

        default:
            std::cout << "Received on default";
            break;
        }
    }
};