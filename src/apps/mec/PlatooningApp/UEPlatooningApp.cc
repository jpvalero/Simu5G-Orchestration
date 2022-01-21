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

#include "apps/mec/PlatooningApp/UEPlatooningApp.h"
#include "apps/mec/PlatooningApp/packets/PlatooningPacket_m.h"
#include "apps/mec/PlatooningApp/PlatooningUtils.h"
#include "apps/mec/DeviceApp/DeviceAppMessages/DeviceAppPacket_m.h"
#include "apps/mec/DeviceApp/DeviceAppMessages/DeviceAppPacket_Types.h"

#include "inet/common/TimeTag_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

#include <fstream>

using namespace inet;
using namespace std;

Define_Module(UEPlatooningApp);

UEPlatooningApp::UEPlatooningApp()
{
    selfStart_ = nullptr;
    selfStop_ = nullptr;
    status_ = NOT_JOINED;
}

UEPlatooningApp::~UEPlatooningApp()
{
    cancelAndDelete(selfStart_);
    cancelAndDelete(selfStop_);
}

void UEPlatooningApp::initialize(int stage)
{
    EV << "UEPlatooningApp::initialize - stage " << stage << endl;
    cSimpleModule::initialize(stage);
    // avoid multiple initializations
    if (stage!=inet::INITSTAGE_APPLICATION_LAYER)
        return;

    //retrieve parameters
    joinRequestPacketSize_ = par("joinRequestPacketSize");
    leaveRequestPacketSize_ = par("leaveRequestPacketSize");
    controllerIndex_ = par("controllerIndex");
    if (controllerIndex_ >= 1000)
        throw cRuntimeError("UEPlatooningApp::initialize - controller index %d is not valid. Indexes >= 1000 are reserved", controllerIndex_);

    localPort_ = par("localPort");
    deviceAppPort_ = par("deviceAppPort");

    char* deviceAppAddressStr = (char*)par("deviceAppAddress").stringValue();
    deviceAppAddress_ = inet::L3AddressResolver().resolve(deviceAppAddressStr);

    //binding socket
    socket.setOutputGate(gate("socketOut"));
    socket.bind(localPort_);

    int tos = par("tos");
    if (tos != -1)
        socket.setTos(tos);

    //retrieving car cModule
    ue = this->getParentModule();

    //retrieving mobility module
    cModule *temp = getParentModule()->getSubmodule("mobility");
    if(temp != NULL)
    {
        // if the mobility module is other than LinearAccelerationMobility, an exception is thrown here
        mobility = check_and_cast<LinearAccelerationMobility*>(temp);
    }
    else
    {
        throw cRuntimeError("UEPlatooningApp::initialize - \tWARNING: Mobility module NOT FOUND!");
    }

    mecAppName = par("mecAppName").stringValue();

    //initializing the auto-scheduling messages
    selfStart_ = new cMessage("selfStart");
    selfStop_ = new cMessage("selfStop");

    //starting UEPlatooningApp
    simtime_t startTime = par("startTime");
    EV << "UEPlatooningApp::initialize - starting sendStartMEWarningAlertApp() in " << startTime << " seconds " << endl;
    scheduleAt(simTime() + startTime, selfStart_);

    //testing
    EV << "UEPlatooningApp::initialize - binding to port: local:" << localPort_ << " , dest:" << deviceAppPort_ << endl;
}

