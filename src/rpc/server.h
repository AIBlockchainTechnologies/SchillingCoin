// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_RPC_SERVER_H
#define SCHILLINGCOIN_RPC_SERVER_H

#include <rpc/request.h>
#include <rpc/util.h>

#include <functional>
#include <map>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <univalue.h>

class CRPCCommand;

namespace RPCServer
{
void OnStarted(std::function<void()> slot);
void OnStopped(std::function<void()> slot);
}

/** Query whether RPC is running */
bool IsRPCRunning();

/** Throw JSONRPCError if RPC is not running */
void RpcInterruptionPoint();

/**
 * Set the RPC warmup status. When this is done, all RPC calls will error out
 * immediately with RPC_IN_WARMUP.
 */
void SetRPCWarmupStatus(const std::string& new_status);

/** Mark warmup as done. RPC calls will be processed from now on. */
void SetRPCWarmupFinished();

/** Return the current warmup state. */
bool RPCIsInWarmup(std::string* out_status);

/** Opaque base class for timers returned by NewTimer. */
class RPCTimerBase
{
public:
    virtual ~RPCTimerBase() = default;
};

/** RPC timer driver. */
class RPCTimerInterface
{
public:
    virtual ~RPCTimerInterface() = default;

    /** Implementation name. */
    virtual const char* Name() = 0;

    /**
     * Create a timer that calls func after millis milliseconds.
     */
    virtual RPCTimerBase* NewTimer(std::function<void()>& func, int64_t millis) = 0;
};

/** Set the timer interface. */
void RPCSetTimerInterface(RPCTimerInterface* iface);

/** Set the timer interface only if it is currently unset. */
void RPCSetTimerInterfaceIfUnset(RPCTimerInterface* iface);

/** Unset the timer interface. */
void RPCUnsetTimerInterface(RPCTimerInterface* iface);

/**
 * Run func n_seconds from now.
 * Overrides a previous timer with the same name.
 */
void RPCRunLater(const std::string& name, std::function<void()> func, int64_t n_seconds);

using RpcMethodFnType = RPCHelpMan (*)();

class CRPCCommand
{
public:
    /**
     * RPC method handler reading a request and assigning a result.
     *
     * Return true when the request is fully handled. Return false when the
     * request should be passed to a subsequent handler.
     */
    using Actor = std::function<bool(
        const JSONRPCRequest& request,
        UniValue& result,
        bool last_handler)>;

    /** Constructor supporting multiple handlers for the same RPC method. */
    CRPCCommand(
        std::string category,
        std::string name,
        Actor actor,
        std::vector<std::string> args,
        intptr_t unique_id)
        : category(std::move(category)),
          name(std::move(name)),
          actor(std::move(actor)),
          argNames(std::move(args)),
          unique_id(unique_id)
    {
    }

    /** Simplified constructor accepting an RPCHelpMan factory. */
    CRPCCommand(std::string category, RpcMethodFnType fn)
        : CRPCCommand(
              std::move(category),
              fn().m_name,
              [fn](const JSONRPCRequest& request, UniValue& result, bool) {
                  result = fn().HandleRequest(request);
                  return true;
              },
              fn().GetArgNames(),
              intptr_t(fn))
    {
    }

    std::string category;
    std::string name;
    Actor actor;
    std::vector<std::string> argNames;
    intptr_t unique_id;
};

/** RPC command dispatcher. */
class CRPCTable
{
private:
    std::map<std::string, std::vector<const CRPCCommand*>> mapCommands;

public:
    CRPCTable();

    std::string help(const std::string& name, const JSONRPCRequest& help_request) const;

    /**
     * Execute an RPC method.
     *
     * @param request The JSONRPCRequest to execute.
     * @return Result of the RPC call.
     */
    UniValue execute(const JSONRPCRequest& request) const;

    /** Return the registered command names. */
    std::vector<std::string> listCommands() const;

    /**
     * Return named arguments that the client must convert from strings to
     * other JSON types.
     */
    UniValue dumpArgMap(const JSONRPCRequest& request) const;

    /**
     * Add a command to the dispatch table.
     *
     * Commands with different names but the same unique_id are aliases.
     * RPC commands must be added before the RPC server starts.
     */
    void appendCommand(const std::string& name, const CRPCCommand* command);

    /** Remove a command from the dispatch table. */
    bool removeCommand(const std::string& name, const CRPCCommand* command);
};

bool IsDeprecatedRPCEnabled(const std::string& method);

extern CRPCTable tableRPC;

void StartRPC();
void InterruptRPC();
void StopRPC();

std::string JSONRPCExecBatch(const JSONRPCRequest& request, const UniValue& requests);

#endif // SCHILLINGCOIN_RPC_SERVER_H