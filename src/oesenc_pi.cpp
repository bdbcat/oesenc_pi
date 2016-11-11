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

#include "oesenc_pi.h"
#include "eSENCChart.h"
#include "src/myiso8211/iso8211.h"
#include "dsa_utils.h"
#include "s57RegistrarMgr.h"
#include "S57ClassRegistrar.h"
#include "s52plib.h"
#include "s52utils.h"
#include "Osenc.h"

#ifdef __WXOSX__
#include "GL/gl.h"
#include "GL/glu.h"
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#ifdef __MSVC__
#include <windows.h>
#include <Shlobj.h>
#endif

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

//wxString                        g_fpr_file;
wxString                        g_UserKey;
wxString                        g_old_UserKey;

bool                            g_PIbDebugS57;
wxString                        g_fpr_file;

oesenc_pi_event_handler         *g_event_handler;

double g_overzoom_emphasis_base;
bool g_oz_vector_scale;
float g_ChartScaleFactorExp;

float g_GLMinCartographicLineWidth;
bool  g_b_EnableVBO;
float g_GLMinSymbolLineWidth;
GLenum g_texture_rectangle_format;
bool pi_bopengl;

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

bool shutdown_SENC_server( void );


// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new oesenc_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}



static int ExtensionCompare( const wxString& first, const wxString& second )
{
    wxFileName fn1( first );
    wxFileName fn2( second );
    wxString ext1( fn1.GetExt() );
    wxString ext2( fn2.GetExt() );

    return ext1.Cmp( ext2 );
}



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
      // Create the PlugIn icons
      m_pplugin_icon = new wxBitmap(default_pi);

      g_pi = this;              // Store a global handle to the PlugIn itself

      g_event_handler = new oesenc_pi_event_handler(this);

      wxFileName fn_exe(GetOCPN_ExePath());

      //        Specify the location of the oeserverd helper.
      g_sencutil_bin = fn_exe.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + _T("oeserverd");
      //g_sencutil_bin = _T("/home/dsr/Projects/oeserver_dp/build/oeserverd");
      

#ifdef __WXMSW__
      g_sencutil_bin = _T("\"") + fn_exe.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) +
      _T("plugins\\oesenc_pi\\oeserverd.exe\"");
#endif

#ifdef __WXOSX__
      fn_exe.RemoveLastDir();
      g_sencutil_bin = _T("\"") + fn_exe.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) +
      _T("PlugIns/oesenc_pi/oeserverd\"");
#endif

      g_bSENCutil_valid = false;                // not confirmed yet


      g_backchannel_port = 49500; //49152;       //ports 49152–65535 are unallocated

      g_pScreenLog = NULL;
      g_pPanelScreenLog = NULL;

      g_frontchannel_port = 50000;

      g_s57data_dir = *GetpSharedDataLocation();
      g_s57data_dir += _T("s57data");

      //    Get a pointer to the opencpn configuration object
      g_pconfig = GetOCPNConfigObject();

      m_up_text = NULL;
      LoadConfig();


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

      //        Set up a globally accesible string pointing to the eSENC storage location
      g_SENCdir = g_CommonDataDir;
      g_SENCdir += _T("s63SENC");


      gb_global_log = false;

}

oesenc_pi::~oesenc_pi()
{
      delete m_pplugin_icon;
      if(g_pScreenLog) {
          g_pScreenLog->Close();
          g_pScreenLog->Destroy();
          g_pScreenLog = NULL;
      }
}

int oesenc_pi::Init(void)
{
//    ScreenLogMessage( _T("s63_pi Init()\n") );

    //  Get the path of the PlugIn itself
    g_pi_filename = GetPlugInPath(this);

    AddLocaleCatalog( _T("opencpn-oesenc_pi") );

      //    Build an arraystring of dynamically loadable chart class names
    m_class_name_array.Add(_T("eSENCChart"));

#if 0
    //  Make sure the Certificate directory exists, and is populated with the most current IHO.PUB key file
    wxString dir = GetCertificateDir();

    if( !wxFileName::DirExists( dir ) ){
        wxFileName::Mkdir(dir, 0777, wxPATH_MKDIR_FULL);
    }

    wxString iho_pub = dir + wxFileName::GetPathSeparator() + _T("IHO.PUB");
    if(!::wxFileExists( iho_pub )){
        wxTextFile file(iho_pub);
        file.Create();
        file.AddLine(i0, wxTextFileType_Dos);
        file.AddLine(i1, wxTextFileType_Dos);
        file.AddLine(i2, wxTextFileType_Dos);
        file.AddLine(i3, wxTextFileType_Dos);
        file.AddLine(i4, wxTextFileType_Dos);
        file.AddLine(i5, wxTextFileType_Dos);
        file.AddLine(i6, wxTextFileType_Dos);
        file.AddLine(i7, wxTextFileType_Dos);

        file.Write();
        file.Close();
    }
#endif

    wxLogMessage(_T("Path to oeserverd is: ") + g_sencutil_bin);

    g_benable_screenlog = g_buser_enable_screenlog;
    
// #ifdef __WXMSW__
//     wxExecute(_T("net start oeserverd"));              // exec asynchronously
// #endif    
    

    return (INSTALLS_PLUGIN_CHART_GL | WANTS_PLUGIN_MESSAGING
    | WANTS_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK | WANTS_PREFERENCES );

}

bool oesenc_pi::DeInit(void)
{
    SaveConfig();
    if(g_pScreenLog) {
        g_pScreenLog->Close();
 //       delete g_pScreenLog;
//        g_pScreenLog->Destroy();
//        g_pScreenLog = NULL;
    }
    
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
}

