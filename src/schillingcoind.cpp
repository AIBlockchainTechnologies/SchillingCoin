// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/schillingcoin-config.h>
#endif

#include <chainparams.h>
#include <clientversion.h>
#include <common/init.h>
#include <compat/compat.h>
#include <init.h>
#include <interfaces/chain.h>
#include <interfaces/init.h>
#include <node/context.h>
#include <node/interface_ui.h>
#include <noui.h>
#include <shutdown.h>
#include <stacktraces.h>
#include <util/check.h>
#include <util/exception.h>
#include <util/strencodings.h>
#include <util/syserror.h>
#include <util/system.h>
#include <util/threadnames.h>
#include <util/tokenpipe.h>
#include <util/translation.h>

#include <cstdio>
#include <functional>

using node::NodeContext;

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

#if HAVE_DECL_FORK
/**
 * Custom implementation of daemon(). This implements the same order of
 * operations as glibc. Opens a pipe to the child process to allow the parent
 * process to wait for initialization status.
 *
 * @returns 0 if successful, and in child process.
 *          >0 if successful, and in parent process.
 *          -1 in case of error, in parent process.
 */
int fork_daemon(bool nochdir, bool noclose, TokenPipeEnd& endpoint)
{
    std::optional<TokenPipe> umbilical = TokenPipe::Make();
    if (!umbilical) {
        return -1;
    }

    int pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid != 0) {
        endpoint = umbilical->TakeReadEnd();
        umbilical->TakeWriteEnd().Close();

        int status = endpoint.TokenRead();
        if (status != 0) {
            endpoint.Close();
            return -1;
        }

        return pid;
    }

    endpoint = umbilical->TakeWriteEnd();
    umbilical->TakeReadEnd().Close();

#if HAVE_DECL_SETSID
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }
#endif

    if (!nochdir) {
        if (chdir("/") != 0) {
            exit(EXIT_FAILURE);
        }
    }

    if (!noclose) {
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            bool error =
                dup2(fd, STDIN_FILENO) < 0 ||
                dup2(fd, STDOUT_FILENO) < 0 ||
                dup2(fd, STDERR_FILENO) < 0;

            if (fd > STDERR_FILENO) {
                close(fd);
            }

            if (error) {
                exit(EXIT_FAILURE);
            }
        } else {
            exit(EXIT_FAILURE);
        }
    }

    endpoint.TokenWrite(0);
    return 0;
}
#endif // HAVE_DECL_FORK

static bool ParseArgs(ArgsManager& args, int argc, char* argv[])
{
    SetupServerArgs(args);

    std::string error;
    if (!args.ParseParameters(argc, argv, error)) {
        return InitError(
            Untranslated(strprintf(
                "Error parsing command line arguments: %s",
                error)));
    }

    if (args.IsArgSet("-printcrashinfo")) {
        return true;
    }

    if (auto config_error = common::InitConfig(args)) {
        return InitError(config_error->message, config_error->details);
    }

    for (int i = 1; i < argc; ++i) {
        if (!IsSwitchChar(argv[i][0])) {
            return InitError(
                Untranslated(strprintf(
                    "Command line contains unexpected token '%s', "
                    "see schillingcoind -h for a list of options.",
                    argv[i])));
        }
    }

    return true;
}

static bool ProcessInitCommands(ArgsManager& args)
{
    if (args.IsArgSet("-printcrashinfo")) {
        std::cout
            << GetCrashInfoStrFromSerializedStr(
                   args.GetArg("-printcrashinfo", ""))
            << std::endl;
        return true;
    }

    if (HelpRequested(args) || args.IsArgSet("-version")) {
        std::string usage =
            CLIENT_NAME + " version " + FormatFullVersion() + "\n";

        if (args.IsArgSet("-version")) {
            usage += FormatParagraph(LicenseInfo());
        } else {
            usage +=
                "\nUsage:  schillingcoind [options]"
                "                     Start " +
                CLIENT_NAME +
                "\n\n";
            usage += args.GetHelpMessage();
        }

        tfm::format(std::cout, "%s", usage);
        return true;
    }

    return false;
}

