/******************************************************************************
 *
 * Project:  oesenc_pi
 * Purpose:  oesenc_pi Plugin core
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2018 by David S. Register                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */
#include "config.h"

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include <wx/fileconf.h>
#include <wx/uri.h>
#include "wx/tokenzr.h"
#include <wx/dir.h>
#include <wx/textwrapper.h>
#include "ochartShop.h"
#include "ocpn_plugin.h"

#ifdef __OCPN_USE_CURL__
#include "wxcurl/wx/curl/http.h"
#include "wxcurl/wx/curl/thread.h"
#endif

#include <tinyxml.h>
//#include <../../opencpn/plugins/wmm_pi/src/WMMHeader.h>
#include "wx/wfstream.h"
#include <wx/zipstrm.h>
#include <memory>


#ifdef __OCPN__ANDROID__
#include <QtAndroidExtras/QAndroidJniObject>
#include "qdebug.h"
wxString callActivityMethod_vs(const char *method);
wxString callActivityMethod_ss(const char *method, wxString parm);
wxString callActivityMethod_s4s(const char *method, wxString parm1, wxString parm2, wxString parm3, wxString parm4);
wxString callActivityMethod_s5s(const char *method, wxString parm1, wxString parm2, wxString parm3, wxString parm4, wxString parm5);
wxString callActivityMethod_s6s(const char *method, wxString parm1, wxString parm2="", wxString parm3="", wxString parm4="", wxString parm5="", wxString parm6="");
wxString callActivityMethod_s2s(const char *method, wxString parm1, wxString parm2);
void androidShowBusyIcon();
void androidHideBusyIcon();
#endif

#include <wx/arrimpl.cpp> 
WX_DEFINE_OBJARRAY(ArrayOfCharts);
WX_DEFINE_OBJARRAY(ArrayOfChartPanels);

//  Static variables
ArrayOfCharts g_ChartArray;

wxString userURL(_T("https://o-charts.org/shop/index.php"));
wxString adminURL(_T("http://test.o-charts.org/shop/index.php"));

int g_timeout_secs = 5;

wxArrayString g_systemNameChoiceArray;
wxArrayString g_systemNameServerArray;
wxArrayString g_systemNameDisabledArray;

extern int g_admin;
extern wxString g_systemName;
extern wxString g_loginKey;
extern wxString g_loginUser;
extern wxString g_PrivateDataDir;
extern wxString g_debugShop;
extern wxString g_versionString;
extern wxString g_systemOS;

shopPanel *g_shopPanel;

#ifdef __OCPN_USE_CURL__
OESENC_CURL_EvtHandler *g_CurlEventHandler;
wxCurlDownloadThread *g_curlDownloadThread;
#endif

wxFFileOutputStream *downloadOutStream;
bool g_chartListUpdatedOK;
wxString g_statusOverride;
wxString g_lastInstallDir;

unsigned int g_dongleSN;
wxString g_dongleName;
double g_targetDownloadSizeMB;
double g_targetDownloadSize;
bool g_bconnected;
long g_FileDownloadHandle;
double dl_now;
double dl_total;
time_t g_progressTicks;
InProgressIndicator *g_ipGauge;

extern wxString g_UUID;
extern wxString g_WVID;
extern int      g_SDK_INT;
extern wxString  g_sencutil_bin;

#define N_MESSAGES 22
#if 0
wxString errorMessages[] = {
        _T("Undetermined error"),
        _T("Succesful"),
        _T("Production server in maintenance mode"),
        _T("Wrong data"), 
        _T("Wrong password"),
        _T("Wrong user"),
        _T("Wrong key"),
        _T("System name already exists"),
        _T("System name does not exist"),
        _T("Wrong xfpr name (extension)"),
        _T("Wrong xfpr name (length)"),
        _T("Wrong xfpr name (OS)"),
        _T("Wrong xfpr name (version oc01 or oc03)"),
        _T("Wrong xfpr name (date)"),
        _T("Production server error"),
        _T("Webshop database error"),
        _T("Expired chart"),
        _T("Chart already has a system assigned"),
        _T("Chart already has that system assigned"),
        _T("This request is already being processed or available for downloading"),
        _T("This chart does not have this system assigned"),
        _T("Wrong System name")
};
#endif



#define ID_CMD_BUTTON_INSTALL 7783
#define ID_CMD_BUTTON_INSTALL_CHAIN 7784

bool IsDongleAvailable();
unsigned int GetDongleSN();


// Utility Functions

class HardBreakWrapper : public wxTextWrapper
    {
    public:
        HardBreakWrapper(wxWindow *win, const wxString& text, int widthMax)
        {
            m_lineCount = 0;
            Wrap(win, text, widthMax);
        }
        wxString const& GetWrapped() const { return m_wrapped; }
        int const GetLineCount() const { return m_lineCount; }
        wxArrayString GetLineArray(){ return m_array; }
        
    protected:
        virtual void OnOutputLine(const wxString& line)
        {
            m_wrapped += line;
            m_array.Add(line);
        }
        virtual void OnNewLine()
        {
            m_wrapped += '\n';
            m_lineCount++;
        }
    private:
        wxString m_wrapped;
        int m_lineCount;
        wxArrayString m_array;
    };

#ifdef __OCPN__ANDROID__
bool AndroidUnzip(wxString zipFile, wxString destDir, wxString &tlDir, int nStrip, bool bRemoveZip)
{
//    qDebug() << "AndroidUnzip" << zipFile.mb_str() << destDir.mb_str();
    
    wxString ns;
    ns.Printf(_T("%d"), nStrip);
    
    wxString br = _T("0");
    if(bRemoveZip)
        br = _T("1");
    
//    qDebug() << "br" << br.mb_str();
    
    wxString stat = callActivityMethod_s4s( "unzipFile", zipFile, destDir, ns, br  );
    
    if(wxNOT_FOUND == stat.Find(_T("OK")))
        return false;
    
//    qDebug() << "unzip start";
    
    bool bDone = false;
    wxString rtopDir;
    while (!bDone){
        wxMilliSleep(1000);
        //wxSafeYield(NULL, true);
        
//        qDebug() << "unzip poll";
        
        wxString result = callActivityMethod_ss( "getUnzipStatus", _T("") );
        if(wxNOT_FOUND != result.Find(_T("DONE"))){
            bDone = true;
            rtopDir = result.AfterFirst(' '); //"UNZIPDONE shieldtabl3331-FO-2019-17/"
        }
     }

     if(!rtopDir.Length()){        // if there is no directory in the zip file, then declare
                                            // the tlDir as the zip file name itself, without extension
        wxFileName fn(zipFile);
        rtopDir = destDir + wxFileName::GetPathSeparator() + fn.GetName();
     }
     else
        rtopDir = destDir + wxFileName::GetPathSeparator() + rtopDir;
         

     // Strip trailing '/'
     if(rtopDir.EndsWith(wxFileName::GetPathSeparator()))
        rtopDir = rtopDir.BeforeLast(wxFileName::GetPathSeparator());
    
     tlDir = rtopDir;

//    qDebug() << "unzip done";
    
    return true;    
    
}
#endif

std::string UriEncode(const std::string & sSrc)
{
   const char DEC2HEX[16 + 1] = "0123456789ABCDEF";
   const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
   const int SRC_LEN = sSrc.length();
   unsigned char * const pStart = new unsigned char[SRC_LEN * 3];
   unsigned char * pEnd = pStart;
   const unsigned char * const SRC_END = pSrc + SRC_LEN;

   for (; pSrc < SRC_END; ++pSrc)
   {
      if ( (*pSrc >= 'a' && *pSrc <= 'z') ||
           (*pSrc >= 'A' && *pSrc <= 'Z') ||
           (*pSrc >= '0' && *pSrc <= '9') )
    
         *pEnd++ = *pSrc;
      else
      {
         // escape this char
         *pEnd++ = '%';
         *pEnd++ = DEC2HEX[*pSrc >> 4];
         *pEnd++ = DEC2HEX[*pSrc & 0x0F];
      }
   }

   std::string sResult((char *)pStart, (char *)pEnd);
   delete [] pStart;
   return sResult;
}
// Private class implementations

size_t wxcurl_string_write_UTF8(void* ptr, size_t size, size_t nmemb, void* pcharbuf)
{
    size_t iRealSize = size * nmemb;
    wxCharBuffer* pStr = (wxCharBuffer*) pcharbuf;
    
//     if(pStr)
//     {
//         wxString str = wxString(*pStr, wxConvUTF8) + wxString((const char*)ptr, wxConvUTF8);
//         *pStr = str.mb_str();
//     }
 
    if(pStr)
    {
#ifdef __WXMSW__        
        wxString str1a = wxString(*pStr);
        wxString str2 = wxString((const char*)ptr, wxConvUTF8, iRealSize);
        *pStr = (str1a + str2).mb_str();
#else        
        wxString str = wxString(*pStr, wxConvUTF8) + wxString((const char*)ptr, wxConvUTF8, iRealSize);
        *pStr = str.mb_str(wxConvUTF8);
#endif        
    }
 
    return iRealSize;
}

#ifdef __OCPN_USE_CURL__
class wxCurlHTTPNoZIP : public wxCurlHTTP
{
public:
    wxCurlHTTPNoZIP(const wxString& szURL = wxEmptyString,
               const wxString& szUserName = wxEmptyString,
               const wxString& szPassword = wxEmptyString,
               wxEvtHandler* pEvtHandler = NULL, int id = wxID_ANY,
               long flags = wxCURL_DEFAULT_FLAGS);
    
   ~wxCurlHTTPNoZIP();
    
   bool Post(wxInputStream& buffer, const wxString& szRemoteFile /*= wxEmptyString*/);
   bool Post(const char* buffer, size_t size, const wxString& szRemoteFile /*= wxEmptyString*/);
   std::string GetResponseBody() const;
   
protected:
    void SetCurlHandleToDefaults(const wxString& relativeURL);
    
};

wxCurlHTTPNoZIP::wxCurlHTTPNoZIP(const wxString& szURL /*= wxEmptyString*/, 
                       const wxString& szUserName /*= wxEmptyString*/, 
                       const wxString& szPassword /*= wxEmptyString*/, 
                       wxEvtHandler* pEvtHandler /*= NULL*/, 
                       int id /*= wxID_ANY*/,
                       long flags /*= wxCURL_DEFAULT_FLAGS*/)
: wxCurlHTTP(szURL, szUserName, szPassword, pEvtHandler, id, flags)

{
}

wxCurlHTTPNoZIP::~wxCurlHTTPNoZIP()
{
    ResetPostData();
}

void wxCurlHTTPNoZIP::SetCurlHandleToDefaults(const wxString& relativeURL)
{
    wxCurlBase::SetCurlHandleToDefaults(relativeURL);
    
    SetOpt(CURLOPT_ENCODING, "identity");               // No encoding, plain ASCII
    
    if(m_bUseCookies)
    {
        SetStringOpt(CURLOPT_COOKIEJAR, m_szCookieFile);
    }
}

bool wxCurlHTTPNoZIP::Post(const char* buffer, size_t size, const wxString& szRemoteFile /*= wxEmptyString*/)
{
    wxMemoryInputStream inStream(buffer, size);
    
    return Post(inStream, szRemoteFile);
}

bool wxCurlHTTPNoZIP::Post(wxInputStream& buffer, const wxString& szRemoteFile /*= wxEmptyString*/)
{
    curl_off_t iSize = 0;
    
    if(m_pCURL && buffer.IsOk())
    {
        SetCurlHandleToDefaults(szRemoteFile);
        
        SetHeaders();
        iSize = buffer.GetSize();
        
        if(iSize == (~(ssize_t)0))      // wxCurlHTTP does not know how to upload unknown length streams.
            return false;
        
        SetOpt(CURLOPT_POST, TRUE);
        SetOpt(CURLOPT_POSTFIELDSIZE_LARGE, iSize);
        SetStreamReadFunction(buffer);
        
        //  Use a private data write trap function to handle UTF8 content
        //SetStringWriteFunction(m_szResponseBody);
        SetOpt(CURLOPT_WRITEFUNCTION, wxcurl_string_write_UTF8);         // private function
        SetOpt(CURLOPT_WRITEDATA, (void*)&m_szResponseBody);
        
        if(Perform())
        {
            ResetHeaders();
            return IsResponseOk();
        }
    }
    
    return false;
}

std::string wxCurlHTTPNoZIP::GetResponseBody() const
{
#ifndef OCPN_ARMHF
    wxString s = wxString((const char *)m_szResponseBody, wxConvLibc);
    return std::string(s.mb_str());
    
#else    
    return std::string((const char *)m_szResponseBody);
#endif
    
}

#endif   //__OCPN_USE_CURL__

// itemChart
//------------------------------------------------------------------------------------------

itemChart::itemChart( wxString &order_ref, wxString &chartid, wxString &quantity) {
    //ident = id;
    orderRef = order_ref;
    chartID = chartid;
    quantityId = quantity;
    m_status = STAT_UNKNOWN;
}




// void itemChart::setDownloadPath(int slot, wxString path) {
//     if (slot == 0)
//         fileDownloadPath0 = path;
//     else if (slot == 1)
//         fileDownloadPath1 = path;
// }

// wxString itemChart::getDownloadPath(int slot) {
//     if (slot == 0)
//         return fileDownloadPath0;
//     else if (slot == 1)
//         return fileDownloadPath1;
//     else
//         return _T("");
// }

bool itemChart::isChartsetAssignedToMe(wxString systemName){
    
    // is this chartset assigned to this system?
        if (sysID0.IsSameAs(systemName)) {
            return true;
        }
    
        if (sysID1.IsSameAs(systemName)) {
            return true;
        }
    
    return false;
    
}


bool itemChart::isChartsetFullyAssigned() {
    
    if (statusID0.IsSameAs("unassigned") || !statusID0.Len())
        return false;
    
    if (statusID1.IsSameAs("unassigned") || !statusID1.Len())
        return false;
    
    return true;
}

bool itemChart::isChartsetExpired() {
    
    bool bExp = false;
    if (statusID0.IsSameAs("expired") || statusID1.IsSameAs("expired")) {
        bExp = true;
    }
    return bExp;
}

bool itemChart::isChartsetAssignedToAnyDongle() {
    if(!g_dongleName.Length())
        return false;
    
    if(isSlotAssignedToAnyDongle(0))
        return true;
    if(isSlotAssignedToAnyDongle(1))
        return true;
    return false;
}

bool itemChart::isSlotAssignedToAnyDongle( int slot ) {
    long tl;
    if( slot == 0 ){
        if (sysID0.StartsWith("sgl")){
            if(sysID0.Mid(4).ToLong(&tl, 16))
                return true;
        }
    }
    else{
        if (sysID1.StartsWith("sgl")){
            if(sysID1.Mid(4).ToLong(&tl, 16))
                return true;
        }
    }
    return false;
}   
    
bool itemChart::isSlotAssignedToMyDongle( int slot ) {
    long tl;
    if( slot == 0 ){
        if (sysID0.StartsWith("sgl")){
            if(sysID0.Mid(4).ToLong(&tl, 16)){
                if(tl == g_dongleSN)
                    return true;
            }
        }
    }
    else{
        if (sysID1.StartsWith("sgl")){
            if(sysID1.Mid(4).ToLong(&tl, 16)){
                if(tl == g_dongleSN)
                    return true;
            }
        }
    }
    return false;
}   
    

bool itemChart::isChartsetDontShow()
{
    if(isChartsetFullyAssigned() && !isChartsetAssignedToMe(g_systemName))
        return true;
    
    else if(isChartsetExpired() && !isChartsetAssignedToMe(g_systemName))
        return true;
    
    else
        return false;
}
    
bool itemChart::isChartsetShow()
{
    if(!isChartsetFullyAssigned())
        return true;

    if(isChartsetAssignedToMe(g_systemName))
        return true;
    
    if(isChartsetAssignedToMe( g_dongleName))
        return true;

    return false;
}

    
//  Current status can be one of:
/*
 *      1.  Available for Installation.
 *      2.  Installed, Up-to-date.
 *      3.  Installed, Update available.
 *      4.  Expired.
 */        

