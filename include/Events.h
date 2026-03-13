#pragma once


namespace Events {

    void RegisterForEvents();

    class HitSink : public RE::BSTEventSink<RE::TESHitEvent> {
    public:
        static HitSink* GetSingleton() {
            static HitSink sink;
            return &sink;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* evn,
                                              RE::BSTEventSource<RE::TESHitEvent>*) override;
    };

}