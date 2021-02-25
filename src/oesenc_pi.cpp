/******************************************************************************
 *
 * Project:  oesenc_pi
 * Purpose:  oesenc_pi Plugin core
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

#include "config.h"

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include <wx/textfile.h>
#include "wx/tokenzr.h"
#include "wx/dir.h"
#include "wx/filename.h"
#include "wx/file.h"
#include "wx/stream.h"
#include "wx/wfstream.h"
#include <wx/statline.h>
#include <wx/progdlg.h>
#include "wx/artprov.h"

#include "oesenc_pi.h"
#include "eSENCChart.h"
#include "iso8211.h"
#include "dsa_utils.h"
#include "s57RegistrarMgr.h"
#include "S57ClassRegistrar.h"
#include "s52plib.h"
#include "s52utils.h"
#include "Osenc.h"
#include "chartsymbols.h"
#include "wx/json_defs.h"
#include "wx/jsonwriter.h"
#include "wx/jsonreader.h"
#include "dsa_utils.h"
#include "sha1.h"
#include "InstallDirs.h"

//#ifndef __OCPN__ANDROID__
#include "ochartShop.h"
//#endif


#ifdef __WXOSX__
#include "GL/gl.h"
#include "GL/glu.h"
#else

    #ifndef __OCPN__ANDROID__
        #include <GL/gl.h>
        #include <GL/glu.h>
        #include <GL/glext.h>
        #ifndef __WXMSW__
            #include <GL/glx.h>
        #endif    
    #else
        #include <qopengl.h>
        #include <GL/gl_private.h>              // this is a cut-down version of gl.h
    #endif

#endif

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include <string>
#include <map>
#include <unordered_map>

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

bool testSENCServer();
bool validate_SENC_server(void);

void init_S52Library();
void init_GLLibrary();

bool IsDongleAvailable();

#include <wx/arrimpl.cpp> 
WX_DEFINE_OBJARRAY(EULAArray);

//      Some PlugIn global variables
wxString                        g_sencutil_bin;
S63ScreenLogContainer           *g_pScreenLog;
S63ScreenLog                    *g_pPanelScreenLog;
unsigned int                    g_backchannel_port;
unsigned int                    g_frontchannel_port;
wxString                        g_s57data_dir;

wxString                        g_userpermit;
wxString                        g_installpermit;
oesenc_pi                       *g_pi;
wxString                        g_pi_filename;
wxString                        g_SENCdir;

//bool                            g_bsuppress_log;
wxProgressDialog                *g_pprog;
wxString                        g_old_installpermit;
wxString                        g_old_userpermit;
bool                            g_benable_screenlog;
wxArrayString                   g_logarray;
bool                            gb_global_log;
bool                            g_b_validated;
bool                            g_bSENCutil_valid;
wxString                        g_CommonDataDir;
extern int                      s_PI_bInS57;
bool                            g_buser_enable_screenlog;
bool                            g_bshown_sse15;
bool                            g_brendered_expired;
bool                            g_bnoShow_sse25;

wxString                        g_UserKey;
wxString                        g_old_UserKey;

bool                            g_PIbDebugS57;
wxString                        g_fpr_file;
bool                            g_bEULA_OK = false;
bool                            g_bEULA_Rejected = false;
bool                            g_bDeclaredInvalid = false;

wxString                        g_infoRule;
bool                            g_binfoShown;
wxString                        g_infoRaw;
bool                            g_bUserKeyHintTaken;
int                             g_serverDebug;
int                             g_debugLevel;

oesenc_pi_event_handler         *g_event_handler;
int                             global_color_scheme;
double                          g_pix_per_mm;

int                             g_coreVersionMajor;
int                             g_coreVersionMinor;
int                             g_coreVersionPatch;

wxString                        g_pipeParm;

wxArrayString                   g_ChartInfoArray;
EULAArray                       g_EULAArray;
wxArrayString                   g_EULAShaArray;
wxString                        g_PrivateDataDir;

int                             g_admin;
wxString                        g_debugShop;
wxString g_versionString;

std::map<std::string, ChartInfoItem *> info_hash;

std::unordered_map<std::string, int> chartFailCount;

bool                            g_GLOptionsSet;

double g_overzoom_emphasis_base;
bool g_oz_vector_scale;
float g_ChartScaleFactorExp;
int g_chart_zoom_modifier_vector;
double g_display_size_mm;

float g_GLMinCartographicLineWidth;
bool  g_b_EnableVBO;
float g_GLMinSymbolLineWidth;
GLenum g_oe_texture_rectangle_format;
bool pi_bopengl;

bool g_b_useStencil;
bool g_b_useStencilAP;
bool g_b_useScissorTest;
bool g_b_useFBO;
bool g_GLSetupOK;

oesencPrefsDialog               *g_prefs_dialog;

PFNGLGENBUFFERSPROC                 s_glGenBuffers;
PFNGLBINDBUFFERPROC                 s_glBindBuffer;
PFNGLBUFFERDATAPROC                 s_glBufferData;
PFNGLDELETEBUFFERSPROC              s_glDeleteBuffers;


s57RegistrarMgr                 *pi_poRegistrarMgr;
S57ClassRegistrar               *pi_poRegistrar;
s52plib                         *ps52plib;
wxFileConfig                    *g_pconfig;
wxString                        g_csv_locn;

long                            g_serverProc;
wxString                        g_deviceInfo;
wxString                        g_systemName;
wxString                        g_UUID;
wxString                        g_WVID;
wxString                        g_sSDK_INT;
int                             g_SDK_INT;
wxString                        g_loginUser;
wxString                        g_loginKey;
wxString                        g_systemOS;

bool                            g_GenericMessageShown;

bool shutdown_SENC_server( void );
bool ShowAlwaysEULAs();
extern OE_ChartSymbols          *g_oeChartSymbols;

#ifdef __OCPN__ANDROID__
extern JavaVM *java_vm;         // found in androidUtil.cpp, accidentally exported....

// Older Android devices do not export atof from their libc.so
double atof(const char *nptr)
{
    return (strtod(nptr, NULL));
}

#endif

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new oesenc_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}


/*
static int ExtensionCompare( const wxString& first, const wxString& second )
{
    wxFileName fn1( first );
    wxFileName fn2( second );
    wxString ext1( fn1.GetExt() );
    wxString ext2( fn2.GetExt() );

    return ext1.Cmp( ext2 );
}
*/

#if 1
class  OESENC_HTMLMessageDialog: public wxDialog
{
    
public:
    OESENC_HTMLMessageDialog(wxWindow *parent, const wxString& message,
                             const wxString& caption = wxMessageBoxCaptionStr,
                             long style = wxOK|wxCENTRE,  
                             bool bFixedFont = false,
                             const wxPoint& pos = wxDefaultPosition);
    ~OESENC_HTMLMessageDialog();
    
    void OnYes(wxCommandEvent& event);
    void OnNo(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnClose( wxCloseEvent& event );
    void OnTimer(wxTimerEvent &evt);
    
    
private:
    int m_style;
    wxTimer m_timer;
    
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(OESENC_HTMLMessageDialog, wxDialog)
EVT_BUTTON(wxID_YES, OESENC_HTMLMessageDialog::OnYes)
EVT_BUTTON(wxID_OK, OESENC_HTMLMessageDialog::OnYes)
EVT_BUTTON(wxID_NO, OESENC_HTMLMessageDialog::OnNo)
EVT_BUTTON(wxID_CANCEL, OESENC_HTMLMessageDialog::OnCancel)
EVT_CLOSE(OESENC_HTMLMessageDialog::OnClose)
EVT_TIMER(-1, OESENC_HTMLMessageDialog::OnTimer)

END_EVENT_TABLE()


OESENC_HTMLMessageDialog::OESENC_HTMLMessageDialog( wxWindow *parent,
                                                    const wxString& message,
                                                    const wxString& caption,
                                                    long style,
                                                    bool bFixedFont,
                                                    const wxPoint& pos)
: wxDialog( parent, wxID_ANY, caption, pos, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP )
{
    m_style = style;
    if(bFixedFont){
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize();
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size,wxFONTFAMILY_TELETYPE, dFont->GetStyle(), dFont->GetWeight());
        SetFont( *qFont );
    }
    
    wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );
    
    wxHtmlWindow *msgWindow = new wxHtmlWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                                wxHW_SCROLLBAR_AUTO | wxHW_NO_SELECTION );
    msgWindow->SetBorders( 1 );
    
    topsizer->Add( msgWindow, 1, wxALIGN_CENTER_HORIZONTAL | wxEXPAND, 5 );
    
    wxString html;
    html << message;
    
    wxCharBuffer buf = html.ToUTF8();
    if( buf.data() )                            // string OK?
       msgWindow->SetPage( html );
    
    // 3) buttons
       int AllButtonSizerFlags = wxOK|wxCANCEL|wxYES|wxNO|wxHELP|wxNO_DEFAULT;
       int center_flag = wxEXPAND;
       if (style & wxYES_NO)
           center_flag = wxALIGN_CENTRE;
       wxSizer *sizerBtn = CreateSeparatedButtonSizer(style & AllButtonSizerFlags);
       if ( sizerBtn )
           topsizer->Add(sizerBtn, 0, center_flag | wxALL, 10 );
       
       SetSizer( topsizer );
       
       topsizer->Fit( this );
       
       wxSize szyv = msgWindow->GetVirtualSize();
       
       SetClientSize(szyv.x + 20, szyv.y + 20); 
       
       //Centre( /*wxBOTH | wxCENTER_FRAME*/);
       CentreOnParent();
       m_timer.SetOwner( this, -1 );
       
       int timeout_sec = 60;
       if(timeout_sec > 0)
           m_timer.Start( timeout_sec * 1000, wxTIMER_ONE_SHOT );
       
}

OESENC_HTMLMessageDialog::~OESENC_HTMLMessageDialog()
{
}

void OESENC_HTMLMessageDialog::OnYes(wxCommandEvent& WXUNUSED(event))
{
    SetReturnCode(wxID_YES);
    if(IsModal())
        EndModal( wxID_YES );
    else
        Hide();
    RequestRefresh(GetOCPNCanvasWindow());
}

void OESENC_HTMLMessageDialog::OnNo(wxCommandEvent& WXUNUSED(event))
{
    SetReturnCode(wxID_NO);
    if(IsModal())
        EndModal( wxID_NO );
    else
        Hide();
}

void OESENC_HTMLMessageDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
    // Allow cancellation via ESC/Close button except if
    // only YES and NO are specified.
    if ( (m_style & wxYES_NO) != wxYES_NO || (m_style & wxCANCEL) )
    {
        SetReturnCode(wxID_CANCEL);
        EndModal( wxID_CANCEL );
    }
}

void OESENC_HTMLMessageDialog::OnClose( wxCloseEvent& event )
{
    SetReturnCode(wxID_CANCEL);
    if(IsModal())
        EndModal( wxID_CANCEL );
    else
        Hide();
    RequestRefresh(GetOCPNCanvasWindow());
}

void OESENC_HTMLMessageDialog::OnTimer(wxTimerEvent &evt)
{
    if(IsModal())
        EndModal( wxID_YES );
    else
        Hide();
}
#endif

#if 0
class  OESENC_HTMLMessageDialog: public wxWindow
{
    
public:
    OESENC_HTMLMessageDialog(wxWindow *parent, const wxString& message,
                             const wxString& caption = wxMessageBoxCaptionStr,
                             long style = wxOK|wxCENTRE,  
                             bool bFixedFont = false,
                             const wxPoint& pos = wxDefaultPosition);
    
    void OnYes(wxCommandEvent& event);
    void OnNo(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnClose( wxCloseEvent& event );
    void OnTimer(wxTimerEvent &evt);
    
    
private:
    int m_style;
    wxTimer m_timer;
    
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(OESENC_HTMLMessageDialog, wxWindow)
//EVT_BUTTON(wxID_YES, OESENC_HTMLMessageDialog::OnYes)
//EVT_BUTTON(wxID_NO, OESENC_HTMLMessageDialog::OnNo)
//EVT_BUTTON(wxID_CANCEL, OESENC_HTMLMessageDialog::OnCancel)
EVT_CLOSE(OESENC_HTMLMessageDialog::OnClose)
EVT_TIMER(-1, OESENC_HTMLMessageDialog::OnTimer)

END_EVENT_TABLE()


OESENC_HTMLMessageDialog::OESENC_HTMLMessageDialog( wxWindow *parent,
                                                    const wxString& message,
                                                    const wxString& caption,
                                                    long style,
                                                    bool bFixedFont,
                                                    const wxPoint& pos)
: wxWindow( parent, wxID_ANY,  pos, wxDefaultSize, wxCAPTION | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER)
{
    m_style = style;
    if(bFixedFont){
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize();
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size,wxFONTFAMILY_TELETYPE, dFont->GetStyle(), dFont->GetWeight());
        SetFont( *qFont );
    }
    
    wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );
    
    wxHtmlWindow *msgWindow = new wxHtmlWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                                wxHW_SCROLLBAR_AUTO | wxHW_NO_SELECTION );
    msgWindow->SetBorders( 1 );
    
    topsizer->Add( msgWindow, 1, wxALIGN_CENTER_HORIZONTAL | wxEXPAND, 5 );
    
    wxString html;
    html << message;
    
    wxCharBuffer buf = html.ToUTF8();
    if( buf.data() )                            // string OK?
       msgWindow->SetPage( html );
 /*   
    // 3) buttons
       int AllButtonSizerFlags = wxOK|wxCANCEL|wxYES|wxNO|wxHELP|wxNO_DEFAULT;
       int center_flag = wxEXPAND;
       if (style & wxYES_NO)
           center_flag = wxALIGN_CENTRE;
       wxSizer *sizerBtn = CreateSeparatedButtonSizer(style & AllButtonSizerFlags);
       if ( sizerBtn )
           topsizer->Add(sizerBtn, 0, center_flag | wxALL, 10 );
       */

       SetSizer( topsizer );
       
       topsizer->Fit( this );
       
       wxSize szyv = msgWindow->GetVirtualSize();
       
       SetClientSize(szyv.x + 20, szyv.y + 20); 
       SetClientSize(600, szyv.y + 20); 
       
       //Centre( /*wxBOTH | wxCENTER_FRAME*/);
       CentreOnParent();
       m_timer.SetOwner( this, -1 );
       
       int timeout_sec = 10;
       if(timeout_sec > 0)
           m_timer.Start( timeout_sec * 1000, wxTIMER_ONE_SHOT );
       Hide();
       
}

void OESENC_HTMLMessageDialog::OnYes(wxCommandEvent& WXUNUSED(event))
{
//     SetReturnCode(wxID_YES);
//     if(IsModal())
//         EndModal( wxID_YES );
//     else
        Hide();
}

void OESENC_HTMLMessageDialog::OnNo(wxCommandEvent& WXUNUSED(event))
{
//     SetReturnCode(wxID_NO);
//     if(IsModal())
//         EndModal( wxID_NO );
//     else
        Hide();
}

void OESENC_HTMLMessageDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
//     // Allow cancellation via ESC/Close button except if
//     // only YES and NO are specified.
//     if ( (m_style & wxYES_NO) != wxYES_NO || (m_style & wxCANCEL) )
//     {
//         SetReturnCode(wxID_CANCEL);
//         EndModal( wxID_CANCEL );
//     }
}

void OESENC_HTMLMessageDialog::OnClose( wxCloseEvent& event )
{
//     SetReturnCode(wxID_CANCEL);
//     if(IsModal())
//         EndModal( wxID_CANCEL );
//     else
        Hide();
}

void OESENC_HTMLMessageDialog::OnTimer(wxTimerEvent &evt)
{
       Hide();
}
#endif

OESENC_HTMLMessageDialog *pinfoDlg;

//---------------------------------------------------------------------------------------------------------
//
//    PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

#include "default_pi.xpm"


//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

oesenc_pi::oesenc_pi(void *ppimgr)
      :opencpn_plugin_111(ppimgr)
{
      wxString vs;
      vs.Printf(_T("%d.%d.%d"), PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH);
      g_versionString = vs;
      g_GenericMessageShown =false;
      
      // Create the PlugIn icons
      m_pplugin_icon = new wxBitmap(default_pi);

      g_pi = this;              // Store a global handle to the PlugIn itself

      g_event_handler = new oesenc_pi_event_handler(this);

     
      g_bSENCutil_valid = false;                // not confirmed yet


      g_backchannel_port = 49500; //49152;       //ports 49152–65535 are unallocated

      g_pScreenLog = NULL;
      g_pPanelScreenLog = NULL;

      g_frontchannel_port = 50000;

      g_s57data_dir = *GetpSharedDataLocation();
      g_s57data_dir += _T("s57data");

      //    Get a pointer to the opencpn configuration object
      g_pconfig = GetOCPNConfigObject();

      // Get and build if necessary a private data dir
      g_PrivateDataDir = *GetpPrivateApplicationDataLocation();
      g_PrivateDataDir += wxFileName::GetPathSeparator();
      g_PrivateDataDir += _T("oesenc_pi");
      g_PrivateDataDir += wxFileName::GetPathSeparator();
      if(!::wxDirExists( g_PrivateDataDir ))
          ::wxMkdir( g_PrivateDataDir );
      
      m_up_text = NULL;
      m_pOptionsPage = 0;

      LoadConfig();
      ScrubChartinfoList(  );
      g_bEULA_Rejected = false;
      g_bEULA_Rejected = !ShowAlwaysEULAs();
      

      //        Set up a common data location,
      //        Using a config file specified location if found
      if( g_CommonDataDir.Len()){
          if( g_CommonDataDir.Last() != wxFileName::GetPathSeparator() )
              g_CommonDataDir += wxFileName::GetPathSeparator();
      }
      else{
          g_CommonDataDir = *GetpPrivateApplicationDataLocation();
          g_CommonDataDir += wxFileName::GetPathSeparator();
          g_CommonDataDir += _T("s63");
          g_CommonDataDir += wxFileName::GetPathSeparator();
      }


      gb_global_log = false;
      m_shoppanel = NULL;
}

oesenc_pi::~oesenc_pi()
{
      delete m_pplugin_icon;
}

int oesenc_pi::Init(void)
{
    
    //  Get the path of the PlugIn itself
    g_pi_filename = GetPlugInPath(this);

    AddLocaleCatalog( _T("opencpn-oesenc_pi") );

      //    Build an arraystring of dynamically loadable chart class names
    m_class_name_array.Add(_T("oeSENCChart"));
    m_class_name_array.Add(_T("oeEVCChart"));

     
    // Specify the location of the xxserverd helper.
#ifdef __WXMSW__
      g_sencutil_bin = GetPluginDataDir("oesenc_pi") + _T("\\oeserverd.exe");

//#else      
      //wxFileName fn_exe(GetOCPN_ExePath());
      //g_sencutil_bin = fn_exe.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + _T("oeserverd");
#endif
      
      // Search for helper in the $PATH.
      // This will avoid accidentally picking up a helper location lefover from a Legacy installation.
      
      if (!wxFileExists(g_sencutil_bin)) {
        std::string path(find_in_path("oeserverd"));
        if (path == "") {
            wxLogWarning("Cannot locate oeserverd binary in $PATH");
        }
        else {
            g_sencutil_bin = wxString(path.c_str());
        }
      }
      
            // Account for possible "space" in Mac directory name.
#ifdef __WXOSX__
      g_sencutil_bin.Prepend(_T("\""));
      g_sencutil_bin.Append(_T("\""));
#endif    


    
    
    
    

#ifdef __OCPN__ANDROID__
    wxString piLocn = GetPlugInPath(this); //*GetpSharedDataLocation();
    wxFileName fnl(piLocn);
    g_sencutil_bin = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("oeserverda");
    g_serverProc = 0;
#endif

    // Set environment variable for some platforms to find the required sglock dongle library
    wxFileName fn_exe(GetOCPN_ExePath());

#if !defined(__WXMSW__) && !defined(__WXMAC__)
    // Set environment variable to find the required sglock dongle library
    wxFileName libraryPath = fn_exe;
    libraryPath.RemoveLastDir();
    wxString libDir = libraryPath.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + _T("lib/opencpn");
    wxSetEnv(_T("LD_LIBRARY_PATH"), libDir ); //"/usr/local/lib/opencpn");
#endif

#ifdef __WXMAC__
    // Set environment variable to find the required sglock dongle library
    wxString libDir = fn_exe.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + _T("PlugIns/oesenc_pi");
    wxSetEnv(_T("DYLD_LIBRARY_PATH"), libDir ); 
    wxLogMessage(_T("OSX LIB DYLD_LIBRARY_PATH: ") + libDir);
#endif

    wxLogMessage(_T("Path to oeserverd is: ") + g_sencutil_bin);

    if(IsDongleAvailable())
        wxLogMessage(_T("Dongle detected"));
    else
        wxLogMessage(_T("No Dongle detected"));
        
    g_benable_screenlog = g_buser_enable_screenlog;
    
    g_ChartInfoArray.Clear();
   
#ifdef __OCPN__ANDROID__
    g_deviceInfo = callActivityMethod_vs("getDeviceInfo");
    
    wxStringTokenizer tkz(g_deviceInfo, _T("\n"));
    while( tkz.HasMoreTokens() )
    {
        wxString s1 = tkz.GetNextToken();
        if(wxNOT_FOUND != s1.Find(_T("systemName:"))){
            g_systemName = s1.AfterFirst(':');
        }
        if(wxNOT_FOUND != s1.Find(_T("UUID:"))){
            g_UUID = s1.AfterFirst(':');
        }
        if(wxNOT_FOUND != s1.Find(_T("OS SDK_INT:"))){
            g_sSDK_INT = s1.AfterFirst(':');
        }
        if(wxNOT_FOUND != s1.Find(_T("OCPNWVID:"))){
            g_WVID = s1.AfterFirst(':');
        }
    }
    qDebug() << "Init() systemName by deviceInfo: " << g_systemName.mb_str();
    qDebug() << "Init() UUID by deviceInfo: " << g_UUID.mb_str();
    qDebug() << "Init() OS SDK_INT by deviceInfo: " << g_sSDK_INT.mb_str();
    qDebug() << "Init() WVID by deviceInfo: " << g_WVID.mb_str();
    
    long nsdk;
    g_sSDK_INT.ToLong(&nsdk);
    g_SDK_INT = nsdk;
    
#endif
    
    //testSENCServer();
    
    int flags =  INSTALLS_PLUGIN_CHART_GL |
                 WANTS_PLUGIN_MESSAGING   |
                 WANTS_OVERLAY_CALLBACK   |
                 WANTS_OPENGL_OVERLAY_CALLBACK;

    flags |= INSTALLS_TOOLBOX_PAGE;             // for o-charts shop interface
    flags |= WANTS_PREFERENCES;             

    init_S52Library();

    // Establish the system build type for server identification
    /*
    w = Windows
    d = macOS
    l = Linux
    r = Android
    */
    g_systemOS = _T("l.");            // default
#ifdef __WXMSW__
    g_systemOS = _T("w.");
#endif
#ifdef __WXMAC__
    g_systemOS = _T("d.");
#endif
    // Android handled in Java-side interface
    
    
    return flags;
    
}

bool oesenc_pi::DeInit(void)
{
    SaveConfig();
    
    delete pinfoDlg;
    pinfoDlg = NULL;
    
    if( m_pOptionsPage )
    {
        if( DeleteOptionsPage( m_pOptionsPage ) )
            m_pOptionsPage = NULL;
    }
    
    m_class_name_array.Clear();
    
    shutdown_SENC_server();
    
    return true;
}

int oesenc_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}

int oesenc_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}

int oesenc_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int oesenc_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

wxBitmap *oesenc_pi::GetPlugInBitmap()
{
      return m_pplugin_icon;
}

wxString oesenc_pi::GetCommonName()
{
      return _("oeSENC");
}


wxString oesenc_pi::GetShortDescription()
{
      return _("PlugIn for OpenCPN oeSENC charts");
}


