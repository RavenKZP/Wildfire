#include "Events.h"

namespace Events {

    void RegisterForEvents() {
        auto holder = RE::ScriptEventSourceHolder::GetSingleton();
        holder->GetEventSource<RE::TESHitEvent>()->AddEventSink(HitSink::GetSingleton());
    }

    RE::BSEventNotifyControl HitSink::ProcessEvent(const RE::TESHitEvent* evn, RE::BSTEventSource<RE::TESHitEvent>*) {
        if (!evn) return RE::BSEventNotifyControl::kContinue;

        logger::info("Hit event received: cause: {} target: {} source: {:X} projectile: {:X}",
                     evn->cause->GetDisplayFullName(), evn->target->GetDisplayFullName(), evn->source, evn->projectile);
        return RE::BSEventNotifyControl::kContinue;
    }
}