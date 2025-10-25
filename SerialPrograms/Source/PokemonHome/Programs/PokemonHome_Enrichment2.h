/*  Program Name
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonHome_Enrichment2_H
#define PokemonAutomation_PokemonHome_Enrichment2_H

#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


class Enrichment2_Descriptor : public SingleSwitchProgramDescriptor{
public:
    Enrichment2_Descriptor();

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};



class Enrichment2 : public SingleSwitchProgramInstance{
public:
    Enrichment2();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:

private:

    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;


    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};




}
}
}
#endif
