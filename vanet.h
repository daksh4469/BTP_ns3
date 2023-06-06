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

#include <random>

using namespace ns3;
double tim = 0;

NodeContainer vehicles;
NodeContainer rsus;
NodeContainer tas;
// Ptr<Node> ta;
Ipv4InterfaceContainer vehicleInterfaces;
Ipv4InterfaceContainer rsuInterfaces;
Ipv4InterfaceContainer taInterfaces;
uint8_t* kTA = generateRandomBytes();
const uint32_t numVehicle = 25;
const uint32_t numRSU = 25;

int cnt1, cnt2, cnt3, cnt4, cnt5, cnt6,cnt7,cnt8,cnt9, cnt10, cnt11 = 0;

/*static Ptr<Packet>
createPacket(std::string data)
{
    std::vector<uint8_t> temp(data.begin(), data.end());
    uint8_t* bufp = &temp[0];
    return Create<Packet>(bufp, data.length());
}
*/

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

struct VehicleInfo
{
    VehicleInfo()
    {
    }

    void setParams(uint8_t* tvid, uint8_t* hvid, uint8_t* Rta)
    {
        this->tvid = tvid;
        this->hvid = hvid;
        this->Rta = Rta;
    }

    uint8_t* tvid;
    uint8_t* hvid;
    uint8_t* Rta;
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

    // void SendPacket2(Ptr<Packet> packet, InetSocketAddress addr)
    // {
    //     Ptr<Socket> socket = Socket::CreateSocket(GetNode(),
    //     TypeId::LookupByName("ns3::UdpSocketFactory")); InetSocketAddress local =
    //     InetSocketAddress(Ipv4Address::GetAny(), 100); if (socket->Bind(local) == -1)
    //     {
    //         std::cout << "Failed to bind socket";
    //     }
    //     if (socket == nullptr)
    //     {
    //         std::cout << "Socket Not Initialized\n";
    //         return;
    //     }
    //     if (socket->SendTo(packet, 0, addr) == -1)
    //     {
    //         std::cout << "Failed send";
    //     }
    //     socket->Close();
    // }

    void receiveData(Ptr<Socket> socket, uint8_t* buf, uint32_t size)
    {
        // std::cout << "LOG:: receive called\n";
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
        // std::cout << "Remaining data\n";
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
        v_id = convertInt(id);
        v_pw = convertInt(generateRandom());
    }

    void RegisterWithTA()
    {
        std::cout << "LOG: Vehicle " << ConvertByteArrayToInteger(v_id)
                  << " Registration started with TA at Time : " << Simulator::Now() << std::endl;
        this->vu = generateRandomBytes();
        this->alpha = generateRandomBytes();
        this->hvid = sha256(concat(v_id, 4, vu, 32), 36);
        this->hvpw = sha256(concat(concat(vu, 32, v_id, 4), 36, v_pw, 4), 40);

        Ptr<Packet> packet =
            Create<Packet>(concat(this->hvid, 32, xor32(this->hvpw, this->alpha), 32), 64);
        setSerialNumber(packet, 1);
        setSenderId(packet, ConvertByteArrayToInteger(v_id));
        this->SendPacket(packet, InetSocketAddress(taInterfaces.GetAddress(0), 80));
        /*Simulator::ScheduleWithContext(this->GetNode()->GetId(),
                                        Seconds(0.2),
                                        &VehicleApp::SendPacket,
                                        this,
                                        packet,
                                        InetSocketAddress(taInterfaces.GetAddress(0), 80));*/
        cnt1++;
    }

    void LoginandAuth()
    {
        if(!this->reg) return;
        uint8_t* tu = convertInt64(Simulator::Now().GetNanoSeconds());
        this->nu = generateRandomBytes();
        this->a1 = xor32(B1, hvpw);
        uint8_t* msgvt =
            sha256(concat(sha256(concat(nu, 32, a1, 32), 64), 32, concat(tu, 8, tvid, 32), 40), 72);
        uint8_t* x1 = xor32(sha256(concat(nu, 32, a1, 32), 64), sha256(concat(a1, 32, tu, 8), 40));
        uint8_t* msg = concat(concat(msgvt, 32, x1, 32), 64, concat(tu, 8, tvid, 32), 40);

        Ptr<Packet> packet = Create<Packet>(msg, 104);
        setSerialNumber(packet, 3);
        setSenderId(packet, ConvertByteArrayToInteger(v_id));
        cnt7++;
        tstart = Simulator::Now();
        this->SendPacket(packet, InetSocketAddress(taInterfaces.GetAddress(0), 80));
    }

