#pragma once

#include "app-resources/resources.h"
#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceProvider.h>

class OfflineTracker FINAL : private wb::ResourceProvider, public wb::LaunchableModule
{
public:
    /** Name of this class. Used in StartupProvider list. */
    static const char* const LAUNCHABLE_NAME;
    OfflineTracker();
    ~OfflineTracker();

private:
    /** @see whiteboard::ILaunchableModule::initModule */
    virtual bool initModule() OVERRIDE;
    /** @see whiteboard::ILaunchableModule::deinitModule */
    virtual void deinitModule() OVERRIDE;
    /** @see whiteboard::ILaunchableModule::startModule */
    virtual bool startModule() OVERRIDE;
    /** @see whiteboard::ILaunchableModule::stopModule */
    virtual void stopModule() OVERRIDE;

    /** @see whiteboard::ResourceProvider::onGetRequest */
    virtual void onGetRequest(const wb::Request& request,
                              const wb::ParameterList& parameters) OVERRIDE;

    // /** @see whiteboard::ResourceProvider::onSubscribe */
    // virtual void onSubscribe(const wb::Request& request,
    //                          const wb::ParameterList& parameters) OVERRIDE;

    // /** @see whiteboard::ResourceProvider::onUnsubscribe */
    // virtual void onUnsubscribe(const wb::Request& request,
    //                            const wb::ParameterList& parameters) OVERRIDE;

private:

};
