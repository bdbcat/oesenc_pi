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


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include <wx/fileconf.h>
#include <wx/uri.h>
#include "wx/tokenzr.h"
#include <wx/dir.h>
#include "ochartShop.h"
#include "ocpn_plugin.h"
#include "wxcurl/wx/curl/http.h"
#include "wxcurl/wx/curl/thread.h"
#include <tinyxml.h>
//#include <../../opencpn/plugins/wmm_pi/src/WMMHeader.h>
#include "wx/wfstream.h"
#include <wx/zipstrm.h>
#include <memory>


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

shopPanel *g_shopPanel;
OESENC_CURL_EvtHandler *g_CurlEventHandler;
wxCurlDownloadThread *g_curlDownloadThread;
wxFFileOutputStream *downloadOutStream;
bool g_chartListUpdatedOK;
wxString g_statusOverride;
wxString g_lastInstallDir;

unsigned int g_dongleSN;
wxString g_dongleName;

#define ID_CMD_BUTTON_INSTALL 7783
#define ID_CMD_BUTTON_INSTALL_CHAIN 7784

bool IsDongleAvailable();
unsigned int GetDongleSN();

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
            sret = _("Preparing your chartset.");
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
            return _("USB Dongle Key") + _T("  [ ") + sysID0 +_T(" ]");
        if(isSlotAssignedToAnyDongle( 1 ))
            return _("USB Dongle Key") + _T("  [ ") + sysID1 +_T(" ]");
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
            return _("USB Dongle Key") + _T("  [ ") + sysID0 +_T(" ]");
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
            return _("USB Dongle Key") + _T("  [ ") + sysID1 +_T(" ]");
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


