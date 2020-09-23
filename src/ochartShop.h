/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  oesenc Plugin
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

#ifndef _OCHARTSHOP_H_
#define _OCHARTSHOP_H_

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include <wx/statline.h>
//#include <../../wxWidgets/wxWidgets-3.0.2/wxWidgets-3.0.2/include/wx/gtk/gauge.h>

#ifdef __OCPN_USE_CURL__
#include "wxcurl/wx/curl/http.h"
#endif
#include "ocpn_plugin.h"

#ifdef WXC_FROM_DIP
#undef WXC_FROM_DIP
#endif
#if (wxVERSION_NUMBER >= 3100) && !defined(__OCPN__ANDROID__)
#define WXC_FROM_DIP(x) wxWindow::FromDIP(x, NULL)
#else
#define WXC_FROM_DIP(x) x
#endif

wxString ProcessResponse(std::string);


class shopPanel;
class InProgressIndicator;

enum{
        STAT_UNKNOWN = 0,
        STAT_PURCHASED,
        STAT_CURRENT,
        STAT_STALE,
        STAT_EXPIRED,
        STAT_EXPIRED_MINE,
        STAT_PREPARING,
        STAT_READY_DOWNLOAD,
        STAT_REQUESTABLE,
        STAT_NEED_REFRESH
};
        
        
//      A single chart(set) container
class itemChart
{
public:    
    itemChart() { m_downloading = false; m_bEnabled = true; }
    ~itemChart() {};

    itemChart( wxString &order_ref, wxString &chartid, wxString &quantity);
    void setDownloadPath(int slot, wxString path);
    wxString getDownloadPath(int slot); 
    bool isChartsetFullyAssigned();
    bool isChartsetAssignedToMe(wxString systemName);
    bool isChartsetExpired();
    bool isChartsetDontShow();
    bool isChartsetShow();
    bool isChartsetAssignedToAnyDongle();
    bool isSlotAssignedToAnyDongle( int slot );
    bool isSlotAssignedToMyDongle( int slot );

public:    
    wxString getOrderRef() { return orderRef;}
    bool isEnabled(){ return m_bEnabled; }
    wxString getStatusString();
    int getChartStatus();
    wxBitmap& GetChartThumbnail(int size, bool bDL_If_Needed = false);
    wxString getKeytypeString();
    wxString getKeytypeString( int slot, wxColour &tcolor );
    wxString getKeyString( int slot, wxColour &tcolor );

    //wxString ident;
    
    wxString orderRef;
    wxString purchaseDate;
    wxString expDate;
    wxString chartName;
    wxString chartID;
    wxString quantityId;
    wxString currentChartEdition;
    wxString thumbnailURL;
    
    wxString sysID0;
    wxString statusID0;
    wxString fileDownloadURL0;      //  https://.....
    wxString filedownloadSize0;
    wxString fileDownloadPath0;     // Where the file was downloaded
    wxString lastRequestEdition0;
    wxString fileDownloadName0;     // The short name "charts.zip"
    wxString installedFileDownloadPath0;        // The zip file that is currently installed
    wxString installLocation0;
    wxString installedEdition0;
    
    wxString sysID1;
    wxString statusID1;
    wxString fileDownloadURL1;      //  https://.....
    wxString filedownloadSize1;
    wxString fileDownloadPath1;     // Where the file was downloaded
    wxString lastRequestEdition1;
    wxString fileDownloadName1;     // The short name "charts.zip"
    wxString installedFileDownloadPath1;
    wxString installLocation1;
    wxString installedEdition1;
    
    bool m_downloading;
    wxString downloadingFile;
    
    long downloadReference;
    bool m_bEnabled;
    wxImage m_ChartImage;
    wxBitmap m_bm;
    
    wxString lastInstall;          // For updates, the full path of installed chartset
    int m_status;
    wxArrayString(m_nameArrayString);
};

WX_DECLARE_OBJARRAY(itemChart *,      ArrayOfCharts);    



//  The main entry point for ocharts Shop interface
int doShop();
    

class oeSencChartPanel: public wxPanel
{
public:
    oeSencChartPanel( wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, itemChart *p_itemChart, shopPanel *pContainer );
    ~oeSencChartPanel();
    
    void OnChartSelected( wxMouseEvent &event );
    void SetSelected( bool selected );
    void OnPaint( wxPaintEvent &event );
    void OnEraseBackground( wxEraseEvent &event );
    
    bool GetSelected(){ return m_bSelected; }
    int GetUnselectedHeight(){ return m_unselectedHeight; }
    itemChart *m_pChart;
    
private:
    shopPanel *m_pContainer;
    bool m_bSelected;
    wxStaticText *m_pName;
    wxColour m_boxColour;
    int m_unselectedHeight;
    wxArrayString(m_nameArrayString);
    int m_refDim;
    
    DECLARE_EVENT_TABLE()
};



WX_DECLARE_OBJARRAY(oeSencChartPanel *,      ArrayOfChartPanels);    