bool oesenc_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    if(g_brendered_expired && !g_bnoShow_sse25){
        wxString msg = _("SSE 25..The ENC permit for this cell has expired.\n This cell may be out of date and MUST NOT be used for NAVIGATION.");


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
    if(g_brendered_expired && !g_bnoShow_sse25){
        wxString msg = _("SSE 25..The ENC permit for this cell has expired.\n This cell may be out of date and MUST NOT be used for NAVIGATION.");


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
    return false;
}



//      Options Dialog Page management

void oesenc_pi::OnSetupOptions(){
#if 0
    //  Create the S63 Options panel, and load it

    m_s63chartPanelWinTop = AddOptionsPage( PI_OPTIONS_PARENT_CHARTS, _("S63 Charts") );

    wxBoxSizer *chartPanelTopSizer = new wxBoxSizer( wxHORIZONTAL );
    m_s63chartPanelWinTop->SetSizer( chartPanelTopSizer );

    wxBoxSizer* chartPanelTopSizerV = new wxBoxSizer( wxVERTICAL );
    chartPanelTopSizer->Add(chartPanelTopSizerV, wxEXPAND);

    m_s63NB = new wxNotebook( m_s63chartPanelWinTop, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, wxNB_TOP );
    chartPanelTopSizerV->Add( m_s63NB, 0, wxEXPAND, 0 );

    m_s63chartPanelWin = new wxPanel(m_s63NB, wxID_ANY);
    m_s63NB->AddPage(m_s63chartPanelWin, _("Chart Cells"), true );

    wxBoxSizer *chartPanelSizer = new wxBoxSizer( wxVERTICAL );
    m_s63chartPanelWin->SetSizer( chartPanelSizer );

    int border_size = 2;

    wxBoxSizer* cmdButtonSizer = new wxBoxSizer( wxVERTICAL );
    chartPanelSizer->Add( cmdButtonSizer, 0, wxALL, border_size );

    //  Chart cell permit listbox, etc
    wxStaticBoxSizer* sbSizerLB= new wxStaticBoxSizer( new wxStaticBox( m_s63chartPanelWin, wxID_ANY, _("Installed S63 Cell Permits") ), wxVERTICAL );

    wxBoxSizer* bSizer17;
    bSizer17 = new wxBoxSizer( wxHORIZONTAL );

    m_permit_list = new OCPNPermitList( m_s63chartPanelWin );

    wxListItem col0;
    col0.SetId(0);
    col0.SetText( _("Cell Name") );
    m_permit_list->InsertColumn(0, col0);

    wxListItem col1;
    col1.SetId(1);
    col1.SetText( _("Data Server ID") );
    m_permit_list->InsertColumn(1, col1);

    wxListItem col2;
    col2.SetId(2);
    col2.SetText( _("Expiration Date") );
    m_permit_list->InsertColumn(2, col2);

    wxString permit_dir = GetPermitDir();
    m_permit_list->BuildList( permit_dir );

    bSizer17->Add( m_permit_list, 1, wxALL|wxEXPAND, 5 );

    wxBoxSizer* bSizer18;
    bSizer18 = new wxBoxSizer( wxVERTICAL );

    m_buttonImportPermit = new wxButton( m_s63chartPanelWin, wxID_ANY, _("Import Cell Permits..."), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer18->Add( m_buttonImportPermit, 0, wxALL, 5 );

    m_buttonRemovePermit = new wxButton( m_s63chartPanelWin, wxID_ANY, _("Remove Permits"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonRemovePermit->Enable( false );
    bSizer18->Add( m_buttonRemovePermit, 0, wxALL, 5 );

    m_buttonImportCells = new wxButton( m_s63chartPanelWin, wxID_ANY, _("Import Charts/Updates..."), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer18->Add( m_buttonImportCells, 0, wxALL, 5 );

    bSizer17->Add( bSizer18, 0, wxEXPAND, 5 );
    sbSizerLB->Add( bSizer17, 1, wxEXPAND, 5 );

    chartPanelSizer->Add( sbSizerLB, 0, wxEXPAND, 5 );
    chartPanelSizer->AddSpacer( 5 );


    if(g_pScreenLog) {
        g_pScreenLog->Close();
        delete g_pScreenLog;
        g_pScreenLog = NULL;
    }
    g_backchannel_port++;

    wxStaticBoxSizer* sbSizerSL= new wxStaticBoxSizer( new wxStaticBox( m_s63chartPanelWin, wxID_ANY, _("S63_pi Log") ), wxVERTICAL );

    g_pPanelScreenLog = new S63ScreenLog( m_s63chartPanelWin );
    sbSizerSL->Add( g_pPanelScreenLog, 1, wxEXPAND, 5 );

    chartPanelSizer->Add( sbSizerSL, 0, wxEXPAND, 5 );

    g_pPanelScreenLog->SetMinSize(wxSize(-1, 200));

    m_s63chartPanelWin->Layout();


    //  Build the "Keys/Permits" tab

    m_s63chartPanelKeys = new wxPanel(m_s63NB, wxID_ANY);
    m_s63NB->AddPage(m_s63chartPanelKeys, _("Keys/Permits"), false );

    wxBoxSizer *chartPanelSizerKeys = new wxBoxSizer( wxVERTICAL );
    m_s63chartPanelKeys->SetSizer( chartPanelSizerKeys );

    //  Certificate listbox, etc
    wxStaticBoxSizer* sbSizerLBCert= new wxStaticBoxSizer( new wxStaticBox( m_s63chartPanelKeys, wxID_ANY, _("Installed S63 Certificates/Keys") ), wxVERTICAL );

    wxBoxSizer* bSizer17C = new wxBoxSizer( wxHORIZONTAL );

    m_cert_list = new OCPNCertificateList( m_s63chartPanelKeys );

    wxListItem col0c;
    col0c.SetId(0);
    col0c.SetText( _("Certificate Name") );
    m_cert_list->InsertColumn(0, col0c);

    #if 0
    wxListItem col1;
    col1.SetId(1);
    col1.SetText( _("Data Server ID") );
    m_permit_list->InsertColumn(1, col1);

    wxListItem col2;
    col2.SetId(2);
    col2.SetText( _("Expiration Date") );
    m_permit_list->InsertColumn(2, col2);

    #endif

    m_cert_list->BuildList( GetCertificateDir() );


    bSizer17C->Add( m_cert_list, 1, wxALL|wxFIXED_MINSIZE, 5 );

    wxBoxSizer* bSizer18C = new wxBoxSizer( wxVERTICAL );

    m_buttonImportCert = new wxButton( m_s63chartPanelKeys, wxID_ANY, _("Import Certificate..."), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer18C->Add( m_buttonImportCert, 0, wxALL, 5 );

    bSizer17C->Add( bSizer18C, 0, wxEXPAND, 5 );
    sbSizerLBCert->Add( bSizer17C, 1, wxEXPAND, 5 );

    chartPanelSizerKeys->Add( sbSizerLBCert, 0, wxEXPAND, 5 );
    chartPanelSizerKeys->AddSpacer( 5 );

    //  User Permit
    wxStaticBoxSizer* sbSizerUP= new wxStaticBoxSizer( new wxStaticBox( m_s63chartPanelKeys, wxID_ANY, _("UserPermit") ), wxHORIZONTAL );
    m_up_text = new wxStaticText(m_s63chartPanelKeys, wxID_ANY, _T(""));

    if(g_userpermit.Len())
        m_up_text->SetLabel( GetUserpermit() );
    sbSizerUP->Add(m_up_text, wxEXPAND);

    m_buttonNewUP = new wxButton( m_s63chartPanelKeys, wxID_ANY, _("New Userpermit..."), wxDefaultPosition, wxDefaultSize, 0 );
    sbSizerUP->Add( m_buttonNewUP, 0, wxALL | wxALIGN_RIGHT, 5 );

    chartPanelSizerKeys->AddSpacer( 5 );
    chartPanelSizerKeys->Add( sbSizerUP, 0, wxEXPAND, 5 );

    //  Install Permit
    wxStaticBoxSizer* sbSizerIP= new wxStaticBoxSizer( new wxStaticBox( m_s63chartPanelKeys, wxID_ANY, _("InstallPermit") ), wxHORIZONTAL );
    m_ip_text = new wxStaticText(m_s63chartPanelKeys, wxID_ANY, _T(""));

    //if(g_installpermit.Len())
    //    m_ip_text->SetLabel( GetInstallpermit() );
    sbSizerIP->Add(m_ip_text, wxEXPAND);

    m_buttonNewIP = new wxButton( m_s63chartPanelKeys, wxID_ANY, _("New Installpermit..."), wxDefaultPosition, wxDefaultSize, 0 );
    sbSizerIP->Add( m_buttonNewIP, 0, wxALL | wxALIGN_RIGHT, 5 );

    chartPanelSizerKeys->AddSpacer( 5 );
    chartPanelSizerKeys->Add( sbSizerIP, 0, wxEXPAND, 5 );

    //  FPR File Permit
    wxStaticBoxSizer* sbSizerFPR= new wxStaticBoxSizer( new wxStaticBox( m_s63chartPanelKeys, wxID_ANY, _("System Identification") ), wxHORIZONTAL );
    m_fpr_text = new wxStaticText(m_s63chartPanelKeys, wxID_ANY, _T(" "));
    if(g_fpr_file.Len())
        m_fpr_text->SetLabel( g_fpr_file );
    sbSizerFPR->Add(m_fpr_text, wxEXPAND);

    m_buttonNewFPR = new wxButton( m_s63chartPanelKeys, wxID_ANY, _("Create System Identifier file..."), wxDefaultPosition, wxDefaultSize, 0 );
    sbSizerFPR->Add( m_buttonNewFPR, 0, wxALL | wxALIGN_RIGHT, 5 );

    chartPanelSizerKeys->AddSpacer( 5 );
    chartPanelSizerKeys->Add( sbSizerFPR, 0, wxEXPAND, 5 );



    chartPanelSizerKeys->AddSpacer( 15 );
    wxStaticLine *psl = new wxStaticLine(m_s63chartPanelKeys, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    chartPanelSizerKeys->Add( psl, 0, wxEXPAND, 5 );
    chartPanelSizerKeys->AddSpacer( 15 );

    m_cert_list->SetMinSize(wxSize(-1, 80));
    m_s63chartPanelKeys->Layout();



    //  Connect to Events
    m_buttonImportPermit->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(s63_pi_event_handler::OnImportPermitClick), NULL, m_event_handler );

    m_buttonRemovePermit->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(s63_pi_event_handler::OnRemovePermitClick), NULL, m_event_handler );

    m_buttonImportCells->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                                   wxCommandEventHandler(s63_pi_event_handler::OnImportCellsClick), NULL, m_event_handler );

    m_buttonNewUP->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(s63_pi_event_handler::OnNewUserpermitClick), NULL, m_event_handler );

    m_buttonNewIP->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                            wxCommandEventHandler(s63_pi_event_handler::OnNewInstallpermitClick), NULL, m_event_handler );

    m_permit_list->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED,
                  wxListEventHandler( s63_pi_event_handler::OnSelectPermit ), NULL, m_event_handler );

    m_buttonImportCert->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                                  wxCommandEventHandler(s63_pi_event_handler::OnImportCertClick), NULL, m_event_handler );

    m_buttonNewFPR->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                            wxCommandEventHandler(s63_pi_event_handler::OnNewFPRClick), NULL, m_event_handler );


    g_benable_screenlog = true;

    m_buttonImportPermit->SetFocus();
