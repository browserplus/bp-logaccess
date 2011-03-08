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

#include "logaccess_util.h"
#include "bp-file/bpfile.h"
#include "bpservice/bpserviceversion.h"
#include <list>

#ifdef WINDOWS
#include <atlpath.h>
#include <iostream>
#include <ShellApi.h>
#include <shlobj.h>
#include <sstream>
#include <windows.h>
#include <windows.h>
#include <ShellApi.h>
#elif defined(MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#endif

#ifdef MACOSX
// Make a string from a CFStringRef
static std::string
stringRefToUTF8(CFStringRef cfStr) {
    std::string rval;
    CFIndex cfLen = CFStringGetLength(cfStr);
    if (cfLen > 0) {
        char stackBuffer[2048];
        char* dynamicBuf = NULL;
        char* buf = stackBuffer;
        if ((size_t)(cfLen * 4) >= sizeof(stackBuffer)) {
            dynamicBuf = (char*)malloc(cfLen * 4 + 1);
            buf = dynamicBuf;
        }
        CFStringGetCString(cfStr, buf, cfLen * 4 + 1, kCFStringEncodingUTF8);
        rval.append(buf);
        if (dynamicBuf) {
            free(dynamicBuf);
        }
    }
    return rval;
}
#endif

#ifdef WINDOWS
static std::string PlatformVersion() {
    std::string version;
    char buf[1024];
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    ZeroMemory(&buf, sizeof(buf));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
            return std::string("unknown");
        }
    }
    snprintf(buf, sizeof(buf), "%lu.%lu.%lu", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    version.append(buf);
    return version;
}

static bool 
getCSIDL(boost::filesystem::path& path, int csidl) {
    wchar_t wcPath[MAX_PATH] = {0};
    HRESULT bStatus = SHGetFolderPathW(NULL,
                                       csidl | CSIDL_FLAG_CREATE,
                                       NULL,
                                       SHGFP_TYPE_DEFAULT,
                                       wcPath);
    if (bStatus != S_OK) {
        return false;
    }
    path = wcPath;
    return true;
}
#endif

// get a list paths pointing at current logfiles
std::string
logaccess::util::getLogfilePaths(bplus::List& paths) {
    // first we have to determine the path to logfiles, this is complicated
    // because different platforms have different restrictions where different
    // code running in different contexts can write files.  (namely activex
    // controls running under win7 and vista must write under a "LocalLow"
    // directory).  Further complexity comes from a couple factors:
    // 1. we're duplicating logic typically in the platform inside a service
    // 2. we don't know what the latest version of the platform that's installed
    //    is.
    // all that said, let's get to it
    // a. first let's find the user scoped "plugin writable" path.
    boost::filesystem::path pluginWriteDir;
#ifdef WINDOWS
    bool isVistaOrLater = (PlatformVersion().compare("6") >= 0);
    if (isVistaOrLater) {
        if (!getCSIDL(pluginWriteDir, CSIDL_LOCAL_APPDATA)) {
            return std::string("couldn't determine windows AppData directory");
        }
        pluginWriteDir.remove_filename();
        pluginWriteDir /= L"LocalLow";
    } else {
        if (!getCSIDL(pluginWriteDir, CSIDL_LOCAL_APPDATA)) {
            return std::string("couldn't determine winxp AppData directory");
        }
    }
#elif defined(MACOSX)
    // Get application support dir
    FSRef fref;
    OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &fref);
    if (err == noErr) {
        CFURLRef tmpUrl = CFURLCreateFromFSRef(kCFAllocatorSystemDefault, &fref);
        CFStringRef ctmpDir = CFURLCopyFileSystemPath(tmpUrl, kCFURLPOSIXPathStyle);
        pluginWriteDir = stringRefToUTF8(ctmpDir);
        CFRelease(ctmpDir);
        CFRelease(tmpUrl);
    } else {
        return std::string("couldn't find user scoped application support directory");        
    }
