#include "movesense.h"

#include "OfflineTracker.hpp"
#include "common/core/debug.h"
#include "common/core/dbgassert.h"

#include "app-resources/resources.h"
#include "DebugLogger.hpp"

const char* const OfflineTracker::LAUNCHABLE_NAME = "OfflineTracker";

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_HELLO::LID,
};

OfflineTracker::OfflineTracker():
    ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_RES::LOCAL::OFFLINE_HELLO::EXECUTION_CONTEXT),
    LaunchableModule(LAUNCHABLE_NAME, WB_RES::LOCAL::OFFLINE_HELLO::EXECUTION_CONTEXT)
{
}

OfflineTracker::~OfflineTracker()
{
}

bool OfflineTracker::initModule()
{
    if (registerProviderResources(sProviderResources) != wb::HTTP_CODE_OK)
    {
        return false;
    }

    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void OfflineTracker::deinitModule()
{
    unregisterProviderResources(sProviderResources);
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool OfflineTracker::startModule()
{
    mModuleState = WB_RES::ModuleStateValues::STARTED;
    return true;
}

void OfflineTracker::stopModule()
{
    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineTracker::onGetRequest(const wb::Request& request,
                                     const wb::ParameterList& parameters)
{
    DEBUGLOG("OfflineTracker::onGetRequest() called.");

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_HELLO::LID:
    {
        WB_RES::HelloValue hello;
        hello.greeting = "Hei hei!";
        DebugLogger::info(hello.greeting);
        returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty, hello);
        break;
    }

    default:
        ASSERT(0); // would be a system error if we got here, trust the system and save rom.
    }
}

// void HelloWorldService::onSubscribe(const wb::Request& request,
//                                     const wb::ParameterList& parameters)
// {
//     switch (request.getResourceId().localResourceId)
//     {
//     case WB_RES::LOCAL::SAMPLE_HELLOWORLD::LID:
//     {
//         // Someone subscribed to our service. Start timer that greets once every 5 seconds
//         if (mTimer == wb::ID_INVALID_TIMER)
//         {
//             mTimer = startTimer(HELLO_WORLD_PERIOD_MS, true);
//         }

//         returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty);
//         break;
//     }
//     default:
//         ASSERT(0); // would be a system error if we got here, trust the system and save rom.
//     }
// }

// void HelloWorldService::onUnsubscribe(const wb::Request& request,
//                                       const wb::ParameterList& parameters)
// {
//     switch (request.getResourceId().localResourceId)
//     {
//     case WB_RES::LOCAL::SAMPLE_HELLOWORLD::LID:
//         returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty);
//         break;

//     default:
//         ASSERT(0); // would be a system error if we got here, trust the system and save rom.
//     }
// }