    bool reg = false;
    bool loggedIn = false;
    Time tstart;

  private:
    
    uint8_t* v_id;
    uint8_t* v_pw;
    uint8_t* vu;
    uint8_t* alpha;
    uint8_t* hvid;
    uint8_t* hvpw;
    uint8_t* B1;
    uint8_t* C1;
    uint8_t* tvid;
    uint8_t* Z;
    uint8_t* nu;
    uint8_t* a1;

    void HandleRead(Ptr<Socket> socket)
    {
        std::cout << "Received back message on vehicle : " << ConvertByteArrayToInteger(v_id)
                  << "\n";
        uint8_t readBuf[128] = {0};
        Ptr<Packet> packet = socket->Recv();
        packet->CopyData(readBuf, 128);

        switch (getSerialNumber(packet))
        {
        case 1: {
            uint8_t* oldB1 = new uint8_t[32];
            uint8_t* oldC1 = new uint8_t[32];
            this->tvid = new uint8_t[32];
            std::copy(readBuf, readBuf + 32, oldB1);
            std::copy(readBuf + 32, readBuf + 64, oldC1);
            std::copy(readBuf + 64, readBuf + 96, tvid);
            this->Z = xor32(sha256(concat(v_id, 32, v_pw, 32), 64), vu);
            this->B1 = xor32(oldB1, alpha);
            this->C1 = sha256(concat(oldC1, 32, this->hvpw, 32), 64);
            cnt3++;
            reg = true;
        }
        break;
        case 4: {
            uint8_t* x4 = new uint8_t[32];
            uint8_t* x5 = new uint8_t[32];
            uint8_t* tc1 = new uint8_t[8];
            std::copy(readBuf + 32, readBuf + 64, x4);
            std::copy(readBuf + 64, readBuf + 96, x5);
            std::copy(readBuf + 96, readBuf + 104, tc1);
            uint8_t* sk = xor32(x4, sha256(concat3(sha256(concat(this->nu, 32, this->a1, 32), 64), 32, this->hvid, 32, tc1, 8), 72));
            std::cout << "Session key generated: ";
            print32(sk, 32);
            this->tvid =  xor32(
                x5,
                sha256(
                    concat4(this->hvid, 32, this->tvid, 32, sha256(concat(this->nu, 32, this->a1, 32), 64), 32, tc1, 8),
                    104
                )
            );
            tim += (Simulator::Now().GetSeconds() - tstart.GetSeconds());
            std::cout << "Login and Auth completed for vehicle: " << ConvertByteArrayToInteger(this->v_id) << "at time "<<Simulator::Now()<<std::endl;
            loggedIn = true;
            cnt11++;
        }
        default:
            break;
        }
    }
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

    void SetRSU_SID(uint32_t id)
    {
        rsu_id = id;
        s_id = generateRandomBytes();
    }

    void RegisterWithTA()
    {
        std::cout << "LOG: RSU " << rsu_id
                  << " Registration started with TA at Time : " << Simulator::Now() << std::endl;

        vr = generateRandomBytes();
        reg = xor32(s_id, sha256(vr, 32));
        s = xor32(kTA, sha256(vr, 32));

        print32(reg, 32);

        Ptr<Packet> packet = Create<Packet>(concat(this->reg, 32, s, 32), 64);
        SerialNumberTag serialTag;
        serialTag.SetSerialNumber(2);
        SenderIdTag senderTag;
        senderTag.SetSenderId(rsu_id);
        packet->AddPacketTag(serialTag);
        packet->AddPacketTag(senderTag);
        std::cout << Simulator::Now() << std::endl;
        this->SendPacket(packet, InetSocketAddress(taInterfaces.GetAddress(0), 80));
        /*Simulator::ScheduleWithContext(this->GetNode()->GetId(),
                                Seconds(0.1),
                                &RSUApp::SendPacket,
                                this,
                                packet,
                                InetSocketAddress(taInterfaces.GetAddress(0), 80));*/
        cnt4++;
    }

