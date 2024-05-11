#include "event_queue.hpp"

void EventQueue::pushEvent(const EventItem& event) { std::unique_lock lk(m_queueMutex); 
    m_items.push_front(event);
}

void EventQueue::pushEvent(EventItem&& event) { std::unique_lock lk(m_queueMutex); }

EventItem EventQueue::popEvent() { std::unique_lock lk(m_queueMutex); }