#endif
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
                wxString msg1 = _("Fingerprint file created.\n");
                msg1 += fpr_file;
                
                OCPNMessageBox_PlugIn(NULL, msg1, _("S63_PI Message"), wxOK);
            }
            else{
                wxLogMessage(_T("S63_pi: OCPNsenc results:"));
                for(unsigned int i=0 ; i < valup_result.GetCount() ; i++){
                    wxString line = valup_result[i];
                    wxLogMessage( line );
                }
                OCPNMessageBox_PlugIn(NULL, _T("ERROR Creating Fingerprint file\n Check OpenCPN log file."), _("S63_PI Message"), wxOK);
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
}


int oesenc_pi::pi_error( wxString msg )
{
    return 0;
}

bool oesenc_pi::LoadConfig( void )
{
    wxFileConfig *pConf = (wxFileConfig *) g_pconfig;

    if( pConf ) {
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
//         pConf->Read( _T("LastFPRFile"), &g_fpr_file);
        
        pConf->Read( _T("UserKey"), &g_UserKey );
        
    }

    return true;
}

bool oesenc_pi::SaveConfig( void )
{
    wxFileConfig *pConf = (wxFileConfig *) g_pconfig;

    if( pConf ) {
        pConf->SetPath( _T("/PlugIns/oesenc") );

        pConf->Write( _T("UserKey"), g_UserKey );

    }

    return true;
}

void oesenc_pi::ShowPreferencesDialog( wxWindow* parent )
{
    g_prefs_dialog = new oesencPrefsDialog( parent, wxID_ANY, _("oeSENC_PI Preferences"), wxPoint( 20, 20), wxDefaultSize, wxDEFAULT_DIALOG_STYLE );
    g_prefs_dialog->Fit();
    g_prefs_dialog->SetSize(wxSize(300, -1));
    wxColour cl;
    GetGlobalColor(_T("DILG1"), &cl);
    g_prefs_dialog->SetBackgroundColour(cl);
    
    
    g_prefs_dialog->Show();
        
    if(g_prefs_dialog->ShowModal() == wxID_OK)
    {
        SaveConfig();
        
    }
    delete g_prefs_dialog;
}

#if 0
void oesenc_pi::GetNewUserpermit(void)
{
    g_old_userpermit = g_userpermit;

    g_userpermit = _T("");
    wxString new_permit = GetUserpermit();

    if( new_permit != _T("Invalid")){
        g_userpermit = new_permit;
        g_pi->SaveConfig();

        if(m_up_text) {
            m_up_text->SetLabel( g_userpermit );
        }
    }
    else
        g_userpermit = g_old_userpermit;

}


void oesenc_pi::GetNewInstallpermit(void)
{
    g_old_installpermit = g_installpermit;

    g_installpermit = _T("");
    wxString new_permit = GetInstallpermit();

    if( new_permit != _T("Invalid")){
        g_installpermit = new_permit;
        g_pi->SaveConfig();

        if(m_ip_text) {
            m_ip_text->SetLabel( g_installpermit );
        }
    }
    else
        g_installpermit = g_old_installpermit;

}