wxString oesenc_pi::GetLongDescription()
{
      return _("PlugIn for OpenCPN\n\
Provides support of oeSENC charts.\n\n\
");

}

wxArrayString oesenc_pi::GetDynamicChartClassNameArray()
{
      return m_class_name_array;
}

void oesenc_pi::SetPluginMessage(wxString &message_id, wxString &message_body)
{
    
    if(message_id == _T("OpenCPN Config"))
    {

        // construct the JSON root object
        wxJSONValue  root;
        // construct a JSON parser
        wxJSONReader reader;

        // now read the JSON text and store it in the 'root' structure
        // check for errors before retreiving values...
        int numErrors = reader.Parse( message_body, &root );
        if ( numErrors > 0 )  {
            //              const wxArrayString& errors = reader.GetErrors();
            return;
        }

        // Capture the OpenCPN base version information
        g_coreVersionMajor = root[_T("OpenCPN Version Major")].AsInt();
        g_coreVersionMinor = root[_T("OpenCPN Version Minor")].AsInt();
        g_coreVersionPatch = root[_T("OpenCPN Version Patch")].AsInt();
        
        
        // Capture the S52PLIB configuration
        if(ps52plib){
            
            //  We may need a reconfig of global settings
            // If so, do it first, then overide some values per-canvas.
            if(root[_T("OpenCPN S52PLIB GlobalReconfig")].IsBool()){
                bool needReconfig = root[_T("OpenCPN S52PLIB GlobalReconfig")].AsBool();
                if(needReconfig){
                    ps52plib->PLIB_LoadS57GlobalConfig();
                    ps52plib->PLIB_LoadS57ObjectConfig();
                    
                    // Set the chart object scale factor, it may have changed.
                    g_ChartScaleFactorExp = GetOCPNChartScaleFactor_Plugin();
                }
            }

            // Per canvas values can be overriden here.
            if(root[_T("OpenCPN S52PLIB ShowText")].IsBool())             ps52plib->m_bShowS57Text = root[_T("OpenCPN S52PLIB ShowText")].AsBool();
            if(root[_T("OpenCPN S52PLIB ShowSoundings")].IsBool())        ps52plib->m_bShowSoundg = root[_T("OpenCPN S52PLIB ShowSoundings")].AsBool();
            if(root[_T("OpenCPN S52PLIB ShowAnchorConditions")].IsBool()) ps52plib->SetAnchorOn( root[_T("OpenCPN S52PLIB ShowAnchorConditions")].AsBool() );
            if(root[_T("OpenCPN S52PLIB ShowLightDescription")].IsBool()) ps52plib->SetShowLdisText( root[_T("OpenCPN S52PLIB ShowLightDescription")].AsBool() );
            if(root[_T("OpenCPN S52PLIB ShowATONLabel")].IsBool())        ps52plib->SetShowAtonText( root[_T("OpenCPN S52PLIB ShowATONLabel")].AsBool() );
            if(root[_T("OpenCPN S52PLIB ShowQualityOfData")].IsBool())    ps52plib->SetQualityOfData( root[_T("OpenCPN S52PLIB ShowQualityOfData")].AsBool() );
            
            if(root[_T("OpenCPN S52PLIB MetaDisplay")].IsBool())           ps52plib->m_bShowMeta = root[_T("OpenCPN S52PLIB MetaDisplay")].AsBool();
            if(root[_T("OpenCPN S52PLIB DeclutterText")].IsBool())         ps52plib->m_bDeClutterText = root[_T("OpenCPN S52PLIB DeclutterText")].AsBool();
            if(root[_T("OpenCPN S52PLIB ShowNationalText")].IsBool())      ps52plib->m_bShowNationalTexts = root[_T("OpenCPN S52PLIB ShowNationalText")].AsBool();
            if(root[_T("OpenCPN S52PLIB UseSCAMIN")].IsBool())             ps52plib->m_bUseSCAMIN = root[_T("OpenCPN S52PLIB UseSCAMIN")].AsBool();
            if(root[_T("OpenCPN S52PLIB ShowImportantTextOnly")].IsBool()) ps52plib->m_bShowS57ImportantTextOnly = root[_T("OpenCPN S52PLIB ShowImportantTextOnly")].AsBool();
        
            if(root[_T("OpenCPN S52PLIB SymbolStyle")].IsInt())           ps52plib->m_nSymbolStyle = (LUPname)root[_T("OpenCPN S52PLIB SymbolStyle")].AsInt();
            if(root[_T("OpenCPN S52PLIB BoundaryStyle")].IsInt())         ps52plib->m_nBoundaryStyle = (LUPname)root[_T("OpenCPN S52PLIB BoundaryStyle")].AsInt();
            if(root[_T("OpenCPN S52PLIB ColorShades")].IsDouble())        S52_setMarinerParam( S52_MAR_TWO_SHADES, root[_T("OpenCPN S52PLIB ColorShades")].AsDouble());
            if(root[_T("OpenCPN S52PLIB SoundingsFactor")].IsInt())       ps52plib->m_nSoundingFactor = root[_T("OpenCPN S52PLIB SoundingsFactor")].AsInt();

            int icat;
            if( root[_T("OpenCPN S52PLIB DisplayCategory")].AsInt(icat) ){
                _DisCat dcat = (_DisCat)icat;
                ps52plib->SetDisplayCategory( dcat );
            }
            
                            // Detect and manage "LIGHTS" toggle
            if(root[_T("OpenCPN S52PLIB ShowLights")].IsBool()){
                bool bNewVal = root[_T("OpenCPN S52PLIB ShowLights")].AsBool();
                if(bNewVal != !ps52plib->GetLightsOff()){
                    ps52plib->SetLightsOff( !bNewVal );
//                     if(!bNewVal)                     // On, going off
//                         ps52plib->AddObjNoshow("LIGHTS");
//                     else                                   // Off, going on
//                         ps52plib->RemoveObjNoshow("LIGHTS");
                    }
            }
            

            
            ps52plib->SetOCPNVersion( g_coreVersionMajor, g_coreVersionMinor, g_coreVersionPatch);
            
         }
        
        if(root[_T("OpenCPN Zoom Mod Vector")].IsInt())
            g_chart_zoom_modifier_vector = root[_T("OpenCPN Zoom Mod Vector")].AsInt();

        if(root[_T("OpenCPN Display Width")].IsInt()){
            g_display_size_mm = (double)root[_T("OpenCPN Display Width")].AsInt();
            
            wxWindow *cc1 = GetOCPNCanvasWindow();
            if(cc1){
                int display_size_mm = wxMax(g_display_size_mm, 75);
                
                int sx, sy;
                wxDisplaySize( &sx, &sy );
                double max_physical = wxMax(sx, sy);
                
                double pix_per_mm = ( max_physical ) / ( (double) display_size_mm );
                if(ps52plib)
                    ps52plib->SetPPMM( pix_per_mm );
                
                g_pix_per_mm = pix_per_mm;
                
                wxString msg;
                msg.Printf(_T("oesenc_pi:  Calculated pix/mm = %g"), g_pix_per_mm);
                wxLogMessage(msg);
            }
        }
        
        if(ps52plib)
            ps52plib->GenerateStateHash();
            
    }
    else if(message_id == _T("OCPN_OPENGL_CONFIG"))
    {
        // construct the JSON root object
        wxJSONValue  root;
        // construct a JSON parser
        wxJSONReader reader;
        
        // now read the JSON text and store it in the 'root' structure
        // check for errors before retreiving values...
        int numErrors = reader.Parse( message_body, &root );
        if ( numErrors > 0 )  {
            //              const wxArrayString& errors = reader.GetErrors();
            return;
        }
        
        //float g_GLMinCartographicLineWidth;
        // is global ...bool  g_b_EnableVBO;
        //float g_GLMinSymbolLineWidth;
        //GLenum g_texture_rectangle_format;
        //bool pi_bopengl;
        
        // Capture the OpenCPN OpenGL config, and inform the PLIB
        if( root[_T("setupComplete")].AsBool() )
        {
            g_b_EnableVBO = root[_T("useVBO")].AsBool();
            g_oe_texture_rectangle_format = root[_T("TextureRectangleFormat")].AsInt();

            g_b_useStencil = root[_T("useStencil")].AsBool();
            g_b_useStencilAP = root[_T("useStencilAP")].AsBool();
            g_b_useScissorTest = root[_T("useScissorTest")].AsBool();
            g_b_useFBO = root[_T("useFBO")].AsBool();
        
            g_GLOptionsSet = true;
            
            init_GLLibrary();                                  // once
            g_oeChartSymbols->ResetRasterTextureCache();
 
        }
    }
        
}

void oesenc_pi::SetColorScheme(PI_ColorScheme cs)
{
    global_color_scheme = cs;
    
    if(ps52plib)
        ps52plib-> SetPLIBColorScheme((ColorScheme)cs);
}

bool oesenc_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    if(g_brendered_expired && !g_bnoShow_sse25){
        wxString msg = _T("SSE 25..The ENC permit for this cell has expired.\n This cell may be out of date and MUST NOT be used for NAVIGATION.");


        wxFont *pfont = wxTheFontList->FindOrCreateFont(10, wxFONTFAMILY_DEFAULT,
                                                        wxFONTSTYLE_NORMAL,
                                                        wxFONTWEIGHT_NORMAL);

        dc.SetFont( *pfont );
        dc.SetPen( *wxTRANSPARENT_PEN);

        dc.SetBrush( wxColour(243, 229, 47 ) );
        int w, h;
        dc.GetMultiLineTextExtent( msg, &w, &h );
        h += 2;
        int yp = vp->pix_height - 20 - h;

        int label_offset = 10;
        int wdraw = w + ( label_offset * 2 );
        dc.DrawRectangle( 0, yp, wdraw, h );
        dc.DrawLabel( msg, wxRect( label_offset, yp, wdraw, h ),
                    wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL);
        g_brendered_expired = false;
    }
    return false;
}

bool oesenc_pi::RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
#ifndef USE_ANDROID_GLES2    
    if(g_brendered_expired && !g_bnoShow_sse25){
        wxString msg = _T("SSE 25..The ENC permit for this cell has expired.\n This cell may be out of date and MUST NOT be used for NAVIGATION.");


        wxFont *pfont = wxTheFontList->FindOrCreateFont(10, wxFONTFAMILY_DEFAULT,
                                                        wxFONTSTYLE_NORMAL,
                                                        wxFONTWEIGHT_NORMAL);
        m_TexFontMessage.Build(*pfont);
        int w, h;
        m_TexFontMessage.GetTextExtent( msg, &w, &h);
        h += 2;
        int yp = vp->pix_height - 20 - h;

        glColor3ub( 243, 229, 47 );

        glBegin(GL_QUADS);
        glVertex2i(0, yp);
        glVertex2i(w, yp);
        glVertex2i(w, yp+h);
        glVertex2i(0, yp+h);
        glEnd();

        glEnable(GL_BLEND);
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        glColor3ub( 0, 0, 0 );
        glEnable(GL_TEXTURE_2D);
        m_TexFontMessage.RenderString( msg, 5, yp);
        glDisable(GL_TEXTURE_2D);

        g_brendered_expired = false;

    }
#endif    
    return false;
}



//      Options Dialog Page management


void oesenc_pi::OnShowFPRClick( wxCommandEvent &event )
{
    wxExecute( wxString::Format("open -R %s", g_fpr_file) );
}

void oesenc_pi::OnNewFPRClick( wxCommandEvent &event )
{
    
    wxString msg = _("To obtain a User Key, you must generate a unique System Identifier File.\n");
    msg += _("This file is also known as a\"fingerprint\" file.\n");
    msg += _("The fingerprint file contains information to uniquely identifiy this computer.\n\n");
    msg += _("After creating this file, you will need it to obtain your User Key at the o-charts.org shop.\n\n");
    msg += _("Proceed to create Fingerprint file?");
    
    int ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_PI Message"), wxYES_NO);
    
    if(ret == wxID_YES){
#if 0        
        wxString fpr_file;
        wxString fpr_dir = *GetpPrivateApplicationDataLocation(); //GetWritableDocumentsDir();
        #ifdef __WXMSW__
        
        //  On XP, we simply use the root directory, since any other directory may be hidden
        int major, minor;
        ::wxGetOsVersion( &major, &minor );
        if( (major == 5) && (minor == 1) )
            fpr_dir = _T("C:\\");
        #endif        
            
            wxString cmd;
            cmd += _T(" -w ");                  // validate cell permit
            
            cmd += _T(" -o ");
            cmd += fpr_dir;
            
            ::wxBeginBusyCursor();
            
            wxArrayString valup_result = exec_SENCutil_sync( cmd, false);
            
            ::wxEndBusyCursor();
            
            bool berr = false;
            for(unsigned int i=0 ; i < valup_result.GetCount() ; i++){
                wxString line = valup_result[i];
                if(line.Upper().Find(_T("ERROR")) != wxNOT_FOUND){
                    berr = true;
                    break;
                }
                if(line.Upper().Find(_T("FPR")) != wxNOT_FOUND){
                    fpr_file = line.AfterFirst(':');
                }
                
            }
            
            
            if(!berr && fpr_file.Length()){
                wxString msg1 = _T("Fingerprint file created.\n");
                msg1 += fpr_file;
                
                OCPNMessageBox_PlugIn(NULL, msg1, _T("S63_PI Message"), wxOK);
            }
            else{
                wxLogMessage(_T("S63_pi: OCPNsenc results:"));
                for(unsigned int i=0 ; i < valup_result.GetCount() ; i++){
                    wxString line = valup_result[i];
                    wxLogMessage( line );
                }
                OCPNMessageBox_PlugIn(NULL, _T("ERROR Creating Fingerprint file\n Check OpenCPN log file."), _T("S63_PI Message"), wxOK);
            }
            
            g_fpr_file = fpr_file;
            
            m_parent->Set_FPR();
#endif            
    }
}

void oesenc_pi::OnCloseToolboxPanel(int page_sel, int ok_apply_cancel)
{
    m_up_text = NULL;

    if(g_pPanelScreenLog){
        g_pPanelScreenLog->Close();
        delete g_pPanelScreenLog;
        g_pPanelScreenLog = NULL;
    }

    g_backchannel_port++;


}

#if 0
wxString oesenc_pi::GetPermitDir()
{
    wxString os63_dirname = g_CommonDataDir;
    os63_dirname += _T("s63charts");

    return os63_dirname;
}


Catalog31 *oesenc_pi::CreateCatalog31(const wxString &file31)
{
    Catalog31 *rv = new Catalog31();

    DDFModule poModule;
    if( poModule.Open( file31.mb_str() ) ) {
        poModule.Rewind();

        //    Read and parse the file
        //    Each record corresponds to a file in the exchange set

        DDFRecord *pr = poModule.ReadRecord();                              // Record 0

        while(pr){

            Catalog_Entry31 *pentry = new Catalog_Entry31;

            //  Look for records whose bas file name is the same as the .000 file, and is numeric extension
            //  And decide whether to add them to update array

            char *u = NULL;
            u = (char *) ( pr->GetStringSubfield( "CATD", 0, "FILE", 0 ) );
            if( u ) {
                wxString file = wxString( u, wxConvUTF8 );

#ifndef __WXMSW__
                file.Replace(_T("\\"), _T("/"));
#endif

                pentry->m_filename = file;
            }

           u = (char *) ( pr->GetStringSubfield( "CATD", 0, "COMT", 0 ) );
           if(u){
               wxString comt = wxString( u, wxConvUTF8 );
               pentry->m_comt = comt;
           }

           rv->Add(pentry);

           pr = poModule.ReadRecord();
        }

    }


    return rv;
}
#endif


#if 0
int oesenc_pi::ImportCells( void )
{

    return 0;
}




int oesenc_pi::ImportCert(void)
{

    return 0;

}

int oesenc_pi::ImportCellPermits(void)
{

    return 0;
}

#endif


void oesenc_pi::Set_FPR()
{
    g_prefs_dialog->EndModal( wxID_OK );
    g_prefs_dialog->m_buttonShowFPR->Enable( g_fpr_file != wxEmptyString );
}


int oesenc_pi::pi_error( wxString msg )
{
    return 0;
}

bool oesenc_pi::ScrubChartinfoList( void )
{
    //  Get the list of directories that the chart database recognises.
    wxArrayString chartArray;
    wxFileConfig *pConf = (wxFileConfig *) g_pconfig;
    
    if(g_debugLevel) wxLogMessage(_T("Scrub1: "));
    
    pConf->SetPath( _T ( "/ChartDirectories" ) );
    int iDirMax = pConf->GetNumberOfEntries();
    if( iDirMax ) {
        wxString str, val;
        long dummy;
        bool bCont = pConf->GetFirstEntry( str, dummy );
        while( bCont ) {
            pConf->Read( str, &val );              // Get a Directory name
            
            // remove/fix the decorations
            wxString valAdd = val.BeforeFirst('^') + wxString(wxFileName::GetPathSeparator());

            if(g_debugLevel) wxLogMessage(_T("  Dirlist  val: ") + val + _T("  valadd: ") + valAdd);
            
            chartArray.Add(valAdd);
            bCont = pConf->GetNextEntry( str, dummy );
        }
    }
    
    // And walk the hashmap of ChartinfoItems, trying to find a match from the hashmap item to the directory list contents

    if(g_debugLevel) wxLogMessage(_T("Scrub2: "));
    
    pConf->SetPath ( _T ( "/PlugIns/oesenc/ChartinfoList" ) );
    std::map<std::string, ChartInfoItem *>::iterator iter = info_hash.begin();
    while( iter != info_hash.end())
    {
        std::string key = iter->first;
        wxString strk = wxString(key.c_str(), wxConvUTF8);
        
        //  Turn the key back into a directory path by removing/fixing the decorations
        
        wxString strt = strk.Mid(2);
        strt.Replace('!', wxFileName::GetPathSeparator());
        
        if(g_debugLevel) wxLogMessage(_T("strk: ") + strk);
        if(g_debugLevel) wxLogMessage(_T("strt: ") + strt);
        
        bool bfound = false;
        
        //  Of course, the candidate directory must exist...
        if(::wxDirExists(strt)){
            
            for(unsigned int i=0 ; i < chartArray.GetCount() ; i++){
                wxString ts = chartArray.Item(i);
                wxFileName target(ts);
                wxString tara = target.GetPath();
                if(g_debugLevel) wxLogMessage(_T("ChartDir entry considered: ") + tara);
                
                bool done = false;
                wxString cana;
                wxFileName candidate = wxFileName(strt);
                
                while(!done){
                    cana = candidate.GetPath();
                    if(g_debugLevel) wxLogMessage(_T("  Chartinfo candidate tested: ") + cana);
                    
                    if(target.GetPath() == candidate.GetPath()){
                        if(g_debugLevel) wxLogMessage(_T("done1"));
                        done = true;
                    }
                    
                    if(candidate.GetFullPath() == target.GetFullPath()){
                        if(g_debugLevel) wxLogMessage(_T("done2"));
                        
                        done = true;
                        bfound = true;
                        break;
                    }
                    candidate.RemoveLastDir();
                    if(!candidate.GetDirCount()){
                        if(g_debugLevel) wxLogMessage(_T("done3"));
                        done = true;
                    }
                }
                if(bfound)
                    break;
            }
        }
        else{
            if(g_debugLevel) wxLogMessage(_T("  Candidate does not exist: ") + strt);
        }
            
        
        //  Did not find the directory, so remove corresponding entry from the hashmap.
        //  This means that the entry will not be written to config file on app exit, so it is gone.
        if(!bfound){
            info_hash.erase(iter);
            iter = info_hash.begin();
            if(g_debugLevel) wxLogMessage(_T("    dropping: ") + strk + _T("\n"));
        }
        else{
            ++iter;
            if(g_debugLevel) wxLogMessage(_T("    keeping: ") + strk + _T("\n"));
        }
        
    }
    return true;
}
    

bool oesenc_pi::LoadConfig( void )
{
    wxFileConfig *pConf = (wxFileConfig *) g_pconfig;

    if( pConf ) {
        pConf->SetPath( _T ( "/Settings" ) );    
        
        pConf->Read( _T ( "ZoomDetailFactorVector" ), &g_chart_zoom_modifier_vector, 0 );
        g_chart_zoom_modifier_vector = wxMin(g_chart_zoom_modifier_vector,5);
        g_chart_zoom_modifier_vector = wxMax(g_chart_zoom_modifier_vector,-5);
        

        pConf->SetPath( _T("/PlugIns/oesenc") );

        //      Defaults
        g_installpermit = _T("Y");
        g_userpermit = _T("X");

        pConf->Read( _T("Userpermit"), &g_userpermit );
        pConf->Read( _T("Installpermit"), &g_installpermit );
        pConf->Read( _T("LastENCROOT"), &m_last_enc_root_dir);
        pConf->Read( _T("S63CommonDataDir"), &g_CommonDataDir);
        pConf->Read( _T("ShowScreenLog"), &g_buser_enable_screenlog);
        pConf->Read( _T("NoShowSSE25"), &g_bnoShow_sse25);
        pConf->Read( _T("LastFPRFile"), &g_fpr_file);
        pConf->Read( _T("DEBUG_SERVER"), &g_serverDebug);
        pConf->Read( _T("DEBUG_LEVEL"), &g_debugLevel);

        // On initial start, do not override g_systemName as populated in Init() method
        wxString snTest;
        pConf->Read( _T("systemName"), &snTest);
        if(snTest.Length() && g_systemName.IsEmpty())
            g_systemName = snTest;
        
        pConf->Read( _T("loginUser"), &g_loginUser);
        pConf->Read( _T("loginKey"), &g_loginKey);
        pConf->Read( _T("ADMIN"), &g_admin);
        pConf->Read( _T("DEBUG_SHOP"), &g_debugShop);
        
        if( !wxFileExists(g_fpr_file) )
            g_fpr_file = wxEmptyString;
        
        pConf->Read( _T("UserKey"), &g_UserKey );

        //  Load the persistent Chartinfo strings
        pConf->SetPath ( _T ( "/PlugIns/oesenc/ChartinfoList" ) );
        
        wxString strk;
        wxString kval;
        long dummyval;
        bool bContk = pConf->GetFirstEntry( strk, dummyval );
        while( bContk ) {
            pConf->Read( strk, &kval );
            std::string key = std::string(strk.c_str());
            std::map<std::string, ChartInfoItem *>::iterator iter;
            iter = info_hash.find( key );
            if( iter == info_hash.end() ){
                ChartInfoItem *pitem = new ChartInfoItem;
                pitem->config_string = kval;
                info_hash[key] = pitem;
                if(g_debugLevel) wxLogMessage(_T("Loadconfig adding: ") + strk);
                wxLogMessage(_T("Loadconfig adding info string: ") + kval);
                
            }
                
            bContk = pConf->GetNextEntry( strk, dummyval );
        }

        //  Load the persistent EULA information
        pConf->SetPath ( _T ( "/PlugIns/oesenc/EULA" ) );
        
        bContk = pConf->GetFirstEntry( strk, dummyval );
        while( bContk ) {
            pConf->Read( strk, &kval );
            
            ChartSetEULA *cse = new ChartSetEULA;
            wxStringTokenizer tkz( kval, _T(";") );
            wxString EULAShow = tkz.GetNextToken();        // oesencEULAShow, text
            wxString EULAShown = tkz.GetNextToken();        // Has it been shown at least once?  1/0
            wxString EULAFile = tkz.GetNextToken();
            
            cse->fileName = EULAFile;
            
            if(EULAShow.Upper().Find(_T("ONCE")) != wxNOT_FOUND)
                cse->npolicyShow = 1;
            else if(EULAShow.Upper().Find(_T("ALWAYS")) != wxNOT_FOUND)
                cse->npolicyShow = 2;
            else 
                cse->npolicyShow = 0;
            
            if(EULAShown ==_T("1"))
                cse->b_onceShown = true;
            
            g_EULAArray.Add(cse);
            
            
            bContk = pConf->GetNextEntry( strk, dummyval );
        }
        
        
    }

    return true;
}