int itemChart::getChartStatus()
{
    if(!g_chartListUpdatedOK){
        m_status = STAT_NEED_REFRESH;
        return m_status;
    }

    if(isChartsetExpired()){
        m_status = STAT_EXPIRED;
        return m_status;
    }
    
    // The case where one slot is assigned to systemFPR, one slot empty, and dongle is inserted
//     if(g_dongleName.Len()){
//         if(!isChartsetAssignedToAnyDongle()){
//             if(!sysID1.Length() || !sysID0.Length()){
//                 m_status = STAT_PURCHASED;
//                 return m_status;
//             }
//         }
//     }
    
    if(!isChartsetAssignedToMe( g_systemName )){
        if(!g_dongleName.Len()){
            if(!isChartsetAssignedToAnyDongle()){
                m_status = STAT_PURCHASED;
                return m_status;
            }
        }
        else{
            if(!isChartsetAssignedToMe( g_dongleName )){
                if(!sysID1.Length() || !sysID0.Length()){
                    m_status = STAT_PURCHASED;
                    return m_status;
                }
            }
        }
    }
    
    //  Special case:
    //  Already assigned to system FPR, one slot available,and dongle is inserted
    //  Then we want to allow assignment of second slot to dongle 
    if(isChartsetAssignedToMe( g_systemName )){
        if( g_dongleName.Len() && !isChartsetFullyAssigned()){
            m_status = STAT_PURCHASED;
            return m_status;
        }
    }
            
    
    
    // We know that chart is assigned to me, so one of the sysIDx fields will match
    wxString cStat = statusID0;
    int slot = 0;
    if(isChartsetAssignedToAnyDongle()){
        if(isSlotAssignedToMyDongle( 1 )){
            cStat = statusID1;
            slot = 1;
        }
    }
    
    else{
        if(sysID1.IsSameAs(g_systemName)){
            cStat = statusID1;
            slot = 1;
        }
    }
        
    if(cStat.IsSameAs(_T("requestable"))){
        m_status = STAT_REQUESTABLE;
        return m_status;
    }

    if(cStat.IsSameAs(_T("processing"))){
        m_status = STAT_PREPARING;
        return m_status;
    }

    if(cStat.IsSameAs(_T("download"))){
        m_status = STAT_READY_DOWNLOAD;
        
        if(slot == 0){
            if(  (installLocation0.Length() > 0) && (installedFileDownloadPath0.Length() > 0) ){
                m_status = STAT_CURRENT;
                if(!installedEdition0.IsSameAs(currentChartEdition)){
                    m_status = STAT_STALE;
                }
            }
        }
        else if(slot == 1){
            if(  (installLocation1.Length() > 0) && (installedFileDownloadPath1.Length() > 0) ){
                m_status = STAT_CURRENT;
                if(!installedEdition1.IsSameAs(currentChartEdition)){
                    m_status = STAT_STALE;
                }
                
            }
        }
    }

     
    return m_status;
    
}
wxString itemChart::getStatusString()
{
    getChartStatus();
    
    wxString sret;
    
    switch(m_status){
        
        case STAT_UNKNOWN:
            break;
            
        case STAT_PURCHASED:
            sret = _("Available");
            break;
            
        case STAT_CURRENT:
            sret = _("Installed, Up-to-date.");
            break;
            
        case STAT_STALE:
            sret = _("Installed, Update available.");
            break;
            
        case STAT_EXPIRED:
        case STAT_EXPIRED_MINE:
            sret = _("Expired.");
            break;
            
        case STAT_PREPARING:
            sret = _("Preparing your chart set.");
            break;
            
        case STAT_READY_DOWNLOAD:
            sret = _("Ready for download.");
            break;
            
        case STAT_NEED_REFRESH:
            sret = _("Please update Chart List.");
            break;

        case STAT_REQUESTABLE:
            sret = _("Ready for Download Request.");
            break;
            
        default:
            break;
    }
    
    return sret;
    

}

wxString itemChart::getKeytypeString(){
 
    if(isChartsetAssignedToAnyDongle()){
        if(isSlotAssignedToAnyDongle( 0 ))
            return _("USB Key Dongle") + _T("  [ ") + sysID0 +_T(" ]");
        if(isSlotAssignedToAnyDongle( 1 ))
            return _("USB Key Dongle") + _T("  [ ") + sysID1 +_T(" ]");
        else
         return _T("");
    }
        
    else if(isChartsetAssignedToMe(g_systemName))
        return _("System Key") + _T("  [ ") + g_systemName +_T(" ]");
    else
        return _T("");
}

wxString itemChart::getKeytypeString( int slot, wxColour &tcolour ){

    if(slot == 0){
        if(isSlotAssignedToAnyDongle( 0 )){
            if(!sysID0.IsSameAs(g_dongleName))
                tcolour = wxColour(128, 128, 128);
            return _("USB Key Dongle") + _T("  [ ") + sysID0 +_T(" ]");
        }
        else{
            if (sysID0.Length()){
                if( !sysID0.IsSameAs(g_systemName) )
                    tcolour = wxColour(128, 128, 128);
                return _("System Key") + _T("  [ ") + sysID0 +_T(" ]");
            }
            else
                return _T("");
        }

    }

    if(slot == 1){
        if(isSlotAssignedToAnyDongle( 1 )){
             if(!sysID1.IsSameAs(g_dongleName))
                tcolour = wxColour(128, 128, 128);
            return _("USB Key Dongle") + _T("  [ ") + sysID1 +_T(" ]");
        }
        else{
            if (sysID1.Length()){
                if( !sysID1.IsSameAs(g_systemName) )
                    tcolour = wxColour(128, 128, 128);
                return _("System Key") + _T("  [ ") + sysID1 +_T(" ]");
            }
            else
                return _T("");
        }
    }
    
    return _T("");
}

wxString itemChart::getKeyString( int slot, wxColour &tcolour ){

    if(slot == 0){
        if(isSlotAssignedToAnyDongle( 0 )){
            if(!sysID0.IsSameAs(g_dongleName))
                tcolour = wxColour(128, 128, 128);
            return sysID0;
        }
        else{
            if (sysID0.Length()){
                if( !sysID0.IsSameAs(g_systemName) )
                    tcolour = wxColour(128, 128, 128);
                return sysID0;
            }
            else
                return _T("");
        }

    }

    if(slot == 1){
        if(isSlotAssignedToAnyDongle( 1 )){
             if(!sysID1.IsSameAs(g_dongleName))
                tcolour = wxColour(128, 128, 128);
            return sysID1;
        }
        else{
            if (sysID1.Length()){
                if( !sysID1.IsSameAs(g_systemName) )
                    tcolour = wxColour(128, 128, 128);
                return sysID1;
            }
            else
                return _T("");
        }
    }
    
    return _T("");
}

int s_dlbusy;

wxBitmap& itemChart::GetChartThumbnail(int size, bool bDL_If_Needed)
{
    if(!m_ChartImage.IsOk()){
        // Look for cached copy
        wxString fileKey = _T("ChartImage-");
        fileKey += chartID;
        fileKey += _T(".jpg");
 
        wxString file = g_PrivateDataDir + fileKey;
        if(::wxFileExists(file)){
            m_ChartImage = wxImage( file, wxBITMAP_TYPE_ANY);
        }
        else if(bDL_If_Needed){
            if(g_chartListUpdatedOK && thumbnailURL.Length()){  // Do not access network until after first "getList"
#ifdef __OCPN_USE_CURL__
                wxCurlHTTP get;
                get.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
                bool getResult = get.Get(file, thumbnailURL);

            // get the response code of the server
                int iResponseCode = 0;
                get.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
            
                if(iResponseCode == 200){
                    if(::wxFileExists(file)){
                        m_ChartImage = wxImage( file, wxBITMAP_TYPE_ANY);
                    }
                }
#else
                if(!s_dlbusy){
                    
                    wxString fileKeytmp = _T("ChartImage-");
                    fileKeytmp += chartID;
                    fileKeytmp += _T(".tmp");
 
                    wxString filetmp = g_PrivateDataDir + fileKeytmp;

                    wxString file_URI = _T("file://") + filetmp;
                    
                    int iResponseCode = 0;
                    s_dlbusy = 1;
                    _OCPN_DLStatus ret = OCPN_downloadFile( thumbnailURL, file_URI, _T(""), _T(""), wxNullBitmap, NULL, 0, 15);

                    wxLogMessage(_T("DLRET"));
//                    qDebug() << "DL done";
                    if(OCPN_DL_NO_ERROR == ret){
                        wxCopyFile(filetmp, file);
                        iResponseCode = 200;
                    }
                    else
                        iResponseCode = ret;
                    
                    wxRemoveFile( filetmp );
                    s_dlbusy = 0;
                    
                    if(iResponseCode == 200){
                        if(::wxFileExists(file)){
                            m_ChartImage = wxImage( file, wxBITMAP_TYPE_ANY);
                        }
                    }

                }
//                else
//                    qDebug() << "Busy";
                
#endif                
            }
        }
    }
    
    if(m_ChartImage.IsOk()){
        int scaledHeight = size;
        int scaledWidth = m_ChartImage.GetWidth() * scaledHeight / m_ChartImage.GetHeight();
        wxImage scaledImage = m_ChartImage.Rescale(scaledWidth, scaledHeight);
        m_bm = wxBitmap(scaledImage);

        return m_bm;
    }
    else{
        wxImage img(size, size);
        unsigned char *data = img.GetData();
        for(int i=0 ; i < size * size * 3 ; i++)
            data[i] = 200;
        
        m_bm = wxBitmap(img);   // Grey bitmap
        return m_bm;
    }
    
}




//Utility Functions

//  Search g_ChartArray for chart having specified parameters
int findOrderRefChartId(wxString &orderRef, wxString &chartId, wxString &quantity)
{
    for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++){
        if(g_ChartArray.Item(i)->orderRef.IsSameAs(orderRef)
            && g_ChartArray.Item(i)->chartID.IsSameAs(chartId) &&
            g_ChartArray.Item(i)->quantityId.IsSameAs(quantity) ){
                return (i);
            }
    }
    return -1;
}


void loadShopConfig()
{
    //    Get a pointer to the opencpn configuration object
    wxFileConfig *pConf = GetOCPNConfigObject();
    
    if( pConf ) {
        pConf->SetPath( _T("/PlugIns/oesenc") );
        
        if(!g_systemName.Length())
            pConf->Read( _T("systemName"), &g_systemName);
        pConf->Read( _T("loginUser"), &g_loginUser);
        pConf->Read( _T("loginKey"), &g_loginKey);
        pConf->Read( _T("lastInstllDir"), &g_lastInstallDir);
        
        pConf->Read( _T("ADMIN"), &g_admin);
        pConf->Read( _T("DEBUG_SHOP"), &g_debugShop);
        
        pConf->SetPath ( _T ( "/PlugIns/oesenc/charts" ) );
        wxString strk;
        wxString kval;
        long dummyval;
        bool bContk = pConf->GetFirstEntry( strk, dummyval );
        while( bContk ) {
            pConf->Read( strk, &kval );
            
            // Parse the key
            wxStringTokenizer tkzs( strk, _T("-") );
            wxString id = tkzs.GetNextToken();
            wxString qty = tkzs.GetNextToken();
            wxString order = tkzs.GetNextToken();
            
            // Add a chart if necessary
            int index = findOrderRefChartId(order, id, qty);
            itemChart *pItem;
            if(index < 0){
                pItem = new itemChart(order, id, qty);
                g_ChartArray.Add(pItem);
            }
            else
                pItem = g_ChartArray.Item(index);

            // Parse the value
            wxStringTokenizer tkz( kval, _T(";") );
            wxString name = tkz.GetNextToken();
            wxString install0 = tkz.GetNextToken();
            wxString dl0 = tkz.GetNextToken();
            wxString install1 = tkz.GetNextToken();
            wxString dl1 = tkz.GetNextToken();
            wxString ied0 = tkz.GetNextToken();
            wxString ied1 = tkz.GetNextToken();
            
            pItem->chartName = name;
            if(pItem->installLocation0.IsEmpty())   pItem->installLocation0 = install0;
            if(pItem->installedFileDownloadPath0.IsEmpty())  pItem->installedFileDownloadPath0 = dl0;
            if(pItem->installLocation1.IsEmpty())   pItem->installLocation1 = install1;
            if(pItem->installedFileDownloadPath1.IsEmpty())  pItem->installedFileDownloadPath1 = dl1;
            pItem->installedEdition0 = ied0;
            pItem->installedEdition1 = ied1;
            
            bContk = pConf->GetNextEntry( strk, dummyval );
        }
    }
}

void saveShopConfig()
{
    wxFileConfig *pConf = GetOCPNConfigObject();
        
   if( pConf ) {
      pConf->SetPath( _T("/PlugIns/oesenc") );
            
      pConf->Write( _T("systemName"), g_systemName);
      pConf->Write( _T("loginUser"), g_loginUser);
      pConf->Write( _T("loginKey"), g_loginKey);
      pConf->Write( _T("lastInstllDir"), g_lastInstallDir);
      
      pConf->DeleteGroup( _T("/PlugIns/oesenc/charts") );
      pConf->SetPath( _T("/PlugIns/oesenc/charts") );
      
      for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++){
          itemChart *chart = g_ChartArray.Item(i);
          wxString key = chart->chartID + _T("-") + chart->quantityId + _T("-") + chart->orderRef;
          wxString val = chart->chartName + _T(";");
          val += chart->installLocation0 + _T(";");
          val += chart->installedFileDownloadPath0 + _T(";");
          val += chart->installLocation1 + _T(";");
          val += chart->installedFileDownloadPath1 + _T(";");
          val += chart->installedEdition0 + _T(";");
          val += chart->installedEdition1 + _T(";");
          pConf->Write( key, val );
      }
   }
}

#if 0
wxString GetMessageText(int index)
{
    if(index < N_MESSAGES)
        return errorMessages[index];
    else
        return _("Undetermined error");

}
#endif


int checkResult(wxString &result, bool bShowErrorDialog = true)
{
    if(g_shopPanel){
        g_ipGauge->Stop();
    }
    
    wxString resultDigits = result.BeforeFirst(':');
    
    long dresult;
    if(resultDigits.ToLong(&dresult)){
        if(dresult == 1){
            return 0;
        }
        else{
            if(bShowErrorDialog){
                wxString msg = _("o-charts API error code: ");
                wxString msg1;
                msg1.Printf(_T("{%ld}\n\n"), dresult);
                msg += msg1;
                switch(dresult){
                    case 4:
                    case 5:
                        msg += _("Invalid user/email name or password.");
                        break;
                    case 27:
                        msg += _("This oeSENC plugin version is obsolete.");
                        msg += _T("\n");
                        msg += _("Please update your plugin.");
                        msg += _T("\n");
                        msg +=  _("Operation cancelled");
                        break;
                    default:
                        if(result.AfterFirst(':').Length()){
                            msg += result.AfterFirst(':');
                            msg += _T("\n");
                        }
                        msg += _("Operation cancelled");
                        break;
                }
                
                OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
            }
            return dresult;
        }
    }
    else{
        OCPNMessageBox_PlugIn(NULL, _("o-Charts shop interface error") + _T("\n") + result + _T("\n") + _("Operation cancelled"), _("oeSENC_pi Message"), wxOK);
    }
     
    return 98;
}

int checkResponseCode(int iResponseCode)
{
    if(iResponseCode != 200){
        wxString msg = _("internet communications error code: ");
        wxString msg1;
        msg1.Printf(_T("{%d}\n "), iResponseCode);
        msg += msg1;
        msg += _("Check your connection and try again.");
        OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
    }
    
    // The wxCURL library returns "0" as response code,
    // even when it should probably return 404.
    // We will use "99" as a special code here.
    
    if(iResponseCode < 100)
        return 99;
    else
        return iResponseCode;
        
}

int doLogin()
{
    oeSENCLogin *login = new oeSENCLogin(g_shopPanel);
    login->ShowModal();
    if((!login->GetReturnCode()) == 0){
        delete login;
        g_shopPanel->setStatusText( _("Invalid Login."));
        wxYield();
        return 55;
    }
    
    g_loginUser = login->m_UserNameCtl->GetValue();
    wxString pass = login->m_PasswordCtl->GetValue();
    delete login;
    
    wxString loginPass = pass;
#ifndef __WXMSW__    
    // "percent encode" any special characters in password
    std::string spass(pass.mb_str());
    std::string percentPass = UriEncode(spass);
    loginPass = wxString(percentPass.c_str());
#endif
    
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=api");
    
    wxString loginParms;
    loginParms += _T("taskId=login");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&password=") + loginPass;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;
    loginParms += _T("&version=") + g_systemOS + g_versionString;

    //wxLogMessage(_T("loginParms: ") + loginParms);
    int iResponseCode =0;
    TiXmlDocument *doc = 0;
    size_t res = 0;
    
#ifdef __OCPN_USE_CURL__    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    if(iResponseCode == 200){
        doc = new TiXmlDocument();
        doc->Parse( post.GetResponseBody().c_str());
    }

#else

    //qDebug() << url.mb_str();
    //qDebug() << loginParms.mb_str();
    
    wxString postresult;
    _OCPN_DLStatus stat = OCPN_postDataHttp( url, loginParms, postresult, 5 );

    //qDebug() << "doLogin Post Stat: " << stat;
    
    if(stat != OCPN_DL_FAILED){
        wxCharBuffer buf = postresult.ToUTF8();
        std::string response(buf.data());
        
        //qDebug() << response.c_str();
        doc = new TiXmlDocument();
        doc->Parse( response.c_str());
        iResponseCode = 200;
        res = 1;
    }

#endif

    ///////// 