void UEPlatooningApp::handleMessage(cMessage *msg)
{
    EV << "UEPlatooningApp::handleMessage" << endl;
    // Sender Side
    if (msg->isSelfMessage())
    {
        if(!strcmp(msg->getName(), "selfStart"))
            sendStartMECPlatooningApp();
        else if(!strcmp(msg->getName(), "selfStop"))
            sendStopMECPlatooningApp();
        else if (!strcmp(msg->getName(), "joinTimer"))
        {
            sendJoinPlatoonRequest();             // send request for joining a platoon
            delete msg;
        }
        else if (!strcmp(msg->getName(), "leaveTimer"))
        {
            sendLeavePlatoonRequest();             // send request for leaving a platoon
            delete msg;
        }
        else
            throw cRuntimeError("UEPlatooningApp::handleMessage - \tWARNING: Unrecognized self message");
    }
    // Receiver Side
    else
    {
        inet::Packet* packet = check_and_cast<inet::Packet*>(msg);

        inet::L3Address ipAdd = packet->getTag<L3AddressInd>()->getSrcAddress();

        /*
         * From Device app
         * device app usually runs in the UE (loopback), but it could also run in other places
         */
        if(ipAdd == deviceAppAddress_ || ipAdd == inet::L3Address("127.0.0.1")) // dev app
        {
            auto mePkt = packet->peekAtFront<DeviceAppPacket>();

            if (mePkt == 0)
                throw cRuntimeError("UEPlatooningApp::handleMessage - \tFATAL! Error when casting to DeviceAppPacket");

            if( !strcmp(mePkt->getType(), ACK_START_MECAPP) )
                handleAckStartMECPlatooningApp(msg);
            else if(!strcmp(mePkt->getType(), ACK_STOP_MECAPP))
                handleAckStopMECPlatooningApp(msg);
            else
                throw cRuntimeError("UEPlatooningApp::handleMessage - \tFATAL! Error, DeviceAppPacket type %s not recognized", mePkt->getType());
        }
        // From MEC application
        else
        {
            auto mePkt = packet->peekAtFront<PlatooningAppPacket>();
            if (mePkt == 0)
                throw cRuntimeError("UEPlatooningApp::handleMessage - \tFATAL! Error when casting to PlatooningAppPacket");

            if(!strcmp(mePkt->getType(), JOIN_RESPONSE))
                recvJoinPlatoonResponse(msg);
            else if(!strcmp(mePkt->getType(), LEAVE_RESPONSE))
                recvLeavePlatoonResponse(msg);
            else if(!strcmp(mePkt->getType(), PLATOON_CMD))
                recvPlatoonCommand(msg);
            else
                throw cRuntimeError("UEPlatooningApp::handleMessage - \tFATAL! Error, PlatooningAppPacket type %s not recognized", mePkt->getType());
        }
        delete msg;
    }
}

void UEPlatooningApp::finish()
{
}


void UEPlatooningApp::sendStartMECPlatooningApp()
{
    EV << "UEPlatooningApp::sendStartMECPlatooningApp - Sending " << START_MEAPP <<" type PlatooningPacket\n";

    inet::Packet* packet = new inet::Packet("PlatooningAppStart");
    auto start = inet::makeShared<DeviceAppStartPacket>();

    //instantiation requirements and info
    start->setType(START_MECAPP);
    start->setMecAppName(mecAppName.c_str());
    start->setChunkLength(inet::B(2+mecAppName.size()+1));
    start->addTagIfAbsent<inet::CreationTimeTag>()->setCreationTime(simTime());
    packet->insertAtBack(start);

    socket.sendTo(packet, deviceAppAddress_, deviceAppPort_);

    //rescheduling
    scheduleAt(simTime() + 0.5, selfStart_);
}

void UEPlatooningApp::sendStopMECPlatooningApp()
{
    EV << "UEPlatooningApp::sendStopMECPlatooningApp - Sending " << STOP_MEAPP <<" type PlatooningPacket\n";

    inet::Packet* packet = new inet::Packet("DeviceAppStopPacket");
    auto stop = inet::makeShared<DeviceAppStopPacket>();

    //termination requirements and info
    stop->setType(STOP_MECAPP);
    stop->setChunkLength(inet::B(10));
    stop->addTagIfAbsent<inet::CreationTimeTag>()->setCreationTime(simTime());
    packet->insertAtBack(stop);

    socket.sendTo(packet, deviceAppAddress_, deviceAppPort_);

    //rescheduling
    if(selfStop_->isScheduled())
        cancelEvent(selfStop_);
    scheduleAt(simTime() + 0.5, selfStop_);
}