bool oesenc_pi::SaveConfig( void )
{
    wxFileConfig *pConf = (wxFileConfig *) g_pconfig;

    if( pConf ) {
        pConf->SetPath( _T("/PlugIns/oesenc") );

        pConf->Write( _T("UserKey"), g_UserKey );
        pConf->Write( _T("LastFPRFile"), g_fpr_file);

#ifdef __OCPN__ANDROID__        
        pConf->Write( _T("systemName"), g_systemName);
        pConf->Write( _T("loginUser"), g_loginUser);
        pConf->Write( _T("loginKey"), g_loginKey);
#endif        
        
        //  Save the persistent Chartinfo strings
        pConf->DeleteGroup(_T ( "/PlugIns/oesenc/ChartinfoList"));
        pConf->SetPath ( _T ( "/PlugIns/oesenc/ChartinfoList" ) );
        std::map<std::string, ChartInfoItem *>::iterator iter;
        for( iter = info_hash.begin(); iter != info_hash.end(); ++iter )
        {
            ChartInfoItem *pci = iter->second;
            std::string key = iter->first;
            wxString strk = wxString(key.c_str(), wxConvUTF8);
            pConf->Write( strk, pci->config_string );
            
        }

        //  Save the persistent EULA
        pConf->DeleteGroup(_T ( "/PlugIns/oesenc/EULA"));
        pConf->SetPath ( _T ( "/PlugIns/oesenc/EULA" ) );
        
        for(unsigned int i=0 ; i < g_EULAArray.GetCount() ; i++){
            ChartSetEULA *cse = g_EULAArray.Item(i);
            
            wxString config_val;
            wxString EULAShow = _T("never");
            if(cse->npolicyShow == 1)
                EULAShow = _T("once");
            if(cse->npolicyShow == 2)
                EULAShow = _T("always");
            
            config_val += EULAShow + _T(";");
            if(cse->b_onceShown)
                config_val += _T("1;");
            else
                config_val += _T("0;");
            
            config_val += cse->fileName;
            
            wxString key;
            key.Printf(_T("EULA_%02d"), i);

            pConf->Write( key, config_val );
        }
        
    }

    return true;
}

#define ANDROID_DIALOG_BACKGROUND_COLOR    wxColour(_T("#7cb0e9"))
#define ANDROID_DIALOG_BODY_COLOR         wxColour(192, 192, 192)

void oesenc_pi::ShowPreferencesDialog( wxWindow* parent )
{
    wxString titleString =  _("oeSENC_PI Preferences");

    long style = wxDEFAULT_DIALOG_STYLE;
#ifdef __WXOSX__
        style |= wxSTAY_ON_TOP;
#endif

    g_prefs_dialog = new oesencPrefsDialog( parent, wxID_ANY, titleString, wxPoint( 20, 20), wxDefaultSize, style );
    g_prefs_dialog->Fit();
//    g_prefs_dialog->SetSize(wxSize(300, -1));
    //wxColour cl;
    //GetGlobalColor(_T("DILG1"), &cl);
//    g_prefs_dialog->SetBackgroundColour(cl);
    
    
    g_prefs_dialog->Show();
        
    if(g_prefs_dialog->ShowModal() == wxID_OK)
    {
        SaveConfig();
        
    }
    delete g_prefs_dialog;
    g_prefs_dialog = NULL;
}

void oesenc_pi::ProcessChartManageResult( wxString result )
{
    if(g_prefs_dialog)
       g_prefs_dialog->EndModal(0); 

#ifdef __OCPN__ANDROID__    
    qDebug() << "ProcessChartManageResult: " << result.mb_str();
    bool b_forceUpdate = false;
   
    wxStringTokenizer st(result, _T(";"), wxTOKEN_DEFAULT);
    while( st.HasMoreTokens() )
    {
        wxString token = st.GetNextToken();
        if(token.StartsWith(_T("InstallDir"))){
            wxString dir = token.AfterFirst(':');
            
            // Strip any trailing '/'
            wxString rest;
            if(dir.EndsWith("/", &rest)){
                dir = rest;
            }
            
            bool covered = false;
            for( size_t i = 0; i < GetChartDBDirArrayString().GetCount(); i++ ){
                if( dir.StartsWith((GetChartDBDirArrayString().Item(i))) ) {
                    covered = true;
                    break;
                }
            }
            if( !covered ){
                AddChartDirectory( dir );
                wxLogMessage(_T("osenc_pi adding chart directory: ") + dir);
                qDebug() << "adding dir: " << dir.mb_str();
            }
            
            b_forceUpdate = true;
        }
        
        else if(token.StartsWith(_T("UserName"))){
            g_loginUser = token.AfterFirst(':');
            qDebug() << "g_loginUser: " << g_loginUser.mb_str();
        }
        
        else if(token.StartsWith(_T("LoginKey"))){
            g_loginKey = token.AfterFirst(':');
            qDebug() << "g_loginKey: " << g_loginKey.mb_str();
        }
        
        else if(token.StartsWith(_T("SystemName"))){
            g_systemName = token.AfterFirst(':');
            qDebug() << "g_systemName: " << g_systemName.mb_str();
        }
    }
    
    // This is a bit harsh, but always works...
    if(b_forceUpdate)
        ForceChartDBUpdate();
#endif

}


#if 0
// An Event handler class to catch events from S63 UI dialog
//      Implementation

s63_pi_event_handler::s63_pi_event_handler(oesenc_pi *parent)
{
    m_parent = parent;
}

s63_pi_event_handler::~s63_pi_event_handler()
{
}

void s63_pi_event_handler::OnImportPermitClick( wxCommandEvent &event )
{
    m_parent->ImportCellPermits();
}

void s63_pi_event_handler::OnRemovePermitClick( wxCommandEvent &event )
{
    m_parent->RemoveCellPermit();
}

void s63_pi_event_handler::OnImportCellsClick( wxCommandEvent &event )
{
    SendPluginMessage(_T("S63_CALLBACK_PRIVATE_1"), wxEmptyString);

//    m_parent->ImportCells();
}


void s63_pi_event_handler::OnSelectPermit( wxListEvent& event )
{
    m_parent->EnablePermitRemoveButton(true);
}

void s63_pi_event_handler::OnNewUserpermitClick( wxCommandEvent& event )
{
//    m_parent->GetNewUserpermit();
}

void s63_pi_event_handler::OnNewInstallpermitClick( wxCommandEvent& event )
{
//    m_parent->GetNewInstallpermit();
}

void s63_pi_event_handler::OnImportCertClick( wxCommandEvent &event )
{
    m_parent->ImportCert();
}
#endif

#if 0
//      Private logging functions
void ScreenLogMessage(wxString s)
{
    if(!g_benable_screenlog)
        return;

    if(!g_pScreenLog && !g_pPanelScreenLog){
        g_pScreenLog = new S63ScreenLogContainer( GetOCPNCanvasWindow() );
        g_pScreenLog->Centre();

    }

    if( g_pScreenLog ) {
        g_pScreenLog->LogMessage(s);
    }
    else if( g_pPanelScreenLog ){
        g_pPanelScreenLog->LogMessage(s);
    }
}
void HideScreenLog(void)
{
    if( g_pScreenLog ) {
        g_pScreenLog->Hide();
    }
    else if( g_pPanelScreenLog ) {
        g_pPanelScreenLog->Hide();
    }

}

void ClearScreenLog(void)
{
    if( g_pScreenLog ) {
        g_pScreenLog->ClearLog();
    }
    else if( g_pPanelScreenLog ) {
        g_pPanelScreenLog->ClearLog();
    }

}

void ClearScreenLogSeq(void)
{
    if( g_pScreenLog ) {
        g_pScreenLog->m_slog->ClearLogSeq();
    }
    else if( g_pPanelScreenLog ) {
        g_pPanelScreenLog->ClearLogSeq();
    }

}
#endif

BEGIN_EVENT_TABLE(InfoWinDialog, wxDialog)
EVT_PAINT ( InfoWinDialog::OnPaint )
EVT_ERASE_BACKGROUND(InfoWinDialog::OnEraseBackground)
EVT_TIMER(-1, InfoWinDialog::OnTimer)
END_EVENT_TABLE()


// Define a constructor
InfoWinDialog::InfoWinDialog( wxWindow *parent, const wxString&s, bool show_gauge ) :
wxDialog( parent, wxID_ANY, _T("Info"), wxDefaultPosition, wxDefaultSize,  wxSTAY_ON_TOP )
{
    int ststyle = wxALIGN_LEFT | wxST_NO_AUTORESIZE;
    m_pInfoTextCtl = new wxStaticText( this, -1, _T ( "" ), wxDefaultPosition, wxDefaultSize,
                                       ststyle );


    m_pGauge = NULL;
    m_bGauge = show_gauge;
    SetString( s );

    if(m_bGauge) {
        m_timer.SetOwner( this, -1 );
        m_timer.Start( 100 );
    }


    Hide();
}

InfoWinDialog::~InfoWinDialog()
{
    delete m_pInfoTextCtl;
}

void InfoWinDialog::OnTimer(wxTimerEvent &evt)
{
    if(m_pGauge)
        m_pGauge->Pulse();

    #ifdef __WXMAC__
        Raise();
    #endif


}


void InfoWinDialog::OnEraseBackground( wxEraseEvent& event )
{
}

void InfoWinDialog::OnPaint( wxPaintEvent& event )
{
    int width, height;
    GetClientSize( &width, &height );
    wxPaintDC dc( this );

    wxColour c;

    GetGlobalColor( _T ( "UIBCK" ), &c );
    dc.SetBrush( wxBrush( c ) );

    GetGlobalColor( _T ( "UITX1" ), &c );
    dc.SetPen( wxPen( c ) );

    dc.DrawRectangle( 0, 0, width, height );
}

void InfoWinDialog::Realize()
{
    wxColour c;

    GetGlobalColor( _T ( "UIBCK" ), &c );
    SetBackgroundColour( c );

    GetGlobalColor( _T ( "UIBCK" ), &c );
    m_pInfoTextCtl->SetBackgroundColour( c );

    GetGlobalColor( _T ( "UITX1" ), &c );
    m_pInfoTextCtl->SetForegroundColour( c );

    int x;
    GetTextExtent(m_string, &x, NULL);

    m_pInfoTextCtl->SetSize( (m_size.x - x)/2, 4, x + 10, m_size.y - 6  );
    m_pInfoTextCtl->SetLabel( m_string );

    if(m_bGauge){
        if(m_pGauge)
            delete m_pGauge;
        m_pGauge = new wxGauge(this, -1, 10, wxPoint(10, 20), wxSize(m_size.x - 20, 20),  wxGA_HORIZONTAL | wxGA_SMOOTH);
    }

    SetSize( m_position.x, m_position.y, m_size.x, m_size.y );

    Show();
}

void InfoWinDialog::SetString(const wxString &s)
{
    m_string = s;

    wxSize size;

    size.x = (GetCharWidth() * m_string.Len()) + 20;
    size.y = GetCharHeight()+10;

    if(m_bGauge)
        size.y += 30;

    SetWinSize( size );

}




BEGIN_EVENT_TABLE(InfoWin, wxWindow)
EVT_PAINT ( InfoWin::OnPaint )
EVT_ERASE_BACKGROUND(InfoWin::OnEraseBackground)
EVT_TIMER(-1, InfoWin::OnTimer)
END_EVENT_TABLE()


// Define a constructor
InfoWin::InfoWin( wxWindow *parent, const wxString&s, bool show_gauge ) :
wxWindow( parent, wxID_ANY, wxDefaultPosition, wxDefaultSize )
{
    int ststyle = wxALIGN_LEFT | wxST_NO_AUTORESIZE;
    m_pInfoTextCtl = new wxStaticText( this, -1, _T ( "" ), wxDefaultPosition, wxDefaultSize,
                                       ststyle );


    m_pGauge = NULL;
    m_bGauge = show_gauge;
    SetString( s );

    if(m_bGauge) {
        m_timer.SetOwner( this, -1 );
        m_timer.Start( 100 );
    }


    Hide();
}

InfoWin::~InfoWin()
{
    delete m_pInfoTextCtl;
}

void InfoWin::OnTimer(wxTimerEvent &evt)
{
    if(m_pGauge)
        m_pGauge->Pulse();

}


void InfoWin::OnEraseBackground( wxEraseEvent& event )
{
}

void InfoWin::OnPaint( wxPaintEvent& event )
{
    int width, height;
    GetClientSize( &width, &height );
    wxPaintDC dc( this );

    wxColour c;

    GetGlobalColor( _T ( "UIBCK" ), &c );
    dc.SetBrush( wxBrush( c ) );

    GetGlobalColor( _T ( "UITX1" ), &c );
    dc.SetPen( wxPen( c ) );

    dc.DrawRectangle( 0, 0, width-1, height-1 );
}

void InfoWin::Realize()
{
    wxColour c;

    GetGlobalColor( _T ( "UIBCK" ), &c );
    SetBackgroundColour( c );

    GetGlobalColor( _T ( "UIBCK" ), &c );
    m_pInfoTextCtl->SetBackgroundColour( c );

    GetGlobalColor( _T ( "UITX1" ), &c );
    m_pInfoTextCtl->SetForegroundColour( c );

    int x;
    GetTextExtent(m_string, &x, NULL);

    m_pInfoTextCtl->SetSize( (m_size.x - x)/2, 4, x + 10, m_size.y - 6 );
    m_pInfoTextCtl->SetLabel( m_string );

    if(m_bGauge){
        if(m_pGauge)
            delete m_pGauge;
        m_pGauge = new wxGauge(this, -1, 10, wxPoint(10, 20), wxSize(m_size.x - 20, 20),  wxGA_HORIZONTAL | wxGA_SMOOTH);
    }

    SetSize( m_position.x, m_position.y, m_size.x, m_size.y );

    Show();
}

void InfoWin::SetString(const wxString &s)
{
    m_string = s;

    wxSize size;

    size.x = (GetCharWidth() * m_string.Len()) + 20;
    size.y = GetCharHeight()+10;

    if(m_bGauge)
        size.y += 30;

    SetWinSize( size );

}






//      On Screen log container

S63ScreenLogContainer::S63ScreenLogContainer( wxWindow *parent )
{
    Create( parent, -1, _T("S63_pi Log"), wxDefaultPosition, wxSize(500,400) );
    m_slog = new S63ScreenLog( this );

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
    SetSizer( itemBoxSizer2 );

    itemBoxSizer2->Add( m_slog, 1, wxEXPAND, 5 );

    Hide();
}

S63ScreenLogContainer::~S63ScreenLogContainer()
{
    if( m_slog  )
        m_slog->Destroy();
}

void S63ScreenLogContainer::LogMessage(wxString &s)
{
    if( m_slog  ) {
        m_slog->LogMessage( s );
        Show();
    }
}

void S63ScreenLogContainer::ClearLog(void)
{
    if( m_slog  ) {
        m_slog->ClearLog();
    }
}



#define SERVER_ID       5000
#define SOCKET_ID       5001

BEGIN_EVENT_TABLE(S63ScreenLog, wxWindow)
EVT_SIZE(S63ScreenLog::OnSize)
EVT_SOCKET(SERVER_ID,  S63ScreenLog::OnServerEvent)
EVT_SOCKET(SOCKET_ID,  S63ScreenLog::OnSocketEvent)
END_EVENT_TABLE()

S63ScreenLog::S63ScreenLog(wxWindow *parent):
    wxWindow( parent, -1, wxDefaultPosition, wxDefaultSize)
{

//    Create(parent, -1, _T("S63_pi Log"), wxDefaultPosition, wxDefaultSize,
//                           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER /*| wxDIALOG_NO_PARENT*/ );


    wxBoxSizer *LogSizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( LogSizer );

    m_plogtc = new wxTextCtrl(this, -1, _T(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE );
    LogSizer->Add(m_plogtc, 1, wxEXPAND, 0);


    m_nseq = 0;


    // Create a server socket to catch "back channel" messages from SENC utility

    // Create the address - defaults to localhost:0 initially
    wxIPV4address addr;
    addr.Service(g_backchannel_port);
    addr.AnyAddress();

    // Create the socket
    m_server = new wxSocketServer(addr);

    // We use Ok() here to see if the server is really listening
    if (! m_server->Ok())
    {
        m_plogtc->AppendText(_T("S63_pi backchannel could not listen at the specified port !\n"));
    }
    else
    {
        m_plogtc->AppendText(_T("S63_pi backchannel server listening.\n\n"));
    }

    // Setup the event handler and subscribe to connection events
    m_server->SetEventHandler(*this, SERVER_ID);
    m_server->SetNotify(wxSOCKET_CONNECTION_FLAG);
    m_server->Notify(true);


}

S63ScreenLog::~S63ScreenLog()
{
    if(this == g_pPanelScreenLog)
        g_pPanelScreenLog = NULL;
    else if( g_pScreenLog && (this == g_pScreenLog->m_slog) )
        g_pScreenLog = NULL;

    if( !g_pPanelScreenLog && !g_pScreenLog ){
        if(!g_buser_enable_screenlog)
            g_benable_screenlog = false;
    }

    g_backchannel_port++;

    delete m_plogtc;
    if(m_server) {
        m_server->Notify(false);
        delete m_server;
//        m_server->Destroy();
    }
}

void S63ScreenLog::OnSize( wxSizeEvent& event)
{
    Layout();
}

void S63ScreenLog::LogMessage(wxString &s)
{
    if( m_plogtc  ) {
        wxString seq;
        seq.Printf(_T("%6d: "), m_nseq++);

        wxString sp = s;

        if(sp[0] == '\r'){
            int lp = m_plogtc->GetInsertionPoint();
            int nol = m_plogtc->GetNumberOfLines();
            int ll = m_plogtc->GetLineLength(nol-1);

            if(ll)
                m_plogtc->Remove(lp-ll, lp);
            m_plogtc->SetInsertionPoint(lp - ll );
            m_plogtc->WriteText(s.Mid(1));
            m_plogtc->SetInsertionPointEnd();

        }
        else {
            m_plogtc->AppendText(seq + sp);
//            m_plogtc->AppendText(sp);
        }

        Show();

        if(gb_global_log)
            g_logarray.Add(seq + sp);

    }
}

void S63ScreenLog::ClearLog(void)
{
    if(m_plogtc){
        m_plogtc->Clear();
    }
}

void S63ScreenLog::OnServerEvent(wxSocketEvent& event)
{
    wxString s; // = _T("OnServerEvent: ");
    wxSocketBase *sock;

    switch(event.GetSocketEvent())
    {
        case wxSOCKET_CONNECTION :
//            s.Append(_T("wxSOCKET_CONNECTION\n"));
            break;
        default                  :
            s.Append(_T("Unexpected event !\n"));
            break;
    }

    m_plogtc->AppendText(s);

    // Accept new connection if there is one in the pending
    // connections queue, else exit. We use Accept(false) for
    // non-blocking accept (although if we got here, there
    // should ALWAYS be a pending connection).

    sock = m_server->Accept(false);

    if (sock)
    {
//       m_plogtc->AppendText(_T("New client connection accepted\n\n"));
    }
    else
    {
        m_plogtc->AppendText(_T("Error: couldn't accept a new connection\n\n"));
        return;
    }

    sock->SetEventHandler(*this, SOCKET_ID);
    sock->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
    sock->Notify(true);
    sock->SetFlags(wxSOCKET_BLOCK);


}

void S63ScreenLog::OnSocketEvent(wxSocketEvent& event)
{
    wxString s; // = _T("OnSocketEvent: ");
    wxSocketBase *sock = event.GetSocket();

    // First, print a message
    switch(event.GetSocketEvent())
    {
        case wxSOCKET_INPUT :
//            s.Append(_T("wxSOCKET_INPUT\n"));
            break;
        case wxSOCKET_LOST  :
//            s.Append(_T("wxSOCKET_LOST\n"));
            break;
        default             :
            s.Append(_T("Unexpected event !\n"));
            break;
    }

    m_plogtc->AppendText(s);

    // Now we process the event
    switch(event.GetSocketEvent())
    {
        case wxSOCKET_INPUT:
        {
            // We disable input events, so that the test doesn't trigger
            // wxSocketEvent again.
            sock->SetNotify(wxSOCKET_LOST_FLAG);

            char buf[160];

            sock->ReadMsg( buf, sizeof(buf) );
            size_t rlen = sock->LastCount();
            if(rlen < sizeof(buf))
                buf[rlen] = '\0';
            else
                buf[0] = '\0';

            if(rlen) {
                wxString msg(buf, wxConvUTF8);
//                 if(!g_bsuppress_log)
//                     LogMessage(msg);
            }

            // Enable input events again.
            sock->SetNotify(wxSOCKET_LOST_FLAG | wxSOCKET_INPUT_FLAG);
            break;
        }
                case wxSOCKET_LOST:
                {

                    // Destroy() should be used instead of delete wherever possible,
                    // due to the fact that wxSocket uses 'delayed events' (see the
                    // documentation for wxPostEvent) and we don't want an event to
                    // arrive to the event handler (the frame, here) after the socket
                    // has been deleted. Also, we might be doing some other thing with
                    // the socket at the same time; for example, we might be in the
                    // middle of a test or something. Destroy() takes care of all
                    // this for us.

//                    m_plogtc->AppendText(_T("Deleting socket.\n\n"));

                    sock->Destroy();
                    break;
                }
                default: ;
    }

}



/*!
 * SENCGetUserKeyDialog type definition
 */

IMPLEMENT_DYNAMIC_CLASS( SENCGetUserKeyDialog, wxDialog )
/*!
 * SENCGetUserKeyDialog event table definition
 */BEGIN_EVENT_TABLE( SENCGetUserKeyDialog, wxDialog )

 ////@begin SENCGetUserKeyDialog event table entries

 EVT_BUTTON( ID_GETIP_CANCEL, SENCGetUserKeyDialog::OnCancelClick )
 EVT_BUTTON( ID_GETIP_OK, SENCGetUserKeyDialog::OnOkClick )

 ////@end SENCGetUserKeyDialog event table entries

 END_EVENT_TABLE()

 /*!
  * SENCGetUserKeyDialog constructors
  */

 SENCGetUserKeyDialog::SENCGetUserKeyDialog()
 {
 }

 SENCGetUserKeyDialog::SENCGetUserKeyDialog( int legendID, wxWindow* parent, wxWindowID id, const wxString& caption,
                                         const wxPoint& pos, const wxSize& size, long style )
 {

     long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
     wxDialog::Create( parent, id, caption, pos, size, wstyle );

     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     CreateControls(legendID);
     GetSizer()->SetSizeHints( this );
     Centre();

 }

 SENCGetUserKeyDialog::~SENCGetUserKeyDialog()
 {
     delete m_UserKeyCtl;
 }

 /*!
  * SENCGetUserKeyDialog creator
  */

 bool SENCGetUserKeyDialog::Create( int legendID, wxWindow* parent, wxWindowID id, const wxString& caption,
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
     
     SetTitle( _("OpenCPN oeSENC UserKey Required"));

     CreateControls( legendID );
     Centre();
     return TRUE;
 }

 /*!
  * Control creation for SENCGetUserKeyDialog
  */

 void SENCGetUserKeyDialog::CreateControls( int legendID )
 {
     SENCGetUserKeyDialog* itemDialog1 = this;

     wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
     itemDialog1->SetSizer( itemBoxSizer2 );

     wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( itemDialog1, wxID_ANY, _("Enter UserKey") );

     wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer( itemStaticBoxSizer4Static, wxVERTICAL );
     itemBoxSizer2->Add( itemStaticBoxSizer4, 0, wxEXPAND | wxALL, 5 );

     wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
     itemStaticBoxSizer4->Add( itemStaticText5, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP, 5 );

     m_UserKeyCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition,
     wxSize( 180, -1 ), 0 );
     itemStaticBoxSizer4->Add( m_UserKeyCtl, 0,  wxALIGN_LEFT | wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5 );

     wxStaticText *itemStaticTextLegend = NULL;
     switch(legendID){
         case LEGEND_NONE:
             break;
             
         case LEGEND_FIRST:
             itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,
_("A valid oeSENC UserKey has the alphanumeric format:  AAAA-BBBB-CCCC-DDDD-EEEE-FF\n\n\
Your oeSENC UserKey may be obtained from your chart provider."),
                                                      wxDefaultPosition, wxDefaultSize, 0);
             break;
         case LEGEND_SECOND:
             itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,
_("ERROR: The UserKey entered is not valid for this oeSENC chart set.\n\
Please verify your UserKey and try again.\n\n\
A valid oeSENC UserKey has the alphanumeric format:  AAAA-BBBB-CCCC-DDDD-EEEE-FF\n\
Your oeSENC UserKey may be obtained from your chart provider.\n\n"),
                                                      wxDefaultPosition, wxDefaultSize, 0);
             break;
         case LEGEND_THIRD:
             itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,
