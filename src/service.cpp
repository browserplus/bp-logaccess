/**
 * ***** BEGIN LICENSE BLOCK *****
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is BrowserPlus (tm).
 * 
 * The Initial Developer of the Original Code is Yahoo!.
 * Portions created by Yahoo! are Copyright (C) 2006-2010 Yahoo!.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 * ***** END LICENSE BLOCK ***** */

#include "la_util.hh"
#include "bpservice/bpservice.h"
#include "bpservice/bpcallback.h"
#include "bp-file/bpfile.h"
#include "bputil/bpurl.h"

#include <map>

#if defined(WIN32)
#include <windows.h>
#endif

using namespace std;
using namespace bp::file;
using namespace bplus::service;
namespace bfs = boost::filesystem;

// our service
class LogAccess : public Service
{
public:
    BP_SERVICE(LogAccess);
    
    LogAccess() : Service() {
    }
    ~LogAccess() {
    }

    void get(const Transaction& tran, const bplus::Map& args);
};

BP_SERVICE_DESC(LogAccess, "LogAccess", "1.0.0",
                "Lets you get file handles for BrowserPlus log files "
                "from a webpage.")

ADD_BP_METHOD(LogAccess, get,
              "Returns a list in \"files\" of filehandles associated "
              "with BrowserPlus logfiles.")

END_BP_SERVICE_DESC


static bool checkWhitelist(const std::string & sUrl) 
{
    bplus::url::Url pUrl;
    if (!pUrl.parse(sUrl)) {
        return false;
    }
    
    if (pUrl.scheme() != "http" && pUrl.scheme() != "https") {
        return false;
    }
    std::vector<std::string> whitelist;
    whitelist.push_back("browserplus.org");
    whitelist.push_back("browserpl.us");
    whitelist.push_back("yahoo.com");    

    for (unsigned int i = 0; i < whitelist.size(); i++) {
        if (whitelist[i].length() > pUrl.host().length()) {
            continue;
        }

        // if the hostname is larger than the whitelist entry it must
        // have a separator (so suckbrowserplus.org isn't whitelisted
        // for browserplus.org entry)
        if (whitelist[i].length() < pUrl.host().length() &&
            '.' != pUrl.host()[pUrl.host().length() - whitelist[i].length() - 1])
        {
            continue;
        }
        
        if (!pUrl.host().substr(pUrl.host().length() - whitelist[i].length(),
                                whitelist[i].length()).compare(whitelist[i]))
        {
            return true;
        }
    }

    return false;
}


void
LogAccess::get(const Transaction& tran, 
                const bplus::Map& args)
{
    std::string error;
    bplus::List paths;

    if (!checkWhitelist(context("uri")) ) {
        tran.error("bp.permissionDenied", NULL);
    } else {
        error = la::util::getLogfilePaths(paths);

        if (!error.empty()) {
            tran.error("bp.couldntGetLogs", error.c_str());
        } else {
            tran.complete(paths);
        }
    }
}