#endif

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
        m_plogtc->AppendText(_("S63_pi backchannel could not listen at the specified port !\n"));
    }
    else
    {
        m_plogtc->AppendText(_("S63_pi backchannel server listening.\n\n"));
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
    wxString s; // = _("OnServerEvent: ");
    wxSocketBase *sock;

    switch(event.GetSocketEvent())
    {
        case wxSOCKET_CONNECTION :
//            s.Append(_("wxSOCKET_CONNECTION\n"));
            break;
        default                  :
            s.Append(_("Unexpected event !\n"));
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
//       m_plogtc->AppendText(_("New client connection accepted\n\n"));
    }
    else
    {
        m_plogtc->AppendText(_("Error: couldn't accept a new connection\n\n"));
        return;
    }

    sock->SetEventHandler(*this, SOCKET_ID);
    sock->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
    sock->Notify(true);
    sock->SetFlags(wxSOCKET_BLOCK);


}

void S63ScreenLog::OnSocketEvent(wxSocketEvent& event)
{
    wxString s; // = _("OnSocketEvent: ");
    wxSocketBase *sock = event.GetSocket();

    // First, print a message
    switch(event.GetSocketEvent())
    {
        case wxSOCKET_INPUT :
//            s.Append(_("wxSOCKET_INPUT\n"));
            break;
        case wxSOCKET_LOST  :
//            s.Append(_("wxSOCKET_LOST\n"));
            break;
        default             :
            s.Append(_("Unexpected event !\n"));
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

//                    m_plogtc->AppendText(_("Deleting socket.\n\n"));

                    sock->Destroy();
                    break;
                }
                default: ;
    }

}

//-------------------------------------------------------------------------------------
//
//      OCPNPermitList implementation
//      List control for management of cell permits
//
//-------------------------------------------------------------------------------------

OCPNPermitList::OCPNPermitList(wxWindow *parent)
{
    Create( parent, -1, wxDefaultPosition, wxSize(-1, 200), wxLC_REPORT | wxLC_HRULES );

}

OCPNPermitList::~OCPNPermitList()
{
}

void OCPNPermitList::BuildList( const wxString &permit_dir )
{

    DeleteAllItems();

    if( wxDir::Exists(permit_dir) ){

        m_permit_file_array.Clear();
        wxArrayString file_array;
        size_t nfiles = wxDir::GetAllFiles(permit_dir, &file_array, _T("*.os63"));

        for (size_t i = 0; i < nfiles; i++){
            wxTextFile file(file_array[i]);
            if(file.Open()){
                wxString line = file.GetFirstLine();

                while( !file.Eof() ){
                    if(line.StartsWith( _T("cellpermit" ) ) ) {

                        //      Keep an array of file names, store index in item
                        //      May be useful for list managment, e.g.item deletion
                        int pfa_index = m_permit_file_array.Add( file_array[i] );

                        wxString permit_string = line.Mid(11);

                        wxListItem li;
                        li.SetId( i );
                        li.SetData( pfa_index );
                        li.SetText( _T("") );

                        long itemIndex = InsertItem( li );

                        SetItem(itemIndex, 0, permit_string.Mid(0,8));

                        wxString sdate = permit_string.Mid(8, 8);
                        wxDateTime exdate;
                        exdate.ParseFormat(sdate, _T("%Y%m%d"));

                        wxString fdate = exdate.FormatDate();

                        wxStringTokenizer tkz(line.AfterFirst(':'), _T(",") );
                        wxString token = tkz.GetNextToken();
                        token = tkz.GetNextToken();
                        token = tkz.GetNextToken();
                        token = tkz.GetNextToken();             // Data server ID

                        //      Set Data Server ID string
                        SetItem(itemIndex, 1, token);


                        //TODO why, on GTK, can I not set an item/column colour?
                        wxListItem lid;
                        lid.SetId( itemIndex );
                        lid.SetColumn(2);
//                        lid.SetTextColour(*wxRED );
                        lid.SetText(fdate);
                        SetItem(lid);

//                       SetItemTextColour(itemIndex, *wxRED);

                        break;

                    }
                    else
                        line = file.GetNextLine();
                }
            }
        }
    }



#ifdef __WXOSX__
    SetColumnWidth( 0, wxLIST_AUTOSIZE );
    SetColumnWidth( 1, wxLIST_AUTOSIZE );
    SetColumnWidth( 2, wxLIST_AUTOSIZE );
#else
    SetColumnWidth( 0, wxLIST_AUTOSIZE_USEHEADER );
    SetColumnWidth( 1, wxLIST_AUTOSIZE_USEHEADER );
    SetColumnWidth( 2, wxLIST_AUTOSIZE_USEHEADER );
#endif


}

//-------------------------------------------------------------------------------------
//
//      OCPNCertificate implementation
//      List control for management of certificates/public keys
//
//-------------------------------------------------------------------------------------

OCPNCertificateList::OCPNCertificateList(wxWindow *parent)
{
    Create( parent, -1, wxDefaultPosition, wxSize(-1, 100), wxLC_REPORT | wxLC_HRULES);

}

OCPNCertificateList::~OCPNCertificateList()
{
}

void OCPNCertificateList::BuildList( const wxString &cert_dir )
{

    DeleteAllItems();

    if( wxDir::Exists(cert_dir) ){

 //       m_permit_file_array.Clear();
        wxArrayString file_array;
        size_t nfiles = wxDir::GetAllFiles(cert_dir, &file_array, _T("*.PUB"));

        for (size_t i = 0; i < nfiles; i++){
            wxTextFile file(file_array[i]);
            if(file.Open()){
                wxString line = file.GetFirstLine();

                while( !file.Eof() ){
                    if(line.Upper().Find( _T("BIG" ) ) != wxNOT_FOUND ) {

                        //      Keep an array of file names, store index in item
                        //      May be useful for list managment, e.g.item deletion
//                        int pfa_index = m_permit_file_array.Add( file_array[i] );

//                        wxString permit_string = line.Mid(11);

                        wxListItem li;
                        li.SetId( i );
//                        li.SetData( pfa_index );
                        li.SetText( _T("") );

                        long itemIndex = InsertItem( li );

                        wxFileName fn( file_array[i] );

                        SetItem(itemIndex, 0, fn.GetFullName());

#if 0
                        wxString sdate = permit_string.Mid(8, 8);
                        wxDateTime exdate;
                        exdate.ParseFormat(sdate, _T("%Y%m%d"));

                        wxString fdate = exdate.FormatDate();

                        wxStringTokenizer tkz(line.AfterFirst(':'), _T(",") );
                        wxString token = tkz.GetNextToken();
                        token = tkz.GetNextToken();
                        token = tkz.GetNextToken();
                        token = tkz.GetNextToken();             // Data server ID

                        //      Set Data Server ID string
                        SetItem(itemIndex, 1, token);


                        //TODO why, on GTK, can I not set an item/column colour?
                        wxListItem lid;
                        lid.SetId( itemIndex );
                        lid.SetColumn(2);
                        //                        lid.SetTextColour(*wxRED );
                        lid.SetText(fdate);
                        SetItem(lid);

                        //                       SetItemTextColour(itemIndex, *wxRED);
#endif
                        break;

                    }
                    else
                        line = file.GetNextLine();
                }
            }
        }
    }



    #ifdef __WXOSX__
    SetColumnWidth( 0, wxLIST_AUTOSIZE );
//    SetColumnWidth( 1, wxLIST_AUTOSIZE );
//    SetColumnWidth( 2, wxLIST_AUTOSIZE );
    #else
    SetColumnWidth( 0, wxLIST_AUTOSIZE_USEHEADER );
//    SetColumnWidth( 1, wxLIST_AUTOSIZE_USEHEADER );
//    SetColumnWidth( 2, wxLIST_AUTOSIZE_USEHEADER );
    #endif


}