_("ERROR: The UserKey entered is not valid for this oeSENC chart set.\n\n\
oeSENC charts will be disabled for this session.\n\
Please verify your UserKey and restart OpenCPN.\n\n\
Your oeSENC UserKey may be obtained from your chart provider.\n\n"),
                                                    wxDefaultPosition, wxDefaultSize, 0);
             
             m_UserKeyCtl->Disable();
             break;
             
         case LEGEND_FOURTH:
             itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,
                                                      _("UserKey accepted.\n\n"),
                                                      wxDefaultPosition, wxDefaultSize, 0);
             break;

         default:
             break;
     }
                                       
     if(itemStaticTextLegend){
         itemBoxSizer2->Add( itemStaticTextLegend, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP, 5 );
     }
         
                                       
#if 0     
     wxBoxSizer* itemBoxSizerTest = new wxBoxSizer( wxVERTICAL );
     itemBoxSizer2->Add( itemBoxSizerTest, 0, wxALIGN_LEFT | wxALL | wxEXPAND, 5 );

     m_testBtn = new wxButton(itemDialog1, ID_GETIP_TEST, _T("Test Installpermit"));
     m_testBtn->Disable();
     itemBoxSizerTest->Add( m_testBtn, 0, wxALIGN_LEFT | wxALL, 5 );

     wxStaticBox* itemStaticBoxTestResults = new wxStaticBox( itemDialog1, wxID_ANY,
                                                                  _T("Test Results"), wxDefaultPosition, wxSize(-1, 40) );

     wxStaticBoxSizer* itemStaticBoxSizerTest = new wxStaticBoxSizer( itemStaticBoxTestResults,  wxHORIZONTAL );
     itemBoxSizerTest->Add( itemStaticBoxSizerTest, 0,  wxALIGN_RIGHT |wxALL | wxEXPAND, 5 );


     m_TestResult = new wxStaticText( itemDialog1, -1, _T(""), wxDefaultPosition, wxSize( -1, -1 ), 0 );

     itemStaticBoxSizerTest->Add( m_TestResult, 0, wxALIGN_LEFT | wxALL | wxEXPAND, 5 );

#endif
     wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
     itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );

     if( (legendID == LEGEND_FIRST) || (legendID == LEGEND_SECOND) ){
        m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
        itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
        m_CancelButton->SetDefault();
     }
     else
         m_CancelButton = NULL;

     m_OKButton = new wxButton( itemDialog1, ID_GETIP_OK, _("OK"), wxDefaultPosition,
     wxDefaultSize, 0 );
     itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

     m_UserKeyCtl->AppendText(g_old_UserKey);
     
 }


 bool SENCGetUserKeyDialog::ShowToolTips()
 {
     return TRUE;
 }


 void SENCGetUserKeyDialog::OnCancelClick( wxCommandEvent& event )
 {
    EndModal(2);
 }

 void SENCGetUserKeyDialog::OnOkClick( wxCommandEvent& event )
 {
     if( m_UserKeyCtl->GetValue().Length() == 0 )
         EndModal(1);
     else {
         g_UserKey = m_UserKeyCtl->GetValue();
         g_pi->SaveConfig();

         EndModal(0);
     }
 }

 wxString GetUserKey( int legendID, bool bforceNew)
 {
     if(g_UserKey.Len() && !bforceNew)
         return g_UserKey;
     else
     {
         g_old_UserKey = g_UserKey;
         SENCGetUserKeyDialog dlg( legendID, GetOCPNCanvasWindow());
         
         wxSize dialogSize(500, -1);
         
#ifdef __OCPN__ANDROID__
         wxSize ss = ::wxGetDisplaySize();
         dialogSize.x = ss.x * 8 / 10;
#endif         
         dlg.SetSize(dialogSize);
         dlg.Centre();
         
         if(pinfoDlg)
             pinfoDlg->Hide();

#ifdef __OCPN__ANDROID__
         androidHideBusyIcon();
#endif             
         int ret = dlg.ShowModal();
         if(ret == 0)
             return g_UserKey;
         else
             return _T("Invalid");
     }
 }
 
void ShowGenericErrorMessage(wxString s_file)
{
    if(g_GenericMessageShown)
        return;
        
    wxString msg =
_("This chart cannot be loaded due to any of the following reasons:\n\n\
- You have made important hardware changes on your computer.\n\
- Your OS has been updated and your license has been suspended.\n\
- This chart set was prepared for another system.\n\
- This chart set was prepared for a USB key dongle, but dongle is not detected.\n\
- There are corrupted files due to errors during download or unzip.\n\n\
Please contact info@o-charts.org if the problem persists.\n");
    msg << _T("\n") << s_file; //Show one example file
    
    OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"),  wxOK, -1, -1);
    
    g_GenericMessageShown = true;
}

 
bool validateUserKey( wxString sencFileName)
{
    if(g_debugLevel)printf("\n-----------validateUserKey\n");
    
    wxLogMessage(_T("validateUserKey"));
    
    wxLogMessage(_T("Current userKey: ") + g_UserKey);
    
    if(g_bDeclaredInvalid)
        return false;
    
    bool b_Set = true;
    if((g_UserKey.Length() == 0) || (g_UserKey == _T("Invalid"))){
        b_Set = false;
        g_UserKey = GetUserKey( LEGEND_FIRST, true );
    }
        
    // Try to read the header of the supplied oeSENC file name
    Osenc senc;
    senc.setKey(g_UserKey);

    int retCode = senc.ingestHeader( sencFileName );

    if(retCode != SENC_NO_ERROR){
       //wxASSERT( 0 );
       wxLogMessage(_T("validateUserKey E1"));
        
    
        if(( ERROR_SIGNATURE_FAILURE == retCode )  || ( ERROR_SENC_CORRUPT == retCode ) ){
            wxLogMessage(_T("validateUserKey E1.5"));
            
            // We try once, quietly
            int retCode_retry0 = senc.ingestHeader( sencFileName );
            if(retCode_retry0 == SENC_NO_ERROR){
                wxLogMessage(_T("OK after quiet retry."));
                return true;
            }
                
            wxLogMessage(_T("validateUserKey E2, reset server"));
            
            validate_SENC_server();             // reset the server
 
            int retCode_retry1 = senc.ingestHeader( sencFileName );
            if(retCode_retry1 == SENC_NO_ERROR){
                wxLogMessage(_T("OK after server reset."));
                return true;
            }
            
            wxLogMessage(_T("validateUserKey E2.5, extracting userKey from Chartinfo.txt"));

            //  On a hard signature error, we try to extract a userKey from the chartinfo file, if present
            
               // get the Chartinfo as a wxTextFile
            wxFileName fn(sencFileName);
            wxString infoFile = fn.GetPath(  wxPATH_GET_VOLUME + wxPATH_GET_SEPARATOR );
            infoFile += _T("Chartinfo.txt");
            wxString new_userKey;
            
            if(wxFileExists(infoFile)){
                wxTextFile info_file( infoFile );
                if( info_file.Open() ){
                    wxString line = info_file.GetFirstLine();
        
                    while( !info_file.Eof() ){
                        if(line.StartsWith( _T("UserKey:" ) ) ) {
                            wxString content = line.AfterFirst(':').Trim().Trim(false);
                            new_userKey = content;
                                                 
                            break;
                        }
            
                        line = info_file.GetNextLine();
                    }
                }
            }
            
            wxLogMessage(_T("userKey from Chartinfo.txt: ") + new_userKey);
            
            if(new_userKey.Len() && (!new_userKey.IsSameAs(g_UserKey))){
                wxLogMessage(_T("Switching userKey to: ") + new_userKey);
                g_UserKey = new_userKey;
            }
                
            
            validate_SENC_server();             // reset the server
 
            senc.setKey(g_UserKey);             // key from the chartinfo file
            int retCode_retry21 = senc.ingestHeader( sencFileName );
            if(retCode_retry21 == SENC_NO_ERROR){
                wxLogMessage(_T("OK using ChartInfo userKey."));
                return true;
            }

            wxLogMessage(_T("validateUserKey E2.6"));

            ShowGenericErrorMessage(sencFileName);
            return false;
#if 0            

            //  No other choice here but to ask the user to enter a new key
            wxString key = GetUserKey( LEGEND_SECOND, true );
            
            if(key.Upper() == _T("INVALID")){
                GetUserKey( LEGEND_THIRD, true );                  // Bail out on cancel
                g_bDeclaredInvalid = true;
                return false;
            }
            
            senc.setKey(key);
            int retCode_retry = senc.ingestHeader( sencFileName );
            
            if(retCode_retry != SENC_NO_ERROR){
                GetUserKey( LEGEND_THIRD, true );                  // Bail out
                g_bDeclaredInvalid = true;
                return false;
            }
            else{
                wxLogMessage(_T("validateUserKey E3"));
                
                if(!b_Set)
                    GetUserKey( LEGEND_FOURTH, true );                  // Inform the user
                g_UserKey = key;
            }
#endif
        }
    }
    else{
        wxLogMessage(_T("validateUserKey E4"));
        
//         if(!b_Set)
//             GetUserKey( LEGEND_FOURTH, true );                  // Inform the user
    }
    return true;
}



void LoadS57Config()
{
    if( !ps52plib )
        return;
    
    int read_int;
    double dval;
    
    g_pconfig->SetPath( _T ( "/Settings" ) );
    g_pconfig->Read( _T ( "DebugS57" ), &g_PIbDebugS57, 0 );         // Show LUP and Feature info in object query
    
    g_pconfig->SetPath( _T ( "/Settings/GlobalState" ) );
    
    g_pconfig->Read( _T ( "bShowS57Text" ), &read_int, 0 );
    ps52plib->SetShowS57Text( !( read_int == 0 ) );
    
    g_pconfig->Read( _T ( "bShowS57ImportantTextOnly" ), &read_int, 0 );
    ps52plib->SetShowS57ImportantTextOnly( !( read_int == 0 ) );
    
    g_pconfig->Read( _T ( "bShowLightDescription" ), &read_int, 0 );
    ps52plib->SetShowLdisText( !( read_int == 0 ) );
    
    g_pconfig->Read( _T ( "bExtendLightSectors" ), &read_int, 0 );
    ps52plib->SetExtendLightSectors( !( read_int == 0 ) );
    
    g_pconfig->Read( _T ( "nDisplayCategory" ), &read_int, (enum _DisCat) STANDARD );
    ps52plib->SetDisplayCategory((enum _DisCat) read_int );
    
    g_pconfig->Read( _T ( "nSymbolStyle" ), &read_int, (enum _LUPname) PAPER_CHART );
    ps52plib->m_nSymbolStyle = (LUPname) read_int;
    
    g_pconfig->Read( _T ( "nBoundaryStyle" ), &read_int, PLAIN_BOUNDARIES );
    ps52plib->m_nBoundaryStyle = (LUPname) read_int;
    
    g_pconfig->Read( _T ( "bShowSoundg" ), &read_int, 1 );
    ps52plib->m_bShowSoundg = !( read_int == 0 );
    
    g_pconfig->Read( _T ( "bShowMeta" ), &read_int, 0 );
    ps52plib->m_bShowMeta = !( read_int == 0 );
    
    g_pconfig->Read( _T ( "bUseSCAMIN" ), &read_int, 1 );
    ps52plib->m_bUseSCAMIN = !( read_int == 0 );
    
    g_pconfig->Read( _T ( "bShowAtonText" ), &read_int, 1 );
    ps52plib->m_bShowAtonText = !( read_int == 0 );
    
    g_pconfig->Read( _T ( "bDeClutterText" ), &read_int, 0 );
    ps52plib->m_bDeClutterText = !( read_int == 0 );
    
    g_pconfig->Read( _T ( "bShowNationalText" ), &read_int, 0 );
    ps52plib->m_bShowNationalTexts = !( read_int == 0 );
    
    if( g_pconfig->Read( _T ( "S52_MAR_SAFETY_CONTOUR" ), &dval, 5.0 ) ) {
        S52_setMarinerParam( S52_MAR_SAFETY_CONTOUR, dval );
        S52_setMarinerParam( S52_MAR_SAFETY_DEPTH, dval ); // Set safety_contour and safety_depth the same
    }
    
    if( g_pconfig->Read( _T ( "S52_MAR_SHALLOW_CONTOUR" ), &dval, 3.0 ) ) S52_setMarinerParam(
        S52_MAR_SHALLOW_CONTOUR, dval );
    
    if( g_pconfig->Read( _T ( "S52_MAR_DEEP_CONTOUR" ), &dval, 10.0 ) ) S52_setMarinerParam(
        S52_MAR_DEEP_CONTOUR, dval );
    
    if( g_pconfig->Read( _T ( "S52_MAR_TWO_SHADES" ), &dval, 0.0 ) ) S52_setMarinerParam(
        S52_MAR_TWO_SHADES, dval );
    
    ps52plib->UpdateMarinerParams();
    
    g_pconfig->SetPath( _T ( "/Settings/GlobalState" ) );
    g_pconfig->Read( _T ( "S52_DEPTH_UNIT_SHOW" ), &read_int, 1 );   // default is metres
    read_int = wxMax(read_int, 0);                      // qualify value
    read_int = wxMin(read_int, 2);
    ps52plib->m_nDepthUnitDisplay = read_int;
    
    //    S57 Object Class Visibility
    
    OBJLElement *pOLE;
    
    g_pconfig->SetPath( _T ( "/Settings/ObjectFilter" ) );
    
    int iOBJMax = g_pconfig->GetNumberOfEntries();
    if( iOBJMax ) {
        
        wxString str;
        long val;
        long dummy;
        
        wxString sObj;
        
        bool bCont = g_pconfig->GetFirstEntry( str, dummy );
        while( bCont ) {
            g_pconfig->Read( str, &val );              // Get an Object Viz
            
            bool bNeedNew = true;
            
            if( str.StartsWith( _T ( "viz" ), &sObj ) ) {
                for( unsigned int iPtr = 0; iPtr < ps52plib->pOBJLArray->GetCount(); iPtr++ ) {
                    pOLE = (OBJLElement *) ( ps52plib->pOBJLArray->Item( iPtr ) );
                    if( !strncmp( pOLE->OBJLName, sObj.mb_str(), 6 ) ) {
                        pOLE->nViz = val;
                        bNeedNew = false;
                        break;
                    }
                }
                
                if( bNeedNew ) {
                    pOLE = (OBJLElement *) calloc( sizeof(OBJLElement), 1 );
                    strncpy( pOLE->OBJLName, sObj.mb_str(), 6 );
                    pOLE->nViz = 1;
                    
                    ps52plib->pOBJLArray->Add( (void *) pOLE );
                }
            }
            bCont = g_pconfig->GetNextEntry( str, dummy );
        }
    }
}



       
static GLboolean QueryExtension( const char *extName )
{
    /*
     * * Search for extName in the extensions string. Use of strstr()
     ** is not sufficient because extension names can be prefixes of
     ** other extension names. Could use strtok() but the constant
     ** string returned by glGetString might be in read-only memory.
     */
    char *p;
    char *end;
    int extNameLen;
    
    extNameLen = strlen( extName );
    
    p = (char *) glGetString( GL_EXTENSIONS );
    if( NULL == p ) {
        return GL_FALSE;
    }
    
    end = p + strlen( p );
    
    while( p < end ) {
        int n = strcspn( p, " " );
        if( ( extNameLen == n ) && ( strncmp( extName, p, n ) == 0 ) ) {
            return GL_TRUE;
        }
        p += ( n + 1 );
    }
    return GL_FALSE;
}

typedef void (*GenericFunction)(void);
//void (*glXGetProcAddress(const GLubyte *procname))( void );

#if defined(__WXMSW__)
#define systemGetProcAddress(ADDR) wglGetProcAddress(ADDR)
#elif defined(__WXOSX__)
#include <dlfcn.h>
#define systemGetProcAddress(ADDR) dlsym( RTLD_DEFAULT, ADDR)
#elif defined(__OCPN__ANDROID__)
#define systemGetProcAddress(ADDR) eglGetProcAddress(ADDR)
#else
#define systemGetProcAddress(ADDR) glXGetProcAddress((const GLubyte*)ADDR)
#endif

GenericFunction ocpnGetProcAddress(const char *addr, const char *extension)
{
    char addrbuf[256];
    if(!extension)
        return (GenericFunction)NULL;
    
#ifndef __OCPN__ANDROID__    
        //  If this is an extension entry point,
        //  We look explicitly in the extensions list to confirm
        //  that the request is actually supported.
        // This may be redundant, but is conservative, and only happens once per session.    
        if(extension && strlen(extension)){
            wxString s_extension(&addr[2], wxConvUTF8);
            wxString s_family;
            s_family = wxString(extension, wxConvUTF8);
            s_extension.Prepend(_T("_"));
            s_extension.Prepend(s_family);
            
            s_extension.Prepend(_T("GL_"));
            
            if(!QueryExtension( s_extension.mb_str() )){
                return (GenericFunction)NULL;
            }
        }
#endif    
        
        snprintf(addrbuf, sizeof addrbuf, "%s%s", addr, extension);
        return (GenericFunction)systemGetProcAddress(addrbuf);
        
}

bool  b_glEntryPointsSet;

static void GetglEntryPoints( void )
{
    b_glEntryPointsSet = true;
    
    // the following are all part of framebuffer object,
    // according to opengl spec, we cannot mix EXT and ARB extensions
    // (I don't know that it could ever happen, but if it did, bad things would happen)
    
#ifndef __OCPN__ANDROID__
    const char *extensions[] = {"", "ARB", "EXT", 0 };
#else
    const char *extensions[] = {"", "OES", 0 };
#endif
    
    unsigned int n_ext = (sizeof extensions) / (sizeof *extensions);
    
    unsigned int i;
    for(i=0; i<n_ext; i++) {
        if((s_glGenBuffers = (PFNGLGENBUFFERSPROC)
            ocpnGetProcAddress( "glGenBuffers", extensions[i])))
            break;
    }
    
    if(i<n_ext){
#if 0
        s_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSEXTPROC)
        ocpnGetProcAddress( "glGenRenderbuffers", extensions[i]);
        s_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)
        ocpnGetProcAddress( "glFramebufferTexture2D", extensions[i]);
        s_glBindFramebuffer = (PFNGLBINDFRAMEBUFFEREXTPROC)
        ocpnGetProcAddress( "glBindFramebuffer", extensions[i]);
        s_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)
        ocpnGetProcAddress( "glFramebufferRenderbuffer", extensions[i]);
        s_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEEXTPROC)
        ocpnGetProcAddress( "glRenderbufferStorage", extensions[i]);
        s_glBindRenderbuffer = (PFNGLBINDRENDERBUFFEREXTPROC)
        ocpnGetProcAddress( "glBindRenderbuffer", extensions[i]);
        s_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)
        ocpnGetProcAddress( "glCheckFramebufferStatus", extensions[i]);
        s_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSEXTPROC)
        ocpnGetProcAddress( "glDeleteFramebuffers", extensions[i]);
        s_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSEXTPROC)
        ocpnGetProcAddress( "glDeleteRenderbuffers", extensions[i]);
        s_glGenerateMipmap = (PFNGLGENERATEMIPMAPEXTPROC)
        ocpnGetProcAddress( "glGenerateMipmap", extensions[i]);
#endif        
        //VBO
        s_glGenBuffers = (PFNGLGENBUFFERSPROC)
        ocpnGetProcAddress( "glGenBuffers", extensions[i]);
        s_glBindBuffer = (PFNGLBINDBUFFERPROC)
        ocpnGetProcAddress( "glBindBuffer", extensions[i]);
        s_glBufferData = (PFNGLBUFFERDATAPROC)
        ocpnGetProcAddress( "glBufferData", extensions[i]);
        s_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)
        ocpnGetProcAddress( "glDeleteBuffers", extensions[i]);
        
//         s_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIV)
//         ocpnGetProcAddress( "glGetBufferParameteriv", extensions[i]);
        
    }
    
    //  Retry VBO entry points with all extensions
    if(0 == s_glGenBuffers){
        for( i=0; i<n_ext; i++) {
            if((s_glGenBuffers = (PFNGLGENBUFFERSPROC)ocpnGetProcAddress( "glGenBuffers", extensions[i])) )
                break;
        }
        
        if( i < n_ext ){
            s_glBindBuffer = (PFNGLBINDBUFFERPROC) ocpnGetProcAddress( "glBindBuffer", extensions[i]);
            s_glBufferData = (PFNGLBUFFERDATAPROC) ocpnGetProcAddress( "glBufferData", extensions[i]);
            s_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC) ocpnGetProcAddress( "glDeleteBuffers", extensions[i]);
        }
    }
    
 
#if 0 
#ifndef __OCPN__ANDROID__            
    for(i=0; i<n_ext; i++) {
        if((s_glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)
            ocpnGetProcAddress( "glCompressedTexImage2D", extensions[i])))
            break;
    }
    
    if(i<n_ext){
        s_glGetCompressedTexImage = (PFNGLGETCOMPRESSEDTEXIMAGEPROC)
        ocpnGetProcAddress( "glGetCompressedTexImage", extensions[i]);
    }
#else    
    s_glCompressedTexImage2D =          glCompressedTexImage2D;
#endif
#endif
    
}

void init_S52Library(void)
{
    // General variables
    g_overzoom_emphasis_base = 0;
    g_oz_vector_scale = false;
    g_ChartScaleFactorExp = GetOCPNChartScaleFactor_Plugin();
    
    //  Class Registrar Manager
    
    if( pi_poRegistrarMgr == NULL ) {
        wxString csv_dir = *GetpSharedDataLocation();
        csv_dir += _T("s57data");
        
        pi_poRegistrarMgr = new s57RegistrarMgr( csv_dir, NULL );
    }

    g_csv_locn = *GetpSharedDataLocation();
    g_csv_locn += _T("s57data");
    
    //  S52 Plib
    if(ps52plib) // already loaded?
        return;

    wxString plib_data = *GetpSharedDataLocation();
    plib_data += _T("s57data/"); //TODO use sep
    
    ps52plib = new s52plib( plib_data, false );


    if( ps52plib->m_bOK ) {
        
        // Load up any S52 PLIB patch files found
        wxString dataLocn =*GetpSharedDataLocation() +
        _T("plugins") + wxFileName::GetPathSeparator() +
        _T("oesenc_pi") + wxFileName::GetPathSeparator() +
        _T("data");
        
        wxArrayString patchFiles;
        wxDir::GetAllFiles(dataLocn, &patchFiles, _T("*.xml"));
        for(unsigned int i=0 ; i < patchFiles.GetCount() ; i++){

            g_oeChartSymbols->PatchConfigFile( ps52plib, patchFiles.Item(i));
        }

            //    Preset some object class visibilites for "Mariner's Standard" display category
            //  They may be overridden in LoadS57Config
        for( unsigned int iPtr = 0; iPtr < ps52plib->pOBJLArray->GetCount(); iPtr++ ) {
            OBJLElement *pOLE = (OBJLElement *) ( ps52plib->pOBJLArray->Item( iPtr ) );
            if( !strncmp( pOLE->OBJLName, "DEPARE", 6 ) ) pOLE->nViz = 1;
            if( !strncmp( pOLE->OBJLName, "LNDARE", 6 ) ) pOLE->nViz = 1;
            if( !strncmp( pOLE->OBJLName, "COALNE", 6 ) ) pOLE->nViz = 1;
        }

        LoadS57Config();
        ps52plib->m_myConfig = PI_GetPLIBStateHash();
        
        ps52plib->SetPLIBColorScheme( GLOBAL_COLOR_SCHEME_RGB );
        
        wxWindow *cc1 = GetOCPNCanvasWindow();
        if(cc1){
            
            if(!g_display_size_mm)
                g_display_size_mm = wxGetDisplaySizeMM().GetWidth();
            
            int display_size_mm = wxMax(g_display_size_mm, 200);
            
            int sx, sy;
            wxDisplaySize( &sx, &sy );
            double max_physical = wxMax(sx, sy);
            
            double pix_per_mm = ( max_physical ) / ( (double) display_size_mm );
            ps52plib->SetPPMM( pix_per_mm );
            
        }
    } else {
        wxLogMessage( _T("   S52PLIB Initialization failed, oesenc_pi disabling Vector charts.") );
        delete ps52plib;
        ps52plib = NULL;
    }
}


