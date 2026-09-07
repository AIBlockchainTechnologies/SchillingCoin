// Copyright (c) 2015 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HTTPSERVER_H
#define BITCOIN_HTTPSERVER_H

#include <string>
#include <stdint.h>
#include <functional>

#include <deque>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <utility>
#include <exception>
#include <cassert>

#include "util.h" // Ensure logging macros (LogPrintf / LogPrint) are visible to templates

static const int DEFAULT_HTTP_THREADS = 4;
static const int DEFAULT_HTTP_WORKQUEUE = 16;
static const int DEFAULT_HTTP_SERVER_TIMEOUT = 30;

struct evhttp_request;
struct event_base;
struct event;
struct timeval;
class CService;
class HTTPRequest;

/**
 * WorkQueue template used by the HTTP server.
 *
 * - Template definitions must be header-visible to avoid two-phase lookup issues.
 * - Uses std::unique_ptr for ownership of work items to avoid manual new/delete.
 * - Uses explicit member names (m_ prefix) for clarity and to avoid dependent-name lookup pitfalls.
 * - Uses condition_variable::wait with a predicate to avoid spurious-wake races.
 *
 * API contract:
 * - Enqueue(std::unique_ptr<T>&) accepts a unique_ptr to a type T that derives from WorkItem.
 *   Ownership is transferred into the queue only on success; on failure the caller retains ownership.
 * - Enqueue(std::unique_ptr<WorkItem>) accepts a moved unique_ptr<WorkItem> and transfers ownership on success.
 *
 * Notes:
 * - This class is non-copyable and non-movable.
 * - Enqueue overloads are exception-safe: ownership is transferred into a temporary unique_ptr<WorkItem>
 *   before pushing into the container so that if push_back throws the pointer is still managed.
 */
template <typename WorkItem>
class WorkQueue
{
private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::deque<std::unique_ptr<WorkItem>> m_queue;
    bool m_running;
    size_t m_maxDepth;
    int m_numThreads;

    // Non-copyable, non-movable
    WorkQueue(const WorkQueue&) = delete;
    WorkQueue& operator=(const WorkQueue&) = delete;
    WorkQueue(WorkQueue&&) = delete;
    WorkQueue& operator=(WorkQueue&&) = delete;

    class ThreadCounter {
    public:
        WorkQueue &wq;
        ThreadCounter(WorkQueue &w): wq(w) {
            std::lock_guard<std::mutex> lock(wq.m_mutex);
            ++wq.m_numThreads;
        }
        ~ThreadCounter() {
            std::lock_guard<std::mutex> lock(wq.m_mutex);
            --wq.m_numThreads;
            wq.m_cond.notify_all();
        }
    };

public:
    explicit WorkQueue(size_t maxDepth)
        : m_mutex(), m_cond(), m_queue(), m_running(true), m_maxDepth(maxDepth), m_numThreads(0) {}
    ~WorkQueue() {
        Interrupt();
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.clear();
    }

    /**
     * Enqueue overload for derived work item types.
     * Accepts a reference to std::unique_ptr<T> where T must derive from WorkItem.
     * On success ownership is transferred (item becomes null). On failure item remains owned by caller.
     */
    template <typename T>
    bool Enqueue(std::unique_ptr<T> &item) {
        static_assert(std::is_base_of<WorkItem, T>::value, "T must derive from WorkItem");
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.size() >= m_maxDepth) {
            return false; // caller retains ownership
        }
        // Exception-safe transfer: create a temporary unique_ptr<WorkItem> that will clean up if push_back throws.
        std::unique_ptr<WorkItem> tmp(static_cast<WorkItem*>(item.release()));
        m_queue.push_back(std::move(tmp));
        m_cond.notify_one();
        return true;
    }

    /**
     * Enqueue overload that accepts a unique_ptr<WorkItem> by value (move).
     * On failure the passed-in unique_ptr will be destroyed by the caller's context (it is moved).
     */
    bool Enqueue(std::unique_ptr<WorkItem> item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.size() >= m_maxDepth) return false;
        m_queue.push_back(std::move(item));
        m_cond.notify_one();
        return true;
    }

    // Worker thread main loop
    void Run() {
        ThreadCounter count(*this);
        while (true) {
            std::unique_ptr<WorkItem> item;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cond.wait(lock, [this]{ return !m_running || !m_queue.empty(); });
                if (!m_running && m_queue.empty()) break;
                item = std::move(m_queue.front());
                m_queue.pop_front();
            }
            if (item) {
                try {
                    (*item)();
                } catch (const std::exception& e) {
                    // util.h provides LogPrintf; include it above so this compiles at template instantiation sites.
                    LogPrintf("WorkQueue worker caught exception: %s\n", e.what());
                } catch (...) {
                    LogPrintf("WorkQueue worker caught unknown exception\n");
                }
            }
        }
    }

    void Interrupt() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_running = false;
        m_cond.notify_all();
    }

    void WaitExit() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this]{ return m_numThreads == 0; });
    }

    size_t Depth() const noexcept {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
};

/// HTTP server API declarations follow (unchanged)
bool InitHTTPServer();
bool StartHTTPServer();
void InterruptHTTPServer();
void StopHTTPServer();

typedef std::function<void(HTTPRequest* req, const std::string &)> HTTPRequestHandler;
void RegisterHTTPHandler(const std::string &prefix, bool exactMatch, const HTTPRequestHandler &handler);
void UnregisterHTTPHandler(const std::string &prefix, bool exactMatch);

struct event_base* EventBase();

class HTTPRequest
{
private:
    struct evhttp_request* req;
    bool replySent;

public:
    HTTPRequest(struct evhttp_request* req);
    ~HTTPRequest();

    enum RequestMethod {
        UNKNOWN,
        GET,
        POST,
        HEAD,
        PUT
    };

    std::string GetURI();
    CService GetPeer();
    RequestMethod GetRequestMethod();
    std::pair<bool, std::string> GetHeader(const std::string& hdr);
    std::string ReadBody();
    void WriteHeader(const std::string& hdr, const std::string& value);
    void WriteReply(int nStatus, const std::string& strReply = "");
};

class HTTPClosure
{
public:
    virtual void operator()() = 0;
    virtual ~HTTPClosure() {}
};

class HTTPEvent
{
public:
    HTTPEvent(struct event_base* base, bool deleteWhenTriggered, const std::function<void(void)>& handler);
    ~HTTPEvent();
    void trigger(struct timeval* tv);

    bool deleteWhenTriggered;
    std::function<void(void)> handler;
private:
    struct event* ev;
};

#endif // BITCOIN_HTTPSERVER_H