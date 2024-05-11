#pragma once
#include <deque>
#include <mutex>
#include <variant>

enum class DHCP6ExporterState {
    Fail = 0,

};

class EventItem {};

class EventQueue {
  public:
    EventQueue()                  = default;
    EventQueue(const EventQueue&) = delete;

    void pushEvent(const EventItem& event);

    void pushEvent(EventItem&& event);

    EventItem popEvent();

  private:
    std::deque<EventItem> m_items;
    std::mutex            m_queueMutex;
};