#if 0    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    const char* s = loginParms.mb_str(wxConvUTF8);    
    
    size_t res = post.Post( s, strlen(s), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
#endif

    if(iResponseCode == 200){
//        TiXmlDocument * doc = new TiXmlDocument();
//        const char *rr = doc->Parse( post.GetResponseBody().c_str());
        
//        wxString p = wxString(post.GetResponseBody().c_str(), wxConvUTF8);
//        wxLogMessage(_T("doLogin results:"));
//        wxLogMessage(p);
        
        wxString queryResult;
        wxString loginKey;
        
        if( res )
        {
            TiXmlElement * root = doc->RootElement();
            if(!root){
                wxString r = _T("50");
                checkResult(r);                              // ProcessResponse parse error
                return false;
            }
            
            wxString rootName = wxString::FromUTF8( root->Value() );
            TiXmlNode *child;
            for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
                wxString s = wxString::FromUTF8(child->Value());
                
                if(!strcmp(child->Value(), "result")){
                    TiXmlNode *childResult = child->FirstChild();
                    queryResult =  wxString::FromUTF8(childResult->Value());
                }
                else if(!strcmp(child->Value(), "key")){
                    TiXmlNode *childResult = child->FirstChild();
                    loginKey =  wxString::FromUTF8(childResult->Value());
                }
            }
        }
        
        if(queryResult == _T("1"))
            g_loginKey = loginKey;
        else
            checkResult(queryResult, true);
        
        long dresult;
        if(queryResult.ToLong(&dresult)){
            return dresult;
        }
        else{
            return 53;
        }
    }
    else
        return 54;
    
}

std::string correct_non_utf_8(std::string *str)
{
    int i,f_size=str->size();
    unsigned char c,c2,c3,c4;
    std::string to;
    to.reserve(f_size);

    for(i=0 ; i<f_size ; i++){
        c=(unsigned char)(*str)[i];
        
        if(c<32){//control char
            if(c==9 || c==10 || c==13){//allow only \t \n \r
                to.append(1,c);
            }
            continue;
        }else if(c<127){//normal ASCII
            to.append(1,c);
            continue;
        }else if(c<160){//control char (nothing should be defined here either ASCI, ISO_8859-1 or UTF8, so skipping)
            if(c2==128){//fix microsoft mess, add euro
                to.append(1,226);
                to.append(1,130);
                to.append(1,172);
            }
            if(c2==133){//fix IBM mess, add NEL = \n\r
                to.append(1,10);
                to.append(1,13);
            }
            continue;
        }else if(c<192){//invalid for UTF8, converting ASCII
            to.append(1,(unsigned char)194);
            to.append(1,c);
            continue;
        }else if(c<194){//invalid for UTF8, converting ASCII
            to.append(1,(unsigned char)195);
            to.append(1,c-64);
            continue;
        }else if(c<224 && i+1<f_size){//possibly 2byte UTF8
            c2=(unsigned char)(*str)[i+1];
            if(c2>127 && c2<192){//valid 2byte UTF8
                if(c==194 && c2<160){//control char, skipping
                    ;
                }else{
                    to.append(1,c);
                    to.append(1,c2);
                }
                i++;
                continue;
            }
        }else if(c<240 && i+2<f_size){//possibly 3byte UTF8
            c2=(unsigned char)(*str)[i+1];
            c3=(unsigned char)(*str)[i+2];
            if(c2>127 && c2<192 && c3>127 && c3<192){//valid 3byte UTF8
                to.append(1,c);
                to.append(1,c2);
                to.append(1,c3);
                i+=2;
                continue;
            }
        }else if(c<245 && i+3<f_size){//possibly 4byte UTF8
            c2=(unsigned char)(*str)[i+1];
            c3=(unsigned char)(*str)[i+2];
            c4=(unsigned char)(*str)[i+3];
            if(c2>127 && c2<192 && c3>127 && c3<192 && c4>127 && c4<192){//valid 4byte UTF8
                to.append(1,c);
                to.append(1,c2);
                to.append(1,c3);
                to.append(1,c4);
                i+=3;
                continue;
            }
        }
        //invalid UTF8, converting ASCII (c>245 || string too short for multi-byte))
        to.append(1,(unsigned char)195);
        to.append(1,c-64);
    }
    return to;
}

wxString ProcessResponse(std::string body)
{
    // Validate/correct the input
        std::string body_corrected = correct_non_utf_8(&body);

        wxString ss;
        for(unsigned int i=0 ; i<body_corrected.size() ; i++){
            unsigned char c=(unsigned char)(body_corrected)[i];
            wxString sm;
            sm.Printf(_T("%X,"), c);
            ss.Append(sm);
        }

        wxLogMessage(_T("ProcessResponse RAW:"));
        wxLogMessage(ss);

        TiXmlDocument * doc = new TiXmlDocument();
        const char *rr = doc->Parse( body_corrected.c_str());
    
        if(!rr){
            wxLogMessage(_T("TinyXML parse result 1:"));
            wxString p1 = wxString(doc->ErrorDesc(), wxConvUTF8);
            wxLogMessage(p1);
            
            //Try again with explicit encoding instruction
            rr = doc->Parse( body_corrected.c_str(), NULL, TIXML_ENCODING_UTF8);

            if(!rr){
                wxLogMessage(_T("TinyXML parse result 2:"));
                wxString p2 = wxString(doc->ErrorDesc(), wxConvUTF8);
                wxLogMessage(p2);
            }
        }
            
           
        //doc->Print();
        
        wxString queryResult = _T("50");  // Default value, general error.;
        wxString chartOrder;
        wxString chartPurchase;
        wxString chartExpiration;
        wxString chartID;
        wxString chartEdition;
        wxString chartPublication;
        wxString chartName;
        wxString chartQuantityID;
        wxString chartSlot;
        wxString chartAssignedSystemName;
        wxString chartLastRequested;
        wxString chartState;
        wxString chartLink;
        wxString chartSize;
        wxString chartThumbURL;

          wxString p = wxString(body_corrected.c_str(), wxConvUTF8);
          wxLogMessage(_T("ProcessResponse results:"));
          wxLogMessage(p);

        
            TiXmlElement * root = doc->RootElement();
            if(!root){
                return _T("57");                              // undetermined error??
            }
            
            wxString rootName = wxString::FromUTF8( root->Value() );
            TiXmlNode *child;
            for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
                wxString s = wxString::FromUTF8(child->Value());
                
                if(!strcmp(child->Value(), "result")){
                    TiXmlNode *childResult = child->FirstChild();
                    queryResult =  wxString::FromUTF8(childResult->Value());
                }
                
                else if(!strcmp(child->Value(), "systemName")){
                    TiXmlNode *childsystemName = child->FirstChild();
                    wxString sName =  wxString::FromUTF8(childsystemName->Value());
                    if(g_systemNameChoiceArray.Index(sName) == wxNOT_FOUND)
                        g_systemNameChoiceArray.Add(sName);
                    
                    //  Maintain a separate list of systemNames known to the server
                    if(g_systemNameServerArray.Index(sName) == wxNOT_FOUND)
                        g_systemNameServerArray.Add(sName);
                        
                }
                
                else if(!strcmp(child->Value(), "disabledSystemName")){
                    TiXmlNode *childsystemNameDisabled = child->FirstChild();
                    wxString sName =  wxString::FromUTF8(childsystemNameDisabled->Value());
                    if(g_systemNameDisabledArray.Index(sName) == wxNOT_FOUND)
                        g_systemNameDisabledArray.Add(sName);
                    
                }
                
                else if(!strcmp(child->Value(), "chart")){
                    chartAssignedSystemName = _T("");
                    chartLastRequested = _T("");
                    
                    TiXmlNode *childChart = child->FirstChild();
                    for ( childChart = child->FirstChild(); childChart!= 0; childChart = childChart->NextSibling()){
                        const char *chartVal =  childChart->Value();
                        
                        if(!strcmp(chartVal, "order")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartOrder = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "purchase")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartPurchase = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "expiration")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartExpiration = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "chartid")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartID = wxString::FromUTF8(childVal->Value());
                        }                        
                        else if(!strcmp(chartVal, "thumbLink")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartThumbURL = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "chartEdition")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartEdition = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "chartPublication")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartPublication = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "chartName")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartName = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "quantityId")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartQuantityID = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "slot")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartSlot = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "assignedSystemName")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartAssignedSystemName = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "lastRequested")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartLastRequested = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "state")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartState = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "link")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartLink = wxString::FromUTF8(childVal->Value());
                        }
                        else if(!strcmp(chartVal, "size")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) chartSize = wxString::FromUTF8(childVal->Value());
                        }
                    }
                    
                    // Process this chart node
                    
                    // As identified uniquely by order, chartid, and quantityid parameters....
                    // Does this chart already exist in the table?
                    itemChart *pItem;
                    int index = findOrderRefChartId(chartOrder, chartID, chartQuantityID);
                    if(index < 0){
                        pItem = new itemChart(chartOrder, chartID, chartQuantityID);
                        g_ChartArray.Add(pItem);
                    }
                    else
                        pItem = g_ChartArray.Item(index);
                    
                    // Populate in the rest of "item"
                    pItem->chartID = chartID;
                    pItem->quantityId = chartQuantityID;
                    pItem->chartName = chartName;
                    pItem->orderRef = chartOrder;
                    pItem->currentChartEdition = chartEdition;
                    pItem->thumbnailURL = chartThumbURL;
                    pItem->expDate = chartExpiration;
                    pItem->purchaseDate = chartPurchase;
                    
                    if(chartSlot.IsSameAs(_T("0"))){
                        pItem->statusID0 = chartState;
                        pItem->sysID0 = chartAssignedSystemName;
                        pItem->fileDownloadURL0 = chartLink;
                        pItem->filedownloadSize0 = chartSize;
                        pItem->lastRequestEdition0 = chartLastRequested;
                    }
                    else if(chartSlot.IsSameAs(_T("1"))){
                        pItem->statusID1 = chartState;
                        pItem->sysID1 = chartAssignedSystemName;
                        pItem->fileDownloadURL1 = chartLink;
                        pItem->filedownloadSize1 = chartSize;
                        pItem->lastRequestEdition1 = chartLastRequested;
                    }
                
                }
            }
        
        
        return queryResult;
}

    

