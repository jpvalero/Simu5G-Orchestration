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

#include "CbrResponder.h"

Define_Module(CbrResponder);
using namespace inet;

simsignal_t CbrResponder::cbrReqFrameLossSignal_ = registerSignal("cbrReqFrameLossSignal");
simsignal_t CbrResponder::cbrReqFrameDelaySignal_ = registerSignal("cbrReqFrameDelaySignal");
simsignal_t CbrResponder::cbrReqJitterSignal_ = registerSignal("cbrReqJitterSignal");
simsignal_t CbrResponder::cbrReqReceivedThroughtput_ = registerSignal("cbrReqReceivedThroughtputSignal");
simsignal_t CbrResponder::cbrReqReceivedBytesSignal_ = registerSignal("cbrReqReceivedBytesSignal");

void CbrResponder::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL)
    {
        mInit_ = true;
        numReceived_ = 0;
        recvBytes_ = 0;
        cbrReqRcvdPkt_ = registerSignal("cbrReqRcvdPkt");
        respSize_ = par("responseSize");
        enableVimComputing_ = false;
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER)
    {
        int port = par("localPort");
        EV << "CbrResponder::initialize - binding to port: local:" << port << endl;
        if (port != -1)
        {
            socket.setOutputGate(gate("socketOut"));
            socket.bind(port);

            destAddress_ = inet::L3AddressResolver().resolve(par("destAddress").stringValue());
            destPort_ = par("destPort");

            vim = check_and_cast<VirtualisationInfrastructureManager*>(getParentModule()->getSubmodule("vim"));
            processingTimer_  = new cMessage("computeMsg");
        }
    }
}

void CbrResponder::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {
        socket.sendTo(respPacket_, destAddress_, destPort_);
        return;
    }
    else
    {
        Packet* pPacket = check_and_cast<Packet*>(msg);
        auto cbrHeader = pPacket->popAtFront<CbrRequest>();

        numReceived_++;
        totFrames_ = cbrHeader->getNframes(); // XXX this value can be written just once
        int pktSize = (int)cbrHeader->getPayloadSize();

        // just to make sure we do not update recvBytes AND we avoid dividing by 0
        if( simTime() > getSimulation()->getWarmupPeriod() )
        {
            recvBytes_ += pktSize;
            emit( cbrReqReceivedBytesSignal_ , pktSize );
        }

        simtime_t delay = simTime()-cbrHeader->getPayloadTimestamp();
        emit(cbrReqFrameDelaySignal_,delay );

        EV << "CbrResponder::handleMessage - Packet received: FRAME[" << cbrHeader->getIDframe() << "/" << cbrHeader->getNframes() << "] with delay["<< delay << "]" << endl;

        emit(cbrReqRcvdPkt_, (long)cbrHeader->getIDframe());


        // generate reply
        simtime_t processingTime = 0;
        if( par("enableVimComputing").boolValue() )
        {
            int numInstructions = par("serviceComplexity").intValue() * 1000000;
            processingTime = vim->calculateProcessingTime(-1, numInstructions) ;
            EV << "CbrResponder::handleMessage - requesting a processing time of " << processingTime << " seconds for " << numInstructions << " instructions" <<endl;
        }


        respPacket_ = new Packet("CBR");
        auto cbr = makeShared<CbrResponse>();
        cbr->setNframes(totFrames_);
        cbr->setIDframe((int)cbrHeader->getIDframe());
        cbr->setPayloadTimestamp(cbrHeader->getPayloadTimestamp());
        cbr->setPayloadSize(respSize_);
        cbr->setChunkLength(B(respSize_));
        //cbr->addTag<CreationTimeTag>()->setCreationTime(simTime());
        respPacket_->insertAtBack(cbr);

        EV << "CbrResponder::handleMessage - scheduling response in " << processingTime << " seconds." << endl;
        scheduleAfter(processingTime, processingTimer_);

        delete msg;
    }
}

void CbrResponder::finish()
{
    double lossRate = 0;
    if(totFrames_ > 0)
        lossRate = 1.0-(numReceived_/(totFrames_*1.0));

    emit(cbrReqFrameLossSignal_,lossRate);

    simtime_t elapsedTime = simTime() - getSimulation()->getWarmupPeriod();
    emit( cbrReqReceivedThroughtput_, recvBytes_ / elapsedTime.dbl() );
}