#else
#error "platforms not yet supported"
#endif
    if (pluginWriteDir.empty()) {
        return std::string("couldn't determine plugin writable directory");
    }
    // append Yahoo!/BrowserPlus
    pluginWriteDir /= boost::filesystem::path("Yahoo!")/boost::filesystem::path("BrowserPlus");
    if (!bp::file::isDirectory(pluginWriteDir)) {
        return std::string("logfile directory does not exist!");
    }
    // b. now we must figure out the latest version of the platform that is installed.
    //    first we'll do a non-recursive iteration to find the dirs that exist that are
    //    well formed versions, and we'll follow up with a pass to find BrowserPlusCore.log
    //    inside those directories.  In case there's multiple such log files, the parent
    //    of the newest created logfile wins.  If BrowserPlusCore.log does not exist (like
    //    perhaps this service is first to run since BrowserPlus has been installed/reinstalled)
    //    we'll use the directory of the newest created BrowserPlus.config file.
    std::list<boost::filesystem::path> versionDirs;
    try {
        boost::filesystem::directory_iterator end;
        for (boost::filesystem::directory_iterator it(pluginWriteDir); it != end; ++it) {
            bplus::service::Version v;
            if (v.parse(it->path().filename().string())) {
                versionDirs.push_back(it->path());
            }
        }
    } catch (const boost::filesystem::filesystem_error&) {
        return std::string("unable to iterate thru plugin writable dir");
    }
    // c. now that we've got the platform directories, we have to find the appropriate
    //    child path to the installation id directory where logfiles are stored
    boost::filesystem::path logDir;
    boost::filesystem::path logDir_backup;
    std::time_t lastWrite = 0;
    for (std::list<boost::filesystem::path>::const_iterator it = versionDirs.begin(); it != versionDirs.end(); it++) {
        try {
            boost::filesystem::recursive_directory_iterator end;
            for (boost::filesystem::recursive_directory_iterator rdit(*it); rdit != end; ++rdit) {
                if (rdit->path().filename() == "BrowserPlusCore.log") {
                    try {
                        std::time_t curWrite = boost::filesystem::last_write_time(*rdit);
                        if (curWrite > lastWrite) {
                            curWrite = lastWrite;
                            logDir = rdit->path();
                            logDir.remove_filename();
                        }
                    } catch (const boost::filesystem::filesystem_error& e) {
                        // error reading timestamp of this file!  if no other
                        // BrowserPlusCore.log files have been found, we'll
                        // assume this is our guy.
                        if (logDir.empty()) {
                            logDir = rdit->path();                        
                            logDir.remove_filename();
                        }
                    }
                }
                else if (rdit->path().filename() == "BrowserPlus.config") {
                    try {
                        std::time_t curWrite = boost::filesystem::last_write_time(*rdit);
                        if (curWrite > lastWrite) {
                            curWrite = lastWrite;
                            logDir_backup = rdit->path();
                            logDir_backup.remove_filename();
                        }
                    } catch (const boost::filesystem::filesystem_error& e) {
                        // error reading timestamp of this file!  if no other
                        // BrowserPlus.config files have been found, we'll
                        // assume this is our guy.
                        if (logDir_backup.empty()) {
                            logDir_backup = rdit->path();                        
                            logDir_backup.remove_filename();
                        }
                    }
                }
            }
        } catch (const boost::filesystem::filesystem_error& e) {
            return std::string("unable to iterate thru platform version dir");
        }
    }
    if (logDir.empty() && !logDir_backup.empty()) {
        logDir = logDir_backup;
    }
    if (logDir.empty()) {
        return std::string("unable to find current log directory");        
    }
    // d. now we've got what we're reasonably sure is the current logfile directory, lets'
    //    add all .log files to the output parameter
    try {
        boost::filesystem::directory_iterator end;
        for (boost::filesystem::directory_iterator it(logDir); it != end; ++it) {
            if (bp::file::isRegularFile(it->path())) {
                if (it->path().extension().string() == ".log") {
                    paths.append(new bplus::Path(bp::file::nativeUtf8String(it->path())));
                }
            }
        }
    } catch (const boost::filesystem::filesystem_error& e) {
        return std::string("unable to iterate thru plugin writable dir");
    }
    // success!
    return std::string();
}