void init_GLLibrary(void)
{
   
    // OpenGL variables
    
    if(g_GLOptionsSet && !g_GLSetupOK){
        char *p = (char *) glGetString( GL_EXTENSIONS );
        if( NULL == p )
            pi_bopengl = false;
        else
            pi_bopengl = true;
        
    
        char *str = (char *) glGetString( GL_RENDERER );
        if (str == NULL)
            wxLogMessage(_T("oeSENC_pi failed to initialize OpenGL"));

        GetglEntryPoints();

        char render_string[80];
        wxString renderer;
        if(str){
            strncpy( render_string, str, 79 );
            renderer = wxString( render_string, wxConvUTF8 );
        }
        
        g_GLMinCartographicLineWidth = 1.0;
        g_GLMinSymbolLineWidth = 1.0;
        
        //  Set the minimum line width
        glGetError();       // Clear errors
            
        GLint parms[2];
        glGetIntegerv( GL_SMOOTH_LINE_WIDTH_RANGE, &parms[0] );
        if(glGetError())
            glGetIntegerv( GL_ALIASED_LINE_WIDTH_RANGE, &parms[0] );
        if(!glGetError()){
            g_GLMinSymbolLineWidth = wxMax(parms[0], 1);
            g_GLMinCartographicLineWidth = wxMax(parms[0], 1);
        }
        
        wxString lwmsg;
        lwmsg.Printf(_T("oeSENC_PI:  OpenGL-> Minimum cartographic line width: %4.1f"), g_GLMinCartographicLineWidth);
        wxLogMessage(lwmsg);
        
        //    Some GL renderers do a poor job of Anti-aliasing very narrow line widths.
        //    This is most evident on rendered symbols which have horizontal or vertical line segments
        //    Detect this case, and adjust the render parameters.
        
        if( renderer.Upper().Find( _T("MESA") ) != wxNOT_FOUND ){
            GLfloat parf;
            glGetFloatv(  GL_SMOOTH_LINE_WIDTH_GRANULARITY, &parf );
            
            g_GLMinSymbolLineWidth = wxMax(((float)parms[0] + parf), 1);
        }

        // Intel integrated GPU processors do not handle VBO in legacy direct mode very well.
        // Mainly, problems are seen with loss of mapping, and leakage of deleted buffers.
        // Performance seems not markedly better using VBO, anyway.
        // So, on MSW and MacOS platforms, Intel graphics, we override the core GL options and disable VBO

#if defined( __WXMSW__ ) || defined(__WXOSX__)
    if( renderer.Upper().Find( _T("INTEL") ) != wxNOT_FOUND )
        g_b_EnableVBO = false;
#endif
 
        //  Setup device dependent OpenGL options as communicated from core by JSON message
        ps52plib->SetGLOptions(g_b_useStencil, g_b_useStencilAP, g_b_useScissorTest, g_b_useFBO,  g_b_EnableVBO, g_oe_texture_rectangle_format);

        pi_bopengl = true;
        g_GLSetupOK = true;
    }
}


bool testSENCServer()
{
#ifdef __OCPN__ANDROID__    
    qDebug() << "Testing SENC server";
    
    //  The target binary executable
    wxString cmd = g_sencutil_bin;
    
    //  Set up the parameter passed as the local app storage directory
    wxString dataLoc = *GetpPrivateApplicationDataLocation();
    wxFileName fn(dataLoc);
    wxString dataDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        
    //  Set up the parameter passed to runtime environment as LD_LIBRARY_PATH
    // This will be {dir of g_sencutil_bin}/lib
    wxFileName fnl(cmd);
    wxString libDir = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("lib");
        
    wxLogMessage(_T("oesenc_pi: Starting: ") + cmd );
    
    wxString result = callActivityMethod_s4s("createProcSync", cmd, _T("-w"), dataDir, libDir);
    
    wxLogMessage(_T("oesenc_pi: Start Result: ") + result);
    
 
#endif    
    return true;
}

bool validate_SENC_server(void)
{
      
    
    if (g_debugLevel) {
        printf("\n-------validate_SENC_server\n");
        wxLogMessage(_T("validate_SENC_server"));
    }

    if(g_serverProc){
    // Check to see if the server is already running, and available
        //qDebug() << "Check running server Proc";
        Osenc_instream testAvail;
        if(testAvail.isAvailable(g_UserKey)){
            return true;
        }

        wxString tmsg;
        int nLoop = 1;
        while(nLoop < 10){
            tmsg.Printf(_T(" nLoop: %d"), nLoop);
            if(g_debugLevel)printf("      validate_SENC_server, retry: %d \n", nLoop);
            wxLogMessage(_T("Available FALSE, retry...") + tmsg);
            wxMilliSleep(500);
            Osenc_instream testAvailRetry;
            if(testAvailRetry.isAvailable(g_UserKey)){
                wxLogMessage(_T("Available TRUE"));
                return true;
            }
            nLoop++;
        }
    }
    
    // Not running, so start it up...

    wxString bin_test = g_sencutil_bin;
    
#ifndef __OCPN__ANDROID__    
    //Verify that oeserverd actually exists, and runs.
    wxLogMessage(_T("Validation Path to oeserverd is: ") + g_sencutil_bin);

    if(wxNOT_FOUND != g_sencutil_bin.Find('\"'))
        bin_test = g_sencutil_bin.Mid(1).RemoveLast();
    
    wxLogMessage(_T("Validation Path to server test is: ") + bin_test);

    wxString msg = _T("Checking oeserverd utility at ");
    msg += _T("{");
    msg += bin_test;
    msg += _T("}");
    wxLogMessage(_T("oesenc_pi: ") + msg);
    
    
    if(!::wxFileExists(bin_test)){
        wxString msg = _("Cannot find the oserverd utility at \n");
        msg += _T("{");
        msg += bin_test;
        msg += _T("}");
        OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"),  wxOK, -1, -1);
        
        wxString msge= _T("Cannot find the oeserverd utility at \n");
        msge += _T("{");
        msge += bin_test;
        msge += _T("}");

        wxLogMessage(_T("oesenc_pi: ") + msge);
        
        //g_sencutil_bin.Clear();
        return false;
    }

    // now start the server...
    wxString cmds = g_sencutil_bin;


    wxString pipeParm;
    
    int flags = wxEXEC_ASYNC;
#ifdef __WXMSW__    
    flags |= wxEXEC_HIDE_CONSOLE;
    long pid = ::wxGetProcessId();
    pipeParm.Printf(_T("OCPN%04d"), pid % 10000);
    g_pipeParm = pipeParm;
#endif

    if(g_pipeParm.Length())
        cmds += _T(" -p ") + g_pipeParm;
    
    if(g_serverDebug)
        cmds += _T(" -d");
    
    wxLogMessage(_T("oesenc_pi: starting oeserverd utility: ") + cmds);
    g_serverProc = wxExecute(cmds, flags);              // exec asynchronously
    wxMilliSleep(1000);

    
#else           // Android
    qDebug() << "Starting SENC server";
    
    //  The target binary executable
    wxString cmd = g_sencutil_bin;
    
    //  Set up the parameter passed as the local app storage directory
    wxString dataLoc = *GetpPrivateApplicationDataLocation();
    wxFileName fn(dataLoc);
    wxString dataDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        
    //  Set up the parameter passed to runtime environment as LD_LIBRARY_PATH
    // This will be {dir of g_sencutil_bin}/lib
    wxFileName fnl(cmd);
    wxString libDir = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("lib");
    
//     wxLogMessage(_T("oesenc_pi: Starting for version: ") + cmd );
//     wxString vresult = callActivityMethod_s6s("createProcSync5stdout", cmd, "-a");
//     wxLogMessage(_T("  Version result: ") + vresult );
//     qDebug() << vresult.mb_str();
    
    wxLogMessage(_T("oesenc_pi: Starting: ") + cmd );
    
    wxString result;
    if(g_SDK_INT < 21){          // Earlier than Android 5
        result = callActivityMethod_s4s("createProc", cmd, "-q", dataDir, libDir);
    }
    else if(g_SDK_INT < 29){            // Strictly earlier than Android 10
        result = callActivityMethod_s4s("createProc", cmd, "-z", g_UUID, libDir);
    }
    else
        result = callActivityMethod_s4s("createProc", cmd, "-y", g_WVID, libDir);
        
    wxLogMessage(_T("oesenc_pi: Start Result: ") + result);
    
    long pid;
    if(result.ToLong(&pid))
        g_serverProc = pid;
    
    wxMilliSleep(1000);
    
#endif    
    
    // Check to see if the server function is available
    if(g_serverProc){
        bool bAvail = false;
        int nLoop = 3;
        
        while(nLoop){
            Osenc_instream testAvail_One;
            if(!testAvail_One.isAvailable(_T("?")))
                wxSleep(1);
            else{
                bAvail = true;
                break;
            }
            nLoop--;
        }
        
        if(!bAvail){
            wxString msg = _("oeserverd utility at \n");
            msg += _T("{");
            msg += bin_test;
            msg += _T("}\n");
            msg += _(" reports Unavailable.\n\n");
            //            OCPNMessageBox_PlugIn(NULL, msg, _("oesenc_pi Message"),  wxOK, -1, -1);
            wxLogMessage(_T("oesenc_pi: ") + msg);
            
            ///_sencutil_bin.Clear();
            return false;
            
        }
        else{
            wxString nc;
            nc.Printf(_T("LoopCount: %d"), nLoop);
            
            //  Get the decrypt type into the logfile
            Osenc_instream testAvail_type;
            testAvail_type.isAvailable( g_UserKey );
            
            wxLogMessage(_T("oesenc_pi: oeserverd Check OK...") + nc);
        }
    }
    else{
        wxString msg = _("oeserverd utility at \n");
        msg += _T("{");
        msg += bin_test;
        msg += _T("}\n");
        msg += _(" could not be started.\n\n");
        OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"),  wxOK, -1, -1);
        wxLogMessage(_T("oesenc_pi: ") + msg);
        
        g_sencutil_bin.Clear();
        return false;
    }
    
    return true;
}

bool shutdown_SENC_server( void )
{
    
    // Check to see if the server is already running, and available
    Osenc_instream testAvail;
    if(1){
        testAvail.Shutdown();
        return true;
    }
    else{
        return false;
    }
}

#ifdef __OCPN__ANDROID__
bool CheckPendingJNIException()
{
    JNIEnv* jenv;
    
    if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) 
        return true;
    
    if( (jenv)->ExceptionCheck() == JNI_TRUE ) {
        
        // Handle exception here.
        (jenv)->ExceptionDescribe(); // writes to logcat
        (jenv)->ExceptionClear();
        
        return false;           // There was a pending exception, but cleared OK
        // interesting discussion:  http://blog.httrack.com/blog/2013/08/23/catching-posix-signals-on-android/
    }
    
    return false;
    
}

wxString callActivityMethod_vs(const char *method)
{
    if(CheckPendingJNIException())
        return _T("NOK");
    
    JNIEnv* jenv;
    
    wxString return_string;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                                           "activity", "()Landroid/app/Activity;");
    if(CheckPendingJNIException())
        return _T("NOK");
    
    if ( !activity.isValid() ){
        //qDebug() << "Activity is not valid";
        return return_string;
    }
    
    //  Call the desired method
    QAndroidJniObject data = activity.callObjectMethod(method, "()Ljava/lang/String;");
    if(CheckPendingJNIException())
        return _T("NOK");
    
    jstring s = data.object<jstring>();
    //qDebug() << s;
    
    if(s){
        //  Need a Java environment to decode the resulting string
        if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) {
            //qDebug() << "GetEnv failed.";
        }
        else {
            const char *ret_string = (jenv)->GetStringUTFChars(s, NULL);
            return_string = wxString(ret_string, wxConvUTF8);
        }
    }
    
    return return_string;
}


wxString callActivityMethod_s2s(const char *method, wxString parm1, wxString parm2)
{
    wxLogMessage(_T("PI s2s: ") + parm1 + parm2);
    
    if(CheckPendingJNIException())
        return _T("NOK");
    JNIEnv* jenv;
    
    wxString return_string;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                                           "activity", "()Landroid/app/Activity;");
    if(CheckPendingJNIException())
        return _T("NOK");
    
    if ( !activity.isValid() ){
        return return_string;
    }
    
    //  Need a Java environment to decode the resulting string
    if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) {
        return _T("jenv Error");
    }
    
     wxCharBuffer p1b = parm1.ToUTF8();
     jstring p1 = (jenv)->NewStringUTF(p1b.data());
     
     wxCharBuffer p2b = parm2.ToUTF8();
     jstring p2 = (jenv)->NewStringUTF(p2b.data());

     if( (jenv)->GetStringLength( p2 )){
         const char *v_string = (jenv)->GetStringUTFChars(p2, NULL);
         wxString vw_string = wxString(v_string, wxConvUTF8);
         wxLogMessage(_T("PI s2s p2String: ") + vw_string);
     }
     
    QAndroidJniObject data = activity.callObjectMethod(method, "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", p1, p2);
    
    (jenv)->DeleteLocalRef(p1);
    (jenv)->DeleteLocalRef(p2);
    
    if(CheckPendingJNIException())
        return _T("NOK");
    
        
    jstring s = data.object<jstring>();
        
    if( (jenv)->GetStringLength( s )){
            const char *ret_string = (jenv)->GetStringUTFChars(s, NULL);
            return_string = wxString(ret_string, wxConvUTF8);
    }
        
    return return_string;
        
}

wxString callActivityMethod_s4s(const char *method, wxString parm1, wxString parm2, wxString parm3, wxString parm4)
{
    if(CheckPendingJNIException())
        return _T("NOK");
    JNIEnv* jenv;
    
    wxString return_string;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                                           "activity", "()Landroid/app/Activity;");
    if(CheckPendingJNIException())
        return _T("NOK");
    
    if ( !activity.isValid() ){
        return return_string;
    }
    
    //  Need a Java environment to decode the resulting string
    if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) {
        return _T("jenv Error");
    }
    
    wxCharBuffer p1b = parm1.ToUTF8();
    jstring p1 = (jenv)->NewStringUTF(p1b.data());
    
    wxCharBuffer p2b = parm2.ToUTF8();
    jstring p2 = (jenv)->NewStringUTF(p2b.data());
    
    wxCharBuffer p3b = parm3.ToUTF8();
    jstring p3 = (jenv)->NewStringUTF(p3b.data());
    
    wxCharBuffer p4b = parm4.ToUTF8();
    jstring p4 = (jenv)->NewStringUTF(p4b.data());
    
    QAndroidJniObject data = activity.callObjectMethod(method, "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                                                       p1, p2, p3, p4);
    (jenv)->DeleteLocalRef(p1);
    (jenv)->DeleteLocalRef(p2);
    (jenv)->DeleteLocalRef(p3);
    (jenv)->DeleteLocalRef(p4);
    
    if(CheckPendingJNIException())
        return _T("NOK");
    
    //qDebug() << "Back from method_s4s";
        
        jstring s = data.object<jstring>();
        
        if( (jenv)->GetStringLength( s )){
            const char *ret_string = (jenv)->GetStringUTFChars(s, NULL);
            return_string = wxString(ret_string, wxConvUTF8);
        }
        
    return return_string;
        
}

wxString callActivityMethod_s5s(const char *method, wxString parm1, wxString parm2, wxString parm3, wxString parm4, wxString parm5)
{
    if(CheckPendingJNIException())
        return _T("NOK");
    JNIEnv* jenv;
    
    wxString return_string;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                                           "activity", "()Landroid/app/Activity;");
    if(CheckPendingJNIException())
        return _T("NOK");
    
    if ( !activity.isValid() ){
        return return_string;
    }
    
    //  Need a Java environment to decode the resulting string
    if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) {
        return _T("jenv Error");
    }
    
    wxCharBuffer p1b = parm1.ToUTF8();
    jstring p1 = (jenv)->NewStringUTF(p1b.data());
    
    wxCharBuffer p2b = parm2.ToUTF8();
    jstring p2 = (jenv)->NewStringUTF(p2b.data());
    
    wxCharBuffer p3b = parm3.ToUTF8();
    jstring p3 = (jenv)->NewStringUTF(p3b.data());
    
    wxCharBuffer p4b = parm4.ToUTF8();
    jstring p4 = (jenv)->NewStringUTF(p4b.data());
    
    wxCharBuffer p5b = parm5.ToUTF8();
    jstring p5 = (jenv)->NewStringUTF(p5b.data());
    
    QAndroidJniObject data = activity.callObjectMethod(method, "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                                                       p1, p2, p3, p4, p5);
    (jenv)->DeleteLocalRef(p1);
    (jenv)->DeleteLocalRef(p2);
    (jenv)->DeleteLocalRef(p3);
    (jenv)->DeleteLocalRef(p4);
    (jenv)->DeleteLocalRef(p5);
    
    if(CheckPendingJNIException())
        return _T("NOK");
    
    jstring s = data.object<jstring>();
        
    if( (jenv)->GetStringLength( s )){
        const char *ret_string = (jenv)->GetStringUTFChars(s, NULL);
        return_string = wxString(ret_string, wxConvUTF8);
    }
        
    return return_string;
        
}

wxString callActivityMethod_s6s(const char *method, wxString parm1, wxString parm2, wxString parm3, wxString parm4, wxString parm5, wxString parm6)
{
    if(CheckPendingJNIException())
        return _T("NOK");
    JNIEnv* jenv;
    
    wxString return_string;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                                           "activity", "()Landroid/app/Activity;");
    if(CheckPendingJNIException())
        return _T("NOK");
    
    if ( !activity.isValid() ){
        return return_string;
    }
    
    //  Need a Java environment to decode the resulting string
    if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) {
        return _T("jenv Error");
    }
    
    wxCharBuffer p1b = parm1.ToUTF8();
    jstring p1 = (jenv)->NewStringUTF(p1b.data());
    
    wxCharBuffer p2b = parm2.ToUTF8();
    jstring p2 = (jenv)->NewStringUTF(p2b.data());
    
    wxCharBuffer p3b = parm3.ToUTF8();
    jstring p3 = (jenv)->NewStringUTF(p3b.data());
    
    wxCharBuffer p4b = parm4.ToUTF8();
    jstring p4 = (jenv)->NewStringUTF(p4b.data());
    
    wxCharBuffer p5b = parm5.ToUTF8();
    jstring p5 = (jenv)->NewStringUTF(p5b.data());

    wxCharBuffer p6b = parm6.ToUTF8();
    jstring p6 = (jenv)->NewStringUTF(p6b.data());
    
    QAndroidJniObject data = activity.callObjectMethod(method, "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                                                       p1, p2, p3, p4, p5, p6);
    (jenv)->DeleteLocalRef(p1);
    (jenv)->DeleteLocalRef(p2);
    (jenv)->DeleteLocalRef(p3);
    (jenv)->DeleteLocalRef(p4);
    (jenv)->DeleteLocalRef(p5);
    (jenv)->DeleteLocalRef(p6);
    
    if(CheckPendingJNIException())
        return _T("NOK");
    
    jstring s = data.object<jstring>();
    
    if( (jenv)->GetStringLength( s )){
        const char *ret_string = (jenv)->GetStringUTFChars(s, NULL);
        return_string = wxString(ret_string, wxConvUTF8);
    }
    
    return return_string;
    
}


wxString callActivityMethod_ss(const char *method, wxString parm)
{
    if(CheckPendingJNIException())
        return _T("NOK");
    JNIEnv* jenv;
    
    wxString return_string;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                                           "activity", "()Landroid/app/Activity;");
    if(CheckPendingJNIException())
        return _T("NOK");
    
    if ( !activity.isValid() ){
        return return_string;
    }
    
    //  Need a Java environment to decode the resulting string
    if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) {
        return _T("jenv Error");
    }
    
    jstring p = (jenv)->NewStringUTF(parm.c_str());
    
    
    QAndroidJniObject data = activity.callObjectMethod(method, "(Ljava/lang/String;)Ljava/lang/String;", p);
    
    (jenv)->DeleteLocalRef(p);
    
    if(CheckPendingJNIException())
        return _T("NOK");
    
//    qDebug() << "OK return";
    
//    return _T("OK");
    
    jstring s = data.object<jstring>();
        
    if( (jenv)->GetStringLength( s )){
        const char *ret_string = (jenv)->GetStringUTFChars(s, NULL);
        return_string = wxString(ret_string, wxConvUTF8);
    }
        
    return return_string;
}

#endif



BEGIN_EVENT_TABLE( oesencPrefsDialog, wxDialog )
EVT_BUTTON( wxID_OK, oesencPrefsDialog::OnPrefsOkClick )
END_EVENT_TABLE()

oesencPrefsDialog::oesencPrefsDialog( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style )
{
    wxDialog::Create( parent, id, title, pos, size, style );
    
#ifdef __OCPN__ANDROID__
    SetBackgroundColour(ANDROID_DIALOG_BACKGROUND_COLOR);
#endif    
    
        this->SetSizeHints( wxDefaultSize, wxDefaultSize );
    
        wxBoxSizer* bSizerTop = new wxBoxSizer( wxVERTICAL );
        
        wxPanel *content = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBG_STYLE_ERASE );
        bSizerTop->Add(content, 0, wxALL|wxEXPAND, WXC_FROM_DIP(10));
        
        wxBoxSizer* bSizer2 = new wxBoxSizer( wxVERTICAL );
        content->SetSizer(bSizer2);
        
        // Plugin Version
        wxString extVersion;
        extVersion.Printf(_T("%d.%d.%d.%d"), PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH, PLUGIN_VERSION_TWEAK);

        wxString versionText = _(" oeSENC Version: ") + extVersion;
        wxStaticText *versionTextBox = new wxStaticText(content, wxID_ANY, versionText);
        bSizer2->Add(versionTextBox, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 20 );
 
        //  Show EULA
        m_buttonShowEULA = new wxButton( content, wxID_ANY, _("Show EULA"), wxDefaultPosition, wxDefaultSize, 0 );
        bSizer2->AddSpacer( 10 );
        bSizer2->Add( m_buttonShowEULA, 0, wxALIGN_CENTER_HORIZONTAL, 50 );
        m_buttonShowEULA->Connect( wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(oesenc_pi_event_handler::OnShowEULA), NULL, g_event_handler );
        bSizer2->AddSpacer( 20 );