class chartScroller : public wxScrolledWindow
{
    DECLARE_EVENT_TABLE()

public:
    chartScroller(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
    void OnEraseBackground(wxEraseEvent& event);
    void DoPaint(wxDC& dc);
    void OnPaint( wxPaintEvent &event );
    
    
};


class shopPanel : public wxPanel
{
    DECLARE_EVENT_TABLE()
    
protected:
    wxScrolledWindow* m_scrollWinChartList;
    wxButton* m_button37;
    wxButton* m_button39;
    wxButton* m_button41;
    wxButton* m_button43;
    wxButton* m_button45;
    wxButton* m_button47;
    wxStaticText* m_staticTextSystemName;
    wxStaticText* m_staticText111;
    wxStaticText* m_staticText113;
    wxStaticText* m_staticText115;
    wxTextCtrl* m_textCtrl117;
    wxTextCtrl* m_textCtrl119;
    wxStaticLine* m_staticLine121;
    wxButton* m_buttonAssign;
    wxButton* m_buttonDownload;
    wxButton* m_buttonInstall;
    wxButton* m_buttonUpdate;
    wxBoxSizer* boxSizerCharts;
    
    ArrayOfChartPanels m_panelArray;
    oeSencChartPanel *m_ChartSelected;
    
    wxChoice* m_choiceSystemName;
    wxButton* m_buttonNewSystemName;
    wxTextCtrl*  m_sysName;
    wxButton* m_buttonChangeSystemName;
    wxStaticText *m_staticTextStatus;
    wxStaticText *m_staticTextStatusProgress;
    wxPanel *m_chartListPanel;
    wxBoxSizer *m_boxSizerchartListPanel;
    

protected:
    
public:
    wxButton* GetButton37() { return m_button37; }
    wxButton* GetButton39() { return m_button39; }
    wxButton* GetButton41() { return m_button41; }
    wxButton* GetButton43() { return m_button43; }
    wxButton* GetButton45() { return m_button45; }
    wxButton* GetButton47() { return m_button47; }
    wxScrolledWindow* GetScrollWinChartList() { return m_scrollWinChartList; }
    wxStaticText* GetStaticTextSystemName() { return m_staticTextSystemName; }
    wxStaticText* GetStaticText111() { return m_staticText111; }
    wxStaticText* GetStaticText113() { return m_staticText113; }
    wxStaticText* GetStaticText115() { return m_staticText115; }
    wxTextCtrl* GetTextCtrl117() { return m_textCtrl117; }
    wxTextCtrl* GetTextCtrl119() { return m_textCtrl119; }
    wxStaticLine* GetStaticLine121() { return m_staticLine121; }
    //wxButton* GetButtonAssign() { return m_buttonAssign; }
    //wxButton* GetButtonDownload() { return m_buttonDownload; }
    wxButton* GetButtonInstall() { return m_buttonInstall; }
    wxButton* GetButtonUpdate() { return m_buttonUpdate; }
    void RefreshSystemName();
    int GetScrollRate(){ return m_scrollRate; }
    
    shopPanel(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(500,600), long style = wxTAB_TRAVERSAL);
    virtual ~shopPanel();
    
    void SelectChart( oeSencChartPanel *chart );
    void SelectChartByID( wxString& id, wxString& order, wxString& qty);
    
    oeSencChartPanel *GetSelectedChart(){ return m_ChartSelected; }
    
    void OnButtonUpdate( wxCommandEvent& event );
    void OnButtonCancelOp( wxCommandEvent& event );
    void OnButtonInstall( wxCommandEvent& event );
    void OnButtonInstallChain( wxCommandEvent& event );
    
    void OnPrepareTimer(wxTimerEvent &evt);
    int doPrepareGUI();
    int doDownloadGui();
    
    void UpdateChartList();
    void OnGetNewSystemName( wxCommandEvent& event );
    void OnChangeSystemName( wxCommandEvent& event );
    bool doSystemNameWizard( bool *bnew );
    wxString doGetNewSystemName( );
    void UpdateActionControls();
    void setStatusText( const wxString &text ){ m_staticTextStatus->SetLabel( text );  m_staticTextStatus->Refresh(); }
    void setStatusTextProgress( const wxString &text ){ m_staticTextStatus/*m_staticTextStatusProgress*/->SetLabel( text );  /*m_staticTextStatusProgress->Refresh();*/ }
    void MakeChartVisible(oeSencChartPanel *chart);
    int GetActiveSlotAction( itemChart *chart );
    void onDLEvent(OCPN_downloadEvent &evt);
    void doDownload(oeSencChartPanel *chartDownload, int slot);

