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

#include "bpservice/bpservice.h"
#include "bputil/bpurl.h"
#include "logaccess_util.h"

// our service
class LogAccess : public bplus::service::Service {
public:
    BP_SERVICE(LogAccess);
    LogAccess() : bplus::service::Service() {}
    ~LogAccess() {}
public:
    void get(const bplus::service::Transaction& tran, const bplus::Map& args);
    void getServiceLogs(const bplus::service::Transaction& tran, const bplus::Map& args);
};

BP_SERVICE_DESC(LogAccess, "LogAccess", "1.3.0",
                "Lets you get file handles for BrowserPlus log files "
                "from a webpage.")
ADD_BP_METHOD(LogAccess, get,
              "Returns a list in \"files\" of filehandles associated "
              "with BrowserPlus logfiles.")
ADD_BP_METHOD(LogAccess, getServiceLogs,
              "Returns a list in \"files\" of filehandles associated "
              "with BrowserPlus service logfiles.")
ADD_BP_METHOD_ARG(getServiceLogs, "services", List, true,
                  "A list of service names whose logs are fetched.")
END_BP_SERVICE_DESC

static bool
checkWhitelist(const std::string& sUrl) {
    bplus::url::Url pUrl;
    if (!pUrl.parse(sUrl)) {
        return false;
    }
    if (pUrl.scheme() != "http" && pUrl.scheme() != "https") {
        return false;
    }
    std::vector<std::string> whitelist;
    whitelist.push_back("yahoo.com");    
    whitelist.push_back("browserplus.org");
    whitelist.push_back("browserpl.us");
    whitelist.push_back("localhost");
    for (std::vector<std::string>::const_iterator i = whitelist.begin(); i != whitelist.end(); i++) {
        if (i->length() > pUrl.host().length()) {
            continue;
        }
        // if the hostname is larger than the whitelist entry it must
        // have a separator (so suckbrowserplus.org isn't whitelisted
        // for browserplus.org entry)
        if (i->length() < pUrl.host().length() &&
            '.' != pUrl.host()[pUrl.host().length() - i->length() - 1]) {
            continue;
        }
        if (!pUrl.host().substr(pUrl.host().length() - i->length(),
                                i->length()).compare(*i)) {
            return true;
        }
    }
    return false;
}

void
LogAccess::get(const bplus::service::Transaction& tran, const bplus::Map& args) {
    if (!checkWhitelist(clientUri()) ) {
        tran.error("bp.permissionDenied", NULL);
        return;
    }
    bplus::List paths;
    std::string error = logaccess::util::getLogfilePaths(paths);
    if (!error.empty()) {
        tran.error("bp.couldntGetLogs", error.c_str());
        return;
    }
    tran.complete(paths);
}

void
LogAccess::getServiceLogs(const bplus::service::Transaction& tran, const bplus::Map& args) {
    if (!checkWhitelist(clientUri()) ) {
        tran.error("bp.permissionDenied", NULL);
        return;
    }
    const bplus::List* serviceList = NULL;
    if (!args.getList("services", serviceList)) {
        tran.error("bp.couldntGetLogs", "required services parameter missing");
        return;
    }
    bplus::List paths;
    for (unsigned int i = 0; i < serviceList->size(); i++) {
        const bplus::String* s = dynamic_cast<const bplus::String*>(serviceList->value(i));
        if (s) {
            std::string error = logaccess::util::getServiceLogfilePaths(s->value(), paths);
            if (!error.empty()) {
                tran.error("bp.couldntGetLogs", error.c_str());
                return;
            }
        }
    }
    tran.complete(paths);
}