int getChartList( bool bShowErrorDialogs = true){
    
    // We query the server for the list of charts associated with our account
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=api");
    
    wxString loginParms;
    loginParms += _T("taskId=getlist");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&key=") + g_loginKey;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;
    loginParms += _T("&version=") + g_systemOS + g_versionString;

    int iResponseCode = 0;
    size_t res = 0;
    std::string responseBody;
    
#ifdef __OCPN_USE_CURL__    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
    res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    std::string a = post.GetDetailedErrorString();
    std::string b = post.GetErrorString();
    std::string c = post.GetResponseBody();
    
    responseBody = post.GetResponseBody();
    //printf("%s", post.GetResponseBody().c_str());
    
    //wxString tt(post.GetResponseBody().data(), wxConvUTF8);
    //wxLogMessage(tt);
#else
     wxString postresult;
    //qDebug() << url.mb_str();
    //qDebug() << loginParms.mb_str();

    _OCPN_DLStatus stat = OCPN_postDataHttp( url, loginParms, postresult, g_timeout_secs );

    //qDebug() << "getChartList Post Stat: " << stat;
    
    if(stat != OCPN_DL_FAILED){
        wxCharBuffer buf = postresult.ToUTF8();
        std::string response(buf.data());
        
        //qDebug() << response.c_str();
        responseBody = response.c_str();
        iResponseCode = 200;
        res = 1;
    }

#endif    
    
#if 0
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
    const char* s = loginParms.mb_str(wxConvUTF8);    
    size_t res = post.Post( s, strlen(s), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    std::string a = post.GetDetailedErrorString();
    std::string b = post.GetErrorString();
    std::string c = post.GetResponseBody();
    
    //printf("%s", post.GetResponseBody().c_str());
    
    wxString tt(post.GetResponseBody().data(), wxConvUTF8);
    //wxLogMessage(tt);
#endif    
    if(iResponseCode == 200){
        wxString result = ProcessResponse(responseBody);
        
        return checkResult( result, bShowErrorDialogs );
            
    }
    else
        return checkResponseCode(iResponseCode);
}

#if 0
<chart>
    <order>OAUMRVVRP</order>
    <purchase>2017-07-06 19:27:41</purchase>
    <expiration>2018-07-06 19:27:41</expiration>
    <chartid>10</chartid>
    <chartEdition>2018-2</chartEdition>
    <chartPublication>1522533600</chartPublication>
    <chartName>Netherlands and Belgium 2017</chartName>
    <quantityId>1</quantityId>
    <slot>1</slot>
    <assignedSystemName />
    <lastRequested />
    <state>unassigned</state>
    <link />
    <size />
</chart>
#endif

int doAssign(itemChart *chart, int slot, wxString systemName)
{
    wxString msg = _("This action will PERMANENTLY assign the chart set:");
    msg += _T("\n        ");
    msg += chart->chartName;
    msg += _T("\n\n");
    msg += _("to this systemName:");
    msg += _T("\n        ");
    msg += systemName;
    if(systemName.StartsWith("sgl")){
        msg += _T(" (") + _("USB Key Dongle") + _T(")");
    }
    
    msg += _T("\n\n");
    msg += _("Proceed?");
    
    int ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_PI Message"), wxYES_NO);
    
    if(ret != wxID_YES){
        return 1;
    }
        
    // Assign a chart to this system name
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=api");
    
    wxString sSlot;
    sSlot.Printf(_T("%1d"), slot);
    
    wxString loginParms;
    loginParms += _T("taskId=assign");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&key=") + g_loginKey;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;
    loginParms += _T("&version=") + g_systemOS + g_versionString;

    loginParms += _T("&systemName=") + systemName;
    loginParms += _T("&chartid=") + chart->chartID;
    loginParms += _T("&order=") + chart->orderRef;
    loginParms += _T("&quantityId=") + chart->quantityId;
    loginParms += _T("&slot=") + sSlot;

    int iResponseCode = 0;
    size_t res = 0;
    std::string responseBody;
    
#ifdef __OCPN_USE_CURL__    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
    res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    std::string a = post.GetDetailedErrorString();
    std::string b = post.GetErrorString();
    std::string c = post.GetResponseBody();
    
    responseBody = post.GetResponseBody();
    //printf("%s", post.GetResponseBody().c_str());
    
    //wxString tt(post.GetResponseBody().data(), wxConvUTF8);
    //wxLogMessage(tt);
#else
     wxString postresult;
    //qDebug() << url.mb_str();
    //qDebug() << loginParms.mb_str();

    _OCPN_DLStatus stat = OCPN_postDataHttp( url, loginParms, postresult, g_timeout_secs );

    //qDebug() << "getChartList Post Stat: " << stat;
    
    if(stat != OCPN_DL_FAILED){
        wxCharBuffer buf = postresult.ToUTF8();
        std::string response(buf.data());
        
        //qDebug() << response.c_str();
        responseBody = response.c_str();
        iResponseCode = 200;
        res = 1;
    }

#endif    
#if 0
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    const char* s = loginParms.mb_str(wxConvUTF8);    
    size_t res = post.Post( s, strlen(s), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
#endif

    if(iResponseCode == 200){
        wxString result = ProcessResponse(responseBody);
        
        return checkResult(result);
    }
    else
        return checkResponseCode(iResponseCode);
}

extern wxString getFPR( bool bCopyToDesktop, bool &bCopyOK, bool bSGLock);

int doUploadXFPR(bool bDongle)
{
    wxString err;
    wxString stringFPR;
    wxString fprName;
    
#ifndef __OCPN__ANDROID__    
    // Generate the FPR file
    bool b_copyOK = false;
    
    wxString fpr_file = getFPR( false, b_copyOK, bDongle);              // No copy needed
    
    fpr_file = fpr_file.Trim(false);            // Trim leading spaces...
    
    if(fpr_file.Len()){
        //Read the file, convert to ASCII hex, and build a string
        if(::wxFileExists(fpr_file)){
            wxFileInputStream stream(fpr_file);
            while(stream.IsOk() && !stream.Eof() ){
                unsigned char c = stream.GetC();
                if(!stream.Eof()){
                    wxString sc;
                    sc.Printf(_T("%02X"), c);
                    stringFPR += sc;
                }
            }
            wxFileName fnxpr(fpr_file);
            fprName = fnxpr.GetFullName();
        }
        else if(fpr_file.IsSameAs(_T("DONGLE_NOT_PRESENT"))){
            err = _("[USB Key Dongle not found.]");
        }
            
        else{
            err = _("[fpr file not found.]");
        }
    }
    else{
            err = _("[fpr file not created.]");
    }
#else   // Android

    // Get the FPR directly from the helper oeserverda, in ASCII HEX
    wxString cmd = g_sencutil_bin;
    wxString result;
    wxString prefix = "oc03R_";
    if(g_SDK_INT < 21){          // Earlier than Android 5
        //  Set up the parameter passed as the local app storage directory
        wxString dataLoc = *GetpPrivateApplicationDataLocation();
        wxFileName fn(dataLoc);
        wxString dataDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        result = callActivityMethod_s6s("createProcSync5stdout", cmd, "-q", dataDir, "-g");
    }
    else if(g_SDK_INT < 29){            // Strictly earlier than Android 10
        result = callActivityMethod_s6s("createProcSync5stdout", cmd, "-z", g_UUID, "-g");
    }
    else{
        result = callActivityMethod_s6s("createProcSync5stdout", cmd, "-y", g_WVID, "-k");
        prefix = "oc04R_";
    }
    
    //qDebug() << result.mb_str();
    
    wxString kv, fpr;
    wxStringTokenizer tkz(result, _T(";"));
    kv = tkz.GetNextToken();
    fpr = tkz.GetNextToken();
    //qDebug() << kv.mb_str();
    //qDebug() << fpr.mb_str();   
    
    stringFPR = fpr;
    fprName = prefix + kv + ".fpr";
    
    if(stringFPR.IsEmpty())
        err = _("[fpr not created.]");

    //qDebug() << "[" << stringFPR.mb_str() << "]";
    //qDebug() << "[" << fprName.mb_str() << "]";
    

#endif
        
    if(stringFPR.Length()){        
            
        // Prepare the upload command string
        wxString url = userURL;
        if(g_admin)
            url = adminURL;
            
        url +=_T("?fc=module&module=occharts&controller=api");
            
            
        wxString loginParms;
        loginParms += _T("taskId=xfpr");
        loginParms += _T("&username=") + g_loginUser;
        loginParms += _T("&key=") + g_loginKey;
        if(g_debugShop.Len())
            loginParms += _T("&debug=") + g_debugShop;
        loginParms += _T("&version=") + g_systemOS + g_versionString;

        if(!bDongle)
            loginParms += _T("&systemName=") + g_systemName;
        else
            loginParms += _T("&systemName=") + g_dongleName;
                
        loginParms += _T("&xfpr=") + stringFPR;
        loginParms += _T("&xfprName=") + fprName;
          
            //wxLogMessage(loginParms);
            
        int iResponseCode = 0;
        size_t res = 0;
        std::string responseBody;
    
#ifdef __OCPN_USE_CURL__    
        wxCurlHTTPNoZIP post;
        post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
        res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
        // get the response code of the server
    
        post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
        std::string a = post.GetDetailedErrorString();
        std::string b = post.GetErrorString();
        std::string c = post.GetResponseBody();
    
        responseBody = post.GetResponseBody();
        //printf("%s", post.GetResponseBody().c_str());
    
        //wxString tt(post.GetResponseBody().data(), wxConvUTF8);
        //wxLogMessage(tt);
#else
        wxString postresult;
        //qDebug() << url.mb_str();
        //qDebug() << loginParms.mb_str();

        _OCPN_DLStatus stat = OCPN_postDataHttp( url, loginParms, postresult, g_timeout_secs );

        //qDebug() << "getChartList Post Stat: " << stat;
    
        if(stat != OCPN_DL_FAILED){
            wxCharBuffer buf = postresult.ToUTF8();
            std::string response(buf.data());
        
            //qDebug() << response.c_str();
            responseBody = response.c_str();
            iResponseCode = 200;
            res = 1;
        }
#endif    

        if(iResponseCode == 200){
            wxString result = ProcessResponse(responseBody);
                
            int iret = checkResult(result);
            return iret;
        }
        else
            return checkResponseCode(iResponseCode);
    }
    
    if(err.Len()){
        wxString msg = _("ERROR Creating Fingerprint file") + _T("\n");
        msg += _("Check OpenCPN log file.") + _T("\n"); 
        msg += err;
        OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
        return 1;
    }
        
    return 0;
}


int doPrepare(oeSencChartPanel *chartPrepare, int slot)
{
    // Request a chart preparation
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=api");
    
    wxString sSlot;
    sSlot.Printf(_T("%1d"), slot);
    
    itemChart *chart = chartPrepare->m_pChart;
    
    wxString aSysName = chart->sysID0;
    if(slot == 1)
        aSysName = chart->sysID1;
    
        
    wxString loginParms;
    loginParms += _T("taskId=request");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&key=") + g_loginKey;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;
    loginParms += _T("&version=") + g_systemOS + g_versionString;

    loginParms += _T("&assignedSystemName=") + aSysName;
    loginParms += _T("&chartid=") + chart->chartID;
    loginParms += _T("&order=") + chart->orderRef;
    loginParms += _T("&quantityId=") + chart->quantityId;
    loginParms += _T("&slot=") + sSlot;
    
    int iResponseCode = 0;
    size_t res = 0;
    std::string responseBody;
    
#ifdef __OCPN_USE_CURL__    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
    res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    std::string a = post.GetDetailedErrorString();
    std::string b = post.GetErrorString();
    std::string c = post.GetResponseBody();
    
    responseBody = post.GetResponseBody();
    //printf("%s", post.GetResponseBody().c_str());
    
    //wxString tt(post.GetResponseBody().data(), wxConvUTF8);
    //wxLogMessage(tt);
#else
     wxString postresult;
    //qDebug() << url.mb_str();
    //qDebug() << loginParms.mb_str();

    _OCPN_DLStatus stat = OCPN_postDataHttp( url, loginParms, postresult, g_timeout_secs );

    //qDebug() << "getChartList Post Stat: " << stat;
    
    if(stat != OCPN_DL_FAILED){
        wxCharBuffer buf = postresult.ToUTF8();
        std::string response(buf.data());
        
        //qDebug() << response.c_str();
        responseBody = response.c_str();
        iResponseCode = 200;
        res = 1;
    }

#endif    

#if 0
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    const char* s = loginParms.mb_str(wxConvUTF8);    

    size_t res = post.Post( s, strlen(s), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
#endif
if(iResponseCode == 200){
        wxString result = ProcessResponse(responseBody);
        
        return checkResult(result);
    }
    else
        return checkResponseCode(iResponseCode);
    

    return 0;
}

void shopPanel::doDownload(oeSencChartPanel *chartDownload, int slot)
{
    itemChart *chart = chartDownload->m_pChart;

    //  Create a destination file name for the download.
    wxURI uri;
    
    wxString downloadURL = chart->fileDownloadURL0;
    if(slot == 1)
        downloadURL = chart->fileDownloadURL1;

    chart->filedownloadSize0.ToDouble(&g_targetDownloadSizeMB);
    if( slot == 1 )
        chart->filedownloadSize1.ToDouble(&g_targetDownloadSizeMB);
    
    uri.Create(downloadURL);
    
    wxString serverFilename = uri.GetPath();
    wxString b = uri.GetServer();
    
    wxFileName fn(serverFilename);
    wxString fileTarget = fn.GetFullName();
    
    wxString downloadFile = g_PrivateDataDir + fileTarget;
    chart->downloadingFile = downloadFile;
    
    //wxLogMessage(_T("downloadURL0 ") + chart->fileDownloadURL0);
    //wxLogMessage(_T("downloadURL1 ") + chart->fileDownloadURL1);
    //wxLogMessage(_T("downloadURL: ") + downloadURL);
    //wxLogMessage(_T("serverFilename: ") + serverFilename);
    //wxLogMessage(_T("fileTarget: ") + fileTarget);
    //wxLogMessage(_T("downloadFile: ") + downloadFile);
    
    if(fileTarget.IsEmpty() || !g_targetDownloadSizeMB ){
        wxLogMessage(_T("fileTarget is empty, download aborted"));
        return;
    }
        

#if __OCPN_USE_CURL__        
    downloadOutStream = new wxFFileOutputStream(downloadFile);
    
    g_curlDownloadThread = new wxCurlDownloadThread(g_CurlEventHandler);
    g_curlDownloadThread->SetURL(downloadURL);
    g_curlDownloadThread->SetOutputStream(downloadOutStream);
    g_curlDownloadThread->Download();
#else
    if(!g_bconnected){
        Connect(wxEVT_DOWNLOAD_EVENT, (wxObjectEventFunction)(wxEventFunction)&shopPanel::onDLEvent);
        g_bconnected = true;
    }
 
    OCPN_downloadFileBackground( downloadURL, downloadFile, this, &g_FileDownloadHandle);

#endif
    return;
}

#ifndef __OCPN_USE_CURL__

void shopPanel::onDLEvent(OCPN_downloadEvent &evt)
{
    //qDebug() << "onDLEvent";
    
    wxDateTime now = wxDateTime::Now();
    
    switch(evt.getDLEventCondition()){
        case OCPN_DL_EVENT_TYPE_END:
        {
            m_bTransferComplete = true;
            m_bTransferSuccess = (evt.getDLEventStatus() == OCPN_DL_NO_ERROR) ? true : false;
            
            g_ipGauge->SetValue(100);
            setStatusTextProgress(_T(""));
            setStatusText( _("Status: OK"));
            m_buttonCancelOp->Hide();
            GetButtonUpdate()->Enable();

            //  Send an event to chain back to "Install" button
            wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED);
            event.SetId( ID_CMD_BUTTON_INSTALL_CHAIN );
            g_shopPanel->GetEventHandler()->AddPendingEvent(event);

            break;
        }   
        case OCPN_DL_EVENT_TYPE_PROGRESS:

            dl_now = evt.getTransferred();
            dl_total = evt.getTotal();
            g_targetDownloadSize = dl_total;

            
            // Calculate the gauge value
            if(dl_total > 0){
                float progress = dl_now/dl_total;
                
                g_ipGauge->SetValue(progress * 100);
                g_ipGauge->Refresh();
                
            }
            
            if(now.GetTicks() != g_progressTicks){
                
                //  Set text status
                wxString tProg;
                tProg = _("Downloaded:  ");
                wxString msg;
                msg.Printf( _T("(%6.1f MiB / %4.0f MiB)    "), (float)(evt.getTransferred() / 1e6), (float)(evt.getTotal() / 1e6));
                tProg += msg;
                
                setStatusTextProgress( tProg );
                
                g_progressTicks = now.GetTicks();
            }
            
            break;
            
        case OCPN_DL_EVENT_TYPE_START:
        case OCPN_DL_EVENT_TYPE_UNKNOWN:    
        default:
            break;
    }
}


#endif

bool ExtractZipFiles( const wxString& aZipFile, const wxString& aTargetDir, wxString &tlDir, bool aStripPath, wxDateTime aMTime, bool aRemoveZip )
{
    bool ret = true;
    wxString topDir;
    
#ifdef __OCPN__ANDROID__
    int nStrip = 0;
    if(aStripPath)
        nStrip = 1;
    
    wxString tlDirAndroid;
    ret = AndroidUnzip(aZipFile, aTargetDir, tlDirAndroid, nStrip, true);
    tlDir = tlDirAndroid;
    
#else

    std::unique_ptr<wxZipEntry> entry(new wxZipEntry());
    
    do
    {
        //wxLogError(_T("chartdldr_pi: Going to extract '")+aZipFile+_T("'."));
        wxFileInputStream in(aZipFile);
        
        if( !in )
        {
            wxLogError(_T("Can not open file '")+aZipFile+_T("'."));
            ret = false;
            break;
        }
        wxZipInputStream zip(in);
        ret = false;
        
        while( entry.reset(zip.GetNextEntry()), entry.get() != NULL )
        {
            // access meta-data
            wxString name = entry->GetName();
            if( aStripPath )
            {
                wxFileName fn(name);
                /* We can completly replace the entry path */
                //fn.SetPath(aTargetDir);
                //name = fn.GetFullPath();
                /* Or only remove the first dir (eg. ENC_ROOT) */
                if (fn.GetDirCount() > 0)
                    fn.RemoveDir(0);
                name = aTargetDir + wxFileName::GetPathSeparator() + fn.GetFullPath();
            }
            else
            {
                name = aTargetDir + wxFileName::GetPathSeparator() + name;
            }
            
            // read 'zip' to access the entry's data
            if( entry->IsDir() )
            {
                if(!topDir.Length())            // Save the top level directory
                    topDir = name;
                int perm = entry->GetMode();
                if( !wxFileName::Mkdir(name, perm, wxPATH_MKDIR_FULL) )
                {
                    wxLogError(_T("Can not create directory '") + name + _T("'."));
                    ret = false;
                    break;
                }
            }
            else
            {
                if( !zip.OpenEntry(*entry.get()) )
                {
                    wxLogError(_T("Can not open zip entry '") + entry->GetName() + _T("'."));
                    ret = false;
                    break;
                }
                if( !zip.CanRead() )
                {
                    wxLogError(_T("Can not read zip entry '") + entry->GetName() + _T("'."));
                    ret = false;
                    break;
                }
                
                wxFileName fn(name);
                if( !fn.DirExists() )
                {
                    if( !wxFileName::Mkdir(fn.GetPath()) )
                    {
                        wxLogError(_T("Can not create directory '") + fn.GetPath() + _T("'."));
                        ret = false;
                        break;
                    }
                }
                
                wxFileOutputStream file(name);
                
                g_shopPanel->setStatusText( _("Unzipping chart set files...") + fn.GetFullName());
                wxYield();
                
                if( !file )
                {
                    wxLogError(_T("Can not create file '")+name+_T("'."));
                    ret = false;
                    break;
                }
                zip.Read(file);
                fn.SetTimes(&aMTime, &aMTime, &aMTime);
                ret = true;
            }
            
        }
        
    }
    while(false);
    
    if(!topDir.Length()){        // if there is no directory in the zip file, then declare
                                 // the tlDir as the zip file name itself, without extension
       wxFileName fn(aZipFile);
       topDir = aTargetDir + wxFileName::GetPathSeparator() + fn.GetName();
    }

    // Strip trailing '/'
    if(topDir.EndsWith(wxFileName::GetPathSeparator()))
        topDir = topDir.BeforeLast(wxFileName::GetPathSeparator());
    
    tlDir = topDir;
    
    if( aRemoveZip )
        wxRemoveFile(aZipFile);
#endif    
    return ret;
}


int doUnzip(itemChart *chart, int slot)
{
    wxString installDir;
    wxString downloadFile;
    
    if(slot == 0){
        downloadFile = chart->fileDownloadPath0;
        if(chart->installLocation0.Length()){
            installDir = chart->installLocation0;
        }
    }
    else if(slot == 1){
        downloadFile = chart->fileDownloadPath1;
        if(chart->installLocation1.Length()){
            installDir = chart->installLocation1;
        }
    }
    
    wxString chosenInstallDir;
    if(1/*!installDir.Length()*/){
        
        wxString installLocn = g_PrivateDataDir;
#ifdef __OCPN__ANDROID__
        installLocn = GetWritableDocumentsDir();        // /storage/emulated/0, typically
#endif        
        if(installDir.Length())
            installLocn = installDir;
        else if(g_lastInstallDir.Length())
            installLocn = g_lastInstallDir;
        
        wxString dirPath;
        int result = PlatformDirSelectorDialog( NULL, &dirPath, _("Choose chart set install location."), installLocn);
#if 0
        wxDirDialog dirSelector( NULL, _("Choose chart set install location."), installLocn, wxDD_DEFAULT_STYLE  );
        int result = dirSelector.ShowModal();
        
        if(result == wxID_OK){
            chosenInstallDir = dirSelector.GetPath();
        }
        else{
            return 1;
        }
#else
       if(result == wxID_OK){
            chosenInstallDir = dirPath;
        }
        else{
            return 1;
        }
#endif
    }
    else{
        chosenInstallDir = installDir;
    }
    
    g_shopPanel->setStatusText( _("Ready for unzipping chart set files."));
    g_shopPanel->Refresh(true);
    wxYield();
    
    // Ready for unzip

    g_shopPanel->setStatusText( _("Unzipping chart set files..."));
    wxYield();
        
    ::wxBeginBusyCursor();
    wxString tlDir;
    bool ret = ExtractZipFiles( downloadFile, chosenInstallDir, tlDir, false, wxDateTime::Now(), false);
    ::wxEndBusyCursor();
        
    if(!ret){
        wxLogError(_T("oesenc_pi: Unable to extract: ") + downloadFile );
        OCPNMessageBox_PlugIn(NULL, _("Error extracting zip file"), _("oeSENC_pi Message"), wxOK);
        return 2;
    }

    // The unzip process will return the name of the first directory encountered in the zip file,
    //  or the zip file name itself, if no embedded dirs are found
    //  This is the name to add to the chart database
    wxString targetAddDir = tlDir;
#ifdef __OCPN__ANDROID__
        qDebug() << "CoverCheck1: " << targetAddDir.mb_str();
#endif
        
    //  If the currect core chart directories do not cover this new directory, then add it
    bool covered = false;
    for( size_t i = 0; i < GetChartDBDirArrayString().GetCount(); i++ )
    {
#ifdef __OCPN__ANDROID__
        qDebug() << "CoverCheck2: " << GetChartDBDirArrayString().Item(i).mb_str();
#endif

        if( targetAddDir.StartsWith((GetChartDBDirArrayString().Item(i))) )
        {
            covered = true;
            break;
        }
    }
#ifdef __OCPN__ANDROID__
        qDebug() << "CoverCheck3: " << covered;
#endif
    if( !covered )
    {
        AddChartDirectory( targetAddDir );
    }
    
    //  Is this an update?
    wxString lastInstalledZip;
    bool b_update = false;
    
    if(slot == 0){
        if(!chart->installedEdition0.IsSameAs(chart->lastRequestEdition0)){
            b_update = true;
            lastInstalledZip = chart->installedFileDownloadPath0;
        }
    }
    else if(slot == 1){
        if(!chart->installedEdition1.IsSameAs(chart->lastRequestEdition1)){
            b_update = true;
            lastInstalledZip = chart->installedFileDownloadPath1;
        }
    }
    
    if(b_update){
        
        // It would be nice here to remove the now obsolete chart dir from the OCPN core set.
        //  Not possible with current API, 
        //  So, best we can do is to rename it, so that it will disappear from the scanning.
 
        wxString installParent;
        if(slot == 0)
            installParent = chart->installLocation0;
        else if(slot == 1)
            installParent = chart->installLocation1;
 
        if(installParent.Len() && lastInstalledZip.Len()){
            wxFileName fn(lastInstalledZip);
            wxString lastInstall = installParent + wxFileName::GetPathSeparator() + fn.GetName();
                
            if(!lastInstall.IsSameAs(targetAddDir)){
                if(::wxDirExists(lastInstall)){
                    
                    //const wxString obsDir = lastInstall + _T(".OBSOLETE");
                    //bool success = ::wxRenameFile(lastInstall, obsDir);
                    
                    // Delete all the files in this directory
                    wxArrayString files;
                    wxDir::GetAllFiles(lastInstall, &files);
                    for(unsigned int i = 0 ; i < files.GetCount() ; i++){
                        ::wxRemoveFile(files[i]);
                    }
                    ::wxRmdir(lastInstall);
                    
                }
            }
        }
    }
    
    // Update the config persistence
    if(slot == 0){
        chart->installLocation0 = chosenInstallDir;
        chart->installedEdition0 = chart->lastRequestEdition0;
        chart->installedFileDownloadPath0 = chart->fileDownloadPath0;
    }
    else if(slot == 1){
        chart->installLocation1 = chosenInstallDir;
        chart->installedEdition1 = chart->lastRequestEdition1;
        chart->installedFileDownloadPath1 = chart->fileDownloadPath1;
    }
    
    g_lastInstallDir = chosenInstallDir;
    
    // We can delete the downloaded zip file here
    wxRemoveFile( downloadFile );
    
    ForceChartDBUpdate();
    
    saveShopConfig();
    
    
    return 0;
}

    

int doShop(){
    
    loadShopConfig();
   
    //  Do we need an initial login to get the persistent key?
    if(g_loginKey.Len() == 0){
        doLogin();
        saveShopConfig();
    }
    
    getChartList();
    
    return 0;
}


class MyStaticTextCtrl : public wxStaticText {
public:
    MyStaticTextCtrl(wxWindow* parent,
                                       wxWindowID id,
                                       const wxString& label,
                                       const wxPoint& pos,
                                       const wxSize& size = wxDefaultSize,
                                       long style = 0,
                                       const wxString& name= "staticText" ):
                                       wxStaticText(parent,id,label,pos,size,style,name){};
                                       void OnEraseBackGround(wxEraseEvent& event) {};
                                       DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MyStaticTextCtrl,wxStaticText)
EVT_ERASE_BACKGROUND(MyStaticTextCtrl::OnEraseBackGround)
END_EVENT_TABLE()


BEGIN_EVENT_TABLE(oeSencChartPanel, wxPanel)
EVT_PAINT ( oeSencChartPanel::OnPaint )
EVT_ERASE_BACKGROUND(oeSencChartPanel::OnEraseBackground)
END_EVENT_TABLE()

oeSencChartPanel::oeSencChartPanel(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, itemChart *p_itemChart, shopPanel *pContainer)
:wxPanel(parent, id, pos, size, wxBORDER_NONE)
{
    m_pContainer = pContainer;
    m_pChart = p_itemChart;
    m_bSelected = false;

    m_refDim = GetCharHeight();
    SetMinSize(wxSize(-1, 5 * m_refDim));
    m_unselectedHeight = 5 * m_refDim;          // might be recalculated based on chart title length
    m_selectedHeight = 9 * m_refDim;
#ifdef __OCPN__ANDROID__
    m_selectedHeight = 18 * m_refDim;
#endif    
    
    Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(oeSencChartPanel::OnClickDown), NULL, this);
#ifdef __OCPN__ANDROID__
    Connect(wxEVT_LEFT_UP, wxMouseEventHandler(oeSencChartPanel::OnClickUp), NULL, this);
#endif
    
    
}

oeSencChartPanel::~oeSencChartPanel()
{
}

static  wxStopWatch swclick;
static  int downx, downy;

void oeSencChartPanel::OnClickDown( wxMouseEvent &event )
{
#ifdef __OCPN__ANDROID__    
    swclick.Start();
    event.GetPosition( &downx, &downy );
#else
    DoChartSelected();
#endif    
}

void oeSencChartPanel::OnClickUp( wxMouseEvent &event )
{
#ifdef __OCPN__ANDROID__    
    qDebug() << swclick.Time();
    if(swclick.Time() < 200){
        int upx, upy;
        event.GetPosition(&upx, &upy);
        if( (fabs(upx-downx) < GetCharWidth()) && (fabs(upy-downy) < GetCharWidth()) ){
            DoChartSelected();
        }
    }
    swclick.Start();
#endif    
}

void oeSencChartPanel::DoChartSelected( )
{
    // Do not allow de-selection by mouse if this chart is busy, i.e. being prepared, or being downloaded 
    if(m_pChart){
       if(g_statusOverride.Length())
           return;
    }
           
    if(!m_bSelected){
        SetSelected( true );
        m_pContainer->SelectChart( this );
    }
    else{
        SetSelected( false );
        m_pContainer->SelectChart( NULL );
    }
}

void oeSencChartPanel::SetSelected( bool selected )
{
    m_bSelected = selected;
    wxColour colour;
   
    bool bCompact = false;
#ifdef __OCPN__ANDROID__
    if(g_shopPanel->GetSize().x < 60 * m_refDim)
        bCompact = true;
#endif
    
    wxString nameString = m_pChart->chartName;
    if(!m_pChart->quantityId.IsSameAs(_T("1")))
        nameString += _T(" (") + m_pChart->quantityId + _T(")");

    if (selected)
    {
        GetGlobalColor(_T("UIBCK"), &colour);
        m_boxColour = colour;
        SetMinSize(wxSize(-1, m_selectedHeight));
    }
    else
    {
        GetGlobalColor(_T("DILG0"), &colour);
        m_boxColour = colour;

        // Measure the required size
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 3/2;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
           
        SetFont( *qFont );
        HardBreakWrapper wrapper(this, nameString, g_shopPanel->GetSize().x / 2);
        SetFont( *dFont );

        int lineCount = wrapper.GetLineCount() + 1;
        m_unselectedHeight = (m_refDim * 3) + (lineCount * (m_refDim * 3/2));
        SetMinSize(wxSize(-1, m_unselectedHeight));
        m_nameArrayString = wrapper.GetLineArray();
    }
    
    Refresh( true );
    
}


extern "C"  DECL_EXP bool GetGlobalColor(wxString colorName, wxColour *pcolour);

void oeSencChartPanel::OnEraseBackground( wxEraseEvent &event )
{
}

void oeSencChartPanel::OnPaint( wxPaintEvent &event )
{
    int width, height;
    GetSize( &width, &height );
    int refDim = m_refDim;
    
    bool bCompact = false;
#ifdef __OCPN__ANDROID__
    if(width < 60 * refDim)
        bCompact = true;
#endif
    
    wxPaintDC dc( this );
 
    //dc.SetBackground(*wxLIGHT_GREY);
    
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.DrawRectangle(GetVirtualSize());
    
    wxColour c;
    
    wxString nameString = m_pChart->chartName;
    if(!m_pChart->quantityId.IsSameAs(_T("1")))
        nameString += _T(" (") + m_pChart->quantityId + _T(")");
    
    if(m_bSelected){
        dc.SetBrush( wxBrush( m_boxColour ) );
        
        GetGlobalColor( _T ( "UITX1" ), &c );
        dc.SetPen( wxPen( wxColor(0xCE, 0xD5, 0xD6), 3 ));
        
        dc.DrawRoundedRectangle( 0, 0, width-1, height-1, height / 10);
        
        int base_offset = height / 10;
        
        // Draw the thumbnail
        int scaledWidth = height;
        
        if(!bCompact){
           int scaledHeight = (height - (2 * base_offset)) * 95 / 100;
           wxBitmap &bm = m_pChart->GetChartThumbnail( scaledHeight );
        
           if(bm.IsOk()){
              dc.DrawBitmap(bm, base_offset + 3, base_offset + 3);
           }
        }
        else
            scaledWidth = 0;
        
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 3/2;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
        dc.SetFont( *qFont );
        
        int text_x = scaledWidth * 12 / 10;
        dc.SetTextForeground(wxColour(0,0,0));
        
        int nameWidth;
        dc.GetTextExtent( nameString, &nameWidth, NULL);
        if(nameWidth > width)
            nameString = wxControl::Ellipsize(nameString, dc, wxELLIPSIZE_END, width); 

        dc.DrawText(nameString, text_x, height * 5 / 100);
        
        int hTitle = dc.GetCharHeight();
        int y_line = (height * 5 / 100) + hTitle;
        dc.DrawLine( text_x, y_line, width - base_offset, y_line);
        
        dc.SetFont( *dFont );           // Restore default font
        int offset = GetCharHeight();
        
        int yPitch = GetCharHeight();
        int yPos = y_line + 4;
        wxString tx;
        
        int text_x_val = scaledWidth + ((width - scaledWidth) * 4 / 10);
        
        if(bCompact)
            text_x_val = scaledWidth + ((width - scaledWidth) * 5 / 10);
            
        // Create and populate the current chart information
        tx = _("Chart Set Edition:");
        dc.DrawText( tx, text_x, yPos);
        int editionLabelWidth;                  // Account for extra long translation
        dc.GetTextExtent( tx, &editionLabelWidth, NULL);
        tx = m_pChart->currentChartEdition;
        if(( editionLabelWidth + text_x) > text_x_val)
            dc.DrawText( tx, text_x + editionLabelWidth + GetCharWidth(), yPos);
        else
            dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Order Reference:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->orderRef;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Purchase date:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->purchaseDate.BeforeFirst(' ');
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Expiration date:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->expDate.BeforeFirst(' ');
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Status:");
        dc.DrawText( tx, text_x, yPos);
         tx = m_pChart->getStatusString();
         if(g_statusOverride.Len())
             tx = g_statusOverride;
         dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;

        tx = _("Assignments:");
        dc.DrawText( tx, text_x, yPos);
        
        wxString txs;
        wxColor tcolor = wxColour(0,0,0);
        tx = _T("1: ");
        if(!bCompact)
            txs = m_pChart->getKeytypeString( 0, tcolor );
        else
            txs = m_pChart->getKeyString( 0, tcolor );
        
        tx += txs;
        if(!txs.Length())
            tx += _("Unassigned");
        dc.SetTextForeground(tcolor);
        dc.DrawText( tx, text_x_val, yPos);
        dc.SetTextForeground(wxColour(0,0,0));
        
        yPos += yPitch;

        tcolor = wxColour(0,0,0);
        tx = _T("2: ");
        if(!bCompact)
            txs = m_pChart->getKeytypeString( 1, tcolor );
        else
            txs = m_pChart->getKeyString( 1, tcolor );

        tx += txs;
        if(!txs.Length())
            tx += _("Unassigned");
        dc.SetTextForeground(tcolor);
        dc.DrawText( tx, text_x_val, yPos);
        dc.SetTextForeground(wxColour(0,0,0));
        yPos += yPitch;
       
    }
    else{
        dc.SetBrush( wxBrush( m_boxColour ) );
    
        GetGlobalColor( _T ( "UITX1" ), &c );
        dc.SetPen( wxPen( c, 1 ) );
    
        int offset = height / 10;
        dc.DrawRectangle( offset, offset, width - (2 * offset), height - (2 * offset));
    
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        if(!bCompact){
            // Draw the thumbnail
            int scaledHeight = (height - (2 * offset)) * 95 / 100;
            wxBitmap &bm = m_pChart->GetChartThumbnail( scaledHeight );
        
            if(bm.IsOk())
                dc.DrawBitmap(bm, offset + 3, offset + 3);
        
            int scaledWidth = bm.GetWidth() * scaledHeight / bm.GetHeight();
        
            double font_size = dFont->GetPointSize() * 3/2;
            wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
            dc.SetFont( *qFont );

            dc.SetTextForeground(wxColour(128, 128, 128));
        
            if(m_pContainer->GetSelectedChart())
                dc.SetTextForeground(wxColour(220,220,220));
        
            dc.DrawText(nameString, scaledWidth * 15 / 10, height * 35 / 100);
        }
        else{
           double font_size = dFont->GetPointSize() * 3/2;
           wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
           dc.SetFont( *qFont );
           
           dc.SetTextForeground(wxColour(128, 128, 128));
        
            if(m_pContainer->GetSelectedChart())
                dc.SetTextForeground(wxColour(220,220,220));
        
            int yp = height * 20 / 100;
            int lineCount = m_nameArrayString.GetCount();
            if(lineCount > 1)
                yp = height * 10 / 100;

            for(unsigned int i = 0; i < m_nameArrayString.GetCount(); i++){
                dc.DrawText(m_nameArrayString.Item(i), refDim, yp);
                yp += GetCharHeight();
            }
        }
        
    }
    
}


BEGIN_EVENT_TABLE( chartScroller, wxScrolledWindow )
//EVT_PAINT(chartScroller::OnPaint)
EVT_ERASE_BACKGROUND(chartScroller::OnEraseBackground)
END_EVENT_TABLE()

chartScroller::chartScroller(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
: wxScrolledWindow(parent, id, pos, size, style)
{
    int yyp = 3;
}

void chartScroller::OnEraseBackground(wxEraseEvent& event)
{
    wxASSERT_MSG
    (
        GetBackgroundStyle() == wxBG_STYLE_ERASE,
     "shouldn't be called unless background style is \"erase\""
    );
    
    wxDC& dc = *event.GetDC();
    dc.SetPen(*wxGREEN_PEN);
    
    // clear any junk currently displayed
    dc.Clear();
    
    PrepareDC( dc );
    
    const wxSize size = GetVirtualSize();
    for ( int x = 0; x < size.x; x += 15 )
    {
        dc.DrawLine(x, 0, x, size.y);
    }
    
    for ( int y = 0; y < size.y; y += 15 )
    {
        dc.DrawLine(0, y, size.x, y);
    }
    
    dc.SetTextForeground(*wxRED);
    dc.SetBackgroundMode(wxSOLID);
    dc.DrawText("This text is drawn from OnEraseBackground", 60, 160);
    
}

void chartScroller::DoPaint(wxDC& dc)
{
    PrepareDC(dc);
    
//    if ( m_eraseBgInPaint )
    {
        dc.SetBackground(*wxRED_BRUSH);
        
        // Erase the entire virtual area, not just the client area.
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(GetBackgroundColour());
        dc.DrawRectangle(GetVirtualSize());
        
        dc.DrawText("Background erased in OnPaint", 65, 110);
    }
//     else if ( GetBackgroundStyle() == wxBG_STYLE_PAINT )
//     {
//         dc.SetTextForeground(*wxRED);
//         dc.DrawText("You must enable erasing background in OnPaint to avoid "
//         "display corruption", 65, 110);
//     }
//     
//     dc.DrawBitmap( m_bitmap, 20, 20, true );
//     
//     dc.SetTextForeground(*wxRED);
//     dc.DrawText("This text is drawn from OnPaint", 65, 65);
}

void chartScroller::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
//     if ( m_useBuffer )
//     {
//         wxAutoBufferedPaintDC dc(this);
//         DoPaint(dc);
//     }
//     else
    {
        wxPaintDC dc(this);
        DoPaint(dc);
    }
}


BEGIN_EVENT_TABLE( shopPanel, wxPanel )
EVT_TIMER( 4357, shopPanel::OnPrepareTimer )
EVT_BUTTON( ID_CMD_BUTTON_INSTALL, shopPanel::OnButtonInstall )
EVT_BUTTON( ID_CMD_BUTTON_INSTALL_CHAIN, shopPanel::OnButtonInstallChain )
END_EVENT_TABLE()


shopPanel::shopPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
: wxPanel(parent, id, pos, size, style)
{
    loadShopConfig();
    
    g_bconnected = false;
    
#ifdef __OCPN_USE_CURL__
    g_CurlEventHandler = new OESENC_CURL_EvtHandler;
#endif    
    g_shopPanel = this;
    m_binstallChain = false;
    m_bAbortingDownload = false;
    
    m_ChartSelected = NULL;
    m_choiceSystemName = NULL;
    int ref_len = GetCharHeight();

    bool bCompact = false;
#ifdef __OCPN__ANDROID__
    qDebug() << "Compact Check" << ::wxGetDisplaySize().x << GetCharWidth();
        
    if(::wxGetDisplaySize().x < 60 * GetCharWidth())
        bCompact = true;
#endif
    
    wxBoxSizer* boxSizerTop = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(boxSizerTop);

    wxString sn = _("System Name:");
    sn += _T(" ");
    sn += g_systemName;
    
    if(!bCompact){
        wxGridSizer *sysBox = new wxGridSizer(2);
        boxSizerTop->Add(sysBox, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));

        m_staticTextSystemName = new wxStaticText(this, wxID_ANY, sn, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
        sysBox->Add(m_staticTextSystemName, 0, wxALL | wxALIGN_LEFT, WXC_FROM_DIP(5));

        m_buttonUpdate = new wxButton(this, wxID_ANY, _("Refresh Chart List"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
        m_buttonUpdate->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonUpdate), NULL, this);
        sysBox->Add(m_buttonUpdate, 0, wxRIGHT | wxALIGN_RIGHT, WXC_FROM_DIP(5));
    }
    else{
        m_staticTextSystemName = new wxStaticText(this, wxID_ANY, sn, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
        boxSizerTop->Add(m_staticTextSystemName, 0, wxALL | wxALIGN_LEFT, WXC_FROM_DIP(5));

        m_buttonUpdate = new wxButton(this, wxID_ANY, _("Refresh Chart List"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
        m_buttonUpdate->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonUpdate), NULL, this);
        boxSizerTop->Add(m_buttonUpdate, 0, wxRIGHT | wxALIGN_RIGHT, WXC_FROM_DIP(5));
    }

    wxStaticBoxSizer* staticBoxSizerChartList = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("My Chart Sets")), wxVERTICAL);
    boxSizerTop->Add(staticBoxSizerChartList, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));