    void HandleRead(Ptr<Socket> socket)
    {
        uint8_t readBuf[128] = {0};
        Ptr<Packet> packet = socket->Recv();
        packet->CopyData(readBuf, 128);

        switch (getSerialNumber(packet))
        {
        case 2: {
            std::cout << "LOG: RSU " << rsu_id << "Received info from TA at " << Simulator::Now()
                      << "\n";
            recvAVT = new uint8_t[32];
            recvPSID = new uint8_t[32];
            std::copy(readBuf, readBuf + 32, recvPSID);
            std::copy(readBuf + 32, readBuf + 64, recvAVT);
            cnt6++;
            registered = true;
            // print32(recvAVT,32);
            // print32(recvPSID,32);
        }
        break;

    case 3: {
        std::cout<<"Recv on nearest RSU with id "<<rsu_id<<"\n";
        uint8_t* msgTV = new uint8_t[32];
        uint8_t* x2 = new uint8_t[32];
        uint8_t* tc = new uint8_t[8];
        std::copy(readBuf,readBuf+32,msgTV);
        std::copy(readBuf+32,readBuf+64,x2);
        std::copy(readBuf+64,readBuf+72,tc);
        uint8_t* ts = convertInt64(Simulator::Now().GetNanoSeconds());
        if( (ByteArrayToInt64(ts) - ByteArrayToInt64(tc)) >= 4e9) break;
        uint8_t* ns = generateRandomBytes();
        uint8_t* kvt = xor32(x2,sha256(concat(concat(recvPSID,32,recvAVT,32),64,tc,8),72));
        uint8_t* sk = sha256(concat(concat(kvt,32,sha256(concat(ns,32,recvPSID,32),64),32),64,ts,8),72);
        uint8_t* x3 = xor32(sk,sha256(concat(concat(concat(recvPSID,32,recvAVT,32),64,x2,32),96,ts,8),104));
        uint8_t* msgAVT = sha256(concat(concat(sk,32,tc,8),40,ts,8),48);
        Ptr<Packet> pkt = Create<Packet>(concat(concat(msgAVT,32,x3,32),64,ts,8),72);
        setSerialNumber(pkt, 4);
        setSenderId(pkt,getSenderId(packet));
        // this->SendPacket(pkt,InetSocketAddress(taInterfaces.GetAddress(0), 80));
        Simulator::ScheduleWithContext(this->GetNode()->GetId(),
                                Seconds(0.2),
                                &RSUApp::SendPacket,
                                this,
                                pkt,
                                InetSocketAddress(taInterfaces.GetAddress(0), 80));
        cnt9++;
        break;
    }
        }
    }

    bool registered = false;
    uint32_t rsu_id;
    uint8_t* s_id;
    uint8_t* vr;

  private:
    uint8_t* reg;
    uint8_t* s;
    uint8_t *recvAVT, *recvPSID;
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


struct vehicleAuthInfo {
    uint8_t* avt, *x2, *tc, *nu, *nt, *tu;
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

  private:
    VehicleInfo vehicleParams[numVehicle];
    uint8_t *psid, *avt;
    RSUParams info[numRSU];
    vehicleAuthInfo vAuth[numVehicle];

