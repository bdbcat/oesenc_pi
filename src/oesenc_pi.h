/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  oesenc Plugin
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2016 by David S. Register   *
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

#ifndef _OESENCPI_H_
#define _OESENCPI_H_

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include "wx/socket.h"
#include <wx/fileconf.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/html/htmlwin.h>

#include "TexFont.h"
#include "version.h"

//  The version number now comes from (${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/include/version.h) file
//  as configured by the CMakeLists.txt script

//#define     PLUGIN_VERSION_MAJOR    0
//#define     PLUGIN_VERSION_MINOR    2

#define     MY_API_VERSION_MAJOR    1
#define     MY_API_VERSION_MINOR    11

#include "ocpn_plugin.h"

#include <algorithm>          // for std::sort
#include <string>
#include <map>


enum {
    ID_BUTTONCELLIMPORT,
    ID_NOTEBOOK
};

//      Private logging functions
void ScreenLogMessage(wxString s);
void HideScreenLog(void);
void ClearScreenLog(void);
void ClearScreenLogSeq(void);

extern "C++" wxString GetUserpermit(void);
//extern "C++" wxString GetInstallpermit(void);
extern "C++" wxArrayString exec_SENCutil_sync( wxString cmd, bool bshowlog );


class   oesenc_pi;
class   OCPNPermitList;
class   OCPNCertificateList;
class oesenc_pi_event_handler;


class Catalog_Entry31
{
public:
    Catalog_Entry31(){};
    ~Catalog_Entry31(){};

    wxString m_filename;
    wxString m_comt;
};

WX_DECLARE_OBJARRAY(Catalog_Entry31,      Catalog31);

class ChartInfoItem {
public:
    ChartInfoItem(){};
    ~ChartInfoItem(){};
    
    wxString config_string;
    wxString display_string;
    bool bShown;
    bool bAccessed;
};


//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------

class oesenc_pi : public opencpn_plugin_111
{
public:
      oesenc_pi(void *ppimgr);
      ~oesenc_pi();

//    The required PlugIn Methods
    int Init(void);
    bool DeInit(void);

    int GetAPIVersionMajor();
    int GetAPIVersionMinor();
    int GetPlugInVersionMajor();
    int GetPlugInVersionMinor();
    wxBitmap *GetPlugInBitmap();
    wxString GetCommonName();
    wxString GetShortDescription();
    wxString GetLongDescription();
    bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp);
    bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp);

    wxArrayString GetDynamicChartClassNameArray();

    void OnSetupOptions();
    void OnCloseToolboxPanel(int page_sel, int ok_apply_cancel);
    void ShowPreferencesDialog( wxWindow* parent );
    void SetColorScheme(PI_ColorScheme cs);
    
    void SetPluginMessage(wxString &message_id, wxString &message_body);
//     int ImportCellPermits( void );
//     int RemoveCellPermit( void );
//     int ImportCells( void );
//     int ImportCert( void );
    void Set_FPR();

//     void EnablePermitRemoveButton(bool benable){ m_buttonRemovePermit->Enable(benable); }
//     void GetNewUserpermit(void);
//     void GetNewInstallpermit(void);

    bool SaveConfig( void );

    wxString GetCertificateDir();

    wxStaticText        *m_up_text;
    wxStaticText        *m_ip_text;
    wxStaticText        *m_fpr_text;

//     wxScrolledWindow    *m_s63chartPanelWinTop;
//     wxPanel             *m_s63chartPanelWin;
//     wxPanel             *m_s63chartPanelKeys;
//     wxNotebook          *m_s63NB;

    void OnNewFPRClick( wxCommandEvent &event );
    void OnShowFPRClick( wxCommandEvent &event );

private:
//    wxString GetPermitDir();
    bool ScrubChartinfoList( void );
    
//    Catalog31 *CreateCatalog31(const wxString &file31);

    int ProcessCellPermit( wxString &permit, bool b_confirm_existing );