static bool AppInit(NodeContext& node)
{
    bool success{false};
    ArgsManager& args = *Assert(node.args);

#if HAVE_DECL_FORK
    /*
     * Communication with the parent process after daemonization.
     *
     * A token signals whether initialization succeeded. An unexpected end of
     * stream is interpreted by the parent as failure to start.
     */
    TokenPipeEnd daemon_endpoint;
#endif

    try {
        args.SoftSetBoolArg("-server", true);

        InitLogging(args);
        InitParameterInteraction(args);

        if (!AppInitBasicSetup(args, node.exit_status)) {
            return false;
        }

        if (!AppInitParameterInteraction(args)) {
            return false;
        }

        node.kernel = std::make_unique<kernel::Context>();

        if (!AppInitSanityChecks(*node.kernel)) {
            return false;
        }

        if (args.GetBoolArg("-daemon", DEFAULT_DAEMON) ||
            args.GetBoolArg("-daemonwait", DEFAULT_DAEMONWAIT)) {
#if HAVE_DECL_FORK
            tfm::format(std::cout, "%s starting\n", CLIENT_NAME);

            switch (fork_daemon(1, 0, daemon_endpoint)) {
            case 0:
                /*
                 * Child process. If -daemonwait is disabled, report successful
                 * daemonization immediately instead of waiting for complete
                 * node initialization.
                 */
                if (!args.GetBoolArg("-daemonwait", DEFAULT_DAEMONWAIT)) {
                    daemon_endpoint.TokenWrite(1);
                    daemon_endpoint.Close();
                }
                break;

            case -1:
                return InitError(
                    Untranslated(strprintf(
                        "fork_daemon() failed: %s",
                        SysErrorString(errno))));

            default: {
                int token = daemon_endpoint.TokenRead();
                if (token) {
                    exit(EXIT_SUCCESS);
                }

                tfm::format(
                    std::cerr,
                    "Error during initialization - "
                    "check debug.log for details\n");
                exit(EXIT_FAILURE);
            }
            }
#else
            return InitError(
                Untranslated(
                    "-daemon is not supported on this operating system"));
#endif // HAVE_DECL_FORK
        }

        if (!AppInitLockDataDirectory()) {
            return false;
        }

        success = AppInitInterfaces(node) && AppInitMain(node);
    } catch (const std::exception& exception) {
        PrintExceptionContinue(&exception, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInit()");
    }

#if HAVE_DECL_FORK
    if (daemon_endpoint.IsOpen()) {
        daemon_endpoint.TokenWrite(success);
        daemon_endpoint.Close();
    }
#endif

    return success;
}

MAIN_FUNCTION
{
    RegisterPrettyTerminateHander();
    RegisterPrettySignalHandlers();

#ifdef WIN32
    util::WinCmdLineArgs win_args;
    std::tie(argc, argv) = win_args.get();
#endif

    NodeContext node;
    int exit_status{EXIT_FAILURE};

    std::unique_ptr<interfaces::Init> init =
        interfaces::MakeNodeInit(node, argc, argv, exit_status);

    if (!init) {
        return exit_status;
    }

    SetupEnvironment();

    noui_connect();
    util::ThreadSetInternalName("init");

    ArgsManager& args = *Assert(node.args);

    if (!ParseArgs(args, argc, argv)) {
        return EXIT_FAILURE;
    }

    if (ProcessInitCommands(args)) {
        return EXIT_SUCCESS;
    }

    if (AppInit(node)) {
        WaitForShutdown();
    } else {
        node.exit_status = EXIT_FAILURE;
    }

    Interrupt(node);
    Shutdown(node);

    return node.exit_status;
}