std::string
logaccess::util::getServiceLogfilePaths(const std::string& service, bplus::List& paths) {
    boost::filesystem::path coreletDataDir;
#ifdef WINDOWS
    bool isVistaOrLater = (PlatformVersion().compare("6") >= 0);
    if (isVistaOrLater) {
        if (!getCSIDL(coreletDataDir, CSIDL_LOCAL_APPDATA)) {
            return std::string("couldn't determine windows AppData directory");
        }
    } else {
        if (!getCSIDL(coreletDataDir, CSIDL_LOCAL_APPDATA)) {
            return std::string("couldn't determine winxp AppData directory");
        }
    }
#elif defined(MACOSX)
    // Get application support dir
    FSRef fref;
    OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &fref);
    if (err == noErr) {
        CFURLRef tmpUrl = CFURLCreateFromFSRef(kCFAllocatorSystemDefault, &fref);
        CFStringRef ctmpDir = CFURLCopyFileSystemPath(tmpUrl, kCFURLPOSIXPathStyle);
        coreletDataDir = stringRefToUTF8(ctmpDir);
        CFRelease(ctmpDir);
        CFRelease(tmpUrl);
    } else {
        return std::string("couldn't find user scoped application support directory");        
    }
#else
#error "platforms not yet supported"
#endif
    if (coreletDataDir.empty()) {
        return std::string("couldn't determine application support directory");
    }
    // append Yahoo!/BrowserPlus/CoreletData/<service>
    coreletDataDir /= boost::filesystem::path("Yahoo!/BrowserPlus/CoreletData") / service;
    if (!bp::file::isDirectory(coreletDataDir)) {
        return std::string("");
    }
    // Now find the latest major version with a log file.
    //    first we'll do a non-recursive iteration to find the dirs that exist that are
    //    well formed major versions, and we'll follow up with a pass to find *.log
    //    inside those directories.  In case there's multiple such log files, the parent
    //    of the newest created logfile wins
    std::list<boost::filesystem::path> versionDirs;
    try {
        boost::filesystem::directory_iterator end;
        for (boost::filesystem::directory_iterator it(coreletDataDir); it != end; ++it) {
            bplus::service::Version v;
            if (v.parse(it->path().filename().string()) && v.majorVer() != -1 && v.minorVer() == -1 && v.microVer() == -1) {
                versionDirs.push_back(it->path());
            }
        }
    } catch (const boost::filesystem::filesystem_error& e) {
        return std::string("unable to iterate thru CoreletData dir for service ") + service;
    }
    // c. now that we've got the version directories, find the one with the most recent
    //    log file
    boost::filesystem::path logDir;
    std::time_t lastWrite = 0;
    for (std::list<boost::filesystem::path>::const_iterator it = versionDirs.begin(); it != versionDirs.end(); it++) {
        try {
            boost::filesystem::recursive_directory_iterator end;
            for (boost::filesystem::recursive_directory_iterator rdit(*it); rdit != end; ++rdit) {
                if (rdit->path().extension().string() == ".log") {
                    try {
                        std::time_t curWrite = boost::filesystem::last_write_time(*rdit);
                        if (curWrite > lastWrite) {
                            curWrite = lastWrite;
                            logDir = rdit->path();
                            logDir.remove_filename();
                        }
                    } catch (const boost::filesystem::filesystem_error& e) {
                        // error reading timestamp of this file!  if no other
                        // .log files have been found, we'll
                        // assume this is our guy.
                        if (logDir.empty()) {
                            logDir = rdit->path();                        
                            logDir.remove_filename();
                        }
                    }
                }
            }
        } catch (const boost::filesystem::filesystem_error& e) {
            return std::string("unable to iterate thru service version dir");
        }
    }
    if (logDir.empty()) {
        return std::string("unable to find current log directory");        
    }
    // d. now we've got what we're reasonably sure is the current logfile directory, lets'
    //    add all .log files to the output parameter
    try {
        boost::filesystem::directory_iterator end;
        for (boost::filesystem::directory_iterator it(logDir); it != end; ++it) {
            if (bp::file::isRegularFile(it->path())) {
                if (it->path().extension().string() == ".log") {
                    paths.append(new bplus::Path(bp::file::nativeUtf8String(it->path())));
                }
            }
        }
    } catch (const boost::filesystem::filesystem_error& e) {
        return std::string("unable to iterate thru service data dir");
    }
    // success!
    return std::string();
}