//    int AuthenticateCell( const wxString & cell_file );

    bool LoadConfig( void );

    int pi_error( wxString msg );

    wxArrayString     m_class_name_array;

    wxBitmap          *m_pplugin_icon;

    oesenc_pi_event_handler *m_event_handler;

    OCPNPermitList      *m_permit_list;
    wxButton            *m_buttonImportPermit;
    wxButton            *m_buttonRemovePermit;
    wxButton            *m_buttonNewUP;
    wxButton            *m_buttonImportCells;
    wxButton            *m_buttonNewIP;

    wxString            m_userpermit;

    Catalog31           *m_catalog;
    wxString            m_last_enc_root_dir;

    OCPNCertificateList *m_cert_list;
    wxButton            *m_buttonImportCert;

    bool                m_bSSE26_shown;
    TexFont             m_TexFontMessage;
    
    

};



class oesencPrefsDialog : public wxDialog 
{
private:
    
protected:
    wxStdDialogButtonSizer* m_sdbSizer1;
    wxButton* m_sdbSizer1OK;
    wxButton* m_sdbSizer1Cancel;
    
public:
    
    
    oesencPrefsDialog( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("oeSENC_PI Preferences"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxCAPTION|wxDEFAULT_DIALOG_STYLE ); 
    ~oesencPrefsDialog();
    void OnPrefsOkClick(wxCommandEvent& event);
    
    wxButton *m_buttonNewFPR;
    wxButton *m_buttonShowFPR;
    wxStaticText *m_fpr_text;
    
    DECLARE_EVENT_TABLE()
    
    
    
};

// An Event handler class to catch events from UI dialog
class oesenc_pi_event_handler : public wxEvtHandler
{
public:
    
    oesenc_pi_event_handler(oesenc_pi *parent);
    ~oesenc_pi_event_handler();
    
    void OnNewFPRClick( wxCommandEvent &event );
    void OnShowFPRClick( wxCommandEvent &event );

    oesenc_pi  *m_parent;
};

class S63ScreenLog : public wxWindow
{
public:
    S63ScreenLog(wxWindow *parent);
    ~S63ScreenLog();

    void LogMessage(wxString &s);
    void ClearLog(void);
    void ClearLogSeq(void){ m_nseq = 0; }

    void OnServerEvent(wxSocketEvent& event);
    void OnSocketEvent(wxSocketEvent& event);
    void OnSize( wxSizeEvent& event);


private:
    wxTextCtrl          *m_plogtc;
    unsigned int        m_nseq;

    wxSocketServer      *m_server;

    DECLARE_EVENT_TABLE()

};

class S63ScreenLogContainer : public wxDialog
{
public:
    S63ScreenLogContainer(wxWindow *parent);
    ~S63ScreenLogContainer();

    void LogMessage(wxString &s);
    void ClearLog(void);
    S63ScreenLog        *m_slog;

private:
};





class OCPNPermitList : public wxListCtrl
{
public:
    OCPNPermitList(wxWindow *parent);
    ~OCPNPermitList();

    void BuildList( const wxString &permit_dir );
    wxArrayString       m_permit_file_array;
};

class OCPNCertificateList : public wxListCtrl
{
public:
    OCPNCertificateList(wxWindow *parent);
    ~OCPNCertificateList();

    void BuildList( const wxString &cert_dir );
//    wxArrayString       m_permit_file_array;
};




/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_GETUP 8100
#define SYMBOL_GETUP_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX
#define SYMBOL_GETUP_TITLE _T("S63_pi Userpermit Required")
#define SYMBOL_GETUP_IDNAME ID_GETUP
#define SYMBOL_GETUP_SIZE wxSize(500, 200)
#define SYMBOL_GETUP_POSITION wxDefaultPosition
#define ID_GETUP_CANCEL 8101
#define ID_GETUP_OK 8102
#define ID_GETUP_UP 8103
#define ID_GETUP_TEST 8104


////@end control identifiers

/*!
 * SENCGetUserpermitDialog class declaration
 */
class SENCGetUserpermitDialog: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( SENCGetUserpermitDialog )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    SENCGetUserpermitDialog( );
    SENCGetUserpermitDialog( wxWindow* parent, wxWindowID id = SYMBOL_GETUP_IDNAME,
                        const wxString& caption = SYMBOL_GETUP_TITLE,
                        const wxPoint& pos = SYMBOL_GETUP_POSITION,
                        const wxSize& size = SYMBOL_GETUP_SIZE,
                        long style = SYMBOL_GETUP_STYLE );