/*!
 * SENCGetUserpermitDialog type definition
 */

IMPLEMENT_DYNAMIC_CLASS( SENCGetUserpermitDialog, wxDialog )
/*!
 * SENCGetUserpermitDialog event table definition
 */BEGIN_EVENT_TABLE( SENCGetUserpermitDialog, wxDialog )

 ////@begin SENCGetUserpermitDialog event table entries

 EVT_BUTTON( ID_GETUP_CANCEL, SENCGetUserpermitDialog::OnCancelClick )
 EVT_BUTTON( ID_GETUP_OK, SENCGetUserpermitDialog::OnOkClick )
 EVT_BUTTON( ID_GETUP_TEST, SENCGetUserpermitDialog::OnTestClick )
 EVT_TEXT(ID_GETUP_UP, SENCGetUserpermitDialog::OnUpdated)

 ////@end SENCGetUserpermitDialog event table entries

 END_EVENT_TABLE()

 /*!
  * SENCGetUserpermitDialog constructors
  */

 SENCGetUserpermitDialog::SENCGetUserpermitDialog()
 {
 }

 SENCGetUserpermitDialog::SENCGetUserpermitDialog( wxWindow* parent, wxWindowID id, const wxString& caption,
                                         const wxPoint& pos, const wxSize& size, long style )
 {

     long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
     wxDialog::Create( parent, id, caption, pos, size, wstyle );

     SetTitle( _("S63_pi Userpermit Required"));

     CreateControls();
     GetSizer()->SetSizeHints( this );
     Centre();

 }

 SENCGetUserpermitDialog::~SENCGetUserpermitDialog()
 {
     delete m_PermitCtl;
 }

 /*!
  * SENCGetUserpermitDialog creator
  */

 bool SENCGetUserpermitDialog::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                  const wxPoint& pos, const wxSize& size, long style )
 {
     SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
     long wstyle = style;
#ifdef __WXMAC__
     wstyle |= wxSTAY_ON_TOP;
#endif

     wxDialog::Create( parent, id, caption, pos, size, wstyle );

     CreateControls();
     GetSizer()->SetSizeHints( this );
     Centre();

     return TRUE;
 }

 /*!
  * Control creation for SENCGetUserpermitDialog
  */

 void SENCGetUserpermitDialog::CreateControls()
 {
     SENCGetUserpermitDialog* itemDialog1 = this;

     wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
     itemDialog1->SetSizer( itemBoxSizer2 );

     wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( itemDialog1, wxID_ANY,
                                                               _("Enter Userpermit") );

     wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer( itemStaticBoxSizer4Static,
                                                                   wxVERTICAL );
     itemBoxSizer2->Add( itemStaticBoxSizer4, 0, wxEXPAND | wxALL, 5 );

     wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _T(""),
     wxDefaultPosition, wxDefaultSize, 0 );
     itemStaticBoxSizer4->Add( itemStaticText5, 0,
                               wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP | wxADJUST_MINSIZE, 5 );

     m_PermitCtl = new wxTextCtrl( itemDialog1, ID_GETUP_UP, _T(""), wxDefaultPosition,
     wxSize( 180, -1 ), 0 );
     itemStaticBoxSizer4->Add( m_PermitCtl, 0,
                               wxALIGN_LEFT | wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5 );

     wxBoxSizer* itemBoxSizerTest = new wxBoxSizer( wxVERTICAL );
     itemBoxSizer2->Add( itemBoxSizerTest, 0, wxALIGN_LEFT | wxALL | wxEXPAND, 5 );

     m_testBtn = new wxButton(itemDialog1, ID_GETUP_TEST, _("Test Userpermit"));
     m_testBtn->Disable();
     itemBoxSizerTest->Add( m_testBtn, 0, wxALIGN_LEFT | wxALL, 5 );

     wxStaticBox* itemStaticBoxTestResults = new wxStaticBox( itemDialog1, wxID_ANY,
                                                                  _("Test Results"), wxDefaultPosition, wxSize(-1, 40) );

     wxStaticBoxSizer* itemStaticBoxSizerTest = new wxStaticBoxSizer( itemStaticBoxTestResults,  wxHORIZONTAL );
     itemBoxSizerTest->Add( itemStaticBoxSizerTest, 0,  wxALIGN_RIGHT | wxALL | wxEXPAND, 5 );


     m_TestResult = new wxStaticText( itemDialog1, -1, _T(""), wxDefaultPosition, wxSize( -1, -1 ), 0 );

     itemStaticBoxSizerTest->Add( m_TestResult, 0, wxALIGN_LEFT | wxALL | wxEXPAND, 5 );



     wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
     itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );

     m_CancelButton = new wxButton( itemDialog1, ID_GETUP_CANCEL, _("Cancel"), wxDefaultPosition,
     wxDefaultSize, 0 );
     itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
     m_CancelButton->SetDefault();

     m_OKButton = new wxButton( itemDialog1, ID_GETUP_OK, _("OK"), wxDefaultPosition,
     wxDefaultSize, 0 );
     itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

     m_OKButton->Disable();

     m_PermitCtl->AppendText(g_old_userpermit);

 }


 bool SENCGetUserpermitDialog::ShowToolTips()
 {
     return TRUE;
 }

 void SENCGetUserpermitDialog::OnTestClick( wxCommandEvent& event )
 {
     wxString cmd;
     cmd += _T(" -y ");                  // validate Userpermit

     cmd += _T(" -u ");
     cmd += m_PermitCtl->GetValue();

     wxArrayString valup_result = exec_SENCutil_sync( cmd, false);

     bool berr = false;
     for(unsigned int i=0 ; i < valup_result.GetCount() ; i++){
         wxString line = valup_result[i];
         if(line.Upper().Find(_T("ERROR")) != wxNOT_FOUND){
             if( line.Upper().Find(_T("S63_PI")) != wxNOT_FOUND)  {
                 m_TestResult->SetLabel(line.Trim());
             }
             else {
                m_TestResult->SetLabel(_("Userpermit invalid"));
             }
             berr = true;
             m_OKButton->Disable();
             break;
         }
     }
     if(!berr){
         m_TestResult->SetLabel(_("Userpermit OK"));
         m_OKButton->Enable();
     }
 }

 void SENCGetUserpermitDialog::OnCancelClick( wxCommandEvent& event )
 {
    EndModal(2);
 }

 void SENCGetUserpermitDialog::OnOkClick( wxCommandEvent& event )
 {
     if( m_PermitCtl->GetValue().Length() == 0 )
         EndModal(1);
     else {
        g_userpermit = m_PermitCtl->GetValue();
        g_pi->SaveConfig();

        EndModal(0);
     }
 }

 void SENCGetUserpermitDialog::OnUpdated( wxCommandEvent& event )
 {
     if( m_PermitCtl->GetValue().Length() )
         m_testBtn->Enable();
     else
         m_testBtn->Disable();
 }







