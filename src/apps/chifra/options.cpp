/*-------------------------------------------------------------------------
 * This source code is confidential proprietary information which is
 * copyright (c) 2018, 2019 TrueBlocks, LLC (http://trueblocks.io)
 * All Rights Reserved
 *------------------------------------------------------------------------*/
/*
 * Parts of this file were generated with makeClass. Edit only those parts of the code
 * outside of the BEG_CODE/END_CODE sections
 */
#include "options.h"

//---------------------------------------------------------------------------------------------------
static const COption params[] = {
    // BEG_CODE_OPTIONS
    // clang-format off
    COption("commands", "", "list<enum[list|export|slurp|names|abi|state|tokens|when|data|blocks|transactions|receipts|logs|traces|quotes|scrape|status|settings|rm|message|leech|seed]>", OPT_REQUIRED | OPT_POSITIONAL, "which command to run"),  // NOLINT
    COption("sleep", "s", "<uint32>", OPT_FLAG, "for the 'scrape' and 'daemon' commands, the number of seconds chifra should sleep between runs (default 14)"),  // NOLINT
    COption("set", "e", "", OPT_HIDDEN | OPT_SWITCH, "for 'settings' only, indicates that this is a --set"),
    COption("start", "S", "<blknum>", OPT_HIDDEN | OPT_FLAG, "first block to process (inclusive)"),
    COption("end", "E", "<blknum>", OPT_HIDDEN | OPT_FLAG, "last block to process (inclusive)"),
    COption("", "", "", OPT_DESCRIPTION, "Main TrueBlocks command line controls."),
    // clang-format on
    // END_CODE_OPTIONS
};
static const size_t nParams = sizeof(params) / sizeof(COption);

extern bool visitIndexFiles(const string_q& path, void* data);
extern string_q addExportMode(format_t fmt);
//---------------------------------------------------------------------------------------------------
bool COptions::parseArguments(string_q& command) {
    ENTER("parseArguments");
    if (!standardOptions(command))
        EXIT_NOMSG(false);

    // BEG_CODE_LOCAL_INIT
    blknum_t start = 0;
    blknum_t end = NOPOS;
    // END_CODE_LOCAL_INIT

    bool tool_help = false;
    bool copy_to_tool = false;
    blknum_t latest = NOPOS;  // getLatestBlock_client();

    Init();
    explode(arguments, command, ' ');
    for (auto arg : arguments) {
        if (false) {
            // do nothing -- make auto code generation easier
            // BEG_CODE_AUTO
        } else if (startsWith(arg, "-s:") || startsWith(arg, "--sleep:")) {
            if (!confirmUint("sleep", sleep, arg))
                return false;

        } else if (startsWith(arg, "-S:") || startsWith(arg, "--start:")) {
            if (!confirmBlockNum("start", start, arg, latest))
                return false;

        } else if (startsWith(arg, "-E:") || startsWith(arg, "--end:")) {
            if (!confirmBlockNum("end", end, arg, latest))
                return false;

            // END_CODE_AUTO

        } else if (arg == "--set") {
            tool_flags += (arg + " ");
            copy_to_tool = true;

        } else if (arg == "-h" || arg == "--help") {
            if (mode.empty()) {
                optionOn(OPT_HELP);
                return usage();
            }
            setenv("PROG_NAME", ("chifra " + mode).c_str(), true);
            tool_help = true;

        } else if (mode.empty() && startsWith(arg, '-')) {
            if (!builtInCmd(arg))
                EXIT_USAGE("Missing mode: " + arg);

        } else {
            if (copy_to_tool) {
                tool_flags += (arg + '~');
                continue;
            }

            string descr = substitute(substitute(params[0].description, "[", "|"), "]", "|");
            if (isTestMode())
                descr += "where|blooms|";

            bool isStatus =
                (mode == "status" && (arg == "blocks" || arg == "transactions" || arg == "traces" || arg == "names"));
            if (!isStatus && contains(descr, "|" + arg + "|")) {
                if (!mode.empty())
                    EXIT_USAGE("Please specify " + params[0].description + ". " + mode + ":" + arg);
                mode = arg;

            } else if (contains(arg, ",")) {
                if (isAddress(arg.substr(0, 42))) {
                    if (mode == "list") {
                        CStringArray parts;
                        explode(parts, arg, ',');
                        arg = parts[0];
                        freshen_flags = substitute(parts[1], "=", ":");

                    } else if (!isAddress(arg)) {
                        EXIT_USAGE("Invalid address: " + arg);
                    }
                    addrs.push_back(toLower(arg));
                } else {
                    tool_flags += substitute(arg, ",", " ");
                }

            } else if (isAddress(arg) || arg == "--known" || arg == "--monitored") {
                addrs.push_back(toLower(arg));

            } else if (mode == "when") {
                addrs.push_back(toLower(arg));

            } else {
                static format_t fmt = TXT1;
                if (arg == "csv") {
                    fmt = CSV1;
                    tool_flags += (arg + " ");
                } else if (arg == "--to_file") {
                    if (getEnvStr("DOCKER_MODE").empty()) {
                        arg = "--output:" + configPath("cache/tmp/" + makeValidName(Now().Format(FMT_EXPORT)) +
                                                       (fmt == CSV1 ? ".csv" : ".txt"));
                        tool_flags += (arg + " ");
                    } else {
                        // ignore --to_file flag in docker mode
                    }

                } else {
                    if (arg == "--staging") {
                        freshen_flags += (arg + " ");

                    } else {
                        tool_flags += (arg + " ");
                    }
                }
            }
        }
    }

    scrapeSleep = (useconds_t)sleep;

    if (mode == "blocks" || mode == "transactions" || mode == "receipts" || mode == "names" || mode == "logs" ||
        mode == "traces" || mode == "state" || mode == "tokens" || mode == "message" || mode == "abi" ||
        mode == "when") {
        tool_flags += (" --" + mode);
        mode = "data";
    }

    if (mode.empty()) {
        optionOn(OPT_HELP);
        COption* option = (COption*)&params[0];
        option->description = "which command to run. See below...";
        EXIT_USAGE("Please specify " + params[0].description);
    }

    if (mode != "scrape") {
        establishFolder(getMonitorPath("", FM_PRODUCTION));
        establishFolder(getMonitorPath("", FM_STAGING));
    }

    if (tool_help) {
        tool_flags += " --help";
    }
    if (isNoHeader) {
        tool_flags += " --no_header";
    }
    if (expContext().asEther) {
        tool_flags += " --ether";
    }
    if (expContext().asDollars) {
        tool_flags += " --dollars";
    }
    if (expContext().isParity) {
        tool_flags += " --parity";
    }
    if (verbose) {
        tool_flags += " -v:" + uint_2_Str(verbose);
        freshen_flags += (" -v:" + uint_2_Str(verbose));
    }
    if (contains(command, "--start") && start != NOPOS) {
        tool_flags += " --start " + uint_2_Str(start);
        freshen_flags += " --start " + uint_2_Str(start);
    }
    if (contains(command, "--end") && end != NOPOS) {
        tool_flags += " --end " + uint_2_Str(end);
        freshen_flags += " --end " + uint_2_Str(end);
    }
    if (true) {
        tool_flags += addExportMode(expContext().exportFmt);
        freshen_flags += addExportMode(expContext().exportFmt);
    }
    if (true) {
        tool_flags = trim(tool_flags, ' ');
        freshen_flags = trim(freshen_flags, ' ');
    }

    if (mode == "scrape" && !isTestMode())
        LOG_INFO("Sleeping every ", scrapeSleep, " seconds.");

    EXIT_NOMSG(true);
}