void UEPlatooningApp::handleAckStartMECPlatooningApp(cMessage* msg)
{
    EV << "UEPlatooningApp::handleAckStartMECPlatooningApp - Received Start ACK packet" << endl;

    inet::Packet* packet = check_and_cast<inet::Packet*>(msg);
    auto pkt = packet->peekAtFront<DeviceAppStartAckPacket>();

    if(pkt->getResult() == true)
    {
        mecAppAddress_ = L3AddressResolver().resolve(pkt->getIpAddress());
        mecAppPort_ = pkt->getPort();
        EV << "UEPlatooningApp::handleAckStartMECPlatooningApp - Received " << pkt->getType() << " type PlatooningPacket. mecApp instance is at: "<< mecAppAddress_<< ":" << mecAppPort_ << endl;
        cancelEvent(selfStart_);
        //scheduling sendStopMEWarningAlertApp()
        if(!selfStop_->isScheduled()){
            simtime_t  stopTime = par("stopTime");
            scheduleAt(simTime() + stopTime, selfStop_);
            EV << "UEPlatooningApp::handleAckStartMECPlatooningApp - Starting sendStopMECPlatooningApp() in " << stopTime << " seconds " << endl;
        }
    }
    else
    {
        EV << "UEPlatooningApp::handleAckStartMECPlatooningApp - MEC application cannot be instantiated! Reason: " << pkt->getReason() << endl;
    }

    // schedule a join request after joinTime seconds
    cMessage* joinTimer = new cMessage("joinTimer");
    double joinTime = par("joinTime");
    if (joinTime == -1)
        scheduleAt(simTime(), joinTimer);
    else
        scheduleAt(simTime() + joinTime, joinTimer);

    // schedule a leave request after leaveTime seconds
    cMessage* leaveTimer = new cMessage("leaveTimer");
    double leaveTime = par("leaveTime");
    if (leaveTime == -1)
        scheduleAt(simTime() + par("stopTime") - 1.0, leaveTimer);  // TODO check this
    else
        scheduleAt(simTime() + leaveTime, leaveTimer);
}


void UEPlatooningApp::sendJoinPlatoonRequest()
{
    if (status_ == JOINED)
    {
        EV << "UEPlatooningApp::sendJoinPlatoonRequest() - UE is already member of a platoon. Duplicate join request will not be sent" << endl;
        return;
    }

    // send request to join a platoon to the MEC application

    inet::Packet* pkt = new inet::Packet("PlatooningJoinPacket");
    auto joinReq = inet::makeShared<PlatooningJoinPacket>();
    joinReq->setType(JOIN_REQUEST);
    joinReq->setControllerIndex(controllerIndex_);
    joinReq->setDirection(mobility->getDirection());
    joinReq->setLastPosition(mobility->getCurrentPosition());   // TODO this should be retrieved by the MecPlatooningConsumerApp via the Location Service
    joinReq->setChunkLength(inet::B(joinRequestPacketSize_));
    joinReq->addTagIfAbsent<inet::CreationTimeTag>()->setCreationTime(simTime());
    pkt->insertAtBack(joinReq);

    socket.sendTo(pkt, mecAppAddress_ , mecAppPort_);
    EV << "UEPlatooningApp::sendJoinPlatoonRequest() - Join request sent to the MEC app" << endl;
}