    int m_prepareTimerCount;
    int m_prepareTimeout;
    int m_prepareProgress;
    wxTimer m_prepareTimer;
    int m_activeSlot;
    int m_action;
    wxString m_ChartSelectedID;
    wxString m_ChartSelectedOrder;
    wxString m_ChartSelectedQty;
    wxButton* m_buttonCancelOp;
    bool m_binstallChain;
    bool m_bAbortingDownload;
    bool m_startedDownload;
    int m_scrollRate;
    bool m_bTransferComplete;
    bool m_bTransferSuccess;

};


#define ID_GETIP 8200
#define SYMBOL_GETIP_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX
#define SYMBOL_GETIP_IDNAME ID_GETIP
#define SYMBOL_GETIP_SIZE wxSize(500, 200)
#define SYMBOL_GETIP_POSITION wxDefaultPosition
#define ID_GETIP_CANCEL 8201
#define ID_GETIP_OK 8202
#define ID_GETIP_IP 8203

class oeSENCGETSystemName: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( oeSENCGETSystemName )
    DECLARE_EVENT_TABLE()
    
public:
    oeSENCGETSystemName( );
    oeSENCGETSystemName( wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                         const wxString& caption =  _("OpenCPN oeSENC System Name"),
                          const wxPoint& pos = SYMBOL_GETIP_POSITION,
                          const wxSize& size = SYMBOL_GETIP_SIZE,
                          long style = SYMBOL_GETIP_STYLE );
    
    ~oeSENCGETSystemName();
    
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                 const wxString& caption =  _("OpenCPN oeSENC System Name"),
                 const wxPoint& pos = SYMBOL_GETIP_POSITION,
                 const wxSize& size = SYMBOL_GETIP_SIZE, long style = SYMBOL_GETIP_STYLE );
    
    
    void CreateControls(  );
    
    void OnCancelClick( wxCommandEvent& event );
    void OnOkClick( wxCommandEvent& event );
    wxString GetNewName();
    
    static bool ShowToolTips();
    
    wxTextCtrl*   m_SystemNameCtl;
    wxButton*     m_CancelButton;
    wxButton*     m_OKButton;
    
    
};


class oeSENCSystemNameSelector: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( oeSENCSystemNameSelector )
    DECLARE_EVENT_TABLE()
    
public:
    oeSENCSystemNameSelector( );
    oeSENCSystemNameSelector( wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                         const wxString& caption =  _("Select OpenCPN/oeSENC System Name"),
                         const wxPoint& pos = wxDefaultPosition,
                         const wxSize& size = wxSize(500, 200),
                         long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
    
    ~oeSENCSystemNameSelector();
    
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                 const wxString& caption =  _("Select OpenCPN/oeSENC System Name"),
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxSize(500, 200), long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
    
    
    void CreateControls(  );
    
    void OnCancelClick( wxCommandEvent& event );
    void OnOkClick( wxCommandEvent& event );
    wxString getRBSelection();
    
    static bool ShowToolTips();
    
    wxButton*     m_CancelButton;
    wxButton*     m_OKButton;
    wxRadioBox*   m_rbSystemNames;
    
};


class InProgressIndicator: public wxGauge
{
    DECLARE_EVENT_TABLE()
    
public:    
    InProgressIndicator();
    InProgressIndicator(wxWindow* parent, wxWindowID id, int range,
                        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                        long style = wxGA_HORIZONTAL, const wxValidator& validator = wxDefaultValidator, const wxString& name = "inprogress");
    
    ~InProgressIndicator();
    
    void OnTimer(wxTimerEvent &evt);
    void Start();
    void Stop();
    
    
    wxTimer m_timer;
    int msec;
    bool m_bAlive;
};

#ifdef __OCPN_USE_CURL__
class OESENC_CURL_EvtHandler : public wxEvtHandler
{
public:
    OESENC_CURL_EvtHandler();
    ~OESENC_CURL_EvtHandler();
    
    void onBeginEvent(wxCurlBeginPerformEvent &evt);
    void onEndEvent(wxCurlEndPerformEvent &evt);
    void onProgressEvent(wxCurlDownloadEvent &evt);
    
};
#endif

class oeSENCLogin: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( oeSENCLogin )
    DECLARE_EVENT_TABLE()
    
public:
    oeSENCLogin( );
    oeSENCLogin( wxWindow* parent, wxWindowID id = wxID_ANY,
                         const wxString& caption =  _("OpenCPN oeSENC Login"),
                        const wxPoint& pos = wxDefaultPosition,
                          const wxSize& size = wxSize(500, 200),
                        long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
    
    ~oeSENCLogin();
    
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                 const wxString& caption =  _("OpenCPN oeSENC Login"),
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxSize(500, 200), long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
    
    
    void CreateControls(  );
    
    void OnCancelClick( wxCommandEvent& event );
    void OnOkClick( wxCommandEvent& event );
    void OnClose( wxCloseEvent& event );
    
    static bool ShowToolTips();
    
    wxTextCtrl*   m_UserNameCtl;
    wxTextCtrl*   m_PasswordCtl;
    wxButton*     m_CancelButton;
    wxButton*     m_OKButton;
    
    
};


#endif          //_OCHARTSHOP_H_