//     wxPanel *cPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBG_STYLE_ERASE );
//     staticBoxSizerChartList->Add(cPanel, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
//     wxBoxSizer *boxSizercPanel = new wxBoxSizer(wxVERTICAL);
//     cPanel->SetSizer(boxSizercPanel);
    
    m_scrollWinChartList = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)),  wxVSCROLL | wxBG_STYLE_ERASE );
#ifdef __OCPN__ANDROID__    
    m_scrollWinChartList->SetScrollRate(0, 1);
    m_scrollRate = 1;
#else
    m_scrollWinChartList->SetScrollRate(0, 15);
    m_scrollRate = 15;
#endif     
   
    staticBoxSizerChartList->Add(m_scrollWinChartList, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    boxSizerCharts = new wxBoxSizer(wxVERTICAL);
    m_scrollWinChartList->SetSizer(boxSizerCharts);
    
    m_chartListPanel = new wxPanel(m_scrollWinChartList, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBG_STYLE_ERASE );
    boxSizerCharts->Add(m_chartListPanel, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    m_boxSizerchartListPanel = new wxBoxSizer(wxVERTICAL);
    m_chartListPanel->SetSizer(m_boxSizerchartListPanel);

    
    int size_scrollerLines = 15;
    if(bCompact)
        size_scrollerLines = 10;

#ifdef __OCPN__ANDROID__
    //size_scrollerLines = (::wxGetDisplaySize().y * 5 / 10) / GetCharHeight();
    size_scrollerLines = (::wxGetDisplaySize().y / GetCharHeight()) - 29;
    size_scrollerLines = wxMax(size_scrollerLines, 10);
#endif
    
    m_scrollWinChartList->SetMinSize(wxSize(-1,size_scrollerLines * GetCharHeight()));
    staticBoxSizerChartList->SetMinSize(wxSize(-1,(size_scrollerLines + 1) * GetCharHeight()));

    wxStaticBoxSizer* staticBoxSizerAction = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Actions")), wxVERTICAL);
    boxSizerTop->Add(staticBoxSizerAction, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));

    m_staticLine121 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    staticBoxSizerAction->Add(m_staticLine121, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    ///Buttons
    if(!bCompact){    
        wxGridSizer* gridSizerActionButtons = new wxFlexGridSizer(1, 2, 0, 0);
        staticBoxSizerAction->Add(gridSizerActionButtons, 1, wxALL|wxEXPAND, WXC_FROM_DIP(2));
    
        m_buttonInstall = new wxButton(this, ID_CMD_BUTTON_INSTALL, _("Install Selected Chart Set"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
        gridSizerActionButtons->Add(m_buttonInstall, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(2));
    
        m_buttonCancelOp = new wxButton(this, wxID_ANY, _("Cancel Operation"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
        m_buttonCancelOp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonCancelOp), NULL, this);
        gridSizerActionButtons->Add(m_buttonCancelOp, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(2));
    }
    else{
        m_buttonInstall = new wxButton(this, ID_CMD_BUTTON_INSTALL, _("Download Selected Chart Set"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
        staticBoxSizerAction->Add(m_buttonInstall, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(2));
    
        m_buttonCancelOp = new wxButton(this, wxID_ANY, _("Cancel Operation"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
        m_buttonCancelOp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonCancelOp), NULL, this);
        staticBoxSizerAction->Add(m_buttonCancelOp, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(2));
    }

    wxStaticLine* sLine1 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    staticBoxSizerAction->Add(sLine1, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    
    ///Status
    m_staticTextStatus = new wxStaticText(this, wxID_ANY, _("Status: Chart List Refresh required."), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    staticBoxSizerAction->Add(m_staticTextStatus, 0, wxALL|wxALIGN_LEFT, WXC_FROM_DIP(5));

    g_ipGauge = new InProgressIndicator(this, wxID_ANY, 100, wxDefaultPosition, wxSize(ref_len * 12, ref_len));
    staticBoxSizerAction->Add(g_ipGauge, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, WXC_FROM_DIP(5));
    
    SetName(wxT("shopPanel"));
    //SetSize(500,600);
    if (GetSizer()) {
        GetSizer()->Fit(this);
    }
    
    //  Turn off all buttons initially.
    m_buttonInstall->Hide();
    m_buttonCancelOp->Hide();
    
    UpdateChartList();
    
}

shopPanel::~shopPanel()
{
}

void shopPanel::RefreshSystemName()
{
    wxString sn = _("System Name:");
    sn += _T(" ");
    sn += g_systemName;
    
    m_staticTextSystemName->SetLabel(sn);
}


void shopPanel::SelectChart( oeSencChartPanel *chart )
{
    if (m_ChartSelected == chart)
        return;
    
    if (m_ChartSelected)
        m_ChartSelected->SetSelected(false);
    
    m_ChartSelected = chart;
    if (m_ChartSelected)
        m_ChartSelected->SetSelected(true);
    
    //m_scrollWinChartList->GetSizer()->Layout();
    m_chartListPanel->GetSizer()->Layout();
    m_scrollWinChartList->FitInside();
    
    MakeChartVisible(m_ChartSelected);
    
    UpdateActionControls();
    
    Layout();
    
    Refresh( true );
}

void shopPanel::SelectChartByID( wxString& id, wxString& order, wxString& qty)
{
    for(unsigned int i = 0 ; i < m_panelArray.GetCount() ; i++){
        itemChart *chart = m_panelArray.Item(i)->m_pChart;
        if(id.IsSameAs(chart->chartID) && order.IsSameAs(chart->orderRef) && qty.IsSameAs(chart->quantityId)){
            SelectChart(m_panelArray.Item(i));
            MakeChartVisible(m_ChartSelected);
        }
    }
}

void shopPanel::MakeChartVisible(oeSencChartPanel *chart)
{
    if(!chart)
        return;
    
    itemChart *vchart = chart->m_pChart;
    int ioff = 0;
    for(unsigned int i = 0 ; i < m_panelArray.GetCount() ; i++){
        itemChart *lchart = m_panelArray.Item(i)->m_pChart;
        if(vchart->chartID.IsSameAs(lchart->chartID) && vchart->orderRef.IsSameAs(lchart->orderRef) && vchart->quantityId.IsSameAs(lchart->quantityId)){
            m_scrollWinChartList->Scroll(-1, ioff / GetScrollRate());
        }
        else
            ioff += m_panelArray.Item(i)->GetUnselectedHeight();
            
    }
    

}


void shopPanel::OnButtonUpdate( wxCommandEvent& event )
{
    loadShopConfig();
    
    // Check the dongle
    g_dongleName.Clear();
    if(IsDongleAvailable()){
        g_dongleSN = GetDongleSN();
        char sName[20];
        snprintf(sName, 19, "sgl%08X", g_dongleSN);

        g_dongleName = wxString(sName);
        
        wxString sn = _("System Name:");
        sn += _T(" ");
        sn += g_dongleName + _T(" (") + _("USB Key Dongle") + _T(")");
        m_staticTextSystemName->SetLabel( sn );
        m_staticTextSystemName->Refresh();
 
    }
    else{
        wxString sn = _("System Name:");
        sn += _T(" ");
        sn += g_systemName;
        m_staticTextSystemName->SetLabel( sn );
        m_staticTextSystemName->Refresh();
    }
 
    
    //  Do we need an initial login to get the persistent key?
    if(g_loginKey.Len() == 0){
        if(doLogin() != 1)
            return;
        saveShopConfig();
    }
    
     setStatusText( _("Contacting o-charts server..."));
     g_ipGauge->Start();
     wxYield();

    ::wxBeginBusyCursor();
    int err_code = getChartList( true );               
    ::wxEndBusyCursor();
 
    // Could be a change in login_key, userName, or password.
    // if so, force a full (no_key) login, and retry
    if((err_code == 4) || (err_code == 5) || (err_code == 6)){
        setStatusText( _("Status: Login error."));
        g_ipGauge->Stop();
        wxYield();
        if(doLogin() != 1)      // if the second login attempt fails, return to GUI
            return;
        saveShopConfig();
        
        // Try to get the status one more time only.
        ::wxBeginBusyCursor();
        int err_code_2 = getChartList( true );               // no error code dialog, we handle here
        ::wxEndBusyCursor();
        
        if(err_code_2 != 0){                  // Some error on second getlist() try, if so just return to GUI
         
            if((err_code_2 == 4) || (err_code_2 == 5) || (err_code_2 == 6))
                setStatusText( _("Status: Login error."));
            else{
                wxString ec;
                ec.Printf(_T(" [ %d ]"), err_code_2);
                setStatusText( _("Status: Communications error.") + ec);
            }
            g_ipGauge->Stop();
            wxYield();
            return;
        }
    }
    
    else if(err_code != 0){                  // Some other error
        wxString ec;
        ec.Printf(_T(" [ %d ]"), err_code);
        setStatusText( _("Status: Communications error.") + ec);
        g_ipGauge->Stop();
        wxYield();
        return;
    }
    g_chartListUpdatedOK = true;
    
    bool bNeedSystemName = false;
    
    // User reset system name, and removed dongle
    if(!g_systemName.Len() && !g_dongleName.Len())
         bNeedSystemName = true;
    
    
    if(bNeedSystemName ){
        bool sname_ok = false;
        int itry = 0;
        while(!sname_ok && itry < 4){
            bool bnew = false;
            bool bcont = doSystemNameWizard( &bnew );
        
            if( !bcont ){
                if(g_systemName.IsSameAs(_T("CancelNameDialog"))){ // user "Cancel"
                    g_systemName.Clear();
                    break;
                }
                else{
                    g_systemName.Clear();       // Invalid systemName
                }                    
            }
            
            if(!g_systemName.Len()){
                wxString msg = _("Invalid System Name");
                msg += _T("\n") + _("A valid System Name is 3 to 15 characters in length.");
                msg += _T("\n") + _("No symbols or spaces are allowed.");
 
                OCPNMessageBox_PlugIn(GetOCPNCanvasWindow(), msg, _("oeSENC_pi Message"), wxOK);
                itry++;
            }
            
            else{
                if(g_systemNameDisabledArray.Index(g_systemName) != wxNOT_FOUND){
                    wxString msg = g_systemName + _T("\n");
                    msg += _("This System Name has been disabled\nPlease choose another SystemName");
                    OCPNMessageBox_PlugIn(GetOCPNCanvasWindow(), msg, _("oeSENC_pi Message"), wxOK);
                    itry++;
                }
                else if(bnew && (g_systemNameServerArray.Index(g_systemName) != wxNOT_FOUND)){
                        wxString msg = g_systemName + _T("\n");
                        msg += _("This System Name already exists.\nPlease choose another SystemName");
                        OCPNMessageBox_PlugIn(GetOCPNCanvasWindow(), msg, _("oeSENC_pi Message"), wxOK);
                        itry++;
                }
                else{
                    sname_ok = true;
                }
            }
        }
    }
    
    // If a new systemName was selected, verify on the server
    // If the server already has a systemName associated with this FPR, cancel the operation.
    if(bNeedSystemName && !g_systemName.IsEmpty()){
        
        if( doUploadXFPR( false ) != 0){

            g_systemName.Clear();
            saveShopConfig();           // Record the blank systemName
            
            wxString sn = _("System Name:");
            m_staticTextSystemName->SetLabel( sn );
            m_staticTextSystemName->Refresh();

            setStatusText( _("Status: Ready"));
            g_ipGauge->Stop();
            return;
        }
    }

    setStatusText( _("Status: Ready"));
    g_ipGauge->Stop();
    
    UpdateChartList();
    
    saveShopConfig();
}

void shopPanel::OnButtonInstallChain( wxCommandEvent& event )
{
    itemChart *chart = m_ChartSelected->m_pChart;
    if(!chart)
        return;
    
    // Chained through from download end event
        if(m_binstallChain){
            
            
            m_binstallChain = false;
            
            if(m_bAbortingDownload){
                m_bAbortingDownload = false;
                OCPNMessageBox_PlugIn(NULL, _("Chart set download cancelled."), _("oeSENC_PI Message"), wxOK);
                m_buttonInstall->Enable();
                return;
            }
            
            //  Download is apparently done.
            g_statusOverride.Clear();
            
            //We can check some data to be sure.
            
            // Check the length
            long dlFileLength = 0;
            wxFile tFile(chart->downloadingFile);
            if(tFile.IsOpened())
                dlFileLength = tFile.Length();

            double fileLength = (double)dlFileLength;
            
            bool lengthMatch = fabs(fileLength - g_targetDownloadSize) < (0.001 * g_targetDownloadSize);
            
            // Does the download destination file exist, and length match?
            
            if(!::wxFileExists( chart->downloadingFile) || !lengthMatch ){
                OCPNMessageBox_PlugIn(NULL, _("Chart set download error, missing file."), _("oeSENC_PI Message"), wxOK);
                m_buttonInstall->Enable();
                return;
            }
            

            // So far, so good.

            // Update the records
            if(m_activeSlot == 0){
                chart->fileDownloadPath0 = chart->downloadingFile;
            }
            else if(m_activeSlot == 1){
                chart->fileDownloadPath1 = chart->downloadingFile;
            }
             
            wxString msg = _("Chart set download complete.");
            msg +=_T("\n\n");
            msg += _("Proceed to install?");
            msg += _T("\n\n");
            int ret = -1;
            
            while(ret != wxID_YES){
                ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_PI Message"), wxYES_NO);
            
                if(ret == wxID_YES){
                    g_statusOverride = _("Installing charts");
                
                    int rv = doUnzip(chart, m_activeSlot);
                
                    g_statusOverride.Clear();
                    setStatusText( _("Status: Ready"));
                
                    if(0 == rv)
                        OCPNMessageBox_PlugIn(NULL, _("Chart set installation complete."), _("oeSENC_pi Message"), wxOK);
                
                    UpdateChartList();
                }
                else if(ret == wxID_NO)
                    break;

            }
            
            
            m_buttonInstall->Enable();
            return;
        }
}

enum{
    ACTION_UNKNOWN = 0,
    ACTION_DOWNLOAD_DONGLE,
    ACTION_ASSIGN_DONGLE,
    ACTION_DOWNLOAD_SYSTEM,
    ACTION_ASSIGN_SYSTEM,
    ACTION_REQUEST_DONGLE,
    ACTION_REQUEST_SYSTEM
};

int shopPanel::GetActiveSlotAction( itemChart *chart )
{
    int rv = -1;
    
    // Dongle present?
    if(g_dongleName.Len()){
        
        // If a dongle chart is in a "download" state, choose that slot
        if(chart->isSlotAssignedToMyDongle( 0 )){
            if(chart->statusID0.IsSameAs(_T("download"))){
                m_activeSlot = 0;
                m_action = ACTION_DOWNLOAD_DONGLE;
                return 0;
            }
        }
        if(chart->isSlotAssignedToMyDongle( 1 )){
            if(chart->statusID1.IsSameAs(_T("download"))){
                m_activeSlot = 1;
                m_action = ACTION_DOWNLOAD_DONGLE;
                return 1;
            }
        }
        
        //  If a slot is already assigned to installed dongle, choose that slot
        if(chart->statusID0.IsSameAs(_T("requestable"))){
            if( chart->sysID0.IsSameAs(g_dongleName)){
                m_activeSlot = 0;
                m_action = ACTION_REQUEST_DONGLE;
                return 0;
            }
        }
        if(chart->statusID1.IsSameAs(_T("requestable"))){
            if( chart->sysID1.IsSameAs(g_dongleName)){
                m_activeSlot = 1;
                m_action = ACTION_REQUEST_DONGLE;
                return 1;
            }
        }

        // If there is an empty slot, choose that slot for assignment of dongle
        if(chart->sysID0.IsEmpty()){
            m_activeSlot = 0;
            m_action = ACTION_ASSIGN_DONGLE;
            return 0;
        }
        if(chart->sysID1.IsEmpty()){
            m_activeSlot = 1;
            m_action = ACTION_ASSIGN_DONGLE;
            return 1;
        }
    }
    
    // No dongle
    // If any system chart is in a "download" state, choose that slot
        if(chart->isChartsetAssignedToMe( g_systemName )){
            if(chart->statusID0.IsSameAs(_T("download")) && chart->sysID0.IsSameAs(g_systemName)){
                m_activeSlot = 0;
                m_action = ACTION_DOWNLOAD_SYSTEM;
                return 0;
            }
            if(chart->statusID1.IsSameAs(_T("download")) && chart->sysID1.IsSameAs(g_systemName)){
                m_activeSlot = 1;
                m_action = ACTION_DOWNLOAD_SYSTEM;
                return 1;
            }
        }
        
        //  If a slot is already assigned to system, choose that slot
        if(chart->statusID0.IsSameAs(_T("requestable"))){
            if( chart->sysID0.IsSameAs(g_systemName)){
                m_activeSlot = 0;
                m_action = ACTION_REQUEST_SYSTEM;
                return 0;
            }
        }
        if(chart->statusID1.IsSameAs(_T("requestable"))){
            if( chart->sysID1.IsSameAs(g_systemName)){
                m_activeSlot = 1;
                m_action = ACTION_REQUEST_SYSTEM;
                return 1;
            }
        }

        // If there is an empty slot, choose that slot for assignment
        if(chart->sysID0.IsEmpty()){
            m_activeSlot = 0;
            m_action = ACTION_ASSIGN_SYSTEM;
            return 0;
        }
        if(chart->sysID1.IsEmpty()){
            m_activeSlot = 1;
            m_action = ACTION_ASSIGN_SYSTEM;
            return 1;
        }

        

        
    
    return rv;
}


void shopPanel::OnButtonInstall( wxCommandEvent& event )
{
        // Check the dongle
    g_dongleName.Clear();
    if(IsDongleAvailable()){
        g_dongleSN = GetDongleSN();
        char sName[20];
        snprintf(sName, 19, "sgl%08X", g_dongleSN);

        g_dongleName = wxString(sName);
    }

   
    itemChart *chart = m_ChartSelected->m_pChart;
    if(!chart)
        return;

    // Choose the "active" slot and action
    int activeSlot = GetActiveSlotAction( chart );
    if(activeSlot < 0){
        wxString msg(_("Unable to determine requested ACTION"));
        msg += _T("\n");
        msg += _("Please contact o-charts support.");
        OCPNMessageBox_PlugIn(GetOCPNCanvasWindow(), msg, _("oeSENC_pi Message"), wxOK);
        return;
    }       

    m_buttonInstall->Disable();
    m_buttonCancelOp->Show();
 
    if((m_action == ACTION_DOWNLOAD_DONGLE) || (m_action == ACTION_DOWNLOAD_SYSTEM) ){
        m_startedDownload = false;
        doDownloadGui();
        return;
    }

    // Otherwise, do the assign/prepare steps
    
    // Is this systemName known to the server?
    //   If not, need to upload XFPR first
    
    if(m_action == ACTION_ASSIGN_DONGLE){
        //if(g_systemNameServerArray.Index(g_dongleName) == wxNOT_FOUND)
        {
            if( doUploadXFPR( true ) != 0){
                g_statusOverride.Clear();
                setStatusText( _("Status: USB Key Dongle FPR upload error"));
                return;
            }
        }
    }
    else if(m_action == ACTION_ASSIGN_SYSTEM){
        //if(g_systemNameServerArray.Index(g_systemName) == wxNOT_FOUND)
        {
            if( doUploadXFPR( false ) != 0){
                g_statusOverride.Clear();
                setStatusText( _("Status: System FPR upload error"));
                return;
            }
        }
    }
    
    //  If need assignment, do it here.

    if(m_action == ACTION_ASSIGN_DONGLE){
        int assignResult = doAssign(chart, m_activeSlot, g_dongleName);
        if(assignResult != 0){
            g_statusOverride.Clear();
            setStatusText( _("Status: Assignment error"));
            m_buttonInstall->Enable();
            return;
        }
        //  If no error, update the action to immediately request a download
        m_action = ACTION_REQUEST_DONGLE;
    }
    
    if(m_action == ACTION_ASSIGN_SYSTEM){
        int assignResult = doAssign(chart, m_activeSlot, g_systemName);
        if(assignResult != 0){
            g_statusOverride.Clear();
            setStatusText( _("Status: Assignment error"));
            m_buttonInstall->Enable();
            return;
        }
        //  If no error, update the action to immediately request a download
        m_action = ACTION_REQUEST_SYSTEM;
    } 
        
    m_ChartSelectedID = chart->chartID;           // save a copy of the selected chart
    m_ChartSelectedOrder = chart->orderRef;
    m_ChartSelectedQty = chart->quantityId;
        
    if((m_action == ACTION_REQUEST_DONGLE) || (m_action == ACTION_REQUEST_SYSTEM) ){
        doPrepareGUI();
    }

    return;
    
}

int shopPanel::doPrepareGUI()
{
    m_buttonCancelOp->Show();
    
    setStatusText( _("Preparing charts..."));
    
    m_prepareTimerCount = 8;            // First status query happens in 2 seconds
    m_prepareProgress = 0;
    m_prepareTimeout = 60;
    
    m_prepareTimer.SetOwner( this, 4357 );
    
    // Might be processing, in which case we don't need request.
    
    itemChart *chart = m_ChartSelected->m_pChart;
    bool bNeedRequest = false;    
    if(m_activeSlot == 0){
        if(!chart->statusID0.IsSameAs(_T("download")))
            bNeedRequest = true;
    }
    else if(m_activeSlot == 1){
        if(!chart->statusID1.IsSameAs(_T("download")))
            bNeedRequest = true;
    }
    
    if(!bNeedRequest){
        m_prepareTimer.Start( 1000 );
        return 0;
    }
    
    
    int err_code = doPrepare(m_ChartSelected, m_activeSlot);
    if(err_code != 0){                  // Some error
            wxString ec;
            ec.Printf(_T(" [ %d ]"), err_code);     
            setStatusText( _("Status: Communications error.") + ec);
            if(g_ipGauge)
                g_ipGauge->SetValue(0);
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            return err_code;
    }
    else{
        m_prepareTimer.Start( 1000 );
    }
    
    return 0;
}

int shopPanel::doDownloadGui()
{
    setStatusText( _("Status: Downloading..."));
    //m_staticTextStatusProgress->Show();
    m_buttonCancelOp->Show();
    GetButtonUpdate()->Disable();
    
    g_statusOverride = _("Downloading...");
    UpdateChartList();
    
    wxYield();
    
    m_binstallChain = true;
    m_bAbortingDownload = false;
    
    doDownload(m_ChartSelected, m_activeSlot);
    
    return 0;
}

void shopPanel::OnButtonCancelOp( wxCommandEvent& event )
{
    if(m_prepareTimer.IsRunning()){
        m_prepareTimer.Stop();
        g_ipGauge->SetValue(0);
    }

#ifdef __OCPN_USE_CURL__
    if(g_curlDownloadThread){
        m_bAbortingDownload = true;
        g_curlDownloadThread->Abort();
        g_ipGauge->SetValue(0);
        setStatusTextProgress(_T(""));
        m_binstallChain = true;
    }
#endif

    setStatusText( _("Status: OK"));
    m_buttonCancelOp->Hide();
    
    g_statusOverride.Clear();
    m_buttonInstall->Enable();
    
    UpdateChartList();
    
}

void shopPanel::OnPrepareTimer(wxTimerEvent &evt)
{
    m_prepareTimerCount++;
    m_prepareProgress++;
    
    float progress = m_prepareProgress * 100 / m_prepareTimeout;
    
    if(g_ipGauge)
        g_ipGauge->SetValue(progress);
    
    
    //  Check the status every n seconds
    
    if((m_prepareTimerCount % 10) == 0){
        int err_code = getChartList(false);     // Do not show error dialogs
        
        /*
         * Safe to ignore errors, since this is only a status loop that will time out eventually
        if(err_code != 0){                  // Some error
            wxString ec;
            ec.Printf(_T(" [ %d ]"), err_code);     
            setStatusText( _("Status: Communications error.") + ec);
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            return;
        }
        */
        
        if(!m_ChartSelected) {               // No chart selected
            setStatusText( _("Status: OK"));
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            return;
        }
        
        itemChart *chart = m_ChartSelected->m_pChart;
        bool bDownloadReady = false;    
        if(m_activeSlot == 0){
            if(chart->statusID0.IsSameAs(_T("download")))
                bDownloadReady = true;
        }
        else if(m_activeSlot == 1){
            if(chart->statusID1.IsSameAs(_T("download")))
                bDownloadReady = true;
        }
        
        if(1/*bDownloadReady*/){
            UpdateChartList();
            wxYield();
        }
        
        if(bDownloadReady){
            if(g_ipGauge)
                g_ipGauge->SetValue(0);
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            doDownloadGui();
        }
            
    }
        
    if(m_prepareTimerCount >= m_prepareTimeout){
        m_prepareTimer.Stop();
        
        if(g_ipGauge)
            g_ipGauge->SetValue(100);
        
        wxString msg = _("Your chart set preparation is not complete.");
        msg += _T("\n");
        msg += _("You may continue to wait, or return to this screen later to complete the download.");
        msg += _T("\n");
        msg += _("You will receive an email message when preparation for download is complete");
        msg += _T("\n\n");
        msg += _("Continue waiting?");
        msg += _T("\n\n");
        
        int ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_PI Message"), wxYES_NO);
        
        if(ret == wxID_YES){
            m_prepareTimerCount = 0;
            m_prepareProgress = 0;
            m_prepareTimeout = 60;
            if(g_ipGauge)
                g_ipGauge->SetValue(0);
            
            m_prepareTimer.Start( 1000 );
        }
        else{
            if(g_ipGauge)
                g_ipGauge->SetValue(0);
            setStatusText( _("Status: OK"));
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            
            return;
        }
    }
}    


void shopPanel::UpdateChartList( )
{
    if(m_ChartSelected){
        itemChart *chart = m_ChartSelected->m_pChart;
        if(chart){
            m_ChartSelectedID = chart->chartID;           // save a copy of the selected chart
            m_ChartSelectedOrder = chart->orderRef;
            m_ChartSelectedQty = chart->quantityId;
        }
    }

    
    delete m_chartListPanel;
    
    m_chartListPanel = new wxPanel(m_scrollWinChartList, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBG_STYLE_ERASE );
    boxSizerCharts->Add(m_chartListPanel, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    m_boxSizerchartListPanel = new wxBoxSizer(wxVERTICAL);
    m_chartListPanel->SetSizer(m_boxSizerchartListPanel);

    m_panelArray.Clear();
    m_ChartSelected = NULL;

    
    // Add new panels
    for(unsigned int i=0 ; i < g_ChartArray.GetCount() ; i++){
        if(g_chartListUpdatedOK && g_ChartArray.Item(i)->isChartsetShow()){
            if(g_ChartArray.Item(i))
                g_ChartArray.Item(i)->GetChartThumbnail(100, true );              // attempt thumbnail download if necessary
            oeSencChartPanel *chartPanel = new oeSencChartPanel( m_chartListPanel, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), g_ChartArray.Item(i), this);
            chartPanel->SetSelected(false);
        
            m_boxSizerchartListPanel->Add( chartPanel, 0, wxEXPAND|wxALL, 0 );
            m_panelArray.Add( chartPanel );
        } 
    }
    
    
    m_scrollWinChartList->ClearBackground();
    m_scrollWinChartList->GetSizer()->Layout();

    m_scrollWinChartList->FitInside();
    Layout();

//     for(unsigned int i=0 ; i < g_ChartArray.GetCount() ; i++){
//         if(g_chartListUpdatedOK && g_ChartArray.Item(i)->isChartsetShow()){
//             chartPanel->SetSelected(false);
//         } 
//     }

    SelectChartByID(m_ChartSelectedID, m_ChartSelectedOrder, m_ChartSelectedQty);

    //m_scrollWinChartList->ClearBackground();
    
    UpdateActionControls();
    
    saveShopConfig();
    
    Refresh( true );
    
    return;
    

    // Capture the state of any selected chart
    if(m_ChartSelected){
        itemChart *chart = m_ChartSelected->m_pChart;
        if(chart){
            m_ChartSelectedID = chart->chartID;           // save a copy of the selected chart
            m_ChartSelectedOrder = chart->orderRef;
            m_ChartSelectedQty = chart->quantityId;
        }
    }
    
    m_scrollWinChartList->ClearBackground();
    
    // Clear any existing panels
    for(unsigned int i = 0 ; i < m_panelArray.GetCount() ; i++){
        delete m_panelArray.Item(i);
    }
    m_panelArray.Clear();
    m_ChartSelected = NULL;

    
    // Add new panels
    for(unsigned int i=0 ; i < g_ChartArray.GetCount() ; i++){
        if(g_chartListUpdatedOK && g_ChartArray.Item(i)->isChartsetShow()){
            oeSencChartPanel *chartPanel = new oeSencChartPanel( m_chartListPanel, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), g_ChartArray.Item(i), this);
            chartPanel->SetSelected(false);
        
            m_boxSizerchartListPanel->Add( chartPanel, 0, wxEXPAND|wxALL, 0 );
            m_panelArray.Add( chartPanel );
        } 
    }
    
    SelectChartByID(m_ChartSelectedID, m_ChartSelectedOrder, m_ChartSelectedQty);
    
   // m_scrollWinChartList->ClearBackground();
    m_scrollWinChartList->GetSizer()->Layout();

    Layout();

    //m_scrollWinChartList->ClearBackground();
    
    UpdateActionControls();
    
    saveShopConfig();
    
    Refresh( true );
}


void shopPanel::UpdateActionControls()
{
    //  Turn off all buttons.
    m_buttonInstall->Hide();
    
    
    if(!m_ChartSelected){                // No chart selected
        m_buttonInstall->Enable();
        return;
    }
    
    if(!g_statusOverride.Length()){
        m_buttonInstall->Enable();
    }
    
    itemChart *chart = m_ChartSelected->m_pChart;


    if(chart->getChartStatus() == STAT_PURCHASED){
        m_buttonInstall->SetLabel(_("Install Selected Chart Set"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_CURRENT){
        m_buttonInstall->SetLabel(_("Reinstall Selected Chart Set"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_STALE){
        m_buttonInstall->SetLabel(_("Update Selected Chart Set"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_READY_DOWNLOAD){
        m_buttonInstall->SetLabel(_("Download Selected Chart Set"));
        m_buttonInstall->Show();       
    }
    else if(chart->getChartStatus() == STAT_REQUESTABLE){
        m_buttonInstall->SetLabel(_("Download Selected Chart Set"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_PREPARING){
        m_buttonInstall->Hide();
    }
    
    
}

    
bool shopPanel::doSystemNameWizard( bool *bnew )
{
    if(bnew)
        *bnew = false;
    
    // Make sure the system name array is current
    
    if( g_systemName.Len() && (g_systemNameChoiceArray.Index(g_systemName) == wxNOT_FOUND))
        g_systemNameChoiceArray.Insert(g_systemName, 0);
    
    oeSENCSystemNameSelector dlg( GetOCPNCanvasWindow());
    
    wxSize dialogSize(500, -1);
    
    #ifdef __OCPN__ANDROID__
    wxSize ss = ::wxGetDisplaySize();
    dialogSize.x = ss.x * 8 / 10;
    #endif         
    dlg.SetSize(dialogSize);
    dlg.Centre();
    
    
    #ifdef __OCPN__ANDROID__
    //androidHideBusyIcon();
    #endif             
    int ret = dlg.ShowModal();
    ret = dlg.GetReturnCode();
    
    if(ret == 0){               // OK
        wxString sName = dlg.getRBSelection();
        if(g_systemNameChoiceArray.Index(sName) == wxNOT_FOUND){
            // Is it the dongle selected?
            if(sName.Find(_T("Dongle")) != wxNOT_FOUND){
                wxString ssName = sName.Mid(0, 11);
                g_systemNameChoiceArray.Insert(ssName, 0);
                sName = ssName;
            }
            else{    
                sName = doGetNewSystemName();
                if(sName.Len()){
                    g_systemNameChoiceArray.Insert(sName, 0);
                    if(bnew)
                        *bnew = true;
                }
                else
                    return false;
            }
        }
        if(sName.Len())
            g_systemName = sName;
    }
    else{
        g_systemName = _T("CancelNameDialog");
        return false;
    }
    
    wxString sn = _("System Name:");
    sn += _T(" ");
    sn += g_systemName;
    m_staticTextSystemName->SetLabel( sn );
    m_staticTextSystemName->Refresh();
    
    saveShopConfig();
    
    return true;
}

wxString shopPanel::doGetNewSystemName( )
{
    oeSENCGETSystemName dlg( GetOCPNCanvasWindow());
    
    wxSize dialogSize(500, -1);
    
    #ifdef __OCPN__ANDROID__
    wxSize ss = ::wxGetDisplaySize();
    dialogSize.x = ss.x * 8 / 10;
    #endif         
    dlg.SetSize(dialogSize);
    dlg.Centre();
    
    int ret = dlg.ShowModal();
    
    wxString sName;
    if(ret == 0){               // OK
        sName = dlg.GetNewName();
        
        // Check system name rules...
        const char *s = sName.c_str();
        if( (strlen(s) < 3) || (strlen(s) > 15))
            return wxEmptyString;
        
        char *t = (char *)s;
        for(unsigned int i = 0; i < strlen(s); i++, t++){
            bool bok = false;
            if( ((*t >= 'a') && (*t <= 'z')) ||
                ((*t >= 'A') && (*t <= 'Z')) ||
                ((*t >= '0') && (*t <= '9')) ){
                
                bok = true;
            }
            else{
                sName.Clear();
                break;
            }
        }
    }
    
    
    return sName;
}

void shopPanel::OnGetNewSystemName( wxCommandEvent& event )
{
    doGetNewSystemName();
}

// void shopPanel::OnChangeSystemName( wxCommandEvent& event )
// {
//     doSystemNameWizard();
// }

   


IMPLEMENT_DYNAMIC_CLASS( oeSENCGETSystemName, wxDialog )
BEGIN_EVENT_TABLE( oeSENCGETSystemName, wxDialog )
   EVT_BUTTON( ID_GETIP_CANCEL, oeSENCGETSystemName::OnCancelClick )
   EVT_BUTTON( ID_GETIP_OK, oeSENCGETSystemName::OnOkClick )
END_EVENT_TABLE()
 
 
 oeSENCGETSystemName::oeSENCGETSystemName()
 {
 }
 
 oeSENCGETSystemName::oeSENCGETSystemName( wxWindow* parent, wxWindowID id, const wxString& caption,
                                             const wxPoint& pos, const wxSize& size, long style )
 {
     
     long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
     wxDialog::Create( parent, id, caption, pos, size, wstyle );
     
     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     CreateControls();
     GetSizer()->SetSizeHints( this );
     Centre();
     
 }
 
 oeSENCGETSystemName::~oeSENCGETSystemName()
 {
     delete m_SystemNameCtl;
 }
 
 /*!
  * oeSENCGETSystemName creator
  */
 
 bool oeSENCGETSystemName::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                    const wxPoint& pos, const wxSize& size, long style )
 {
     SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
     
     long wstyle = style;
     #ifdef __WXMAC__
     wstyle |= wxSTAY_ON_TOP;
     #endif
     wxDialog::Create( parent, id, caption, pos, size, wstyle );
     
     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     SetTitle( _("New OpenCPN oeSENC System Name"));
     
     CreateControls(  );
     Centre();
     return TRUE;
 }
 
 
 void oeSENCGETSystemName::CreateControls(  )
 {
     int ref_len = GetCharHeight();
     
     oeSENCGETSystemName* itemDialog1 = this;
     
     wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
     itemDialog1->SetSizer( itemBoxSizer2 );
     
     wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( itemDialog1, wxID_ANY, _("Enter New System Name") );
     
     wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer( itemStaticBoxSizer4Static, wxVERTICAL );
     itemBoxSizer2->Add( itemStaticBoxSizer4, 0, wxEXPAND | wxALL, 5 );
     
     wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
     itemStaticBoxSizer4->Add( itemStaticText5, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP, 5 );
     
     m_SystemNameCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), 0 );
     itemStaticBoxSizer4->Add( m_SystemNameCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM , 5 );
     
     wxStaticText *itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,  _("A valid System Name is 3 to 15 characters in length."));
     itemBoxSizer2->Add( itemStaticTextLegend, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );

//      wxStaticText *itemStaticTextLegend1 = new wxStaticText( itemDialog1, wxID_STATIC,  _("lower case letters and numbers only."));
//      itemBoxSizer2->Add( itemStaticTextLegend1, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );
     
     wxStaticText *itemStaticTextLegend2 = new wxStaticText( itemDialog1, wxID_STATIC,  _("No symbols or spaces are allowed."));
     itemBoxSizer2->Add( itemStaticTextLegend2, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );
     
     
     wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
     itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );
     
     {
         m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
         itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
         m_CancelButton->SetDefault();
     }
     
     m_OKButton = new wxButton( itemDialog1, ID_GETIP_OK, _("OK"), wxDefaultPosition,
                                wxDefaultSize, 0 );
     itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
     
     
 }
 
 
 bool oeSENCGETSystemName::ShowToolTips()
 {
     return TRUE;
 }
 
 wxString oeSENCGETSystemName::GetNewName()
 {
     return m_SystemNameCtl->GetValue();
 }
 
 
 void oeSENCGETSystemName::OnCancelClick( wxCommandEvent& event )
 {
     EndModal(2);
 }
 
 void oeSENCGETSystemName::OnOkClick( wxCommandEvent& event )
 {
     if( m_SystemNameCtl->GetValue().Length() == 0 )
         EndModal(1);
     else {
         //g_systemNameChoiceArray.Insert(m_SystemNameCtl->GetValue(), 0);   // Top of the list
         
         EndModal(0);
     }
 }
 

 
 IMPLEMENT_DYNAMIC_CLASS( oeSENCSystemNameSelector, wxDialog )
 BEGIN_EVENT_TABLE( oeSENCSystemNameSelector, wxDialog )
    EVT_BUTTON( ID_GETIP_CANCEL, oeSENCSystemNameSelector::OnCancelClick )
    EVT_BUTTON( ID_GETIP_OK, oeSENCSystemNameSelector::OnOkClick )
 END_EVENT_TABLE()
 
 
 oeSENCSystemNameSelector::oeSENCSystemNameSelector()
 {
 }
 
 oeSENCSystemNameSelector::oeSENCSystemNameSelector( wxWindow* parent, wxWindowID id, const wxString& caption,
                                           const wxPoint& pos, const wxSize& size, long style )
 {
     
     long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
     wxDialog::Create( parent, id, caption, pos, size, wstyle );
     
     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     CreateControls();
     GetSizer()->SetSizeHints( this );
     Centre();
     
 }
 
 oeSENCSystemNameSelector::~oeSENCSystemNameSelector()
 {
 }
 
  
 bool oeSENCSystemNameSelector::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                   const wxPoint& pos, const wxSize& size, long style )
 {
     SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
     
     long wstyle = style;
     #ifdef __WXMAC__
     wstyle |= wxSTAY_ON_TOP;
     #endif
     wxDialog::Create( parent, id, caption, pos, size, wstyle );
     
     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     SetTitle( _("Select OpenCPN/oeSENC System Name"));
     
     CreateControls(  );
     Centre();
     return TRUE;
 }
 
 
 void oeSENCSystemNameSelector::CreateControls(  )
 {
     oeSENCSystemNameSelector* itemDialog1 = this;
     
     wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
     itemDialog1->SetSizer( itemBoxSizer2 );
     
     
     wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select your System Name from the following list, or "),
                                                       wxDefaultPosition, wxDefaultSize, 0 );
     
     itemBoxSizer2->Add( itemStaticText5, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );
 
     wxStaticText* itemStaticText6 = new wxStaticText( itemDialog1, wxID_STATIC, _(" create a new System Name for this computer."),
                                                       wxDefaultPosition, wxDefaultSize, 0 );
                                                       
     itemBoxSizer2->Add( itemStaticText6, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );
                                                       
     
     bool bDongleAdded = false;
     wxArrayString system_names;
     for(unsigned int i=0 ; i < g_systemNameChoiceArray.GetCount() ; i++){
         wxString candidate = g_systemNameChoiceArray.Item(i);
         if(candidate.StartsWith("sgl")){
             if(g_systemNameDisabledArray.Index(candidate) == wxNOT_FOUND){
                system_names.Add(candidate + _T(" (") + _("USB Key Dongle") + _T(")"));
                bDongleAdded = true;
             }
         }

         else if(g_systemNameDisabledArray.Index(candidate) == wxNOT_FOUND)
            system_names.Add(candidate);
     }
     
  
     // Add USB dongle if present, and not already added
     
     if(!bDongleAdded && IsDongleAvailable()){
        system_names.Add( g_dongleName  + _T(" (") + _("USB Key Dongle") + _T(")"));
     }
         
     system_names.Add(_("new..."));
     
     m_rbSystemNames = new wxRadioBox(this, wxID_ANY, _("System Names"), wxDefaultPosition, wxDefaultSize, system_names, 0, wxRA_SPECIFY_ROWS);
     
     itemBoxSizer2->Add( m_rbSystemNames, 0, wxALIGN_CENTER | wxALL, 25 );

     wxStaticLine* itemStaticLine = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
     itemBoxSizer2->Add( itemStaticLine, 0, wxEXPAND|wxALL, 0 );
     
     
     wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
     itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );
                                              
     m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
     itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
                                              
     m_OKButton = new wxButton( itemDialog1, ID_GETIP_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
     m_OKButton->SetDefault();
     itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
                                              
                                              
 }
 
 
 bool oeSENCSystemNameSelector::ShowToolTips()
 {
     return TRUE;
 }
 
 wxString oeSENCSystemNameSelector::getRBSelection(  )
 {
     return m_rbSystemNames->GetStringSelection();
 }
 
 void oeSENCSystemNameSelector::OnCancelClick( wxCommandEvent& event )
 {
     EndModal(2);
 }
 
 void oeSENCSystemNameSelector::OnOkClick( wxCommandEvent& event )
 {
     EndModal(0);
 }
 
 
 //------------------------------------------------------------------
 
 BEGIN_EVENT_TABLE( InProgressIndicator, wxGauge )
 EVT_TIMER( 4356, InProgressIndicator::OnTimer )
 END_EVENT_TABLE()
 
 InProgressIndicator::InProgressIndicator()
 {
 }
 
 InProgressIndicator::InProgressIndicator(wxWindow* parent, wxWindowID id, int range,
                     const wxPoint& pos, const wxSize& size,
                     long style, const wxValidator& validator, const wxString& name)
{
    wxGauge::Create(parent, id, range, pos, size, style, validator, name);
    
//    m_timer.Connect(wxEVT_TIMER, wxTimerEventHandler( InProgressIndicator::OnTimer ), NULL, this);
    m_timer.SetOwner( this, 4356 );
    m_timer.Start( 50 );
    
    m_bAlive = false;
    
}

InProgressIndicator::~InProgressIndicator()
{
    m_timer.Stop();
}
 
void InProgressIndicator::OnTimer(wxTimerEvent &evt)
{
    if(m_bAlive)
        Pulse();
}
 
 
void InProgressIndicator::Start() 
{
     m_timer.Start( 50 );
     m_bAlive = true;
}
 
void InProgressIndicator::Stop() 
{
     m_timer.Stop();
     m_bAlive = false;
     SetValue(0);
}

#ifdef __OCPN_USE_CURL__

//-------------------------------------------------------------------------------------------
OESENC_CURL_EvtHandler::OESENC_CURL_EvtHandler()
{
    Connect(wxCURL_BEGIN_PERFORM_EVENT, (wxObjectEventFunction)(wxEventFunction)&OESENC_CURL_EvtHandler::onBeginEvent);
    Connect(wxCURL_END_PERFORM_EVENT, (wxObjectEventFunction)(wxEventFunction)&OESENC_CURL_EvtHandler::onEndEvent);
    Connect(wxCURL_DOWNLOAD_EVENT, (wxObjectEventFunction)(wxEventFunction)&OESENC_CURL_EvtHandler::onProgressEvent);
    
}

OESENC_CURL_EvtHandler::~OESENC_CURL_EvtHandler()
{
}

void OESENC_CURL_EvtHandler::onBeginEvent(wxCurlBeginPerformEvent &evt)
{
 //   OCPNMessageBox_PlugIn(NULL, _("DLSTART."), _("oeSENC_PI Message"), wxOK);
    g_shopPanel->m_startedDownload = true;
}

void OESENC_CURL_EvtHandler::onEndEvent(wxCurlEndPerformEvent &evt)
{
 //   OCPNMessageBox_PlugIn(NULL, _("DLEnd."), _("oeSENC_PI Message"), wxOK);
    
    g_ipGauge->SetValue(0);
    g_shopPanel->setStatusTextProgress(_T(""));
    g_shopPanel->setStatusText( _("Status: OK"));
    g_shopPanel->m_buttonCancelOp->Hide();
    //g_shopPanel->GetButtonDownload()->Hide();
    g_shopPanel->GetButtonUpdate()->Enable();
    
    if(downloadOutStream){
        downloadOutStream->Close();
        downloadOutStream = NULL;
    }
    
    g_curlDownloadThread = NULL;

    if(g_shopPanel->m_bAbortingDownload){
        if(g_shopPanel->GetSelectedChart()){
            itemChart *chart = g_shopPanel->GetSelectedChart()->m_pChart;
            if(chart){
                if( chart->downloadingFile.Length() )
                    wxRemoveFile( chart->downloadingFile );
                chart->downloadingFile.Clear();
            }
        }
    }
            
    //  Send an event to chain back to "Install" button
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED);
    event.SetId( ID_CMD_BUTTON_INSTALL_CHAIN );
    g_shopPanel->GetEventHandler()->AddPendingEvent(event);
    
}


void OESENC_CURL_EvtHandler::onProgressEvent(wxCurlDownloadEvent &evt)
{
    dl_now = evt.GetDownloadedBytes();
    dl_total = evt.GetTotalBytes();
    g_targetDownloadSize = dl_total;
    
    // Calculate the gauge value
    if(evt.GetTotalBytes() > 0){
        float progress = evt.GetDownloadedBytes()/evt.GetTotalBytes();
        g_ipGauge->SetValue(progress * 100);
    }
    
    wxDateTime now = wxDateTime::Now();
    if(now.GetTicks() != g_progressTicks){
        std::string speedString = evt.GetHumanReadableSpeed(" ", 0);
    
    //  Set text status
        wxString tProg;
        tProg = _("Downloaded:  ");
        wxString msg;
        msg.Printf( _T("%6.1f MiB / %4.0f MiB    "), (float)(evt.GetDownloadedBytes() / 1e6), (float)(evt.GetTotalBytes() / 1e6));
        msg += wxString( speedString.c_str(), wxConvUTF8);
        tProg += msg;

        g_shopPanel->setStatusTextProgress( tProg );
        
        g_progressTicks = now.GetTicks();
    }
    
}
#endif

IMPLEMENT_DYNAMIC_CLASS( oeSENCLogin, wxDialog )
BEGIN_EVENT_TABLE( oeSENCLogin, wxDialog )
EVT_BUTTON( ID_GETIP_CANCEL, oeSENCLogin::OnCancelClick )
EVT_BUTTON( ID_GETIP_OK, oeSENCLogin::OnOkClick )
END_EVENT_TABLE()


oeSENCLogin::oeSENCLogin()
{
}

oeSENCLogin::oeSENCLogin( wxWindow* parent, wxWindowID id, const wxString& caption,
                                          const wxPoint& pos, const wxSize& size, long style )
{
    
    long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
    wxDialog::Create( parent, id, caption, pos, size, wstyle );
    
    wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    SetFont( *qFont );
    
    CreateControls();
    GetSizer()->SetSizeHints( this );
    Centre();
    
}

oeSENCLogin::~oeSENCLogin()
{
}

/*!
 * oeSENCLogin creator
 */

bool oeSENCLogin::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                  const wxPoint& pos, const wxSize& size, long style )
{
    SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
    
    long wstyle = style;
    #ifdef __WXMAC__
    wstyle |= wxSTAY_ON_TOP;
    #endif
    wxDialog::Create( parent, id, caption, pos, size, wstyle );
    
    wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    SetFont( *qFont );
    
    
    CreateControls(  );
    Centre();
    return TRUE;
}


void oeSENCLogin::CreateControls(  )
{
    int ref_len = GetCharHeight();
    
    oeSENCLogin* itemDialog1 = this;
    
    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
    itemDialog1->SetSizer( itemBoxSizer2 );
    
    wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( itemDialog1, wxID_ANY, _("Login to o-charts.org") );
    
    wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer( itemStaticBoxSizer4Static, wxVERTICAL );
    itemBoxSizer2->Add( itemStaticBoxSizer4, 0, wxEXPAND | wxALL, 5 );
    
    itemStaticBoxSizer4->AddSpacer(10);
    
    wxStaticLine *staticLine121 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    itemStaticBoxSizer4->Add(staticLine121, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    wxFlexGridSizer* flexGridSizerActionStatus = new wxFlexGridSizer(0, 2, 0, 0);
    flexGridSizerActionStatus->SetFlexibleDirection( wxBOTH );
    flexGridSizerActionStatus->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
    flexGridSizerActionStatus->AddGrowableCol(0);
    
    itemStaticBoxSizer4->Add(flexGridSizerActionStatus, 1, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _("email address:"), wxDefaultPosition, wxDefaultSize, 0 );
    flexGridSizerActionStatus->Add( itemStaticText5, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP, 5 );
    
    m_UserNameCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), 0 );
    flexGridSizerActionStatus->Add( m_UserNameCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM , 5 );
    
 
    wxStaticText* itemStaticText6 = new wxStaticText( itemDialog1, wxID_STATIC, _("Password:"), wxDefaultPosition, wxDefaultSize, 0 );
    flexGridSizerActionStatus->Add( itemStaticText6, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP, 5 );
    
    m_PasswordCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), 0 );
    flexGridSizerActionStatus->Add( m_PasswordCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM , 5 );
    
    
    wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
    itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );
    
    m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    
    m_OKButton = new wxButton( itemDialog1, ID_GETIP_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    m_OKButton->SetDefault();
    
    itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    
    
}


bool oeSENCLogin::ShowToolTips()
{
    return TRUE;
}



void oeSENCLogin::OnCancelClick( wxCommandEvent& event )
{
    EndModal(2);
}

void oeSENCLogin::OnOkClick( wxCommandEvent& event )
{
    if( (m_UserNameCtl->GetValue().Length() == 0 ) || (m_PasswordCtl->GetValue().Length() == 0 ) ){
        SetReturnCode(1);
        EndModal(1);
    }
    else {
        SetReturnCode(0);
        EndModal(0);
    }
}

void oeSENCLogin::OnClose( wxCloseEvent& event )
{
    SetReturnCode(2);
}

