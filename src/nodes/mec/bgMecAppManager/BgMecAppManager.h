/*
 * bgAppManager.h
 *
 *  Created on: Jul 15, 2022
 *      Author: linofex
 */

#ifndef NODES_MEC_BGMECAPPMANAGER_BGMECAPPMANAGER_H_
#define NODES_MEC_BGMECAPPMANAGER_BGMECAPPMANAGER_H_

#include "omnetpp.h"
#include <map>
#include <list>

#include "nodes/mec/utils/MecCommon.h"

// forward declaration
class CbrRequester;
class CbrResponder;

typedef enum
{
    START_TIMER = 0,
    STOP_TIMER = 1
} TimerState;
typedef enum
{
    CREATE = 0,
    DELETE = 1
} Mode;

typedef enum {
    DUMMY_ORCHESTRATION = 0,
    EXTERNAL_ORCHESTRATION = 1,
    UNKNOWN_ORCHESTRATION = 2
}OrchestrationType;

typedef enum {
    PRIMARY = 0,
    SECONDARY = 1
}CurrentResponder;

typedef struct
{
    int id;
    omnetpp::cModule* mecHost;
    omnetpp::cModule* bgMecApp;
    omnetpp::cModule* bgUe;
    double startTime;
    double stopTime;
    double centerX;
    double centerY;
    double radius;
    ResourceDescriptor resources;
    omnetpp::cMessage* timer;
} BgMecAppDescriptor;

typedef struct
{
    double snapShotTime; // the real snapshot if present in file, or the next calulated snapshot
    int numMecApps;
} Snapshot;

class BgMecAppManager : public omnetpp::cSimpleModule {

    private:
        std::map<int, BgMecAppDescriptor> bgMecApps_;
        int lastMecHostActivated_; // maintans the index of mecHosts_ vector of the last mec host
        std::vector<cModule*> mecHosts_;
        std::vector<cModule*> runningMecHosts_;

        omnetpp::cOutVector bgAppsVector_;

        CbrRequester* orchestratedApp_;
        CbrResponder* orchestratedResponder_;

        bool fromTraceFile_;
        std::list<Snapshot> snapshotList_;
        double snapshotPeriod_;
        omnetpp::cMessage* snapMsg_;

        bool admissionControl_;

        double defaultRam_;
        double defaultDisk_;
        double defaultCpu_;

        omnetpp::cMessage* deltaMsg_;
        double deltaTime_;

        // maximum and minimum load of a MEC host from the perspective of the orchestrator
        int maxBgMecApp_;
        int minBgMecApp_;
        int currentBgMecApps_;
        Mode mode_;

        // the function is called at the beginning of a snapshot to create or destroy background applications and keep only "numApps" active. Applications are deployed uniformly across active MEC hosts.
        // It can also be called periodically to balance the load among MEC hosts
        void updateBgMecAppsLoad(int numApps);

        // handles periodic calls of the updateBgMecAppsLoad function
        bool enablePeriodicLoadBalancing_;
        omnetpp::simtime_t balancingInterval_;
        omnetpp::cMessage* balancingTimer_;

        // used to manage the updates in the "updateBgMecAppsLoad" function and avoid unnecessary reconfiguration
        int lastBalancedApps_;
        int lastBalancedHosts_;

        OrchestrationType orchestrationType_;

        // handles the activation time of MEC hosts. Deactivation is instead instantaneous.
        bool enableHostActivationDelay_;
        double mecHostActivationTime_;


        // orchestration functions
        void doOrchestration( int numApps );
        void dummyOrchestration( int numApps );
        void externalOrchestration( int numApps );
        void changeServingEdge( int action );


    public:
        BgMecAppManager();
        ~BgMecAppManager();

        int numInitStages() const { return inet::NUM_INIT_STAGES; }
        void initialize(int stage);
        virtual void handleMessage(omnetpp::cMessage *msg);

        void scheduleNextSnapshot();

        // this method calls createBgMecApp and createBgUe
        bool createBgModules(cModule* mecHost = nullptr);

        // Creates a new background MEC app having the specified id
        cModule * createBgMecApp(int id);
        cModule * createBgUE(int id);

        // relocate an existing application to the specified mec host
        bool relocateBgMecApp(int appId, cModule* mecHost);

        // this method calls deleteBgMecApp and deleteBgUe
        void deleteBgModules();
        void deleteBgMecApp(int id);
        void deleteBgUe(int id);

        void readMecHosts();
        // dummy method. It chooses the last started mecHost ALWYAS assuming it has enough resources
        cModule* chooseMecHost();

        // triggers the delayed activation of a MEC host
        void triggerMecHostActivation();

        void activateNewMecHost();
        void deactivateNewMecHost(cModule* module);
        void deactivateLastMecHost();
        bool isMecHostEmpty(cModule* module);

        void registerOrchestratedApp(CbrRequester* module){ orchestratedApp_ = module; };
        void registerOrchestratedResponder( CbrResponder * module){ orchestratedResponder_ = module; };
};


#endif /* NODES_MEC_BGMECAPPMANAGER_BGMECAPPMANAGER_H_ */