#ifndef __OCPN__ANDROID__        
        //  FPR File Permit
        wxStaticBoxSizer* sbSizerFPR= new wxStaticBoxSizer( new wxStaticBox( content, wxID_ANY, _("System Identification") ), wxHORIZONTAL );
        m_fpr_text = new wxStaticText(content, wxID_ANY, _T(" "));
        if(g_fpr_file.Len())
             m_fpr_text->SetLabel( wxFileName::FileName(g_fpr_file).GetFullName() );
        else
             m_fpr_text->SetLabel( _T("                  "));
         
        sbSizerFPR->Add(m_fpr_text, wxEXPAND);
        bSizer2->Add(sbSizerFPR, 0, wxEXPAND, 50 );

        m_buttonNewFPR = new wxButton( content, wxID_ANY, _("Create System Identifier file..."), wxDefaultPosition, wxDefaultSize, 0 );
        
        bSizer2->AddSpacer( 5 );
        bSizer2->Add( m_buttonNewFPR, 0, wxALIGN_CENTER_HORIZONTAL, 50 );
        
        m_buttonNewFPR->Connect( wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(oesenc_pi_event_handler::OnNewFPRClick), NULL, g_event_handler );

        m_buttonNewDFPR = new wxButton( content, wxID_ANY, _("Create USB key dongle System ID file..."), wxDefaultPosition, wxDefaultSize, 0 );
        
        bSizer2->AddSpacer( 5 );
        bSizer2->Add( m_buttonNewDFPR, 0, wxALIGN_CENTER_HORIZONTAL, 50 );
        
        m_buttonNewDFPR->Connect( wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(oesenc_pi_event_handler::OnNewDFPRClick), NULL, g_event_handler );
            
#ifdef __WXMAC__
        m_buttonShowFPR = new wxButton( content, wxID_ANY, _("Show In Finder"), wxDefaultPosition, wxDefaultSize, 0 );
#else
        m_buttonShowFPR = new wxButton( content, wxID_ANY, _("Show on disk"), wxDefaultPosition, wxDefaultSize, 0 );
#endif
        bSizer2->AddSpacer( 20 );
        bSizer2->Add( m_buttonShowFPR, 0, wxALIGN_CENTER_HORIZONTAL, 50 );

        m_buttonShowFPR->Enable( g_fpr_file != wxEmptyString );

        m_buttonShowFPR->Connect( wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(oesenc_pi_event_handler::OnShowFPRClick), NULL, g_event_handler );

#endif        
        // System Name
        if(g_systemName.Length()){
            wxString nameText = _T(" ") + _("System Name:") + _T(" ") + g_systemName;
            m_nameTextBox = new wxStaticText(content, wxID_ANY, nameText);
            bSizer2->AddSpacer( 20 );
            bSizer2->Add(m_nameTextBox, 1, wxTOP | wxBOTTOM | wxALIGN_CENTER_HORIZONTAL, 10 );
        }
        else
            bSizer2->AddSpacer( 10 );
 
#ifndef __OCPN__ANDROID__        
        m_buttonClearSystemName = new wxButton( content, wxID_ANY, _("Reset System Name"), wxDefaultPosition, wxDefaultSize, 0 );
        
        bSizer2->AddSpacer( 10 );
        bSizer2->Add( m_buttonClearSystemName, 0, wxALIGN_CENTER_HORIZONTAL, 50 );
        
        m_buttonClearSystemName->Connect( wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(oesenc_pi_event_handler::OnClearSystemName), NULL, g_event_handler );
        
        if(!g_systemName.Length())
            m_buttonClearSystemName->Disable();
#endif        
        m_buttonClearCreds = new wxButton( content, wxID_ANY, _("Reset o-charts credentials"), wxDefaultPosition, wxDefaultSize, 0 );
        
        bSizer2->AddSpacer( 10 );
        bSizer2->Add( m_buttonClearCreds, 0, wxALIGN_CENTER_HORIZONTAL, 50 );
        
        m_buttonClearCreds->Connect( wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(oesenc_pi_event_handler::OnClearCredentials), NULL, g_event_handler );
        
        
        m_sdbSizer1 = new wxStdDialogButtonSizer();
        m_sdbSizer1OK = new wxButton( content, wxID_OK );
        m_sdbSizer1->AddButton( m_sdbSizer1OK );
        m_sdbSizer1Cancel = new wxButton( content, wxID_CANCEL );
        m_sdbSizer1->AddButton( m_sdbSizer1Cancel );
        m_sdbSizer1->Realize();
        
        bSizer2->Add( m_sdbSizer1, 0, wxBOTTOM|wxEXPAND|wxTOP, 20 );
        
        
        this->SetSizer( bSizerTop );
        this->Layout();
        bSizerTop->Fit( this );
        
        this->Centre( wxBOTH );
}

oesencPrefsDialog::~oesencPrefsDialog()
{
}

void oesencPrefsDialog::OnPrefsOkClick(wxCommandEvent& event)
{
#if 0    
    m_trackedPointName = m_wpComboPort->GetValue();
    
    wxArrayString guidArray = GetWaypointGUIDArray();
    for(unsigned int i=0 ; i < guidArray.GetCount() ; i++){
        wxString name = getWaypointName( guidArray[i] );
        if(name.Length()){
            if(name.IsSameAs(m_trackedPointName)){
                m_trackedPointGUID = guidArray[i];
                break;
            }
        }
    }
#endif
    EndModal( wxID_OK );
 
}

#ifdef __OCPN__ANDROID__
void androidGetDeviceName()
{
    if(!g_deviceInfo.Length())
        g_deviceInfo = callActivityMethod_vs("getDeviceInfo");
    
    wxStringTokenizer tkz(g_deviceInfo, _T("\n"));
    while( tkz.HasMoreTokens() )
    {
        wxString s1 = tkz.GetNextToken();
        if(wxNOT_FOUND != s1.Find(_T("Device"))){
            int a = s1.Find(_T(":"));
            if(wxNOT_FOUND != a){
                wxString b = s1.Mid(a+1).Trim(true).Trim(false);
                g_systemName = b;
            }
        }
    }
    
}
#endif

bool IsDongleAvailable()
{
#ifndef __OCPN__ANDROID__    
    wxString cmd = g_sencutil_bin;
    cmd += _T(" -s ");                  // Available?

    wxArrayString ret_array, err_array;      
    wxExecute(cmd, ret_array, err_array );
 
    wxLogMessage(_T("IsDongleAvailable()::oeserverd execution results:"));
    for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
        wxString line = ret_array[i];
        wxLogMessage(line);
    }

    // Show error in log
    if( err_array.GetCount() ){
        wxLogMessage(_T("IsDongleAvailable()::oeserverd execution error:"));
        for(unsigned int i=0 ; i < err_array.GetCount() ; i++){
            wxString line = err_array[i];
            wxLogMessage(line);
        }
    }

    for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
        wxString line = ret_array[i];
        if(line.IsSameAs(_T("1")))
            return true;
        if(line.IsSameAs(_T("0")))
            return false;
    }

    // Show error in log
    wxLogMessage(_T("IsDongleAvailable()::oeserverd execution error:"));
    for(unsigned int i=0 ; i < err_array.GetCount() ; i++){
        wxString line = err_array[i];
        wxLogMessage(line);
    }

    //g_sencutil_bin.Clear();
#endif
    
    return false;
}

unsigned int GetDongleSN()
{
    unsigned int rv = 0;
    
#ifndef __OCPN__ANDROID__    
    wxString cmd = g_sencutil_bin;
    cmd += _T(" -t ");                  // SN

    wxArrayString ret_array;      
    wxExecute(cmd, ret_array, ret_array );
            
    for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
        wxString line = ret_array[i];
        long sn;
        line.ToLong(&sn, 10);
        rv = sn;
    }
#endif    
    return rv;
}
    
    
wxString getFPR( bool bCopyToDesktop, bool &bCopyOK, bool bSGLock)
{

#ifndef __OCPN__ANDROID__    
   
            wxString msg1;
            wxString fpr_file;
            wxString fpr_dir = *GetpPrivateApplicationDataLocation(); //GetWritableDocumentsDir();
            
#ifdef __WXMSW__
            
            //  On XP, we simply use the root directory, since any other directory may be hidden
            int major, minor;
            ::wxGetOsVersion( &major, &minor );
            if( (major == 5) && (minor == 1) )
                fpr_dir = _T("C:\\");
#endif        
            
            if( fpr_dir.Last() != wxFileName::GetPathSeparator() )
                fpr_dir += wxFileName::GetPathSeparator();
            
            wxString cmd = g_sencutil_bin;
            if(bSGLock)
                cmd += _T(" -k ");                  // Make SGLock fingerprint
            else
                cmd += _T(" -g ");                  // Make fingerprint
            
#ifndef __WXMSW__
            cmd += _T("\"");
            cmd += fpr_dir;
            
            //cmd += _T("my fpr/");             // testing
            
            //            wxString tst_cedilla = wxString::Format(_T("my fpr copy %cCedilla/"), 0x00E7);       // testing French cedilla
            //            cmd += tst_cedilla;            // testing
            
            cmd += _T("\"");
#else
            cmd += wxString('\"'); 
            cmd += fpr_dir;
            
            //            cmd += _T("my fpr\\");            // testing spaces in path
            
            //            wxString tst_cedilla = wxString::Format(_T("my%c\\"), 0x00E7);       // testing French cedilla
            //            cmd += tst_cedilla;            // testing
#endif            
            wxLogMessage(_T("Create FPR command: ") + cmd);
            
            ::wxBeginBusyCursor();
            
            wxArrayString ret_array;      
            wxExecute(cmd, ret_array, ret_array );
            
            ::wxEndBusyCursor();
            
            bool berr = false;
            for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
                wxString line = ret_array[i];
                wxLogMessage(line);
                if(line.Upper().Find(_T("ERROR")) != wxNOT_FOUND){
                    berr = true;
                    break;
                }
                if(line.Upper().Find(_T("FPR")) != wxNOT_FOUND){
                    fpr_file = line.AfterFirst(':');
                }
                
            }
            if(!berr){
                if(fpr_file.IsEmpty()){                 // Probably dongle not present
                    fpr_file = _T("DONGLE_NOT_PRESENT");
                    return fpr_file;
                }
            }
            
            
            bool berror = false;
            
            if( bCopyToDesktop && !berr && fpr_file.Length()){
                
                bool bcopy = false;
                wxString sdesktop_path;
                
#ifdef __WXMSW__
                TCHAR desktop_path[MAX_PATH*2] = { 0 };
                bool bpathGood = false;
                HRESULT  hr;
                HANDLE ProcToken = NULL;
                OpenProcessToken( GetCurrentProcess(), TOKEN_READ, &ProcToken );
                
                hr = SHGetFolderPath( NULL,  CSIDL_DESKTOPDIRECTORY, ProcToken, 0, desktop_path);
                if (SUCCEEDED(hr))    
                    bpathGood = true;
                
                CloseHandle( ProcToken );
                
                //                wchar_t *desktop_path = 0;
                //                bool bpathGood = false;
                
                //               if( (major == 5) && (minor == 1) ){             //XP
                //                    if(S_OK == SHGetFolderPath( (HWND)0,  CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop_path))
                //                        bpathGood = true;
                
                
                //                 }
                //                 else{
                    //                     if(S_OK == SHGetKnownFolderPath( FOLDERID_Desktop, 0, 0, &desktop_path))
                //                         bpathGood = true;
                //                 }
                
                
                if(bpathGood){
                    
                    char str[128];
                    wcstombs(str, desktop_path, 128);
                    wxString desktop_fpr(str, wxConvAuto());
                    
                    sdesktop_path = desktop_fpr;
                    if( desktop_fpr.Last() != wxFileName::GetPathSeparator() )
                        desktop_fpr += wxFileName::GetPathSeparator();
                    
                    wxFileName fn(fpr_file);
                    wxString desktop_fpr_file = desktop_fpr + fn.GetFullName();
                    
                    
                    wxString exe = _T("xcopy");
                    wxString parms = fpr_file.Trim() + _T(" ") + wxString('\"') + desktop_fpr + wxString('\"');
                    wxLogMessage(_T("FPR copy command: ") + exe + _T(" ") + parms);
                    
                    const wchar_t *wexe = exe.wc_str(wxConvUTF8);
                    const wchar_t *wparms = parms.wc_str(wxConvUTF8);
                    
                    if( (major == 5) && (minor == 1) ){             //XP
                        // For some reason, this does not work...
                        //8:43:13 PM: Error: Failed to copy the file 'C:\oc01W_1481247791.fpr' to '"C:\Documents and Settings\dsr\Desktop\oc01W_1481247791.fpr"'
                        //                (error 123: the filename, directory name, or volume label syntax is incorrect.)
                        //8:43:15 PM: oesenc fpr file created as: C:\oc01W_1481247791.fpr
                        
                        bcopy = wxCopyFile(fpr_file.Trim(false), _T("\"") + desktop_fpr_file + _T("\""));
                    }
                    else{
                        ::wxBeginBusyCursor();
                        
                        // Launch oeserverd as admin
                        SHELLEXECUTEINFO sei = { sizeof(sei) };
                        sei.lpVerb = L"runas";
                        sei.lpFile = wexe;
                        sei.hwnd = NULL;
                        sei.lpParameters = wparms;
                        sei.nShow = SW_SHOWMINIMIZED;
                        sei.fMask = SEE_MASK_NOASYNC;
                        
                        if (!ShellExecuteEx(&sei))
                        {
                            DWORD dwError = GetLastError();
                            if (dwError == ERROR_CANCELLED)
                            {
                                // The user refused to allow privileges elevation.
                                OCPNMessageBox_PlugIn(NULL, _("Administrator priveleges are required to copy fpr.\n  Please try again...."), _("oeSENC_pi Message"), wxOK);
                                berror = true;
                            }
                        }
                        else
                            bcopy = true;
                        
                        ::wxEndBusyCursor();
                        
                    }  
                }
#endif            // MSW

#ifdef __WXOSX__
                wxFileName fn(fpr_file);
                wxString desktop_fpr_path = ::wxGetHomeDir() + wxFileName::GetPathSeparator() +
                _T("Desktop") + wxFileName::GetPathSeparator() + fn.GetFullName();
                
                bcopy =  ::wxCopyFile(fpr_file.Trim(false), desktop_fpr_path);
                sdesktop_path = desktop_fpr_path;
                msg1 += _T("\n\n OSX ");
#endif
                
                
                wxLogMessage(_T("oeSENC fpr file created as: ") + fpr_file);
                if(bCopyToDesktop && bcopy)
                    wxLogMessage(_T("oeSENC fpr file created in desktop folder: ") + sdesktop_path);
                
                if(bcopy)
                    bCopyOK = true;
        }
        else if(berr){
            wxLogMessage(_T("oesenc_pi: oeserverd results:"));
            for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
                wxString line = ret_array[i];
                wxLogMessage( line );
            }
            berror = true;
        }
        
        if(berror)
            return _T("");
        else
            return fpr_file;
#else   // android

        // Get XFPR from the oeserverda helper utility.
        //  The target binary executable
        wxString cmd = g_sencutil_bin;

//  Set up the parameter passed as the local app storage directory, and append "cache/" to it
        wxString dataLoc = *GetpPrivateApplicationDataLocation();
        wxFileName fn(dataLoc);
        wxString dataDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        dataDir += _T("cache/");

        wxString rootDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        
        //  Set up the parameter passed to runtime environment as LD_LIBRARY_PATH
        // This will be {dir of g_sencutil_bin}
        wxFileName fnl(cmd);
//        wxString libDir = fnl.GetPath();
        wxString libDir = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("lib");
      
        wxLogMessage(_T("oernc_pi: Getting XFPR: Starting: ") + cmd );
        wxLogMessage(_T("oernc_pi: Getting XFPR: Parms: ") + rootDir + _T(" ") + dataDir + _T(" ") + libDir );

        qDebug() << "FPR3" << cmd.mb_str();
        wxString result = callActivityMethod_s6s("createProcSync4", cmd, _T("-q"), rootDir, _T("-g"), dataDir, libDir);

        wxLogMessage(_T("oernc_pi: Start Result: ") + result);

        bool berror = true;            //TODO
        
        // Find the file...
        wxArrayString files;
        wxString lastFile = _T("NOT_FOUND");
        time_t tmax = -1;
        size_t nf = wxDir::GetAllFiles(dataDir, &files, _T("*.fpr"), wxDIR_FILES);
        if(nf){
            for(size_t i = 0 ; i < files.GetCount() ; i++){
                qDebug() << "looking at FPR file: " << files[i].mb_str();
                time_t t = ::wxFileModificationTime(files[i]);
                if(t > tmax){
                    tmax = t;
                    lastFile = files[i];
                }
            }
        }

        qDebug() << "Selected FPR file: " << lastFile.mb_str();

        if(::wxFileExists(lastFile))
            return lastFile;
        else
            return _T("");

#endif        
        
}



// An Event handler class to catch events from UI dialog
//      Implementation
#define ANDROID_EVENT_TIMER 4392
#define ACTION_ARB_RESULT_POLL 1

BEGIN_EVENT_TABLE ( oesenc_pi_event_handler, wxEvtHandler )
EVT_TIMER ( ANDROID_EVENT_TIMER, oesenc_pi_event_handler::onTimerEvent )
END_EVENT_TABLE()

oesenc_pi_event_handler::oesenc_pi_event_handler(oesenc_pi *parent)
{
    m_parent = parent;
    m_eventTimer.SetOwner( this, ANDROID_EVENT_TIMER );
    m_timerAction = -1;
    
}

oesenc_pi_event_handler::~oesenc_pi_event_handler()
{
}

void oesenc_pi_event_handler::onTimerEvent(wxTimerEvent &event)
{
#ifdef __OCPN__ANDROID__    
    if(ACTION_ARB_RESULT_POLL == m_timerAction){
        wxString status = callActivityMethod_vs("getArbActivityStatus");
        //qDebug() << status.mb_str();
        
        if(status == _T("COMPLETE")){
            m_eventTimer.Stop();
            m_timerAction = -1;
            
            qDebug() << "Got COMPLETE";
            wxString result = callActivityMethod_vs("getArbActivityResult");
            qDebug() << result.mb_str();
            processArbResult(result);
        }
    }
#endif    
}

void oesenc_pi_event_handler::processArbResult( wxString result )
{
    m_parent->ProcessChartManageResult(result);
}


void oesenc_pi_event_handler::OnShowFPRClick( wxCommandEvent &event )
{
#ifdef __WXMAC__
    wxExecute( wxString::Format("open -R %s", g_fpr_file) );
#endif
#ifdef __WXMSW__		 
    wxExecute( wxString::Format("explorer.exe /select,%s", g_fpr_file) );
#endif
#ifdef __WXGTK__
    wxExecute( wxString::Format("xdg-open %s", wxFileName::FileName(g_fpr_file).GetPath()) );
#endif
}

void oesenc_pi_event_handler::OnClearSystemName( wxCommandEvent &event )
{
    wxString msg = _("System name RESET shall be performed only by request from o-charts technical support staff.");
    msg += _T("\n\n");
    msg += _("Proceed to RESET?");
    int ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_PI Message"), wxYES_NO);
    
    if(ret != wxID_YES)
        return;
        
    g_systemName.Clear();
    if(g_prefs_dialog){
        g_prefs_dialog->m_nameTextBox->SetLabel(_T(" "));
        g_prefs_dialog->m_buttonClearSystemName->Disable();
        
        g_prefs_dialog->Refresh(true);
    }
    wxFileConfig *pConf = GetOCPNConfigObject();
    if( pConf ) {
        pConf->SetPath( _T("/PlugIns/oesenc") );
        pConf->Write( _T("systemName"), g_systemName);
    }
    
#ifndef __OCPN__ANDROID__    
    if(m_parent->m_shoppanel){
        m_parent->m_shoppanel->RefreshSystemName();
    }
#endif    
        
}

void oesenc_pi_event_handler::OnShowEULA( wxCommandEvent &event )
{
    ChartSetEULA *CSE;
    
    for(unsigned int i=0 ; i < g_EULAArray.GetCount() ; i++){
        CSE = g_EULAArray.Item(i);
        wxString file = CSE->fileName;
        file.Replace('!', wxFileName::GetPathSeparator());
        
        if(wxFileExists(file)){
            oesenc_pi_about *pab = new oesenc_pi_about( GetOCPNCanvasWindow(), file );
            pab->SetOKMode();
            pab->ShowModal();
            pab->Destroy();
        
            break;                      // once is enough
        }
    }
}

extern void saveShopConfig();

void oesenc_pi_event_handler::OnClearCredentials( wxCommandEvent &event )
{
    g_loginKey.Clear();
    saveShopConfig();
    
    OCPNMessageBox_PlugIn(NULL, _("Credential Reset Successful"), _("oeSENC_pi Message"), wxOK);
}

void oesenc_pi_event_handler::OnNewDFPRClick( wxCommandEvent &event )
{
#ifndef __OCPN__ANDROID__    
    wxString msg = _("To obtain a chart set, you must generate a Unique System Identifier File.\n");
    msg += _("This file is also known as a\"fingerprint\" file.\n");
    msg += _("The fingerprint file contains information related to a connected USB key dongle.\n\n");
    msg += _("After creating this file, you will need it to obtain your chart sets at the o-charts.org shop.\n\n");
    msg += _("Proceed to create Fingerprint file?");


    int ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_PI Message"), wxYES_NO);
    
    if(ret == wxID_YES){
        wxString msg1;
        
        bool b_copyOK = false;
        wxString fpr_file = getFPR( true , b_copyOK, true);
        
        // Check for missing dongle...
        if(fpr_file.IsSameAs(_T("DONGLE_NOT_PRESENT"))){
            OCPNMessageBox_PlugIn(NULL, _("ERROR Creating Fingerprint file\n USB key dongle not detected."), _("oeSENC_pi Message"), wxOK);
            return;
        }
        
        if(fpr_file.Len()){
            msg1 += _("Fingerprint file created.\n");
            msg1 += fpr_file;
            
            if(b_copyOK)
                msg1 += _("\n\n Fingerprint file is also copied to desktop.");
            
            OCPNMessageBox_PlugIn(NULL, msg1, _("oeSENC_pi Message"), wxOK);
            
            m_parent->Set_FPR();
            
        }
        else{
            OCPNMessageBox_PlugIn(NULL, _("ERROR Creating Fingerprint file\n Check OpenCPN log file."), _("oeSENC_pi Message"), wxOK);
        }
        
        g_fpr_file = fpr_file;
        
    }           // yes
#endif
}



