/*-------------------------------------------------------------------------
 * This source code is confidential proprietary information which is
 * copyright (c) 2018, 2019 TrueBlocks, LLC (http://trueblocks.io)
 * All Rights Reserved
 *------------------------------------------------------------------------*/
#include "options.h"

//------------------------------------------------------------------------------------------------
bool COptions::handle_scrape(void) {
    ENTER("handle_" + mode);

    // scrape mode requires a running node
    nodeRequired();

    if (contains(tool_flags, "help")) {
        ostringstream os;
        os << "blockScrape --help";
        LOG_CALL(os.str());
        // clang-format off
        if (system(os.str().c_str())) {}  // Don't remove cruft. Silences compiler warnings
        // clang-format on
        EXIT_NOMSG(true);
    }

    // syntactic sugar
    tool_flags = substitute(substitute(" " + tool_flags, "--start", " start"), " start", "");
    bool daemonMode = false;

    // The presence of 'waitFile' will either pause or kill the scraper. If 'waitFile'
    // disappears and the scraper is paused, the scraper will resume
    string_q waitFile = configPath("cache/tmp/scraper-off.txt");
    bool wasPaused = fileExists(waitFile);
    if (wasPaused)
        LOG_INFO("The scraper is currently paused");

    if (contains(tool_flags, "restart")) {
        //---------------------------------------------------------------------------------
        // If a seperate instance is not running, we can't restart it
        if (nRunning("chifra scrape") < 2) {
            LOG_WARN("Scraper is not running. Cannot restart...");
            EXIT_NOMSG(true);
        }

        // If it's not paused, let the user know...
        if (!wasPaused) {
            LOG_WARN("Scraper is not paused. Cannot restart...");
            EXIT_NOMSG(true);
        }

        // A seperate instance is running, removing the file will restart the scraper
        ::remove(waitFile.c_str());
        LOG_INFO("Scraper will restart shortly...");
        EXIT_NOMSG(true);

    } else if (contains(tool_flags, "pause")) {
        //---------------------------------------------------------------------------------
        // If a seperate instance is not running, we can't pause it
        if (nRunning("chifra scrape") < 2) {
            LOG_WARN("Scraper is not running. Cannot pause...");
            EXIT_NOMSG(true);
        }

        // If it's already paused, let the user know...
        if (wasPaused) {
            LOG_WARN("Scraper is already paused...");
            EXIT_NOMSG(true);
        }

        // It's running, so we can pause it
        stringToAsciiFile(waitFile, Now().Format(FMT_EXPORT));
        LOG_INFO("Scraper will pause shortly...");
        EXIT_NOMSG(true);

    } else if (contains(tool_flags, "quit")) {
        //---------------------------------------------------------------------------------
        // If a seperate instance is not running, we can't kill it
        if (nRunning("chifra scrape") < 2) {
            LOG_WARN("Scraper is not running. Cannot quit...");
            EXIT_NOMSG(true);
        }

        // Kill it whether it's currently paused or not
        stringToAsciiFile(waitFile, "quit");
        LOG_INFO("Scraper will quit shortly...");
        EXIT_NOMSG(true);

    } else {
        //---------------------------------------------------------------------------------
        // If it's already running, don't start it again...
#ifdef MAC
        // TODO(tjayrush): fix this on non-mac machines
        if (nRunning("chifra scrape") > 1) {
            LOG_WARN("Scraper is already running. Cannot start it again...");
            EXIT_NOMSG(false);
        }
#endif

        // Extract options from the command line that we do not pass on to blockScrape...
        CStringArray optList;
        explode(optList, tool_flags, ' ');
        tool_flags = "";  // reset tool_flag
        for (auto opt : optList) {
            if (opt == "--daemon") {
                opt = "";
                daemonMode = true;

            } else if (!opt.empty() && !startsWith(opt, "-") && !isNumeral(opt)) {
                cerr << "Invalid command " << cYellow << opt << cOff << " to chifra scrape." << endl;
                EXIT_NOMSG(false);
            }
            if (!opt.empty())
                tool_flags += (opt + " ");
        }

        cerr << cYellow << "Scraper is starting with " << tool_flags << "..." << cOff << endl;
    }

    // Run forever...unless told to pause or stop (shouldQuit is true if control+C was hit.
    bool waitFileExists = fileExists(waitFile);
    size_t nRuns = 0;
    size_t maxRuns = (isTestMode() ? 1 : UINT64_MAX);
    while (nRuns++ < maxRuns && !shouldQuit()) {
        if (waitFileExists) {
            if (!wasPaused)
                cerr << cYellow << "\tScraper paused..." << cOff << endl;
            wasPaused = true;
            usleep(max(useconds_t(5), scrapeSleep) * 1000000);  // sleep for at least five seconds

        } else {
            if (wasPaused)
                cerr << cYellow << "\tScraper restarted..." << cOff << endl;
            wasPaused = false;
            ostringstream os;
            os << "blockScrape " << tool_flags;
            LOG_CALL(os.str());
            // clang-format off
            if (system(os.str().c_str())) {}  // Don't remove cruft. Silences compiler warnings
            // clang-format on

            // Catch the timestamp file up to the scraper
            freshenTimestamps(getLatestBlock_cache_ripe());

            // sometimes catch the monitors addresses up to the scraper
            if (!isTestMode() && daemonMode) {
                CStringArray files;
                listFilesInFolder(files, getMonitorPath("*.acct.bin"), false);
                CAddressArray monitors;
                if (files.size()) {
                    for (auto file : files) {
                        file = substitute(substitute(file, getMonitorPath(""), ""), ".acct.bin", "");
                        if (isAddress(file))
                            monitors.push_back(file);
                    }
                    CFreshenArray fa;
                    for (auto a : monitors)
                        fa.push_back(CFreshen(a));
                    freshen_internal(FM_PRODUCTION, fa, "--silent", freshen_flags);
                    for (auto f : fa) {
                        if (f.cntBefore != f.cntAfter) {
                            ostringstream os1;
                            os1 << "acctExport " << f.address << " --freshen";  // << " >/dev/null";
                            LOG_INFO("Calling: ", os1.str());
                            // clang-format off
                            if (system(os1.str().c_str())) {}  // Don't remove cruft. Silences compiler warnings
                            // clang-format on
                            usleep(250000);  // stay responsive to cntrl+C
                            if (shouldQuit())
                                continue;
                        }
                    }
                    cerr << "\t  freshening: " << cYellow << "    finished." << cOff
                         << "                                                           " << endl;
                }
            }

            if (!isTestMode())
                usleep(scrapeSleep == 0 ? 500000 : scrapeSleep * 1000000);  // stay responsive to cntrl+C
        }

        waitFileExists = fileExists(waitFile);
        if (waitFileExists) {
            if (asciiFileToString(waitFile) == "quit") {
                ::remove(waitFile.c_str());
                cerr << cYellow << "\tScraper quitting..." << cOff << endl;
                EXIT_NOMSG(true);
            }
        }
    }
    EXIT_NOMSG(true);
}