wxBitmap& itemChart::GetChartThumbnail(int size)
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
        else{
            if(g_chartListUpdatedOK && thumbnailURL.Length()){  // Do not access network until after first "getList"
                wxCurlHTTP get;
                get.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
                bool getResult = get.Get(file, thumbnailURL);

            // get the response code of the server
                int iResponseCode;
                get.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
            
                if(iResponseCode == 200){
                    if(::wxFileExists(file)){
                        m_ChartImage = wxImage( file, wxBITMAP_TYPE_ANY);
                    }
                }
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

            
int checkResult(wxString &result, bool bShowErrorDialog = true)
{
    if(g_shopPanel){
        g_shopPanel->getInProcessGuage()->Stop();
    }
    
    long dresult;
    if(result.ToLong(&dresult)){
        if(dresult == 1)
            return 0;
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
                    default:    
                        msg += _("Check your configuration and try again.");
                        break;
                }
                
                OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
            }
            return dresult;
        }
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
    oeSENCLogin login(g_shopPanel);
    login.ShowModal();
    if(!login.GetReturnCode() == 0){
        g_shopPanel->setStatusText( _("Invalid Login."));
        wxYield();
        return 55;
    }
    
    g_loginUser = login.m_UserNameCtl->GetValue();
    wxString pass = login.m_PasswordCtl->GetValue();
    
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=api");
    
    wxString loginParms;
    loginParms += _T("taskId=login");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&password=") + pass;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;
    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    if(iResponseCode == 200){
        TiXmlDocument * doc = new TiXmlDocument();
        const char *rr = doc->Parse( post.GetResponseBody().c_str());
        
        wxString p = wxString(post.GetResponseBody().c_str(), wxConvUTF8);
        wxLogMessage(_T("doLogin results:"));
        wxLogMessage(p);
        
        wxString queryResult;
        wxString loginKey;
        
        if( res )
        {
            TiXmlElement * root = doc->RootElement();
            if(!root){
                wxString r = _T("50");
                checkResult(r);                              // undetermined error??
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


wxString ProcessResponse(std::string body)
{
        TiXmlDocument * doc = new TiXmlDocument();
        const char *rr = doc->Parse( body.c_str());
    
        //doc->Print();
        
        wxString queryResult;
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

        // wxString p = wxString(body.c_str(), wxConvUTF8);
        // wxLogMessage(_T("ProcessResponse results:"));
        // wxLogMessage(p);

        
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

    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
    size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    std::string a = post.GetDetailedErrorString();
    std::string b = post.GetErrorString();
    std::string c = post.GetResponseBody();
    
    //printf("%s", post.GetResponseBody().c_str());
    
    wxString tt(post.GetResponseBody().data(), wxConvUTF8);
    //wxLogMessage(tt);
    
    if(iResponseCode == 200){
        wxString result = ProcessResponse(post.GetResponseBody());
        
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
    wxString msg = _("This action will PERMANENTLY assign the chart:");
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

    loginParms += _T("&systemName=") + systemName;
    loginParms += _T("&chartid=") + chart->chartID;
    loginParms += _T("&order=") + chart->orderRef;
    loginParms += _T("&quantityId=") + chart->quantityId;
    loginParms += _T("&slot=") + sSlot;
    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    if(iResponseCode == 200){
        wxString result = ProcessResponse(post.GetResponseBody());
        
        if(result.IsSameAs(_T("8"))){                    // Unknown system name
            return 0;
        }

        if(result.IsSameAs(_T("50"))){                  // General network error
            return 0;                                   // OK
        }
        
        return checkResult(result);
    }
    else
        return checkResponseCode(iResponseCode);
}

extern wxString getFPR( bool bCopyToDesktop, bool &bCopyOK, bool bSGLock);

int doUploadXFPR(bool bDongle)
{
    wxString err;
    
    // Generate the FPR file
    bool b_copyOK = false;
    
    wxString fpr_file = getFPR( false, b_copyOK, bDongle);              // No copy needed
    
    fpr_file = fpr_file.Trim(false);            // Trim leading spaces...
    
    if(fpr_file.Len()){
        
        wxString stringFPR;
        
        //Read the file, convert to ASCII hex, and build a string
        if(::wxFileExists(fpr_file)){
            wxString stringFPR;
            wxFileInputStream stream(fpr_file);
            while(stream.IsOk() && !stream.Eof() ){
                unsigned char c = stream.GetC();
                if(!stream.Eof()){
                    wxString sc;
                    sc.Printf(_T("%02X"), c);
                    stringFPR += sc;
                }
            }
            
            // Prepare the upload command string
            wxString url = userURL;
            if(g_admin)
                url = adminURL;
            
            url +=_T("?fc=module&module=occharts&controller=api");
            
            wxFileName fnxpr(fpr_file);
            wxString fprName = fnxpr.GetFullName();
            
            wxString loginParms;
            loginParms += _T("taskId=xfpr");
            loginParms += _T("&username=") + g_loginUser;
            loginParms += _T("&key=") + g_loginKey;
            if(g_debugShop.Len())
                loginParms += _T("&debug=") + g_debugShop;

            if(!bDongle)
                loginParms += _T("&systemName=") + g_systemName;
            else
                loginParms += _T("&systemName=") + g_dongleName;
                
            loginParms += _T("&xfpr=") + stringFPR;
            loginParms += _T("&xfprName=") + fprName;
            
            wxLogMessage(loginParms);
            
            wxCurlHTTPNoZIP post;
            post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
            size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
            
            // get the response code of the server
            int iResponseCode;
            post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
            
            if(iResponseCode == 200){
                wxString result = ProcessResponse(post.GetResponseBody());
                
                if(result.IsSameAs(_T("8"))){                    // Unknown system name
                    return 0;                                   // OK
                }

                if(result.IsSameAs(_T("50"))){                  // General network error
                    return 0;                                   // OK
                }
                
                int iret = checkResult(result);
                
                return iret;
            }
            else
                return checkResponseCode(iResponseCode);
            
        }
        else if(fpr_file.IsSameAs(_T("DONGLE_NOT_PRESENT")))
            err = _("  [USB Dongle not found.]");
            
        else
            err = _("  [fpr file not found.]");
    }
    else{
        err = _("  [fpr file not created.]");
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

    loginParms += _T("&assignedSystemName=") + aSysName;
    loginParms += _T("&chartid=") + chart->chartID;
    loginParms += _T("&order=") + chart->orderRef;
    loginParms += _T("&quantityId=") + chart->quantityId;
    loginParms += _T("&slot=") + sSlot;
    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    if(iResponseCode == 200){
        wxString result = ProcessResponse(post.GetResponseBody());
        
        if(result.IsSameAs(_T("50"))){                  // General network error
            return 0;                                   // OK
        }
        
        return checkResult(result);
    }
    else
        return checkResponseCode(iResponseCode);
    

    return 0;
}

int doDownload(oeSencChartPanel *chartDownload, int slot)
{
    itemChart *chart = chartDownload->m_pChart;

    //  Create a destination file name for the download.
    wxURI uri;
    
    wxString downloadURL = chart->fileDownloadURL0;
    if(slot == 1)
        downloadURL = chart->fileDownloadURL1;

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
    
    if(fileTarget.IsEmpty()){
        wxLogMessage(_T("fileTarget is empty, download aborted"));
        return 0;
    }
        
    
    downloadOutStream = new wxFFileOutputStream(downloadFile);
    
    g_curlDownloadThread = new wxCurlDownloadThread(g_CurlEventHandler);
    g_curlDownloadThread->SetURL(downloadURL);
    g_curlDownloadThread->SetOutputStream(downloadOutStream);
    g_curlDownloadThread->Download();
 

    return 0;
}

bool ExtractZipFiles( const wxString& aZipFile, const wxString& aTargetDir, bool aStripPath, wxDateTime aMTime, bool aRemoveZip )
{
    bool ret = true;
    
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
                
                g_shopPanel->setStatusText( _("Unzipping chart files...") + fn.GetFullName());
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
    
    if( aRemoveZip )
        wxRemoveFile(aZipFile);
    
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
        if(installDir.Length())
            installLocn = installDir;
        else if(g_lastInstallDir.Length())
            installLocn = g_lastInstallDir;
        
        wxDirDialog dirSelector( NULL, _("Choose chart install location."), installLocn, wxDD_DEFAULT_STYLE  );
        int result = dirSelector.ShowModal();
        
        if(result == wxID_OK){
            chosenInstallDir = dirSelector.GetPath();
        }
        else{
            return 1;
        }
    }
    else{
        chosenInstallDir = installDir;
    }
    
    g_shopPanel->setStatusText( _("Ready for unzipping chart files."));
    g_shopPanel->Refresh(true);
    wxYield();
    
    // Ready for unzip
    if( downloadFile.Lower().EndsWith(_T("zip")) ) //Zip compressed
    {
        g_shopPanel->setStatusText( _("Unzipping chart files..."));
        wxYield();
        
        ::wxBeginBusyCursor();
        bool ret = ExtractZipFiles( downloadFile, chosenInstallDir, false, wxDateTime::Now(), false);
        ::wxEndBusyCursor();
        
        if(!ret){
            wxLogError(_T("oesenc_pi: Unable to extract: ") + downloadFile );
            OCPNMessageBox_PlugIn(NULL, _("Error extracting zip file"), _("oeSENC_pi Message"), wxOK);
            return 2;
        }
    }
    
    //  We know that the unzip process puts all charts in a subdir whose name in the "downloadFile", without extension
    //  This is the dir that we want to add to database.
    wxFileName fn(downloadFile);
    wxString chartDir = fn.GetName();
    wxString targetAddDir = chosenInstallDir + wxFileName::GetPathSeparator() + chartDir;
    
    //  If the currect core chart directories do not cover this new directory, then add it
    bool covered = false;
    for( size_t i = 0; i < GetChartDBDirArrayString().GetCount(); i++ )
    {
        if( targetAddDir.StartsWith((GetChartDBDirArrayString().Item(i))) )
        {
            covered = true;
            break;
        }
    }
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

    int refHeight = GetCharHeight();
    SetMinSize(wxSize(-1, 5 * refHeight));
    m_unselectedHeight = 5 * refHeight;
    
//     wxBoxSizer* itemBoxSizer01 = new wxBoxSizer(wxHORIZONTAL);
//     SetSizer(itemBoxSizer01);
     Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(oeSencChartPanel::OnChartSelected), NULL, this);
//     
    
}

oeSencChartPanel::~oeSencChartPanel()
{
}

void oeSencChartPanel::OnChartSelected( wxMouseEvent &event )
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
    int refHeight = GetCharHeight();
    
    if (selected)
    {
        GetGlobalColor(_T("UIBCK"), &colour);
        m_boxColour = colour;
        SetMinSize(wxSize(-1, 9 * refHeight));
    }
    else
    {
        GetGlobalColor(_T("DILG0"), &colour);
        m_boxColour = colour;
        SetMinSize(wxSize(-1, 5 * refHeight));
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
        
        int scaledHeight = (height - (2 * base_offset)) * 95 / 100;
        wxBitmap &bm = m_pChart->GetChartThumbnail( scaledHeight );
        
        if(bm.IsOk()){
            dc.DrawBitmap(bm, base_offset + 3, base_offset + 3);
        }
        
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 3/2;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
        
        int text_x = scaledWidth * 12 / 10;
        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(0,0,0));
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
        
        // Create and populate the current chart information
        tx = _("Chart Edition:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->currentChartEdition;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Order Reference:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->orderRef;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Purchase date:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->purchaseDate;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Expiration date:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->expDate;
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
        
        wxColor tcolor = wxColour(0,0,0);
        tx = _T("1: ");
        wxString txs = m_pChart->getKeytypeString( 0, tcolor );
        tx += txs;
        if(!txs.Length())
            tx += _("Available");
        dc.SetTextForeground(tcolor);
        dc.DrawText( tx, text_x_val, yPos);
        dc.SetTextForeground(wxColour(0,0,0));
        
        yPos += yPitch;

        tcolor = wxColour(0,0,0);
        tx = _T("2: ");
        txs = m_pChart->getKeytypeString( 1, tcolor );
        tx += txs;
        if(!txs.Length())
            tx += _("Available");
        dc.SetTextForeground(tcolor);
        dc.DrawText( tx, text_x_val, yPos);
        dc.SetTextForeground(wxColour(0,0,0));
        yPos += yPitch;
       
        //  If the chart is unassigned, then there is no defined "key type".
//         if(m_pChart->m_status != STAT_PURCHASED){
//             tx = m_pChart->getKeytypeString();
//             if(tx.Len()){
//                 tx = _("Key type:");
//                 dc.DrawText( tx, text_x, yPos);
//                 tx = m_pChart->getKeytypeString();
//                 dc.DrawText( tx, text_x_val, yPos);
//                 yPos += yPitch;
//             }
//         }


#if 0        
        dc.SetBrush( wxBrush( m_boxColour ) );
        
        GetGlobalColor( _T ( "UITX1" ), &c );
        dc.SetPen( wxPen( c, 3 ));
        
        dc.DrawRoundedRectangle( 0, 0, width-1, height-1, height / 10);
         
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 4/3;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());

        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(0,0,0));
        dc.DrawText(m_pChart->chartName, 5, 5);
        
        dc.SetFont( *dFont );
        
        int offset = GetCharHeight();
        
        int yPitch = GetCharHeight();
        int yPos = yPitch * 2;
        int xcolumn = GetCharHeight();
        wxString tx; 
        // Create and populate the current chart information
/*        
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
  */      
        tx = _("Chart Edition: ") + m_pChart->currentChartEdition;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;

        tx = _("Order Reference: ") + m_pChart->orderRef;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;

        yPos = yPitch * 2;
        xcolumn = width / 2;
        
        tx = _("Purchase date: ") + m_pChart->purchaseDate;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("Expiration date: ") + m_pChart->expDate;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        dc.DrawLine( offset, yPos + 3, width - offset, yPos + 3);
        yPos += 6;
        
        
        //  The two assignment slots
        xcolumn = GetCharHeight();
        int yTable = yPos;
        wxString adjStatus;
        
        tx = _("Assigment 1");
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("System: ") + m_pChart->sysID0;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        tx = _("Installed edition: ") + m_pChart->lastRequestEdition0;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        adjStatus = m_pChart->statusID0;
//         if(adjStatus.IsSameAs(_T("requestable")))
//             adjStatus = _("ok");
        
        tx = _("Status: ") + adjStatus;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
 
        yPos = yTable;
        xcolumn = width / 2;
        
        tx = _("Assigment 2");
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("System: ") + m_pChart->sysID1;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        tx = _("Installed edition: ") + m_pChart->lastRequestEdition1;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        adjStatus = m_pChart->statusID1;
//        if(adjStatus.IsSameAs(_T("requestable")))
//            adjStatus = _("ok");
        
        tx = _("Status: ") + adjStatus;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
#endif
        
        
    }
    else{
        dc.SetBrush( wxBrush( m_boxColour ) );
    
        GetGlobalColor( _T ( "UITX1" ), &c );
        dc.SetPen( wxPen( c, 1 ) );
    
        int offset = height / 10;
        dc.DrawRectangle( offset, offset, width - (2 * offset), height - (2 * offset));
    
        // Draw the thumbnail
        int scaledHeight = (height - (2 * offset)) * 95 / 100;
        wxBitmap &bm = m_pChart->GetChartThumbnail( scaledHeight );
        
        if(bm.IsOk()){
            dc.DrawBitmap(bm, offset + 3, offset + 3);
        }
        
        int scaledWidth = bm.GetWidth() * scaledHeight / bm.GetHeight();
        
        
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 3/2;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());

        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(128, 128, 128));
        
        if(m_pContainer->GetSelectedChart())
            dc.SetTextForeground(wxColour(220,220,220));
        
        dc.DrawText(nameString, scaledWidth * 15 / 10, height * 35 / 100);
        
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
    
    g_CurlEventHandler = new OESENC_CURL_EvtHandler;
    
    g_shopPanel = this;
    m_binstallChain = false;
    m_bAbortingDownload = false;
    
    m_ChartSelected = NULL;
    m_choiceSystemName = NULL;
    int ref_len = GetCharHeight();
    
    wxBoxSizer* boxSizerTop = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(boxSizerTop);
    
    wxGridSizer *sysBox = new wxGridSizer(2);
    boxSizerTop->Add(sysBox, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    wxString sn = _("System Name:");
    sn += _T(" ");
    sn += g_systemName;
    
    m_staticTextSystemName = new wxStaticText(this, wxID_ANY, sn, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    sysBox->Add(m_staticTextSystemName, 0, wxALL | wxALIGN_LEFT, WXC_FROM_DIP(5));

    m_buttonUpdate = new wxButton(this, wxID_ANY, _("Refresh Chart List"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    m_buttonUpdate->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonUpdate), NULL, this);
    sysBox->Add(m_buttonUpdate, 0, wxRIGHT | wxALIGN_RIGHT, WXC_FROM_DIP(5));
    
    wxStaticBoxSizer* staticBoxSizerChartList = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("My Charts")), wxVERTICAL);
    boxSizerTop->Add(staticBoxSizerChartList, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));

    wxPanel *cPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBG_STYLE_ERASE );
    staticBoxSizerChartList->Add(cPanel, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    wxBoxSizer *boxSizercPanel = new wxBoxSizer(wxVERTICAL);
    cPanel->SetSizer(boxSizercPanel);
    
    m_scrollWinChartList = new wxScrolledWindow(cPanel, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBORDER_RAISED | wxVSCROLL | wxBG_STYLE_ERASE );
    m_scrollWinChartList->SetScrollRate(5, 5);
    boxSizercPanel->Add(m_scrollWinChartList, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    boxSizerCharts = new wxBoxSizer(wxVERTICAL);
    m_scrollWinChartList->SetSizer(boxSizerCharts);
 
    m_scrollWinChartList->SetMinSize(wxSize(-1,15 * GetCharHeight()));
    staticBoxSizerChartList->SetMinSize(wxSize(-1,16 * GetCharHeight()));
    
    wxStaticBoxSizer* staticBoxSizerAction = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Actions")), wxVERTICAL);
    boxSizerTop->Add(staticBoxSizerAction, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));

    m_staticLine121 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    staticBoxSizerAction->Add(m_staticLine121, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    ///Buttons
    wxGridSizer* gridSizerActionButtons = new wxGridSizer(1, 2, 0, 0);
    staticBoxSizerAction->Add(gridSizerActionButtons, 1, wxALL|wxEXPAND, WXC_FROM_DIP(2));
    
    m_buttonInstall = new wxButton(this, ID_CMD_BUTTON_INSTALL, _("Install Selected Chart"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    gridSizerActionButtons->Add(m_buttonInstall, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(2));
    
    m_buttonCancelOp = new wxButton(this, wxID_ANY, _("Cancel Operation"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    m_buttonCancelOp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonCancelOp), NULL, this);
    gridSizerActionButtons->Add(m_buttonCancelOp, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(2));

    wxStaticLine* sLine1 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    staticBoxSizerAction->Add(sLine1, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    
    ///Status
    m_staticTextStatus = new wxStaticText(this, wxID_ANY, _("Status: Chart List Refresh required."), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    staticBoxSizerAction->Add(m_staticTextStatus, 0, wxALL|wxALIGN_LEFT, WXC_FROM_DIP(5));

    m_ipGauge = new InProgressIndicator(this, wxID_ANY, 100, wxDefaultPosition, wxSize(ref_len * 12, ref_len));
    staticBoxSizerAction->Add(m_ipGauge, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, WXC_FROM_DIP(5));
/*
    wxString sn = _("System Name:");
    sn += _T(" ");
    sn += g_systemName;
    
    m_staticTextSystemName = new wxStaticText(this, wxID_ANY, sn, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    staticBoxSizerAction->Add(m_staticTextSystemName, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
*/    
    
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
    
    m_scrollWinChartList->GetSizer()->Layout();
    
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
    
    for(unsigned int i = 0 ; i < m_panelArray.GetCount() ; i++){
        itemChart *lchart = m_panelArray.Item(i)->m_pChart;
        if(vchart->chartID.IsSameAs(lchart->chartID) && vchart->orderRef.IsSameAs(lchart->orderRef) && vchart->quantityId.IsSameAs(lchart->quantityId)){
            
            int offset = i * chart->GetUnselectedHeight();
            
            m_scrollWinChartList->Scroll(-1, offset / 5);
        }
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
     m_ipGauge->Start();
     wxYield();

    ::wxBeginBusyCursor();
    int err_code = getChartList( false );               // no error code dialog, we handle here
    ::wxEndBusyCursor();
 
    // Could be a change in login_key, userName, or password.
    // if so, force a full (no_key) login, and retry
    if((err_code == 4) || (err_code == 5) || (err_code == 6)){
        setStatusText( _("Status: Login error."));
        m_ipGauge->Stop();
        wxYield();
        if(doLogin() != 1)      // if the second login attempt fails, return to GUI
            return;
        saveShopConfig();
        
        // Try to get the status one more time only.
        ::wxBeginBusyCursor();
        int err_code_2 = getChartList( false );               // no error code dialog, we handle here
        ::wxEndBusyCursor();
        
        if(err_code_2 != 0){                  // Some error on second getlist() try, if so just return to GUI
         
            if((err_code_2 == 4) || (err_code_2 == 5) || (err_code_2 == 6))
                setStatusText( _("Status: Login error."));
            else{
                wxString ec;
                ec.Printf(_T(" [ %d ]"), err_code_2);
                setStatusText( _("Status: Communications error.") + ec);
            }
            m_ipGauge->Stop();
            wxYield();
            return;
        }
    }
    
    else if(err_code != 0){                  // Some other error
        wxString ec;
        ec.Printf(_T(" [ %d ]"), err_code);
        setStatusText( _("Status: Communications error.") + ec);
        m_ipGauge->Stop();
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
    
    setStatusText( _("Status: Ready"));
    m_ipGauge->Stop();
    
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
                OCPNMessageBox_PlugIn(NULL, _("Chart download cancelled."), _("oeSENC_PI Message"), wxOK);
                m_buttonInstall->Enable();
                return;
            }
            
            //  Download is apparently done.
            g_statusOverride.Clear();
            
            //We can check some data to be sure.
            
            // Does the download destination file exist?
            
            if(!::wxFileExists( chart->downloadingFile )){
                OCPNMessageBox_PlugIn(NULL, _("Chart download error, missing file."), _("oeSENC_PI Message"), wxOK);
                m_buttonInstall->Enable();
                return;
            }
            
            /*        
             *        long dlFileLength = 0;
             *        
             *        wxFile tFile(dlFile);
             *        if(tFile.IsOpened())
             *            dlFileLength = tFile.Length();
             *        else
             *            return;
             *        
             *        long testLength;
             *        if(!dlSize.ToLong(&testLength))
             *            return;
             *        
             *        if(testLength != dlFileLength){
             *            OCPNMessageBox_PlugIn(NULL, _("Chart download error, wrong file length."), _("oeSENC_PI Message"), wxOK);
             *            return;
             }
             */      
            // So far, so good.
            
            // Download exists

            // Update the records
            if(m_activeSlot == 0){
                chart->fileDownloadPath0 = chart->downloadingFile;
            }
            else if(m_activeSlot == 1){
                chart->fileDownloadPath1 = chart->downloadingFile;
            }
             
            wxString msg = _("Chart download complete.");
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
                        OCPNMessageBox_PlugIn(NULL, _("Chart installation complete."), _("oeSENC_pi Message"), wxOK);
                
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
            if(chart->statusID0.IsSameAs(_T("download"))){
                m_activeSlot = 0;
                m_action = ACTION_DOWNLOAD_SYSTEM;
                return 0;
            }
            if(chart->statusID1.IsSameAs(_T("download"))){
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
        if(g_systemNameServerArray.Index(g_dongleName) == wxNOT_FOUND){
            if( doUploadXFPR( true ) != 0){
                g_statusOverride.Clear();
                setStatusText( _("Status: Dongle FPR upload error"));
                return;
            }
        }
    }
    else if(m_action == ACTION_ASSIGN_SYSTEM){
        if(g_systemNameServerArray.Index(g_systemName) == wxNOT_FOUND){
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
    
    setStatusText( _("Preparing your charts..."));
    
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
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
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
    
    int err_code = doDownload(m_ChartSelected, m_activeSlot);
    
    return 0;
}

void shopPanel::OnButtonCancelOp( wxCommandEvent& event )
{
    if(m_prepareTimer.IsRunning()){
        m_prepareTimer.Stop();
        m_ipGauge->SetValue(0);
    }
    
    if(g_curlDownloadThread){
        m_bAbortingDownload = true;
        g_curlDownloadThread->Abort();
        m_ipGauge->SetValue(0);
        setStatusTextProgress(_T(""));
        m_binstallChain = true;
    }
    
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
    
    if(m_ipGauge)
        m_ipGauge->SetValue(progress);
    
    
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
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            doDownloadGui();
        }
            
    }
        
    if(m_prepareTimerCount >= m_prepareTimeout){
        m_prepareTimer.Stop();
        
        if(m_ipGauge)
            m_ipGauge->SetValue(100);
        
        wxString msg = _("Your chart preparation is not complete.");
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
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
            
            m_prepareTimer.Start( 1000 );
        }
        else{
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
            setStatusText( _("Status: OK"));
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            
            return;
        }
    }
}    


void shopPanel::UpdateChartList( )
{
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
            oeSencChartPanel *chartPanel = new oeSencChartPanel( m_scrollWinChartList, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), g_ChartArray.Item(i), this);
            chartPanel->SetSelected(false);
        
            boxSizerCharts->Add( chartPanel, 0, wxEXPAND|wxALL, 0 );
//            boxSizerCharts->Layout();
            m_panelArray.Add( chartPanel );
        } 
    }
    
    SelectChartByID(m_ChartSelectedID, m_ChartSelectedOrder, m_ChartSelectedQty);
    
    m_scrollWinChartList->ClearBackground();
    m_scrollWinChartList->GetSizer()->Layout();

    Layout();

    m_scrollWinChartList->ClearBackground();
    
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
        m_buttonInstall->SetLabel(_("Install Selected Chart"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_CURRENT){
        m_buttonInstall->SetLabel(_("Reinstall Selected Chart"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_STALE){
        m_buttonInstall->SetLabel(_("Update Selected Chart"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_READY_DOWNLOAD){
        m_buttonInstall->SetLabel(_("Download Selected Chart"));
        m_buttonInstall->Show();       
    }
    else if(chart->getChartStatus() == STAT_REQUESTABLE){
        m_buttonInstall->SetLabel(_("Download Selected Chart"));
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
    androidHideBusyIcon();
    #endif             
    int ret = dlg.ShowModal();
    
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
}
 
void InProgressIndicator::OnTimer(wxTimerEvent &evt)
{
    if(m_bAlive)
        Pulse();
}
 
 
void InProgressIndicator::Start() 
{
     m_bAlive = true;
}
 
void InProgressIndicator::Stop() 
{
     m_bAlive = false;
     SetValue(0);
}


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
    
    g_shopPanel->getInProcessGuage()->SetValue(0);
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
                chart->downloadingFile.Clear();
            }
        }
    }
            
    //  Send an event to chain back to "Install" button
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED);
    event.SetId( ID_CMD_BUTTON_INSTALL_CHAIN );
    g_shopPanel->GetEventHandler()->AddPendingEvent(event);
    
}

double dl_now;
double dl_total;
time_t g_progressTicks;

void OESENC_CURL_EvtHandler::onProgressEvent(wxCurlDownloadEvent &evt)
{
    dl_now = evt.GetDownloadedBytes();
    dl_total = evt.GetTotalBytes();
    
    // Calculate the gauge value
    if(evt.GetTotalBytes() > 0){
        float progress = evt.GetDownloadedBytes()/evt.GetTotalBytes();
        g_shopPanel->getInProcessGuage()->SetValue(progress * 100);
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