    void HandleRead(Ptr<Socket> socket)
    {
        uint8_t readBuf[128] = {0};
        Ptr<Packet> packet = socket->Recv();
        packet->CopyData(readBuf, 128);

        switch (getSerialNumber(packet))
        {
        case 1: {
            uint8_t* hvid = new uint8_t[32];
            uint8_t* rem = new uint8_t[32];
            std::copy(readBuf, readBuf + 32, hvid);
            std::copy(readBuf + 32, readBuf + 64, rem);

            uint8_t* Rta = generateRandomBytes();
            uint8_t* tvid = generateRandomBytes();
            uint8_t* A1 = sha256(concat(concat(Rta, 32, kTA, 32), 64, hvid, 32), 96);
            uint8_t* B1 = xor32(A1, rem);
            uint8_t* C1 = sha256(concat(A1, 32, hvid, 32), 64);

            uint32_t senderId = getSenderId(packet);
            std::cout << "Vehicle to TA Msg Received :: Sender Id is : " << senderId << "at time "
                      << Simulator::Now() << "\n";
            vehicleParams[senderId].setParams(tvid, hvid, Rta);
            Ptr<Packet> pkt = Create<Packet>(concat(concat(B1, 32, C1, 32), 64, tvid, 32), 96);
            setSerialNumber(pkt, 1);
            setSenderId(pkt, 0);
            // std::cout <<"Time at which msg sent from TA back to vehicle is : "<< Simulator::Now()
            // << std::endl;
            // this->SendPacket(pkt, InetSocketAddress(vehicleInterfaces.GetAddress(senderId), 80));
            Simulator::ScheduleWithContext(
                this->GetNode()->GetId(),
                Seconds(0.2),
                &TAApp::SendPacket,
                this,
                pkt,
                InetSocketAddress(vehicleInterfaces.GetAddress(senderId), 80));
                
            cnt2++;
            break;
        }

        case 2: {
            uint8_t* s = new uint8_t[32];
            uint8_t* reg = new uint8_t[32];
            uint8_t* s_id;
            uint8_t* hVR;
            std::copy(readBuf, readBuf + 32, reg);
            std::copy(readBuf + 32, readBuf + 64, s);

            hVR = xor32(kTA, s);
            s_id = xor32(hVR, reg);

            uint8_t* x = generateRandomBytes();
            uint8_t* temp1 = concat(s_id, 32, kTA, 32);
            uint8_t* temp2 = concat(temp1, 64, x, 32);
            this->psid = sha256(temp2, 96);
            temp1 = concat(s_id, 32, hVR, 32);
            temp2 = concat(temp1, 64, kTA, 32);
            uint8_t* temp3 = concat(temp2, 96, x, 32);
            this->avt = sha256(temp3, 128);
            // print32(psid,32);
            // print32(avt,32);
            uint32_t senderId = getSenderId(packet);
            std::cout << "RSU to TA Msg Received :: Sender Id is : " << senderId << "at time "
                      << Simulator::Now() << "\n";
            Ptr<Packet> pkt2 = Create<Packet>(concat(psid, 32, avt, 32), 64);
            setSerialNumber(pkt2, 2);
            setSenderId(pkt2, 1);
            // this->SendPacket(pkt2, InetSocketAddress(taInterfaces.GetAddress(senderId+1), 80));
            Simulator::ScheduleWithContext(
                this->GetNode()->GetId(),
                Seconds(0.2),
                &TAApp::SendPacket,
                this,
                pkt2,
                InetSocketAddress(taInterfaces.GetAddress(senderId + 1), 80));
            
            cnt5++;
            info[senderId].setParams(hVR, x);
        }

        break;

        case 3: {
            uint8_t* x1 = new uint8_t[32];
            uint8_t* tu = new uint8_t[4];
            std::copy(readBuf + 32, readBuf + 64, x1);
            std::copy(readBuf + 64, readBuf + 68, tu);
            uint8_t* tc = convertInt64(Simulator::Now().GetNanoSeconds());
            uint32_t senderId = getSenderId(packet);
            if( (ByteArrayToInt64(tc) - ByteArrayToInt64(tu)) >= 4e9) break;
            std::cout << "sender for case 3: " <<  senderId << std::endl;
            vAuth[senderId].tu = tu;
            uint32_t rsu = getRSU(senderId);
            std::cout << rsu << std::endl;
            uint8_t* s_id = DynamicCast<RSUApp>(rsus.Get(rsu)->GetApplication(0))->s_id;
            uint8_t* avt = sha256(concat(concat(s_id, 32, info[rsu].hVR, 32),
                                         64,
                                         concat(kTA, 32, info[rsu].randomSK, 32),
                                         64),
                                  128);
            vAuth[senderId].avt = avt;
            vAuth[senderId].tc = tc;
            uint8_t* nt = generateRandomBytes();
            vAuth[senderId].nt = nt;
            uint8_t* a1 = sha256(concat(concat(vehicleParams[senderId].Rta, 32, kTA, 32),
                                        64,
                                        vehicleParams[senderId].hvid,
                                        32),
                                 96);
            uint8_t* nu = xor32(x1, sha256(concat(a1, 32, tu, 4), 36));
            vAuth[senderId].nu = nu;
            uint8_t* x2 = xor32(
                sha256(
                    concat(
                        nu,
                        32,
                        sha256(concat(concat(nt, 32, avt, 32), 64, vehicleParams[senderId].Rta, 32),
                               96),
                        32),
                    64),
                sha256(
                    concat(
                        concat(sha256(concat(concat(s_id, 32, kTA, 32), 64, info[rsu].randomSK, 32),
                                      96),
                               32,
                               avt,
                               32),
                        64,
                        tc,
                        8),
                    72));
            uint8_t* msgTV = sha256(concat(concat(concat(sha256(concat(concat(s_id,32,kTA,32),64,info[rsu].randomSK,32),96),32,avt,32),64,tc,8),72, sha256(
                    concat(
                        nu,
                        32,
                        sha256(concat(concat(nt, 32, avt, 32), 64, vehicleParams[senderId].Rta, 32),
                               96),
                        32),
                    64),32),104);
                    vAuth[senderId].x2= x2;

                    Ptr<Packet> pkt = Create<Packet>(concat(concat(msgTV,32,x2,32),64,tc,8),72);
                    setSerialNumber(pkt, 3);
                    setSenderId(pkt,senderId);
                    cnt8++;
                    Simulator::ScheduleWithContext(
                this->GetNode()->GetId(),
                Seconds(0.2),
                &TAApp::SendPacket,
                this,
                pkt,
                InetSocketAddress(taInterfaces.GetAddress(rsu+1),80));
                // this->SendPacket(pkt,InetSocketAddress(taInterfaces.GetAddress(rsu+1),80));


        }
        break;
        case 4: {
            uint8_t* msgavt = new uint8_t[32];
            uint8_t* x3 = new uint8_t[32];
            uint8_t* ts = new uint8_t[8];
            std::copy(readBuf, readBuf + 32, msgavt);
            std::copy(readBuf + 32, readBuf + 64, x3);
            std::copy(readBuf + 64, readBuf + 72, ts);

            uint8_t* tc1 = convertInt64(Simulator::Now().GetNanoSeconds());
            if( (ByteArrayToInt64(tc1) - ByteArrayToInt64(ts)) >= 4e9) break;
            uint32_t senderId = getSenderId(packet);
            uint32_t rsu = getRSU(senderId);
            std::cout << "sender for case 4 : " << senderId << std::endl;
            std::cout << rsu << std::endl;
            uint8_t* s_id = DynamicCast<RSUApp>(rsus.Get(rsu)->GetApplication(0))->s_id;
            uint8_t* sk = xor32(x3, sha256(concat4(sha256(concat3(s_id, 32, kTA, 32, info[rsu].randomSK, 32), 96), 32, vAuth[senderId].avt, 32, vAuth[senderId].x2, 32, ts, 8), 104));
            uint8_t* tvidnew = sha256(
                concat(
                    concat3(vAuth[senderId].nt, 32, vAuth[senderId].nu, 32, tc1, 8), 72,
                    concat3(vAuth[senderId].tc, 8, vAuth[senderId].tu, 8, vehicleParams[senderId].tvid, 32), 48
                ), 120
            );
            uint8_t* x4 = xor32(sk, sha256(concat3(vAuth[senderId].nu, 32, vehicleParams[senderId].hvid, 32, tc1, 8), 72));
            uint8_t* x5 = xor32(tvidnew, sha256(concat4(vehicleParams[senderId].hvid, 32, vehicleParams[senderId].tvid, 32, vAuth[senderId].nu, 32, tc1, 8), 104));
            uint8_t* msgatv = sha256(concat3(sk, 32, tvidnew, 32, tc1, 8), 72);
            uint8_t* msg4 = concat4(msgatv, 32, x4, 32, x5, 32, tc1, 8);
            Ptr<Packet> pkt = Create<Packet>(msg4, 104);
                    setSerialNumber(pkt, 4);
                    setSenderId(pkt,senderId);
                    cnt8++;
            
            // this->SendPacket(pkt, InetSocketAddress(vehicleInterfaces.GetAddress(senderId), 80));
            Simulator::ScheduleWithContext(
                this->GetNode()->GetId(),
                Seconds(0.2),
                &TAApp::SendPacket,
                this,
                pkt,
                InetSocketAddress(vehicleInterfaces.GetAddress(senderId),80));

            
            std::cout << "Received at TA LOGAUTH from RSU\n";
            cnt10++;
        }
        break;

        default:
            std::cout << "Received on default";
            break;
        }
    }
};