    ~SENCGetUserpermitDialog();

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_GETUP_IDNAME,
                 const wxString& caption = SYMBOL_GETUP_TITLE,
                 const wxPoint& pos = SYMBOL_GETUP_POSITION,
                 const wxSize& size = SYMBOL_GETUP_SIZE, long style = SYMBOL_GETUP_STYLE );


    void CreateControls();

    void OnCancelClick( wxCommandEvent& event );
    void OnOkClick( wxCommandEvent& event );
    void OnUpdated( wxCommandEvent& event );
    void OnTestClick( wxCommandEvent& event );

    /// Should we show tooltips?
    static bool ShowToolTips();

    wxTextCtrl*   m_PermitCtl;
    wxButton*     m_CancelButton;
    wxButton*     m_OKButton;
    wxButton*     m_testBtn;
    wxStaticText* m_TestResult;


};

/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_GETIP 8200
#define SYMBOL_GETIP_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX
#define SYMBOL_GETIP_TITLE _T("S63_pi Install Permit Required")
#define SYMBOL_GETIP_IDNAME ID_GETIP
#define SYMBOL_GETIP_SIZE wxSize(500, 200)
#define SYMBOL_GETIP_POSITION wxDefaultPosition
#define ID_GETIP_CANCEL 8201
#define ID_GETIP_OK 8202
#define ID_GETIP_IP 8203
#define ID_GETIP_TEST 8204


////@end control identifiers

/*!
 * SENCGetInstallpermitDialog class declaration
 */
class SENCGetInstallpermitDialog: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( SENCGetInstallpermitDialog )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    SENCGetInstallpermitDialog( );
    SENCGetInstallpermitDialog( wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                         const wxString& caption = SYMBOL_GETIP_TITLE,
                         const wxPoint& pos = SYMBOL_GETIP_POSITION,
                         const wxSize& size = SYMBOL_GETIP_SIZE,
                         long style = SYMBOL_GETIP_STYLE );

    ~SENCGetInstallpermitDialog();

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                 const wxString& caption = SYMBOL_GETIP_TITLE,
                 const wxPoint& pos = SYMBOL_GETIP_POSITION,
                 const wxSize& size = SYMBOL_GETIP_SIZE, long style = SYMBOL_GETIP_STYLE );


    void CreateControls();

    void OnCancelClick( wxCommandEvent& event );
    void OnOkClick( wxCommandEvent& event );
    void OnUpdated( wxCommandEvent& event );
    void OnTestClick( wxCommandEvent& event );

    /// Should we show tooltips?
    static bool ShowToolTips();

    wxTextCtrl*   m_PermitCtl;
    wxButton*     m_CancelButton;
    wxButton*     m_OKButton;
    wxButton*     m_testBtn;
    wxStaticText* m_TestResult;


};

/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_GETIP 8200
#define SYMBOL_GETIP_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX
#define SYMBOL_GETIP_TITLE_KEY _("OpenCPN SENC UserKey Required")
#define SYMBOL_GETIP_IDNAME ID_GETIP
#define SYMBOL_GETIP_SIZE wxSize(500, 200)
#define SYMBOL_GETIP_POSITION wxDefaultPosition
#define ID_GETIP_CANCEL 8201
#define ID_GETIP_OK 8202
#define ID_GETIP_IP 8203
#define ID_GETIP_TEST 8204

#define LEGEND_NONE             0
#define LEGEND_FIRST            1
#define LEGEND_SECOND           2
#define LEGEND_THIRD            3
#define LEGEND_FOURTH           4


////@end control identifiers

/*!
 * SENCGetUserKeyDialog class declaration
 */
class SENCGetUserKeyDialog: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( SENCGetUserKeyDialog )
    DECLARE_EVENT_TABLE()
    