wxString GetUserpermit(void)
{
    if(g_userpermit.Len())
        return g_userpermit;
    else {
        SENCGetUserpermitDialog dlg(NULL);
        dlg.SetSize(500,-1);
        dlg.Centre();
        int ret = dlg.ShowModal();
        if(ret == 0)
            return g_userpermit;
        else
            return _T("Invalid");
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

     SetTitle( _("OpenCPN SENC UserKey Required"));

     CreateControls( legendID );
     GetSizer()->SetSizeHints( this );
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
     itemStaticBoxSizer4->Add( itemStaticText5, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP | wxADJUST_MINSIZE, 5 );

     m_UserKeyCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition,
     wxSize( 180, -1 ), 0 );
     itemStaticBoxSizer4->Add( m_UserKeyCtl, 0,  wxALIGN_LEFT | wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5 );

     wxStaticText *itemStaticTextLegend = NULL;
     switch(legendID){
         case LEGEND_NONE:
             break;
             
         case LEGEND_FIRST:
             itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,
_("A valid OESENC UserKey has the alphanumeric format:  AAAA-BBBB-CCCC-DDDD-EEEE-FF\n\n\
Your OESENC UserKey may be obtained from your chart provider."),
                                                      wxDefaultPosition, wxDefaultSize, 0);
             break;
         case LEGEND_SECOND:
             itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,
_("ERROR: The UserKey entered is not valid for this OESENC chart set.\n\
Please verify your UserKey and try again.\n\n\
A valid OESENC UserKey has the alphanumeric format:  AAAA-BBBB-CCCC-DDDD-EEEE-FF\n\
Your OESENC UserKey may be obtained from your chart provider.\n\n"),
                                                      wxDefaultPosition, wxDefaultSize, 0);
             break;
         case LEGEND_THIRD:
             itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,
_("ERROR: The UserKey entered is not valid for this OESENC chart set.\n\n\
OESENC charts will be disabled for this session.\n\
Please verify your UserKey and restart OpenCPN.\n\n\
Your OESENC UserKey may be obtained from your chart provider.\n\n"),
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
         itemBoxSizer2->Add( itemStaticTextLegend, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP | wxADJUST_MINSIZE, 5 );
     }
         
                                       
#if 0     
     wxBoxSizer* itemBoxSizerTest = new wxBoxSizer( wxVERTICAL );
     itemBoxSizer2->Add( itemBoxSizerTest, 0, wxALIGN_LEFT | wxALL | wxEXPAND, 5 );

     m_testBtn = new wxButton(itemDialog1, ID_GETIP_TEST, _("Test Installpermit"));
     m_testBtn->Disable();
     itemBoxSizerTest->Add( m_testBtn, 0, wxALIGN_LEFT | wxALL, 5 );

     wxStaticBox* itemStaticBoxTestResults = new wxStaticBox( itemDialog1, wxID_ANY,
                                                                  _("Test Results"), wxDefaultPosition, wxSize(-1, 40) );

     wxStaticBoxSizer* itemStaticBoxSizerTest = new wxStaticBoxSizer( itemStaticBoxTestResults,  wxHORIZONTAL );
     itemBoxSizerTest->Add( itemStaticBoxSizerTest, 0,  wxALIGN_RIGHT |wxALL | wxEXPAND, 5 );


     m_TestResult = new wxStaticText( itemDialog1, -1, _T(""), wxDefaultPosition, wxSize( -1, -1 ), 0 );

     itemStaticBoxSizerTest->Add( m_TestResult, 0, wxALIGN_LEFT | wxALL | wxEXPAND, 5 );

#endif
     wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
     itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );

     m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition,
     wxDefaultSize, 0 );
     itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
     m_CancelButton->SetDefault();

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
         SENCGetUserKeyDialog dlg( legendID, NULL);
         dlg.SetSize(500,-1);
         dlg.Centre();
         
         int ret = dlg.ShowModal();
         if(ret == 0)
             return g_UserKey;
         else
             return _T("Invalid");
     }
 }
 
 
