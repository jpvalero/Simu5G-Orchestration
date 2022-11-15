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

#include "CbrRequester.h"

#include <cmath>
#include <inet/common/TimeTag_m.h>

#define round(x) floor((x) + 0.5)

Define_Module(CbrRequester);
using namespace inet;
using namespace std;

simsignal_t CbrRequester::cbrReqGeneratedThroughtputSignal_ = registerSignal("cbrReqGeneratedThroughtputSignal");
simsignal_t CbrRequester::cbrReqGeneratedBytesSignal_ = registerSignal("cbrReqGeneratedBytesSignal");
simsignal_t CbrRequester::cbrReqSentPktSignal_ = registerSignal("cbrReqSentPktSignal");
simsignal_t CbrRequester::cbrReqRoundTripSignal_ = registerSignal("cbrReqRoundTripSignal");


CbrRequester::CbrRequester()
{
    initialized_ = false;
    selfSource_ = nullptr;
    selfSender_ = nullptr;
}

CbrRequester::~CbrRequester()
{
    cancelAndDelete(selfSource_);
}

void CbrRequester::initialize(int stage)
{

    cSimpleModule::initialize(stage);
    EV << "CBR Sender initialize: stage " << stage << " - initialize=" << initialized_ << endl;

    if (stage == INITSTAGE_LOCAL)
    {
        selfSource_ = new cMessage("selfSource");
        nframes_ = 0;
        nframesTmp_ = 0;
        iDframe_ = 0;
        timestamp_ = 0;
        size_ = par("PacketSize");
        sampling_time = par("sampling_time");
        localPort_ = par("localPort");
        destPort_ = par("destPort");

        txBytes_ = 0;

    }
    else if (stage == INITSTAGE_APPLICATION_LAYER)
    {
        // calculating traffic starting time
        startTime_ = par("startTime");
        finishTime_ = par("finishTime");

        EV << " finish time " << finishTime_ << endl;
        nframes_ = (finishTime_ - startTime_) / sampling_time;

        initTraffic_ = new cMessage("initTraffic");
        initTraffic();
    }
}

void CbrRequester::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {
        if (!strcmp(msg->getName(), "selfSource"))
        {
            EV << "CbrRequester::handleMessage - now[" << simTime() << "] <= finish[" << finishTime_ <<"]" <<endl;
            if( simTime() <= finishTime_ || finishTime_ == 0 )
                sendCbrRequest();
        }
        else
            initTraffic();
    }
    else // receiving response
    {
        handleResponse(msg);
    }
}

void CbrRequester::initTraffic()
{
    std::string destAddress = par("destAddress").stringValue();
    cModule* destModule = findModuleByPath(par("destAddress").stringValue());
    if (destModule == nullptr)
    {
        // this might happen when users are created dynamically
        EV << simTime() << "CbrRequester::initTraffic - destination " << destAddress << " not found" << endl;

        simtime_t offset = 0.01; // TODO check value
        scheduleAt(simTime()+offset, initTraffic_);
        EV << simTime() << "CbrRequester::initTraffic - the node will retry to initialize traffic in " << offset << " seconds " << endl;
    }
    else
    {
        delete initTraffic_;
        std::cout << par("destAddress").stringValue() << std::endl;
        destAddress_ = inet::L3AddressResolver().resolve(par("destAddress").stringValue());

        // set primary and secondary address
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localPort_);

        int tos = par("tos");
        if (tos != -1)
            socket.setTos(tos);

        EV << simTime() << "CbrRequester::initialize - binding to port: local:" << localPort_ << " , dest: " << destAddress_.str() << ":" << destPort_ << endl;

        // calculating traffic starting time
        simtime_t startTime = par("startTime");

        scheduleAt(simTime()+startTime, selfSource_);
        EV << "\t starting traffic in " << startTime << " seconds " << endl;
    }
}

void CbrRequester::sendCbrRequest()
{
    // ============== DEMO GM
    // check if gNB has changed
    // if yes, switch dest address
    // destAddress_ = inet::L3AddressResolver().resolve(par("secondaryDestAddress").stringValue());

    Packet* packet = new Packet("CBR");
    auto cbr = makeShared<CbrRequest>();
    cbr->setNframes(nframes_);
    cbr->setIDframe(iDframe_++);
    cbr->setPayloadTimestamp(simTime());
    cbr->setPayloadSize(size_);
    cbr->setChunkLength(B(size_));
    cbr->addTag<CreationTimeTag>()->setCreationTime(simTime());
    packet->insertAtBack(cbr);

    emit(cbrReqGeneratedBytesSignal_,size_);

    if( simTime() > getSimulation()->getWarmupPeriod() )
    {
        txBytes_ += size_;
    }
    socket.sendTo(packet, destAddress_, destPort_);

    scheduleAt(simTime() + sampling_time, selfSource_);
}


void CbrRequester::handleResponse(cMessage *msg)
{
    Packet* pPacket = check_and_cast<Packet*>(msg);
    auto cbrHeader = pPacket->popAtFront<CbrResponse>();

    simtime_t rtt = simTime()-cbrHeader->getPayloadTimestamp();
    emit(cbrReqRoundTripSignal_,rtt );

    EV << "CbrRequester::handleMessage - response received after " << rtt << " seconds." << endl;

    delete msg;
}

void CbrRequester::finish()
{
    simtime_t elapsedTime = simTime() - getSimulation()->getWarmupPeriod();
    emit( cbrReqGeneratedThroughtputSignal_, txBytes_ / elapsedTime.dbl() );
}