public:
    /// Constructors
    SENCGetUserKeyDialog( );
    SENCGetUserKeyDialog( int legendID, wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                          const wxString& caption = SYMBOL_GETIP_TITLE_KEY,
                                const wxPoint& pos = SYMBOL_GETIP_POSITION,
                                const wxSize& size = SYMBOL_GETIP_SIZE,
                                long style = SYMBOL_GETIP_STYLE );
    
    ~SENCGetUserKeyDialog();
    
    /// Creation
    bool Create( int legendID, wxWindow* parent, wxWindowID id = SYMBOL_GETIP_IDNAME,
                 const wxString& caption = SYMBOL_GETIP_TITLE,
                 const wxPoint& pos = SYMBOL_GETIP_POSITION,
                 const wxSize& size = SYMBOL_GETIP_SIZE, long style = SYMBOL_GETIP_STYLE );
    
    
    void CreateControls( int legendID );
    
    void OnCancelClick( wxCommandEvent& event );
    void OnOkClick( wxCommandEvent& event );
    
    /// Should we show tooltips?
    static bool ShowToolTips();
    
    wxTextCtrl*   m_UserKeyCtl;
    wxButton*     m_CancelButton;
    wxButton*     m_OKButton;
    
    
};



class InfoWin: public wxWindow
{
public:
    InfoWin( wxWindow *parent, const wxString&s = _T(""), bool show_gauge = true );
    ~InfoWin();

    void SetString(const wxString &s);
    const wxString& GetString(void) { return m_string; }

    void SetPosition( wxPoint pt ){ m_position = pt; }
    void SetWinSize( wxSize sz ){ m_size = sz; }
    void Realize( void );
    wxSize GetWinSize( void ){ return m_size; }
    void OnPaint( wxPaintEvent& event );
    void OnEraseBackground( wxEraseEvent& event );
    void OnTimer( wxTimerEvent& event );

    wxStaticText *m_pInfoTextCtl;
    wxGauge   *m_pGauge;
    wxTimer     m_timer;

private:

    wxString m_string;
    wxSize m_size;
    wxPoint m_position;
    bool m_bGauge;

    DECLARE_EVENT_TABLE()
};

class InfoWinDialog: public wxDialog
{
public:
    InfoWinDialog( wxWindow *parent, const wxString&s = _T(""), bool show_gauge = true );
    ~InfoWinDialog();

    void SetString(const wxString &s);
    const wxString& GetString(void) { return m_string; }

    void SetPosition( wxPoint pt ){ m_position = pt; }
    void SetWinSize( wxSize sz ){ m_size = sz; }
    void Realize( void );
    wxSize GetWinSize( void ){ return m_size; }
    void OnPaint( wxPaintEvent& event );
    void OnEraseBackground( wxEraseEvent& event );
    void OnTimer( wxTimerEvent& event );

    wxStaticText *m_pInfoTextCtl;
    wxGauge   *m_pGauge;
    wxTimer     m_timer;

private:

    wxString m_string;
    wxSize m_size;
    wxPoint m_position;
    bool m_bGauge;

    DECLARE_EVENT_TABLE()
};


//      Constants

#define ID_DIALOG 10001

#define SYMBOL_ABOUT_TITLE _("oeSENC_PI Information")

#define xID_OK          10009
#define xID_CANCEL      10010

#define ID_NOTEBOOK_HELP 10002


class oesenc_pi_about: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( about )
    DECLARE_EVENT_TABLE()
    
public:
    explicit oesenc_pi_about( );
    explicit oesenc_pi_about( wxWindow* parent, 
                    wxWindowID id = ID_DIALOG,
                    const wxString& caption = SYMBOL_ABOUT_TITLE,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxSize(500, 500),
                    long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
    bool Create( wxWindow* parent,
                 wxWindowID id = ID_DIALOG,
                 const wxString& caption = SYMBOL_ABOUT_TITLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxSize(500, 500),
                 long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
    
    void RecalculateSize( void );
    
private:
    void CreateControls( void );
    void Populate( void );
    void OnXidOkClick( wxCommandEvent& event );
    void OnXidRejectClick( wxCommandEvent& event );
    void OnPageChange(wxNotebookEvent& event);
    void OnClose( wxCloseEvent& event );
    
    wxWindow *m_parent;
    bool m_btips_loaded;
    
    wxPanel* itemPanelAbout;
    wxPanel* itemPanelAuthors;
    wxPanel* itemPanelLicense;
    wxPanel* itemPanelTips;
    
    wxTextCtrl *pAuthorTextCtl;
    wxTextCtrl *pLicenseTextCtl;
    wxNotebook *pNotebook;
    wxHtmlWindow *pAboutHTMLCtl;
    wxHtmlWindow *pLicenseHTMLCtl;
    wxHtmlWindow *pAuthorHTMLCtl;
    
    //wxSize m_displaySize;
    
};

#endif