bool validateUserKey( wxString sencFileName)
{
    wxLogMessage(_T("validateUserKey"));
    
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
        wxLogMessage(_T("validateUserKey E1"));
        
    
        if(( ERROR_SIGNATURE_FAILURE == retCode )  || ( ERROR_SENC_CORRUPT == retCode ) ){
        
            wxLogMessage(_T("validateUserKey E2"));
            //  On a signature error, we try once more, allowing user to enter a new key
            wxString key = GetUserKey( LEGEND_SECOND, true );
            senc.setKey(key);
            int retCode_retry = senc.ingestHeader( sencFileName );
            if(retCode_retry != SENC_NO_ERROR){
                GetUserKey( LEGEND_THIRD, true );                  // Bail out
                return false;
            }
            else{
                wxLogMessage(_T("validateUserKey E3"));
                
                if(!b_Set)
                    GetUserKey( LEGEND_FOURTH, true );                  // Inform the user
                g_UserKey = key;
            }
        }
    }
    else{
        wxLogMessage(_T("validateUserKey E4"));
        
        if(!b_Set)
            GetUserKey( LEGEND_FOURTH, true );                  // Inform the user
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


void initLibraries(void)
{
    // General variables
    g_overzoom_emphasis_base = 0;
    g_oz_vector_scale = false;
    g_ChartScaleFactorExp = 1;
    
    // OpenGL variables
    
    char *p = (char *) glGetString( GL_EXTENSIONS );
    if( NULL == p )
        pi_bopengl = false;
    else
        pi_bopengl = true;
    
    
    g_b_EnableVBO = false;
    g_GLMinCartographicLineWidth = 1.0;
    g_GLMinSymbolLineWidth = 1.0;
    
    if( QueryExtension( "GL_ARB_texture_non_power_of_two" ) )
        g_texture_rectangle_format = GL_TEXTURE_2D;
    else if( QueryExtension( "GL_OES_texture_npot" ) )
        g_texture_rectangle_format = GL_TEXTURE_2D;
    else if( QueryExtension( "GL_ARB_texture_rectangle" ) )
        g_texture_rectangle_format = GL_TEXTURE_RECTANGLE_ARB;
    
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

//      Set up a useable CPL library error handler for S57 stuff
    ///TODO???CPLSetErrorHandler( MyCPLErrorHandler );

//      If the config file contains an entry for PresentationLibraryData, use it.
//      Otherwise, default to conditionally set spot under g_pcsv_locn
    wxString plib_data;
    bool b_force_legacy = false;

//     if( g_UserPresLibData.IsEmpty() ) {
//         plib_data = g_csv_locn;
//         appendOSDirSlash( &plib_data );
//         plib_data.Append( _T("S52RAZDS.RLE") );
//     } else {
//         plib_data = g_UserPresLibData;
//         b_force_legacy = true;
//     }

    plib_data = *GetpSharedDataLocation();
    plib_data += _T("s57data/"); //TODO use sep
    
    ps52plib = new s52plib( plib_data, b_force_legacy );


    if( ps52plib->m_bOK ) {
//         wxLogMessage( _T("Using s57data in ") + g_csv_locn );
//         m_pRegistrarMan = new s57RegistrarMgr( g_csv_locn, g_Platform->GetLogFilePtr() );


            //    Preset some object class visibilites for "Mariner's Standard" disply category
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
            int display_size_mm = wxGetDisplaySizeMM().GetWidth();
            
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


bool validate_SENC_server(void)
{
// #ifdef __WXMSW    
//     return true;        // started as service earlier
// #endif    

 printf("      validate_SENC_server\n");
    wxLogMessage(_T("validate_SENC_server"));
    
    // Check to see if the server is already running, and available
    Osenc_instream testAvail;
    if(testAvail.isAvailable()){
        wxLogMessage(_T("Available TRUE"));
        return true;
    }

    printf("      validate_SENC_server, retry\n");
    wxLogMessage(_T("Available FALSE, retry..."));
    wxMilliSleep(500);
    if(testAvail.isAvailable()){
        wxLogMessage(_T("Available TRUE"));
        return true;
    }
    
    
    // Not running, so start it up...
    
    //Verify that oeserverd actually exists, and runs.
    wxString bin_test = g_sencutil_bin;
    
    if(wxNOT_FOUND != g_sencutil_bin.Find('\"'))
        bin_test = g_sencutil_bin.Mid(1).RemoveLast();
    
    wxString msg = _("Checking oeserverd utility at ");
    msg += _T("{");
    msg += bin_test;
    msg += _T("}");
    wxLogMessage(_T("oesenc_pi: ") + msg);
    
    
    if(!::wxFileExists(bin_test)){
        wxString msg = _("Cannot find the oserverd utility at \n");
        msg += _T("{");
        msg += bin_test;
        msg += _T("}");
        OCPNMessageBox_PlugIn(NULL, msg, _("oesenc_pi Message"),  wxOK, -1, -1);
        wxLogMessage(_T("oesenc_pi: ") + msg);
        
        g_sencutil_bin.Clear();
        return false;
    }

    wxString ver_line;
#if 0    
    //Will it run?
    wxArrayString ret_array;
    ret_array.Alloc(1000);
    wxString cmd = g_sencutil_bin;
    cmd += _T(" -a");                 // get version
    long rv = wxExecute(cmd, ret_array, ret_array, wxEXEC_SYNC );
    
    if(0 != rv) {
        wxString msg = _("Cannot execute oeserverd utility at \n");
        msg += _T("{");
        msg += bin_test;
        msg += _T("}");
        OCPNMessageBox_PlugIn(NULL, msg, _("oesenc_pi Message"),  wxOK, -1, -1);
        wxLogMessage(_T("oesenc_pi: ") + msg);
        
        g_sencutil_bin.Clear();
        return false;
    }
    
    // Check results
    bool bad_ver = false;
    for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
        wxString line = ret_array[i];
        if(ret_array[i].Upper().Find(_T("VERSION")) != wxNOT_FOUND){
            ver_line = line;
            wxStringTokenizer tkz(line, _T(" ")); 
            while ( tkz.HasMoreTokens() ){
                wxString token = tkz.GetNextToken();
                double ver;
                if(token.ToDouble(&ver)){
                    if( ver < 1.00)
                        bad_ver = true;
                }
            }
        }
    }
    
    if(!ver_line.Length())                    // really old version.
          bad_ver = true;
    
    
    if(bad_ver) {
        wxString msg = _("oeserverd utility at \n");
        msg += _T("{");
        msg += bin_test;
        msg += _T("}\n");
        msg += _(" is incorrect version, reports as:\n\n");
        msg += ver_line;
        msg += _T("\n\n");
        wxString msg1;
        msg1.Printf(_("This version of oesenc_PI requires oeserverd of version 1.00 or later."));
        msg += msg1;
        OCPNMessageBox_PlugIn(NULL, msg, _("oesenc_pi Message"),  wxOK, -1, -1);
        wxLogMessage(_T("oesenc_pi: ") + msg);
        
        g_sencutil_bin.Clear();
        return false;
        
    }
#endif

    // now start the server...
    wxString cmds = g_sencutil_bin;

#ifndef __WXMSW__    
    cmds += _T(" --daemon");
#endif

    int flags = wxEXEC_ASYNC;
#ifdef __WXMSW__    
	flags |= wxEXEC_HIDE_CONSOLE;
#endif
    
    g_serverProc = wxExecute(cmds, flags);              // exec asynchronously
    wxMilliSleep(100);
    
    // Check to see if the server function is available
    if(g_serverProc){
        bool bAvail = false;
        int nLoop = 10;
        Osenc_instream testAvail_One;

        while(nLoop){
            if(!testAvail_One.isAvailable())
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
            OCPNMessageBox_PlugIn(NULL, msg, _("oesenc_pi Message"),  wxOK, -1, -1);
            wxLogMessage(_T("oesenc_pi: ") + msg);
            
            g_sencutil_bin.Clear();
            return false;
            
        }
        else{
            wxLogMessage(_T("oesenc_pi: oeserverd Check OK...") + ver_line);
        }
    }
    else{
        wxString msg = _("oeserverd utility at \n");
        msg += _T("{");
        msg += bin_test;
        msg += _T("}\n");
        msg += _(" could not be started.\n\n");
        OCPNMessageBox_PlugIn(NULL, msg, _("oesenc_pi Message"),  wxOK, -1, -1);
        wxLogMessage(_T("oesenc_pi: ") + msg);
        
        g_sencutil_bin.Clear();
        return false;
    }
        
    
    return true;
}

bool shutdown_SENC_server( void )
{
// #ifdef __WXMSW__
//     wxExecute(_T("net stop oeserverd"));              // exec asynchronously
//     return true;
// #else    
    
    // Check to see if the server is already running, and available
    Osenc_instream testAvail;
    if(1/*testAvail.isAvailable()*/){
        testAvail.Shutdown();
        return true;
    }
    else{
        return false;
    }
// #endif    
    
}


BEGIN_EVENT_TABLE( oesencPrefsDialog, wxDialog )
EVT_BUTTON( wxID_OK, oesencPrefsDialog::OnPrefsOkClick )
END_EVENT_TABLE()

oesencPrefsDialog::oesencPrefsDialog( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
    
        this->SetSizeHints( wxDefaultSize, wxDefaultSize );
        
        wxBoxSizer* bSizer2;
        bSizer2 = new wxBoxSizer( wxVERTICAL );
        
        //  FPR File Permit
//        wxStaticBoxSizer* sbSizerFPR= new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("System Identification") ), wxHORIZONTAL );
//         m_fpr_text = new wxStaticText(this, wxID_ANY, _T(" "));
//         if(g_fpr_file.Len())
//             m_fpr_text->SetLabel( g_fpr_file );
//         else
//             m_fpr_text->SetLabel( _T("                  "));
//         
//         sbSizerFPR->Add(m_fpr_text, wxEXPAND);
        
        m_buttonNewFPR = new wxButton( this, wxID_ANY, _("Create System Identifier file..."), wxDefaultPosition, wxDefaultSize, 0 );
//        sbSizerFPR->Add( m_buttonNewFPR, 0, wxALL | wxALIGN_RIGHT, 5 );
        
        bSizer2->AddSpacer( 20 );
        bSizer2->Add( m_buttonNewFPR, 0, wxEXPAND, 50 );
        
        m_buttonNewFPR->Connect( wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(oesenc_pi_event_handler::OnNewFPRClick), NULL, g_event_handler );
        
        

        m_sdbSizer1 = new wxStdDialogButtonSizer();
//         m_sdbSizer1OK = new wxButton( this, wxID_OK );
//         m_sdbSizer1->AddButton( m_sdbSizer1OK );
        m_sdbSizer1Cancel = new wxButton( this, wxID_CANCEL );
        m_sdbSizer1->AddButton( m_sdbSizer1Cancel );
        m_sdbSizer1->Realize();
        
        bSizer2->Add( m_sdbSizer1, 0, wxBOTTOM|wxEXPAND|wxTOP, 5 );
        
        
        this->SetSizer( bSizer2 );
        this->Layout();
        bSizer2->Fit( this );
        
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

// An Event handler class to catch events from UI dialog
//      Implementation

oesenc_pi_event_handler::oesenc_pi_event_handler(oesenc_pi *parent)
{
    m_parent = parent;
}

oesenc_pi_event_handler::~oesenc_pi_event_handler()
{
}





void oesenc_pi_event_handler::OnNewFPRClick( wxCommandEvent &event )
{
    
    wxString msg = _("To obtain a User Key, you must generate a unique System Identifier File.\n");
    msg += _("This file is also known as a\"fingerprint\" file.\n");
    msg += _("The fingerprint file contains information to uniquely identifiy this computer.\n\n");
    msg += _("After creating this file, you will need it to obtain your User Key at the o-charts.org shop.\n\n");
    msg += _("Proceed to create Fingerprint file?");
    
    int ret = OCPNMessageBox_PlugIn(NULL, msg, _("S63_PI Message"), wxYES_NO);
    
    if(ret == wxID_YES){
       
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
            cmd += _T(" -f ");                  // Make fingerprint
            
            cmd += fpr_dir;
            
            ::wxBeginBusyCursor();
            
            wxArrayString ret_array;      
            long rv = wxExecute(cmd, ret_array, ret_array );
            
            ::wxEndBusyCursor();
            
            bool berr = false;
            for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
                wxString line = ret_array[i];
                if(line.Upper().Find(_T("ERROR")) != wxNOT_FOUND){
                    berr = true;
                    break;
                }
                if(line.Upper().Find(_T("FPR")) != wxNOT_FOUND){
                    fpr_file = line.AfterFirst(':');
                }
                
            }
            
            bool berror = false;
            
            if(!berr && fpr_file.Length()){

                bool bcopy = false;
                #ifdef __WXMSW__
                wchar_t *desktop_path = 0;
                if(S_OK == SHGetKnownFolderPath( FOLDERID_Desktop, 0, 0, &desktop_path)){
                    
                    char str[128];
                    wcstombs(str, desktop_path, 128);
                    wxString desktop_fpr(str, wxConvUTF8);
                    if( desktop_fpr.Last() != wxFileName::GetPathSeparator() )
                         desktop_fpr += wxFileName::GetPathSeparator();

                    wxString wcmd = g_sencutil_bin;
                    wcmd += _T(" -f ");                  // Make fingerprint
                    wcmd += desktop_fpr;
                    
 
                    wxString exe = g_sencutil_bin;
                    wxString parms = _T(" -f ");
                    parms += desktop_fpr;
                    
                    const wchar_t *wexe = exe.wc_str(wxConvUTF8);
                    const wchar_t *wparms = parms.wc_str(wxConvUTF8);
                    
                    {
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
                                OCPNMessageBox_PlugIn(NULL, _("Administrator priveleges are required to create fpr.\n  Please try again...."), _("oesenc_pi Message"), wxOK);
                                berror = true;
                            }
                        }
                        else
                            bcopy = true;
                        
                        ::wxEndBusyCursor();
                        
                    }  
                }
                #endif            
                
                if(!berror){
                    wxString msg1 = _("Fingerprint file created.\n");
                    msg1 += fpr_file;

                #ifdef __WXMSW__
                    if(bcopy)
                        msg1 += _("\n\n Fingerprint file is also copied to desktop.");
                #endif
                
                    OCPNMessageBox_PlugIn(NULL, msg1, _("oesenc_pi Message"), wxOK);
                }
            }
            else{
                wxLogMessage(_T("oesenc_pi: oeserverd results:"));
                for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
                    wxString line = ret_array[i];
                    wxLogMessage( line );
                }
                OCPNMessageBox_PlugIn(NULL, _T("ERROR Creating Fingerprint file\n Check OpenCPN log file."), _("oesenc_pi Message"), wxOK);
                
                berror = true;
            }
            
            g_fpr_file = fpr_file;
            
            if (!berror)
                m_parent->Set_FPR();
           
    }
}