void UEPlatooningApp::sendLeavePlatoonRequest()
{
    if (status_ == NOT_JOINED)
    {
        EV << "UEPlatooningApp::sendLeavePlatoonRequest() - UE is not member of any platoon. Leave request will not be sent" << endl;
        return;
    }

    // send request to leave a platoon to the MEC application

    inet::Packet* pkt = new inet::Packet("PlatooningLeavePacket");
    auto leaveReq = inet::makeShared<PlatooningLeavePacket>();
    leaveReq->setType(LEAVE_REQUEST);
    leaveReq->setChunkLength(inet::B(leaveRequestPacketSize_));
    leaveReq->addTagIfAbsent<inet::CreationTimeTag>()->setCreationTime(simTime());
    pkt->insertAtBack(leaveReq);

    socket.sendTo(pkt, mecAppAddress_ , mecAppPort_);
    EV << "UEPlatooningApp::sendLeavePlatoonRequest() - Leave request sent to the MEC app" << endl;
}


void UEPlatooningApp::recvJoinPlatoonResponse(cMessage* msg)
{
    EV << "UEPlatooningApp::recvJoinPlatoonResponse() - Join response received from the MEC app" << endl;

    // handle response to a previous join request from the MEC application

    inet::Packet* packet = check_and_cast<inet::Packet*>(msg);
    auto joinResp = packet->peekAtFront<PlatooningJoinPacket>();

    bool resp = joinResp->getResponse();

    // TODO store some local information about the platoon (is this needed? the UE may be unaware of the platoon)
    if (resp)
    {
        status_ = JOINED;

        //updating runtime color of the icon
        ue->getDisplayString().setTagArg("i",1, joinResp->getColor());

        EV << "UEPlatooningApp::recvJoinPlatoonResponse() - Join request accepted" << endl;
    }
    else
    {
        EV << "UEPlatooningApp::recvJoinPlatoonResponse() - Join request rejected" << endl;

    }
}

void UEPlatooningApp::recvLeavePlatoonResponse(cMessage* msg)
{
    EV << "UEPlatooningApp::recvLeavePlatoonResponse() - Leave response received from the MEC app" << endl;

    // handle response to a previous leave request from the MEC application

    inet::Packet* packet = check_and_cast<inet::Packet*>(msg);
    auto leaveResp = packet->peekAtFront<PlatooningLeavePacket>();

    bool resp = leaveResp->getResponse();

    // TODO update some local information about the platoon (is this needed? the UE may be unaware of the platoon)
    if (resp)
    {
        status_ = NOT_JOINED;

        //updating runtime color of the icon
        ue->getDisplayString().setTagArg("i",1, "white");

        EV << "UEPlatooningApp::recvLeavePlatoonResponse() - Leave request accepted" << endl;
    }
    else
    {
        EV << "UEPlatooningApp::recvLeavePlatoonResponse() - Leave request rejected" << endl;

    }
}



void UEPlatooningApp::recvPlatoonCommand(cMessage* msg)
{
    inet::Packet* packet = check_and_cast<inet::Packet*>(msg);
    auto cmd = packet->peekAtFront<PlatooningInfoPacket>();

    EV << "UEPlatooningApp::recvPlatoonCommand - Received " << cmd->getType() << " type PlatooningPacket"<< endl;

    // obtain command from the controller and apply it to the mobility module
    double newAcceleration = cmd->getNewAcceleration();
    mobility->setAcceleration(newAcceleration);

    EV << "UEPlatooningApp::recvPlatoonCommand - New acceleration value set to " << newAcceleration << " m/(s^2)"<< endl;
}
void UEPlatooningApp::handleAckStopMECPlatooningApp(cMessage* msg)
{
    EV << "UEPlatooningApp::handleAckStopMECPlatooningApp - Received Stop ACK packet" << endl;

    inet::Packet* packet = check_and_cast<inet::Packet*>(msg);
    auto pkt = packet->peekAtFront<DeviceAppStopAckPacket>();

    EV << "UEPlatooningApp::handleAckStopMECPlatooningApp - Received " << pkt->getType() << " type PlatooningPacket with result: "<< pkt->getResult() << endl;
    if(pkt->getResult() == false)
        EV << "Reason: "<< pkt->getReason() << endl;

    //updating runtime color of the icon
    ue->getDisplayString().setTagArg("i",1, "white");

    cancelEvent(selfStop_);
}