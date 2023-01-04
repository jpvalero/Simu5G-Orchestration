//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _CBRRECEIVER_H_
#define _CBRRECEIVER_H_

#include <string.h>
#include <omnetpp.h>

#include <inet/common/INETDefs.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/networklayer/common/L3AddressResolver.h>

#include "CbrRequest_m.h"
#include "CbrResponse_m.h"

#include "nodes/mec/VirtualisationInfrastructureManager/VirtualisationInfrastructureManager.h"

class CbrResponder : public omnetpp::cSimpleModule
{
    inet::UdpSocket socket;

    int numReceived_;
    int totFrames_;
    int recvBytes_;

    int respSize_;

    bool mInit_;

    int destPort_;
    inet::L3Address destAddress_;

    bool enableVimComputing_;

    inet::Packet* respPacket_;
    cMessage* processingTimer_;

    static omnetpp::simsignal_t cbrReqFrameLossSignal_;
    static omnetpp::simsignal_t cbrReqFrameDelaySignal_;
    static omnetpp::simsignal_t cbrReqJitterSignal_;
    static omnetpp::simsignal_t cbrReqReceivedThroughtput_;
    static omnetpp::simsignal_t cbrReqReceivedBytesSignal_;

    VirtualisationInfrastructureManager* vim;

    omnetpp::cOutVector rt_stats_;

    omnetpp::simsignal_t cbrReqRcvdPkt_;

  protected:

    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;
    virtual void finish() override;
};

#endif

