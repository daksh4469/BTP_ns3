# BTP
This is the source code of our B.Tech Project performed under the supervision of Prof. Manoj Misra at IIT Roorkee. Our BTP was to implement the VANET Authentication Protocols proposed in the following research papers using [ns-3.38](https://www.nsnam.org/releases/ns-3-38/):

- Secure and Efficient Honey List-Based VANET Protocol
- Blockchain-Based Lightweight V2I Handover VANET Protocol

We have analyzed the following performance measures for both these protocols under different scenarios:

- Average Throughput
- End-to-End Delay
- Packet Delivery Ratio
- Average Login Time
- Concurrent Logins

# How to Run

Download and install the network simulator, ns-3.38 following the instructions provided in their [Download Manual](https://www.nsnam.org/releases/ns-3-38/download/).
After successfully completing these steps, there should be a directory ns-allinone-3.38 in your prefereed location of the setup process.

Add the files of this source code in the `ns-allinone-3.38/ns-3.38/scratch/` directory. To build and run the authentication protocol, say the first one(`vanet.cc`), run the following command in the ns-3.38 directory:

`./ns3 run scratch/vanet.cc`