void oesenc_pi_event_handler::OnNewFPRClick( wxCommandEvent &event )
{
#ifndef x__OCPN__ANDROID__    
    wxString msg = _("To obtain a chart set, you must generate a Unique System Identifier File.\n");
    msg += _("This file is also known as a\"fingerprint\" file.\n");
    msg += _("The fingerprint file contains information to uniquely identify this computer.\n\n");
    msg += _("After creating this file, you will need it to obtain your chart sets at the o-charts.org shop.\n\n");
    msg += _("Proceed to create Fingerprint file?");

    int ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_PI Message"), wxYES_NO);
    
    if((ret == wxID_YES) || (ret == wxID_OK) ){
        wxString msg1;
        
        bool b_copyOK = false;
        wxString fpr_file = getFPR( true , b_copyOK, false);
        
        if(fpr_file.Len()){
            msg1 += _("Fingerprint file created.\n");
            msg1 += fpr_file;
            
            if(b_copyOK)
                msg1 += _("\n\n Fingerprint file is also copied to desktop.");
            
            OCPNMessageBox_PlugIn(NULL, msg1, _("oeSENC_pi Message"), wxOK);
            
            m_parent->Set_FPR();
            
        }
        else{
            OCPNMessageBox_PlugIn(NULL, _T("ERROR Creating Fingerprint file\n Check OpenCPN log file."), _("oeSENC_pi Message"), wxOK);
        }
        
        g_fpr_file = fpr_file;
        
    }           // yes
#else                   // Android

        // Get XFPR from the oeserverda helper utility.
        //  The target binary executable
        wxString cmd = g_sencutil_bin;

//  Set up the parameter passed as the local app storage directory, and append "cache/" to it
        wxString dataLoc = *GetpPrivateApplicationDataLocation();
        wxFileName fn(dataLoc);
        wxString dataDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        dataDir += _T("cache/");

        wxString rootDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        
        //  Set up the parameter passed to runtime environment as LD_LIBRARY_PATH
        // This will be {dir of g_sencutil_bin}/lib
        wxFileName fnl(cmd);
        wxString libDir = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("lib");
        
        wxLogMessage(_T("oesenc_pi: Getting XFPR: Starting: ") + cmd );

        wxString result = callActivityMethod_s6s("createProcSync4", cmd, _T("-q"), rootDir, _T("-g"), dataDir, libDir);

        wxLogMessage(_T("oesenc_pi: Start Result: ") + result);

        
        wxString sFPRPlus;              // The composite string we will pass to the management activity
        
        // Convert the XFPR to an ASCII string for transmission inter-process...
        // Find the file...
        wxArrayString files;
        wxString lastFile = _T("NOT_FOUND");
        time_t tmax = -1;
        size_t nf = wxDir::GetAllFiles(dataDir, &files, _T("*.fpr"), wxDIR_FILES);
        if(nf){
            for(size_t i = 0 ; i < files.GetCount() ; i++){
                qDebug() << "looking at FPR file: " << files[i].mb_str();
                time_t t = ::wxFileModificationTime(files[i]);
                if(t > tmax){
                    tmax = t;
                    lastFile = files[i];
                }
            }
        }
        
        qDebug() << "last FPR file: " << lastFile.mb_str();
            
        //Read the file, convert to ASCII hex, and build a string
        if(::wxFileExists(lastFile)){
            wxString stringFPR;
            wxFileInputStream stream(lastFile);
            while(stream.IsOk() && !stream.Eof() ){
                char c = stream.GetC();
                if(!stream.Eof()){
                    wxString sc;
                    sc.Printf(_T("%02X"), c);
                    stringFPR += sc;
                }
            }
            sFPRPlus += _T("FPR:");                 // name        
            sFPRPlus += stringFPR;                  // values
            sFPRPlus += _T(";");                    // delimiter
        }
        
        //  Add the filename
        wxFileName fnxpr(lastFile);
        wxString fprName = fnxpr.GetName();
        sFPRPlus += _T("fprName:");                 // name        
        sFPRPlus += fprName;                  // values
        sFPRPlus += _T(".fpr");
        sFPRPlus += _T(";");                    // delimiter
        

        // We can safely delete the FPR file now.
        if(::wxFileExists(lastFile))
            wxRemoveFile( lastFile );
        
        // Get and add other name/value pairs to the sFPRPlus string
        sFPRPlus += _T("User:");
        sFPRPlus += g_loginUser;
        sFPRPlus += _T(";");                    // delimiter
        
        sFPRPlus += _T("loginKey:");
        if(!g_loginKey.Length())
            sFPRPlus += _T("?");
        else
            sFPRPlus += g_loginKey;
        sFPRPlus += _T(";");                    // delimiter
        
        //  System Name
        sFPRPlus += _T("systemName:");
        sFPRPlus += g_systemName;
        sFPRPlus += _T(";");                    // delimiter
        
        //  ADMIN mode bit
        sFPRPlus += _T("ADMIN:");
        sFPRPlus += g_admin ? _T("1"):_T("0");
        sFPRPlus += _T(";");                    // delimiter
        
        qDebug() << "sFPRPlus: " << sFPRPlus.mb_str();
        
        m_eventTimer.Stop();
            
        wxLogMessage(_T("sFPRPlus: ") + sFPRPlus);
        
        // Start the Chart management activity
        callActivityMethod_s5s( "startActivityWithIntent", _T("org.opencpn.oesencplugin"), _T("ChartsetListActivity"), _T("FPRPlus"), sFPRPlus, _T("ManageResult") );
        
        // Start a timer to poll for results.
        m_timerAction = ACTION_ARB_RESULT_POLL;
        m_eventTimer.Start(1000, wxTIMER_CONTINUOUS);
        
        
#endif
        
}


void oesenc_pi_event_handler::OnManageShopClick( wxCommandEvent &event )
{
    
#ifdef __OCPN__ANDROID__
    
    g_deviceInfo = callActivityMethod_vs("getDeviceInfo");
    
    wxStringTokenizer tkz(g_deviceInfo, _T("\n"));
    while( tkz.HasMoreTokens() )
    {
        wxString s1 = tkz.GetNextToken();
        if(wxNOT_FOUND != s1.Find(_T("systemName:"))){
            int pos = s1.Find(_T("systemName:"));
            g_systemName = s1.Mid(pos);
        }
    }
    qDebug() << "systemName by deviceInfo: " << g_systemName.mb_str();
#endif

        doShop();

#if 0        

        // Get XFPR from the oeserverda helper utility.
        //  The target binary executable
        wxString cmd = g_sencutil_bin;

//  Set up the parameter passed as the local app storage directory, and append "cache/" to it
        wxString dataLoc = *GetpPrivateApplicationDataLocation();
        wxFileName fn(dataLoc);
        wxString dataDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        dataDir += _T("cache/");

        wxString rootDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        
        //  Set up the parameter passed to runtime environment as LD_LIBRARY_PATH
        // This will be {dir of g_sencutil_bin}/lib
        wxFileName fnl(cmd);
        wxString libDir = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("lib");
        
        wxLogMessage(_T("oesenc_pi: Getting XFPR: Starting: ") + cmd );

        wxString result = callActivityMethod_s6s("createProcSync4", cmd, _T("-q"), rootDir, _T("-g"), dataDir, libDir);

        wxLogMessage(_T("oesenc_pi: Start Result: ") + result);

        
        wxString sFPRPlus;              // The composite string we will pass to the management activity
        
        // Convert the XFPR to an ASCII string for transmission inter-process...
        // Find the file...
        wxArrayString files;
        wxString lastFile = _T("NOT_FOUND");
        time_t tmax = -1;
        size_t nf = wxDir::GetAllFiles(dataDir, &files, _T("*.fpr"), wxDIR_FILES);
        if(nf){
            for(size_t i = 0 ; i < files.GetCount() ; i++){
                qDebug() << "looking at FPR file: " << files[i].mb_str();
                wxString msg;
                msg.Printf(_T("looking at FPR file: %s"), files[i].mb_str());
                wxLogMessage(msg);
                wxFileInputStream stream(files[i]);
                if(stream.IsOk() && !stream.Eof() ){
                    char c = stream.GetC();
                    if(!stream.Eof()){
                        wxString sc;
                        sc.Printf(_T("%02X"), c);
                        wxLogMessage(_T("  First char: ") + sc);
                    }
                }

                time_t t = ::wxFileModificationTime(files[i]);
                if(t > tmax){
                    tmax = t;
                    lastFile = files[i];
                }
            }
        }
        
        wxString msg;
        msg.Printf(_T("last FPR file: %s"), lastFile.mb_str());
        wxLogMessage(msg);
        
        qDebug() << "last FPR file: " << lastFile.mb_str();
            
        //Read the file, convert to ASCII hex, and build a string
        if(::wxFileExists(lastFile)){
            wxString stringFPR;
            wxFileInputStream stream(lastFile);
            while(stream.IsOk() && !stream.Eof() ){
                char c = stream.GetC();
                if(!stream.Eof()){
                    wxString sc;
                    sc.Printf(_T("%02X"), c);
                    stringFPR += sc;
                }
            }
            sFPRPlus += _T("FPR:");                 // name        
            sFPRPlus += stringFPR;                  // values
            sFPRPlus += _T(";");                    // delimiter
        }
        
        //  Add the filename
        wxFileName fnxpr(lastFile);
        wxString fprName = fnxpr.GetName();
        sFPRPlus += _T("fprName:");                 // name        
        sFPRPlus += fprName;                  // values
        sFPRPlus += _T(".fpr");
        sFPRPlus += _T(";");                    // delimiter
        

        // We can safely delete the FPR file now.
        if(::wxFileExists(lastFile))
            wxRemoveFile( lastFile );
        
        // Get and add other name/value pairs to the sFPRPlus string
        sFPRPlus += _T("User:");
        sFPRPlus += g_loginUser;
        sFPRPlus += _T(";");                    // delimiter
        
        sFPRPlus += _T("loginKey:");
        if(!g_loginKey.Length())
            sFPRPlus += _T("?");
        else
            sFPRPlus += g_loginKey;
        sFPRPlus += _T(";");                    // delimiter
        
        //  System Name
        sFPRPlus += _T("systemName:");
        sFPRPlus += g_systemName;
        sFPRPlus += _T(";");                    // delimiter
        
        //  ADMIN mode bit
        sFPRPlus += _T("ADMIN:");
        sFPRPlus += g_admin ? _T("1"):_T("0");
        sFPRPlus += _T(";");                    // delimiter
        
        qDebug() << "sFPRPlus: " << sFPRPlus.mb_str();
        
        m_eventTimer.Stop();
            
        wxLogMessage(_T("sFPRPlus: ") + sFPRPlus);
        
        // Start the Chart management activity
        callActivityMethod_s5s( "startActivityWithIntent", _T("org.opencpn.oesencplugin"), _T("ChartsetListActivity"), _T("FPRPlus"), sFPRPlus, _T("ManageResult") );
        
        // Start a timer to poll for results.
        m_timerAction = ACTION_ARB_RESULT_POLL;
        m_eventTimer.Start(1000, wxTIMER_CONTINUOUS);
        

#endif  // 0

    
}


void oesenc_pi_event_handler::OnGetHWIDClick( wxCommandEvent &event )
{
#ifndef __OCPN__ANDROID__    

#else

        // Get XFPR from the oeserverda helper utility.
        //  The target binary executable
        wxString cmd = g_sencutil_bin;

//  Set up the parameter passed as the local app storage directory, and append "cache/" to it
        wxString dataLoc = *GetpPrivateApplicationDataLocation();
        wxFileName fn(dataLoc);
        wxString dataDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        
        wxString rootDir = fn.GetPath(wxPATH_GET_SEPARATOR);
        
        //  Set up the parameter passed to runtime environment as LD_LIBRARY_PATH
        // This will be {dir of g_sencutil_bin}/lib
        wxFileName fnl(cmd);
        wxString libDir = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("lib");
        
        wxLogMessage(_T("oesenc_pi: Getting HWID: Starting: ") + cmd );

        wxString result = callActivityMethod_s6s("createProcSync4", cmd, _T("-q"), rootDir, _T("-w"), dataDir, libDir);

        wxLogMessage(_T("oesenc_pi: Start Result: ") + result);

#endif
        
}

bool CheckEULA( void )
{
    wxLogMessage(_T("CheckEULA"));
    if(g_bEULA_Rejected)
        return false;
    
    if(g_bEULA_OK && g_UserKey.Length())
        return true;
       
    wxString shareLocn =*GetpSharedDataLocation() +
    _T("plugins") + wxFileName::GetPathSeparator() +
    _T("oesenc_pi") + wxFileName::GetPathSeparator();
    
    oesenc_pi_about *pab = new oesenc_pi_about( GetOCPNCanvasWindow() );
    pab->ShowModal();
    g_bEULA_OK = (pab->GetReturnCode() == 0);
    

    if(!g_bEULA_OK)
        wxLogMessage(_T("EULA Rejected."));
    else
        wxLogMessage(_T("EULA Accepted."));
    
    
    if(g_bEULA_OK && (0 == g_UserKey.Length()) )
        g_UserKey = _T("Pending");
        
    pab->Destroy();
    
    return g_bEULA_OK;
}

wxString getEULASha1( wxString fileName)
{
    wxString result;
    
    if(!::wxFileExists(fileName))
        return _T("");
   
    wxTextFile eula_file( fileName );
    if( eula_file.Open() ){
            
        wxArrayString sig_array;
        wxString line = eula_file.GetFirstLine();
            
        while( !eula_file.Eof() ){
            sig_array.Add(line);
            line = eula_file.GetNextLine();
        }
                
    //  Make one long string of the  file, to treat as a blob
        wxString eula_blob;
        for(unsigned int i=0 ; i < sig_array.Count() ; i++){
            wxString line = sig_array[i];
            eula_blob += line;
        }                

        // calculate SHA1 of the blob
        wxCharBuffer blob_buf = eula_blob.ToUTF8();
        
        SHA1Context sha1;
        uint8_t sha1sum[SHA1HashSize];
        SHA1Reset(&sha1);
        
        SHA1Input(&sha1, (uint8_t *)blob_buf.data(), strlen( blob_buf.data()) );
        SHA1Result(&sha1, sha1sum);
    
        for(int i=0 ; i < 20 ; i ++){
            wxString val;
            val.Printf(_T("%02X"), sha1sum[i]);
            result += val;
        }
    }    
    
    return result;
}
    
bool ShowEULA( wxString fileName )
{
    wxLogMessage(_T("ShowEULA"));
    
    wxString sha = getEULASha1(fileName);
    
    //  look in the session persistent array for a match
    for(unsigned int i=0 ; i < g_EULAShaArray.GetCount() ; i++){
        if(g_EULAShaArray[i] == sha)
            return true;
    }
    
#ifdef __OCPN__ANDROID__
    androidHideBusyIcon();
#endif
    
    oesenc_pi_about *pab = new oesenc_pi_about( GetOCPNCanvasWindow(), fileName );
    pab->ShowModal();
    bool bEULA_OK = (pab->GetReturnCode() == 0);
    
    if(!bEULA_OK)
        wxLogMessage(_T("EULA Rejected."));
    else
        wxLogMessage(_T("EULA Accepted."));
    
    if(bEULA_OK)
        g_EULAShaArray.Add(sha);
    
    if(bEULA_OK && (0 == g_UserKey.Length()) )
        g_UserKey = _T("Pending");
    
    pab->Destroy();
    
    return bEULA_OK;
}

    
            


IMPLEMENT_DYNAMIC_CLASS( oesenc_pi_about, wxDialog )

BEGIN_EVENT_TABLE( oesenc_pi_about, wxDialog )
EVT_BUTTON( xID_OK, oesenc_pi_about::OnXidOkClick )
EVT_BUTTON( xID_CANCEL, oesenc_pi_about::OnXidRejectClick )
EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK_HELP, oesenc_pi_about::OnPageChange)
EVT_CLOSE( oesenc_pi_about::OnClose )
END_EVENT_TABLE()

oesenc_pi_about::oesenc_pi_about( void ) :
    m_parent( NULL ),
    m_btips_loaded ( FALSE ) { }

oesenc_pi_about::oesenc_pi_about( wxWindow* parent, wxWindowID id, const wxString& caption,
                  const wxPoint& pos, const wxSize& size, long style) :
    m_parent( parent ),
    m_btips_loaded ( FALSE )
{
  Create(parent, id, caption, pos, size, style);
}

oesenc_pi_about::oesenc_pi_about( wxWindow* parent, wxString fileName, wxWindowID id, const wxString& caption,
                                  const wxPoint& pos, const wxSize& size, long style) :
                                  m_parent( parent ),
                                  m_btips_loaded ( FALSE )
{
    m_fileName = fileName;
    Create(parent, id, caption, pos, size, style);
}
                                  
                                  
bool oesenc_pi_about::Create( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos,
        const wxSize& size, long style )
{
    m_parent = parent;
#ifdef __WXOSX__
    style |= wxSTAY_ON_TOP;
#endif

    SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
    wxDialog::Create( parent, id, caption, pos, size, style );
    wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    SetFont( *qFont );

    closeButton = NULL;
    rejectButton = NULL;
        
    //m_displaySize = g_Platform->getDisplaySize();
    CreateControls();
    Populate();

    RecalculateSize();

    return TRUE;
}

void oesenc_pi_about::SetOKMode()
{
    if(closeButton)
        closeButton->SetLabel(_T("OK"));
    if(rejectButton)
        rejectButton->Hide();
}
    
#if 0
void oesenc_pi_about::SetColorScheme( void )
{
    DimeControl( this );
    wxColor bg = GetBackgroundColour();
    pAboutHTMLCtl->SetBackgroundColour( bg );
    pLicenseHTMLCtl->SetBackgroundColour( bg );
    pAuthorHTMLCtl->SetBackgroundColour( bg );
    

    // This looks like non-sense, but is needed for __WXGTK__
    // to get colours to propagate down the control's family tree.
    SetBackgroundColour( bg );

#ifdef __WXQT__
    // wxQT has some trouble clearing the background of HTML window...
    wxBitmap tbm( GetSize().x, GetSize().y, -1 );
    wxMemoryDC tdc( tbm );
    tdc.SetBackground( bg );
    tdc.Clear();
    pAboutHTMLCtl->SetBackgroundImage(tbm);
    pLicenseHTMLCtl->SetBackgroundImage(tbm);
    pAuthorHTMLCtl->SetBackgroundImage(tbm);
#endif

}
#endif

void oesenc_pi_about::Populate( void )
{

    wxColor bg = GetBackgroundColour();
    wxColor fg = wxColour( 0, 0, 0 );

    // The HTML Header
    wxString aboutText =
        wxString::Format(
            _T( "<html><body bgcolor=#%02x%02x%02x><font color=#%02x%02x%02x>" ),
            bg.Red(), bg.Blue(), bg.Green(), fg.Red(), fg.Blue(), fg.Green() );

    wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    
    // Do weird font size calculation
    int points = dFont->GetPointSize();
#ifndef __WXOSX__
    ++points;
#endif
    int sizes[7];
    for ( int i = -2; i < 5; i++ ) {
        sizes[i+2] = points + i + ( i > 0 ? i : 0 );
    }
    wxString face = dFont->GetFaceName();
//    pAboutHTMLCtl->SetFonts( face, face, sizes );

    if( wxFONTSTYLE_ITALIC == dFont->GetStyle() )
        aboutText.Append( _T("<i>") );

#if 0
#ifdef __OCPN__ANDROID__    
    aboutText.Append( AboutText + OpenCPNVersionAndroid  + OpenCPNInfoAlt );
#else
    aboutText.Append( AboutText + OpenCPNVersion + OpenCPNInfo );
#endif    

    // Show where the log file is going to be placed
    wxString log_string = _T("Logfile location: ") + g_Platform->GetLogFileName();
    log_string.Replace(_T("/"), _T("/ "));      // allow line breaks, in a cheap way...
    
    aboutText.Append( log_string );

    // Show where the config file is going to be placed
    wxString config_string = _T("<br><br>Config file location: ") + g_Platform->GetConfigFileName();
    config_string.Replace(_T("/"), _T("/ "));      // allow line breaks, in a cheap way...
    aboutText.Append( config_string );
#endif

    if(wxFONTSTYLE_ITALIC == dFont->GetStyle())
        aboutText.Append( _T("</i>") );

    // The HTML Footer
    aboutText.Append( _T("</font></body></html>") );

//    pAboutHTMLCtl->SetPage( aboutText );
    
    
    ///Authors page
    // The HTML Header
    wxString authorText =
    wxString::Format(
        _T( "<html><body bgcolor=#%02x%02x%02x><font color=#%02x%02x%02x>" ),
                     bg.Red(), bg.Blue(), bg.Green(), fg.Red(), fg.Blue(), fg.Green() );
    
//    pAuthorHTMLCtl->SetFonts( face, face, sizes );
    
    
    wxString authorFixText = _T(""); //AuthorText;
    authorFixText.Replace(_T("\n"), _T("<br>"));
    authorText.Append( authorFixText );
    
    // The HTML Footer
    authorText.Append( _T("</font></body></html>") );

//    pAuthorHTMLCtl->SetPage( authorFixText );
    

    ///License page
    // The HTML Header
    wxString licenseText =
    wxString::Format(
        _T( "<html><body bgcolor=#%02x%02x%02x><font color=#%02x%02x%02x>" ),
            bg.Red(), bg.Blue(), bg.Green(), fg.Red(), fg.Blue(), fg.Green() );
        
    pLicenseHTMLCtl->SetFonts( face, face, sizes );
 
    wxString shareLocn =*GetpSharedDataLocation() +
    _T("plugins") + wxFileName::GetPathSeparator() +
    _T("oesenc_pi") + wxFileName::GetPathSeparator();
    
    wxFileName fn(m_fileName);
    bool bhtml = fn.GetExt().Upper() == _T("HTML");
    
    wxTextFile license_filea( m_fileName );
    if ( license_filea.Open() ) {
        for ( wxString str = license_filea.GetFirstLine(); !license_filea.Eof() ; str = license_filea.GetNextLine() ){
            licenseText.Append( str +_T(" ") );
            if(!bhtml)
                licenseText += _T("<br>");
        }
        license_filea.Close();
    } else {
        licenseText.Append(_("Could not open oesenc_pi EULA: ") + m_fileName + _T("<br>"));
        wxLogMessage( _T("Could not open oesenc_pi EULA: ") + m_fileName );
        closeButton->Disable();
    }
    
        
        // The HTML Footer
    licenseText.Append( _T("</font></body></html>") );
        
    pLicenseHTMLCtl->SetPage( licenseText );
    
    pLicenseHTMLCtl->SetBackgroundColour( bg );
    
    #ifdef __WXQT__
    // wxQT has some trouble clearing the background of HTML window...
    wxBitmap tbm( GetSize().x, GetSize().y, -1 );
    wxMemoryDC tdc( tbm );
    tdc.SetBackground( bg );
    tdc.Clear();
    pLicenseHTMLCtl->SetBackgroundImage(tbm);
    #endif
    
        
#if 0    
    wxTextFile license_file( m_DataLocn + _T("license.txt") );
    if ( license_file.Open() ) {
        for ( wxString str = license_file.GetFirstLine(); !license_file.Eof() ; str = license_file.GetNextLine() )
            pLicenseTextCtl->AppendText( str + '\n' );
        license_file.Close();
    } else {
        wxLogMessage( _T("Could not open License file: ") + m_DataLocn );
    }
    
    wxString suppLicense = g_Platform->GetSupplementalLicenseString();
    pLicenseTextCtl->AppendText( suppLicense );
    
    pLicenseTextCtl->SetInsertionPoint( 0 );
#endif

//    SetColorScheme();
}

void oesenc_pi_about::RecalculateSize( void )
{
    //  Make an estimate of the dialog size, without scrollbars showing
    
    wxSize esize;
    esize.x = GetCharWidth() * 110;
    esize.y = GetCharHeight() * 44;
    
    wxSize dsize = GetParent()->GetClientSize();
    esize.y = wxMin(esize.y, dsize.y - (2 * GetCharHeight()));
    esize.x = wxMin(esize.x, dsize.x - (1 * GetCharHeight()));
    SetClientSize(esize);
    
    wxSize fsize = GetSize();
    fsize.y = wxMin(fsize.y, dsize.y - (2 * GetCharHeight()));
    fsize.x = wxMin(fsize.x, dsize.x - (1 * GetCharHeight()));
    
    SetSize(fsize);
    
    Centre();
}