//---------------------------------------------------------------------------------------------------
void COptions::Init(void) {
    registerOptions(nParams, params);
    optionOff(OPT_HELP);

    // BEG_CODE_INIT
    sleep = 14;
    // END_CODE_INIT

    addrs.clear();
    tool_flags = "";
    freshen_flags = "";
    mode = "";
    scrapeSleep = 14;
    minArgs = 0;
}

//---------------------------------------------------------------------------------------------------
COptions::COptions(void) {
    setSorts(GETRUNTIME_CLASS(CBlock), GETRUNTIME_CLASS(CTransaction), GETRUNTIME_CLASS(CReceipt));
    Init();
    // BEG_CODE_NOTES
    // clang-format off
    // clang-format on
    // END_CODE_NOTES

    // clang-format off
    notes.push_back(
        "Valid commands for chifa are noted here. Get additional help with `'chifra <cmd> --help'`.||"
                    "  list          list all appearances of an address anywhere on the chain.|"
                    "  export        export every appearance (as a transaciton, receipt, log, balance, etc.)|"
                    "  slurp         query EtherScan for a list of transactions (note: will be smaller than --list)|"
                    "  names         list all names known by TrueBlocks.|"
                    "  abi           list all abi signatures known by TrueBlocks.|"
                    "  state         query the state of an Ethereum address.|"
                    "  tokens        query the state of an ERC20 Ethereum address.|"
                    "  when          list known (named) blocks.|"
                    "  quotes        query for price data|"
                    "  scrape        scrape the blockchain and build the TrueBlocks address index (i.e. the digests).|"
                    "  status        query for various status reports about the system.|"
                    "  settings      get and set various system settings (API only).|"
                    "  rm            remove or pause a monitored address.|"
                    "  blocks        query the blockchain for block data|"
                    "  transactions  query the blockchain for transaction data|"
                    "  receipts      query the blockchain for receipt data|"
                    "  logs          query the blockchain for log data|"
                    "  traces        query the blockchain for trace data|"
                    "  leech         tbd|"
                    "  seed          tbd|"
    );
    // clang-format on

    // BEG_ERROR_MSG
    // END_ERROR_MSG
}

//--------------------------------------------------------------------------------
COptions::~COptions(void) {
}

//--------------------------------------------------------------------------------
string_q addExportMode(format_t fmt) {
    if (isApiMode() && fmt == API1)
        return "";
    if (!isApiMode() && fmt == TXT1)
        return "";
    switch (fmt) {
        case NONE1:
            return " --fmt none";
        case JSON1:
            return " --fmt json";
        case TXT1:
            return " --fmt txt";
        case CSV1:
            return " --fmt csv";
        case API1:
            return " --fmt api";
        default:
            break;
    }
    return "";
}
