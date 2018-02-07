#pragma once
#include "common.h"

namespace iTease {
	template<typename... TArgs> class Event;

	class EventHandler {
	public:
		struct Base {
			virtual ~Base() { }
		};

	public:
		EventHandler() { }
		EventHandler(EventHandler&& other) {
			std::swap(m_ptr, other.m_ptr);
		}
		template <typename L>
		EventHandler(L&& l) : data(new L(std::move(l)))
		{ }

		EventHandler& operator=(const EventHandler& other) = delete;
		EventHandler& operator=(EventHandler&& other) {
			m_ptr.reset();
			std::swap(m_ptr, other.m_ptr);
			return *this;
		}
		template<typename L>
		EventHandler& operator=(L && l) {
			m_ptr.reset(new L(std::move(l)));
			return *this;
		}

		inline operator bool() { return bool(m_ptr); }

		template<typename H, typename... Args>
		void set_function(Event<Args...>& theEvent, H Handler) {
			m_ptr.reset(new typename Event<Args...>::Listener(theEvent, Handler));
		}

	private:
		std::unique_ptr<Base> m_ptr;
	};

	template<typename... TArgs>
	class Event : private std::shared_ptr<Event<TArgs...>*> {
	public:
		using Handler = std::function<int(TArgs...)>;

	private:
		using ListenerList = std::list<Handler>;
		using Iterator = typename ListenerList::iterator;

	public:
		class Listener : public EventHandler::Base {
			friend Event;
			Listener(const Event& ev, Handler f) {
				observe(ev, f);
			}

		public:
			Listener() { }
			Listener(Listener&& other) {
				m_event = other.m_event;
				m_it = other.m_it;
				other.m_event.reset();
			}
			~Listener() {
				reset();
			}
			Listener(const Listener& other) = delete;
			Listener& operator=(const Listener& other) = delete;
			Listener& operator=(Listener&& other) {
				reset();
				m_event = other.m_event;
				m_it = other.m_it;
				other.m_event.reset();
				return *this;
			}

			inline operator bool() const { return !m_event.expired(); }

			void observe(const Event& ev, Handler h) {
				reset();
				m_event = ev;
				m_it = ev.add_handler(h);
			}
			void reset() {
				if (!m_event.expired()) (*m_event.lock())->remove_handler(m_it);
				m_event.reset();
			}

		private:
			std::weak_ptr<Event*> m_event;
			typename ListenerList::iterator m_it;
		};

		friend Listener;
		Event() : shared_ptr<Event*>(std::make_shared<Event*>(this))
		{ }
		Event(const Event&) = delete;
		Event(Event&&) = default;
		Event& operator=(const Event&) = delete;
		Event& operator=(Event&&) = default;

		auto operator()(TArgs... args) const {
			return notify(std::forward<TArgs>(args)...);
		}

		auto connect(Handler h) const {
			return add_handler(h);
		}
		auto disconnect(Iterator it) const {
			remove_handler(it);
		}
		auto notify(TArgs&&... args) const {
			auto i = std::size_t{ 0 };
			for (auto& f : m_listeners) {
				if (f(std::forward<TArgs>(args)...))
					++i;
			}
			return i;
		}
		auto num_handlers() const {
			return m_listeners.size();
		}
		auto listen(Handler h) const {
			return Listener(*this, h);
		}

	private:
		auto add_handler(Handler fn) const {
			return m_listeners.insert(m_listeners.end(), fn);
		}
		auto remove_handler(Iterator it) const {
			m_listeners.erase(it);
		}

	private:
		mutable ListenerList m_listeners;
	};
}