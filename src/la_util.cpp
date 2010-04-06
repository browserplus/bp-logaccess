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
#include "bp-file/bpfile.h"
#include "bpservice/bpserviceversion.h"

#include <list>

#ifdef WINDOWS
#include <windows.h>
#elif defined(MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#endif

#ifdef MACOSX
// Make a string from a CFStringRef
//
static std::string
stringRefToUTF8(CFStringRef cfStr)
{
    std::string rval;
    CFIndex cfLen = CFStringGetLength(cfStr);

    if (cfLen > 0) {
        char stackBuffer[2048], *dynamicBuf = NULL;
        char * buf = stackBuffer;
        if ((size_t) (cfLen*4) >= sizeof(stackBuffer)) {
            dynamicBuf = (char*) malloc(cfLen*4 + 1);
            buf = dynamicBuf;
        }
        CFStringGetCString(cfStr, buf, cfLen*4 + 1,
                           kCFStringEncodingUTF8);
        rval.append(buf);
        if (dynamicBuf) free(dynamicBuf);
    }

    return rval;
}
#endif

// get a list paths pointing at current logfiles
std::string
la::util::getLogfilePaths(bplus::List & paths)
{
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
    bp::file::Path pluginWriteDir;
#ifdef WINDOWS
    bool isVistaOrLater = (osVersion.compare("6") >= 0);
    if (isVistaOrLater) {
        // TODO: using above requires Vista SDK, so we'll manually
        //       build up the LocalLow path (YIB-1623201)
        // (lth) isn't there a way we can make a runtime switch to use
        //       the vista call if present?
        pluginWriteDir = getCSIDL(CSIDL_LOCAL_APPDATA);
        pluginWriteDir.remove_filename();
        pluginWriteDir /= L"LocalLow";
    } else {
        pluginWriteDir = getCSIDL(CSIDL_LOCAL_APPDATA);
    }
#elif defined(MACOSX)
    // Get application support dir
    FSRef fref;
    OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType,
                             kCreateFolder, &fref);
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
    pluginWriteDir /= bp::file::Path("Yahoo!")/bp::file::Path("BrowserPlus");

    if (!bp::file::isDirectory(pluginWriteDir)) {
        return std::string("logfile directory does not exist!");
    }
    
    // b. now we must figure out the latest version of the platform that is installed.
    //    first we'll do a non-recursive iteration to find the dirs that exist that are
    //    well formed versions, and we'll follow up with a pass to find BrowserPlusCore.log
    //    inside those directories.  In case there's multiple such log files, the parent
    //    of the newest created logfile wins

    std::list<bp::file::Path> versionDirs;

    {
        bp::file::tDirIter di;
        try {
            bp::file::tDirIter end;
            for (bp::file::tDirIter it(pluginWriteDir); it != end; ++it) {
                bplus::service::Version v;
                if (v.parse(it->filename())) {
                    versionDirs.push_back(it->path());
                }
            }
        } catch (const bp::file::tFileSystemError& e) {
            return std::string("unable to iterate thru plugin writable dir");
        }
    }
    
    // c. now that we've got the platform directories, we have to find the appropriate
    //    child path to the installation id directory where logfiles are stored
    bp::file::Path logDir;
    std::time_t lastWrite = 0;

    std::list<bp::file::Path>::const_iterator it;
    for (it = versionDirs.begin(); it != versionDirs.end(); it++) {
        bp::file::tRecursiveDirIter di;
        try {
            bp::file::tRecursiveDirIter end;
            for (bp::file::tRecursiveDirIter rdit(*it); rdit != end; ++rdit) {
                
                if (!rdit->filename().compare("BrowserPlusCore.log")) {
                    std::time_t curWrite = boost::filesystem::last_write_time(*rdit);
                    if (curWrite > lastWrite)
                    {
                        curWrite = lastWrite;
                        logDir = rdit->path();
                        logDir.remove_filename();
                    }
                }
            }
        } catch (const bp::file::tFileSystemError& e) {
            return std::string("unable to iterate thru platform version dir");
        }
    }

    if (logDir.empty()) {
        return std::string("unable to find current log directory");        
    }

    // d. now we've got what we're reasonably sure is the current logfile directory, lets'
    //    add all .log files to the output parameter
    {
        bp::file::tDirIter di;
        try {
            bp::file::tDirIter end;
            bp::file::tString logExt = bp::file::nativeFromUtf8(".log");
            for (bp::file::tDirIter it(logDir); it != end; ++it)
            {
                if (bp::file::isRegularFile(it->path())) {
                    if (boost::filesystem::extension(*it).compare(logExt) == 0) {
                        bp::file::Path p(it->path());
                        paths.append(new bplus::Path(p.externalUtf8()));
                    }
                }
            }
        } catch (const bp::file::tFileSystemError& e) {
            return std::string("unable to iterate thru plugin writable dir");
        }
    }
    
    // success!
    return std::string();
}