void oesenc_pi_about::CreateControls( void )
{
    //  Set the nominal vertical size of the embedded controls
    //int v_size = 300; //g_bresponsive ? -1 : 300;

    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( mainSizer );
    wxStaticText *pST1 = new wxStaticText( this, -1,
        _("oeSENC PlugIn for OpenCPN"), wxDefaultPosition,
        wxSize( -1, 50 /* 500, 30 */ ), wxALIGN_CENTRE /* | wxALIGN_CENTER_VERTICAL */ );

    wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    
    wxFont *headerFont = qFont;// FontMgr::Get().FindOrCreateFont( 14, wxFONTFAMILY_DEFAULT, qFont->GetStyle(), wxFONTWEIGHT_BOLD, false, qFont->GetFaceName() );
    
    pST1->SetFont( *headerFont );
    mainSizer->Add( pST1, 0, wxALL | wxEXPAND, 8 );

#ifndef __OCPN__ANDROID__    
    wxSizer *buttonSizer = new wxBoxSizer( wxHORIZONTAL /*m_displaySize.x < m_displaySize.y ? wxVERTICAL : wxHORIZONTAL*/ );
    mainSizer->Add( buttonSizer, 0, wxALL, 0 );
    
//     wxButton* donateButton = new wxBitmapButton( this, ID_DONATE,
//             g_StyleManager->GetCurrentStyle()->GetIcon( _T("donate") ),
//             wxDefaultPosition, wxDefaultSize, 0 );
// 
//     buttonSizer->Add( new wxButton( this, ID_COPYLOG, _T("Copy Log File to Clipboard") ), 1, wxALL | wxEXPAND, 3 );
//     buttonSizer->Add( new wxButton( this, ID_COPYINI, _T("Copy Settings File to Clipboard") ), 1, wxALL | wxEXPAND, 3 );
//     buttonSizer->Add( donateButton, 1, wxALL | wxEXPAND | wxALIGN_RIGHT, 3 );
#endif
    
    //  Main Notebook
    pNotebook = new wxNotebook( this, ID_NOTEBOOK_HELP, wxDefaultPosition,
            wxSize( -1, -1 ), wxNB_TOP );
    pNotebook->InheritAttributes();
    mainSizer->Add( pNotebook, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 5 );

#if 0    
    //  About Panel
    itemPanelAbout = new wxPanel( pNotebook, -1, wxDefaultPosition, wxDefaultSize,
            wxSUNKEN_BORDER | wxTAB_TRAVERSAL );
    itemPanelAbout->InheritAttributes();
    pNotebook->AddPage( itemPanelAbout, _T("About"), TRUE /* Default page */ );

    pAboutHTMLCtl = new wxHtmlWindow( itemPanelAbout, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxHW_SCROLLBAR_AUTO | wxHW_NO_SELECTION);
    pAboutHTMLCtl->SetBorders( 5 );
    wxBoxSizer* aboutSizer = new wxBoxSizer( wxVERTICAL );
    aboutSizer->Add( pAboutHTMLCtl, 1, wxALIGN_CENTER_HORIZONTAL | wxEXPAND | wxALL, 5 );
    itemPanelAbout->SetSizer( aboutSizer );

    //  Authors Panel

    itemPanelAuthors = new wxPanel( pNotebook, -1, wxDefaultPosition, wxDefaultSize,
                                wxSUNKEN_BORDER | wxTAB_TRAVERSAL );
    itemPanelAuthors->InheritAttributes();
    pNotebook->AddPage( itemPanelAuthors, _T("Authors") );

    pAuthorHTMLCtl = new wxHtmlWindow( itemPanelAuthors, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxHW_SCROLLBAR_AUTO | wxHW_NO_SELECTION);
    pAuthorHTMLCtl->SetBorders( 5 );
    wxBoxSizer* authorSizer = new wxBoxSizer( wxVERTICAL );
    authorSizer->Add( pAuthorHTMLCtl, 1, wxALIGN_CENTER_HORIZONTAL | wxEXPAND | wxALL, 5 );
    itemPanelAuthors->SetSizer( authorSizer );
#endif    
    

    //  License Panel
    itemPanelLicense = new wxPanel( pNotebook, -1, wxDefaultPosition, wxDefaultSize,
            wxSUNKEN_BORDER | wxTAB_TRAVERSAL );
    itemPanelLicense->InheritAttributes();
    pNotebook->AddPage( itemPanelLicense, _("License") );
    
    pLicenseHTMLCtl = new wxHtmlWindow( itemPanelLicense, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxHW_SCROLLBAR_AUTO | wxHW_NO_SELECTION);
    pLicenseHTMLCtl->SetBorders( 5 );
    wxBoxSizer* licenseSizer = new wxBoxSizer( wxVERTICAL );
    licenseSizer->Add( pLicenseHTMLCtl, 1, wxALIGN_CENTER_HORIZONTAL | wxEXPAND | wxALL, 5 );
    itemPanelLicense->SetSizer( licenseSizer );
    
#if 0
    //  Help Panel
    itemPanelTips = new wxPanel( pNotebook, -1, wxDefaultPosition, wxDefaultSize,
            wxSUNKEN_BORDER | wxTAB_TRAVERSAL );
    itemPanelTips->InheritAttributes();
    pNotebook->AddPage( itemPanelTips, _T("Help") );

    wxBoxSizer* helpSizer = new wxBoxSizer( wxVERTICAL );
    itemPanelTips->SetSizer( helpSizer );
#endif

    //   Buttons
    wxSizer *buttonBottomSizer = new wxBoxSizer( wxHORIZONTAL );
    mainSizer->Add( buttonBottomSizer, 0, wxALL, 5 );
    
    
    closeButton = new wxButton( this, xID_OK, _("Accept"), wxDefaultPosition, wxDefaultSize, 0 );
    closeButton->SetDefault();
    closeButton->InheritAttributes();
    buttonBottomSizer->Add( closeButton, 0, wxEXPAND | wxALL, 5 );

    rejectButton = new wxButton( this, xID_CANCEL, _("Reject"), wxDefaultPosition, wxDefaultSize, 0 );
    rejectButton->InheritAttributes();
    buttonBottomSizer->Add( rejectButton, 0, wxEXPAND | wxALL, 5 );
    
     
}


void oesenc_pi_about::OnXidOkClick( wxCommandEvent& event )
{
    SetReturnCode(0);
    EndModal(0);
}

void oesenc_pi_about::OnXidRejectClick( wxCommandEvent& event )
{
    SetReturnCode(1);
    EndModal(1);
}

void oesenc_pi_about::OnClose( wxCloseEvent& event )
{
    EndModal(1);
    Destroy();
}

void oesenc_pi_about::OnPageChange( wxNotebookEvent& event )
{
}


#ifdef __OCPN__ANDROID__
void showChartinfoDialog( void )
{
    if(g_binfoShown)
        return;
    
    if(info_hash.empty())
        return;
    
    wxString hdr = _T("<html><body><center><font size=+1>");
    hdr +=  _("Available Chart sets:");
    hdr += _T("</font></center>");
    
    hdr += _T("<hr />");
    
    hdr += _T("<center><table border=0 bordercolor=#000000 style=background-color:#fbfbf9 width=800 cellpadding=1 cellspacing=1>");
    
    hdr += _T("<tr>");
    hdr += _T("</tr>");
   
    int len_max = 0;
    int ncs = 1;
    std::map<std::string, ChartInfoItem *>::iterator iter;
    for( iter = info_hash.begin(); iter != info_hash.end(); ++iter )
    {
        wxString csn;
        csn.Printf(_T("Chart set %d"), ncs);
        
        hdr += _T("<td><font size=+2>");
        hdr += csn;
        hdr += _T("</font></td>");
        
        wxString formatted;
        
        ChartInfoItem *pci = iter->second;
        std::string key = iter->first;
        wxString strk = wxString(key.c_str(), wxConvUTF8);
        wxString info = pci->config_string;
        len_max = wxMax(info.Len(), len_max);
        
        
        // Get the line fields
         wxStringTokenizer tkx(info, _T(";"));
         while ( tkx.HasMoreTokens() ){
            wxString token = tkx.GetNextToken();        //description
            hdr += _T("<tr><td>  ") + token + _T("</td></tr>");
                    
            token = tkx.GetNextToken();         // version
            hdr += _T("<tr><td>Version:</td></tr>");
            hdr += _T("<tr><td align=\"right\">") + token + _T("</td></tr>");
            
            token = tkx.GetNextToken();         // expiry date
            hdr += _T("<tr><td>Valid Until:</td></tr>");
            hdr += _T("<tr><td align=\"right\"> <font color=#ff0000>") + token + _T("</font><font color=#000000></font></td></tr>");
         }
        
        ncs++;
        hdr += _T("</tr>");
        
    }
 
    hdr += _T("</table></center>");
    hdr += _T("</body></html>");

    callActivityMethod_s2s("displayHTMLAlertDialog", _("oeSENC_PI Message"), hdr);
    g_binfoShown = true;
}
#endif

#ifndef __OCPN__ANDROID__
void showChartinfoDialog( void )
{
    if(g_binfoShown)
        return;
    
    if(info_hash.empty())
        return;
    
    wxString hdr = _T("<html><body><center><font size=+2>");
    hdr +=  _("The following Chart sets are available:");
    hdr += _T("</font></center>");
    
    hdr += _T("<hr />");
    
    hdr += _T("<center><table border=0 bordercolor=#000000 style=background-color:#fbfbf9 width=600 cellpadding=3 cellspacing=3>");
    
    hdr += _T("<tr>");
    
    hdr += _T("<td><font size=+2>");
    hdr += _("Chart set");
    hdr += _T("</font></td>");
    
    hdr += _T("<td><font size=+2>");
    hdr += _("Version");
    hdr += _T("</font></td>");
    
    hdr += _T("<td><font size=+2>");
    hdr += _("Valid until");
    hdr += _T("</font></td>");
    
    hdr += _T("</tr>");
    
    int len_max = 0;
    std::map<std::string, ChartInfoItem *>::iterator iter;
    for( iter = info_hash.begin(); iter != info_hash.end(); ++iter )
    {
        wxString formatted;
        
        ChartInfoItem *pci = iter->second;
        std::string key = iter->first;
        wxString strk = wxString(key.c_str(), wxConvUTF8);
        wxString info = pci->config_string;
        len_max = wxMax(info.Len(), len_max);
        
        hdr += _T("<tr>");
        
        // Get the line fields
        wxStringTokenizer tkx(info, _T(";"));
        while ( tkx.HasMoreTokens() ){
            wxString token = tkx.GetNextToken();        //description
            hdr += _T("<td>") + token + _T("</td>");
            
            token = tkx.GetNextToken();         // version
            hdr += _T("<td>") + token + _T("</td>");
            
            token = tkx.GetNextToken();         // expiry date
            wxDateTime exdate;
            exdate.ParseDate(token);
            wxTimeSpan diff = exdate - wxDateTime::Today();
            // Expired? Red text
            hdr += diff > 0 ? _T("<td>") + token + _T("</td>") :
                              _T("<td><font color=#ff0000>") + token + _T("</font></td>");            
        }
        
        hdr += _T("</tr>");
    }
    
    hdr += _T("</table></center>");
    hdr += _T("</body></html>");
    
    if(GetOCPNCanvasWindow()){
        wxFont *pFont = OCPNGetFont(_T("Dialog"), 12);
        wxScreenDC dc;
        int sx, sy;
        dc.GetTextExtent(_T("W"), &sx, &sy, NULL, NULL, pFont);
        
        //        int parent_font_width = sx;
        //         wxSize sz = wxSize(len_max * parent_font_width * 1.2, -1);
        
        pinfoDlg = new OESENC_HTMLMessageDialog( NULL /*GetOCPNCanvasWindow()*/, hdr, _("oeSENC_PI Message"), wxOK);
        //        pinfoDlg->SetClientSize(sz);
        pinfoDlg->Centre();
        pinfoDlg->Show();
        g_binfoShown = true;
    }
    
}
#endif


bool processChartinfo(const wxString &oesenc_file)
{
    // Do not process anything if a EULA has been rejected
    if (g_bEULA_Rejected) {
        wxLogMessage(_T("processChartInfo but EULA_Rejected ") + oesenc_file);
        return false;
    }
    
    // get the Chartinfo as a wxTextFile
    wxFileName fn(oesenc_file);
    wxString chartInfoDir = fn.GetPath(  wxPATH_GET_VOLUME + wxPATH_GET_SEPARATOR );
    wxString chartInfo = chartInfoDir + _T("Chartinfo.txt");

    if(wxFileExists(chartInfo)){
        
    // Have we processed this exact ChartInfo file in this session?
    // If so, all is well
        if( wxNOT_FOUND != g_ChartInfoArray.Index( chartInfo))
            return true;
    
        g_ChartInfoArray.Add(chartInfo);
    
    }
    else
        return true;                    // no ChartInfo file at all
        
        
    // First, consider the EULA
    wxTextFile info_file( chartInfo );
    if( info_file.Open() ){
        wxString line = info_file.GetFirstLine();
        
        wxString fileEULA, sshowEULA, fullEULAFileName;
        wxArrayString EULAFileArray;
        while( !info_file.Eof() ){
            if(line.StartsWith( _T("oesencEULAFile:" ) ) ) {
                wxString tentativeFileEULA = line.AfterFirst(':').Trim(false);
                EULAFileArray.Add(tentativeFileEULA);
            }
 
            else if(line.StartsWith( _T("oesencEULAShow:" ) ) ) {
                sshowEULA = line.AfterFirst(':').Trim(false).Trim(); 
                
            }
     
            if( (EULAFileArray.GetCount()) && (sshowEULA.Length())){
                ChartSetEULA *CSE;
                
                if(EULAFileArray.GetCount()){               // might be localized EULA files
                    wxString loc = GetLocaleCanonicalName();
                    wxString loc2 = loc.Mid(0,2).Upper();
                    for(unsigned int iloc = 0 ;  iloc < EULAFileArray.GetCount() ; iloc++){
                        wxString tEULA = EULAFileArray.Item(iloc).Mid(0,3).Upper();
                        if((tEULA[2] == '_') || (tEULA[2] == '-') ){
                            if(tEULA.Mid(0,2) == loc2){         // a match
                                // Does it exist?
                                if(wxFileExists(chartInfoDir + EULAFileArray.Item(iloc))){
                                    fileEULA = EULAFileArray.Item(iloc);
                                    break;
                                }
                            }
                        }
                    }
                }
                
                //  Some trouble with localized EULA? If so, find the first really available file in the array
                if(!fileEULA.Length()){
                    for(unsigned int iloc = 0 ;  iloc < EULAFileArray.GetCount() ; iloc++){
                        if(wxFileExists(chartInfoDir + EULAFileArray.Item(iloc))){
                            fileEULA = EULAFileArray.Item(iloc);
                            break;
                        }
                    }
                }
                
                fullEULAFileName = chartInfoDir + fileEULA;

                wxString subEULAFileName = fullEULAFileName;
                subEULAFileName.Replace(wxFileName::GetPathSeparator(), '!');
                
                //  Search the EULA array loaded from config file for a match
                bool b_found = false;
                for(unsigned int i=0 ; i < g_EULAArray.GetCount() ; i++){
                    if(g_debugLevel) wxLogMessage( _T("Garray item: ") + g_EULAArray.Item(i)->fileName);
                    if(g_debugLevel) wxLogMessage( _T("Comparing to: ") + subEULAFileName);
                    if(subEULAFileName == g_EULAArray.Item(i)->fileName){
                        b_found = true;
                        CSE = g_EULAArray.Item(i);
                        break;
                    }
                }
                
                //  If not found, this is a EULA definition coming for the first time
                //  So add it to the global array to be persisted later.
                if(!b_found){
                    if(g_debugLevel) wxLogMessage(_T("not found, so adding...") + subEULAFileName);
                    
                    ChartSetEULA *cse = new ChartSetEULA;
                    cse->fileName = subEULAFileName;
                    if(sshowEULA.Upper().Find(_T("ONCE")) != wxNOT_FOUND)
                        cse->npolicyShow = 1;
                    else if(sshowEULA.Upper().Find(_T("ALWAYS")) != wxNOT_FOUND)
                        cse->npolicyShow = 2;
                    else 
                        cse->npolicyShow = 0;
                    
                    g_EULAArray.Add(cse);
                    CSE = cse;
                }
                
                //  If the EULA is required to be shown, either once or always, do it here
                bool b_show = false;
                if( (CSE->npolicyShow == 1) && (!CSE->b_onceShown))       // once per lifetime
                    b_show = true;
                if( (CSE->npolicyShow == 2) && (!CSE->b_sessionShown))    // once per session
                    b_show = true;
                
                bool b_showResult = false;
                if(b_show){
                    wxString file = CSE->fileName;
                    file.Replace('!', wxFileName::GetPathSeparator());
                    
                    b_showResult = ShowEULA(file);
                    
                    if(!b_showResult){
                        g_bEULA_Rejected = true;
                        return false;                   // User did not accept EULA, or file missing
                    }
                    else{
                        CSE->b_sessionShown = true;
                        CSE->b_onceShown = true;
                    }
                }
                
                //  Done with this EULA
                fullEULAFileName.Clear();
                sshowEULA.Clear();
                EULAFileArray.Clear();
            }
            
            line = info_file.GetNextLine();
        }
    }
    
    std::string key = std::string(fn.GetPath(wxPATH_GET_VOLUME + wxPATH_GET_SEPARATOR).c_str());
    
    if(wxFileExists(chartInfo)){
        wxTextFile info_file( chartInfo );
        if( info_file.Open() ){
            
            //ChartInfo:Oesenc Charts Test Edition (UK_EU);2016/3;2016-09-30
            int nkey = 1;
            wxString line = info_file.GetFirstLine();
        
            while( !info_file.Eof() ){
                if(line.StartsWith( _T("ChartInfo:" ) ) ) {
                    wxString content = line.AfterFirst(':');
                
                    wxString keyn = fn.GetPath(wxPATH_GET_VOLUME + wxPATH_GET_SEPARATOR);
                    wxString ncnt;
                    ncnt.Printf(_T("K%d"), nkey);
                    keyn.Prepend( ncnt );
                    if(g_debugLevel) wxLogMessage(_T("processChartInfo considering: ") + keyn);
                    
                    keyn.Replace(wxFileName::GetPathSeparator(), '!');
                    
                    std::string key = std::string(keyn.c_str());
                    std::map<std::string, ChartInfoItem *>::iterator iter;
                
                    iter = info_hash.find( key );
                    if( iter == info_hash.end() ){
                        ChartInfoItem *pitem = new ChartInfoItem;
                        pitem->config_string = content;
                        info_hash[key] = pitem;
                        if(g_debugLevel) wxLogMessage(_T("processChartInfo adding: ") + keyn);
                        wxLogMessage(_T("processChartInfo adding config_string: ") + content);
                        
                        g_binfoShown = false;                           // added a line, so force re-display
                    }
                    else{
                        if(g_debugLevel) wxLogMessage(_T("processChartInfo found: ") + keyn);
                    }
                    
                    nkey++;
                }
        
                line = info_file.GetNextLine();
                
            }
        }
        
             
        return true;    
    }
    else
        return false;
                
}


void processUserKeyHint(const wxString &oesenc_file)
{
    if(g_debugLevel) wxLogMessage(_T("processUserKeyHint() start."));
                                     
    // get the Chartinfo as a wxTextFile
    wxFileName fn(oesenc_file);
    wxString userkey = fn.GetPath(  wxPATH_GET_VOLUME + wxPATH_GET_SEPARATOR );
    userkey += _T("Chartinfo.txt");
    
    if(!wxFileExists(userkey))
        return;
        
    wxTextFile info_file( userkey );
    if( info_file.Open() ){
        wxString line = info_file.GetFirstLine();
        
        while( !info_file.Eof() ){
            if(line.StartsWith( _T("UserKey:" ) ) ) {
                wxString content = line.AfterFirst(':').Trim().Trim(false);
                g_UserKey = content;
                if(g_debugLevel) wxLogMessage(_T("processUserKeyHint: taking UserKey: ") + content);
                                                 
                break;
            }
            
            line = info_file.GetNextLine();
        }
        
        g_bUserKeyHintTaken = true;
    }
    
    if(g_debugLevel) wxLogMessage(_T("processUserKeyHint() done. g_UserKey: ") + g_UserKey);
}

bool ShowAlwaysEULAs()
{
    bool b_showResult = true;
    
    ChartSetEULA *CSE;
    
    for(unsigned int i=0 ; i < g_EULAArray.GetCount() ; i++){
        CSE = g_EULAArray.Item(i);
        if(CSE->npolicyShow == 2){
            wxString file = CSE->fileName;
            file.Replace('!', wxFileName::GetPathSeparator());
            
            b_showResult = ShowEULA(file);
            if(!b_showResult)
                return false;
        }
    }
    
    return true;
}

#ifdef __OCPN__ANDROID__
void androidShowBusyIcon()
{
//    if(b_androidBusyShown)
//        return;
    
    //  Get a reference to the running native activity
     QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
        "activity", "()Landroid/app/Activity;");

     if ( !activity.isValid() )
         return;
        
        //  Call the desired method
     QAndroidJniObject data = activity.callObjectMethod("showBusyCircle", "()Ljava/lang/String;");
        
//     b_androidBusyShown = true;
}

void androidHideBusyIcon()
{
//    if(!b_androidBusyShown)
//        return;
    
    //  Get a reference to the running native activity
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
        "activity", "()Landroid/app/Activity;");
        
    if ( !activity.isValid() )
        return;
        
        //  Call the desired method
    QAndroidJniObject data = activity.callObjectMethod("hideBusyCircle", "()Ljava/lang/String;");
        
//    b_androidBusyShown = false;
}
#endif



void oesenc_pi::OnSetupOptions( void )
{
#ifdef x__OCPN__ANDROID__
    m_pOptionsPage = AddOptionsPage( PI_OPTIONS_PARENT_CHARTS, _("oeSENC Charts") );
    if( ! m_pOptionsPage )
    {
        wxLogMessage( _T("Error: oesenc_pi::OnSetupOptions AddOptionsPage failed!") );
        return;
    }
    wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
    m_pOptionsPage->SetSizer( sizer );
    
    m_oesencpanel = new oesencPanel( this, m_pOptionsPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );
    
    m_pOptionsPage->InvalidateBestSize();
    sizer->Add( m_oesencpanel, 1, wxALL | wxEXPAND );
    m_oesencpanel->FitInside();
#else
    m_pOptionsPage = AddOptionsPage( PI_OPTIONS_PARENT_CHARTS, _("oeSENC Charts") );
    if( ! m_pOptionsPage )
    {
        wxLogMessage( _T("Error: oesenc_pi::OnSetupOptions AddOptionsPage failed!") );
        return;
    }
    wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
    m_pOptionsPage->SetSizer( sizer );
    
    m_shoppanel = new shopPanel( m_pOptionsPage, wxID_ANY, wxDefaultPosition, wxDefaultSize );
    
    m_pOptionsPage->InvalidateBestSize();
    sizer->Add( m_shoppanel, 1, wxALL | wxEXPAND );
    m_shoppanel->FitInside();

#endif
}

oesencPanel::oesencPanel( oesenc_pi* plugin, wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
    
    int border_size = 2;
    
    //   Main Sizer
    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( mainSizer );
    
    //  Buttons
    mainSizer->AddSpacer(20);
    wxBoxSizer* bSizerBtns = new wxBoxSizer( wxVERTICAL );
    mainSizer->Add( bSizerBtns, 0, wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, border_size );
    
    m_bManageCharts = new wxButton( this, wxID_ANY, _("Add/Update oeSENC chart sets"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );
    //m_bManageCharts->SetToolTip( _("Add a new chart catalog.") );
    bSizerBtns->Add( m_bManageCharts, 0, wxALL|wxEXPAND, 20 );
    bSizerBtns->AddSpacer(20);
    
    m_bVisitOcharts = new wxButton( this, wxID_ANY, _("Visit o-charts.org Website"), wxDefaultPosition, wxDefaultSize, 0 );
    m_bVisitOcharts->SetToolTip( _("Here you may order new oeSENC chart sets.") );
    bSizerBtns->Add( m_bVisitOcharts, 0, wxALL|wxEXPAND, 20 );
    bSizerBtns->AddSpacer(20);
    
    m_bCreateHWID = new wxButton( this, wxID_ANY, _T("Create HWID (ADMIN mode)"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizerBtns->Add( m_bCreateHWID, 0, wxALL|wxEXPAND, 20 );
    bSizerBtns->AddSpacer(20);
    if(!g_admin)
        m_bCreateHWID->Hide();
    
    this->Layout();
    
    
    // Connect Events
    m_bManageCharts->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( oesencPanel::ManageCharts ), NULL, this );
    m_bVisitOcharts->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( oesencPanel::VisitOCharts ), NULL, this );
    m_bCreateHWID->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( oesencPanel::CreateHWID ), NULL, this );

}
oesencPanel::~oesencPanel()
{
}

void oesencPanel::ManageCharts( wxCommandEvent &evt )
{
    if(g_event_handler)
        g_event_handler->OnManageShopClick(evt);
    
}

void oesencPanel::VisitOCharts( wxCommandEvent &evt )
{
#ifdef __OCPN__ANDROID__    
    qDebug() << "VisitOCharts";
    callActivityMethod_ss("launchBrowser", _T("http://o-charts.org"));
#endif    
}

void oesencPanel::CreateHWID( wxCommandEvent &evt )
{
    if(g_event_handler)
        g_event_handler->OnGetHWIDClick(evt);
}




