/***************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  S57 SENC File Object
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2015 by David S. Register                               *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 **************************************************************************/

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include <wx/wfstream.h>
#include <wx/filename.h>

#include "Osenc.h"
#include "pi_s52s57.h"
//#include "s57chart.h"
//#include "cutil.h"
#include "s57RegistrarMgr.h"
#include "cpl_csv.h"

#include "mygdal/ogr_s57.h"
#include "mygdal/cpl_string.h"

#include "mygeom.h"
//#include <../opencpn/plugins/chartdldr_pi/src/unrar/rartypes.hpp>
#include "georef.h"
#include "ocpn_plugin.h"

extern int g_debugLevel;
extern wxString g_pipeParm;

using namespace std;

#include <wx/arrimpl.cpp>
WX_DEFINE_ARRAY( float*, MyFloatPtrArray );

//      External definitions
void OpenCPN_OGRErrorHandler( CPLErr eErrClass, int nError,
                              const char * pszErrorMsg );               // installed GDAL OGR library error handler


#ifdef __OCPN__ANDROID__
#include "qdebug.h"

//--------------------------------------------------------------------------
//      Osenc_instream implementation as Kernel Socket
//--------------------------------------------------------------------------

int makeAddr(const char* name, struct sockaddr_un* pAddr, socklen_t* pSockLen)
{
    // consider this:
    //http://stackoverflow.com/questions/11640826/can-not-connect-to-linux-abstract-unix-socket
    
    int nameLen = strlen(name);
    if (nameLen >= (int) sizeof(pAddr->sun_path) -1)  /* too long? */
        return -1;
    pAddr->sun_path[0] = '\0';  /* abstract namespace */
    strcpy(pAddr->sun_path+1, name);
    pAddr->sun_family = AF_LOCAL;
    *pSockLen = 1 + nameLen + offsetof(struct sockaddr_un, sun_path);
    return 0;
}

Osenc_instream::Osenc_instream()
{
    Init();
}

Osenc_instream::~Osenc_instream()
{
    Close();
    if(publicSocket > 0){
       // qDebug() << "dtor Close Socket" << publicSocket;
        close( publicSocket );
        publicSocket = -1;
    }
        
}

void Osenc_instream::Init()
{
    privatefifo = -1;
    publicfifo = -1;
    m_OK = true;
    m_lastBytesRead = 0;
    m_lastBytesReq = 0;
    m_uncrypt_stream = 0;
    publicSocket = -1;
    
    strcpy(publicsocket_name,"com.whoever.xfer");
    
    if (makeAddr(publicsocket_name, &sockAddr, &sockLen) < 0){
        wxLogMessage(_T("oesenc_pi: Could not makeAddr for PUBLIC socket"));
    }
    
    publicSocket = socket(AF_LOCAL, SOCK_STREAM, PF_UNIX);
    if (publicSocket < 0) {
        wxLogMessage(_T("oesenc_pi: Could not make PUBLIC socket"));
    }
    //else
    //    qDebug() << "Init() create Socket" << publicSocket;
    
    
}

void Osenc_instream::ReInit()
{
    privatefifo = -1;
    publicfifo = -1;
    m_OK = true;
    m_lastBytesRead = 0;
    m_lastBytesReq = 0;
    m_uncrypt_stream = 0;
    publicSocket = -1;
    
    strcpy(publicsocket_name,"com.whoever.xfer");
    
    if (makeAddr(publicsocket_name, &sockAddr, &sockLen) < 0){
        wxLogMessage(_T("oesenc_pi: Could not makeAddr for PUBLIC socket"));
    }
    
    publicSocket = socket(AF_LOCAL, SOCK_STREAM, PF_UNIX);
    if (publicSocket < 0) {
        wxLogMessage(_T("oesenc_pi: Could not make PUBLIC socket"));
    }
    //else
        //qDebug() << "ReInit() create Socket" << publicSocket;
    
    
}

void Osenc_instream::Close()
{
    wxLogMessage(_T("Osenc_instream::Close()"));
    
    if(-1 != privatefifo){
        if(g_debugLevel)printf("   Close private fifo: %s \n", privatefifo_name);
        close(privatefifo);
        if(g_debugLevel)printf("   unlink private fifo: %s \n", privatefifo_name);
        unlink(privatefifo_name);
    }
    
    if(-1 != publicfifo)
        close(publicfifo);
    
    if(m_uncrypt_stream){
        delete m_uncrypt_stream;
    }
    
    if(-1 != publicSocket){
        //qDebug() << "Close() Close Socket" << publicSocket;
        close( publicSocket );
        publicSocket = -1;
    }
    
    ReInit();             // In case it want to be used again
}


bool Osenc_instream::isAvailable( wxString user_key )
{
    if(g_debugLevel)printf("TestAvail\n");
    
    if(m_uncrypt_stream){
        return m_uncrypt_stream->IsOk();
    }
    else{
        if( Open(CMD_TEST_AVAIL, _T(""), user_key) ){
            if(g_debugLevel)printf("TestAvail Open OK\n");
            char response[8];
            memset( response, 0, 8);
            int nTry = 5;
            do{
                if( Read(response, 2).IsOk() ){
                    if(g_debugLevel)printf("TestAvail Response OK\n");
                    return( !strncmp(response, "OK", 2) );
                }
                
                if(g_debugLevel)printf("Sleep on TestAvail: %d\n", nTry);
                wxMilliSleep(100);
                nTry--;
            }while(nTry);
            
            return false;
        }
        else{
            if(g_debugLevel)printf("TestAvail Open Error\n");
            return false;
        }
    }
    
}

void Osenc_instream::Shutdown()
{
    //     if(!m_uncrypt_stream){
        //         Open(CMD_EXIT,  _T(""), _T("")) ;
        //         char response[8];
        //         memset( response, 0, 8);
        //         Read(response, 3);
        //     }
        
        if(Open(CMD_EXIT,  _T(""), _T("?"))) {
            char response[8];
            memset( response, 0, 8);
            Read(response, 3);
        }
}


bool Osenc_instream::Open( unsigned char cmd, wxString senc_file_name, wxString crypto_key )
{
    if(crypto_key.Length()){
        fifo_msg msg;
        
        
        
        if (connect(publicSocket, (const struct sockaddr*) &sockAddr, sockLen) < 0) {
            wxLogMessage(_T("oesenc_pi: Could not connect to PUBLIC socket"));
            return false;
        }
        
        wxCharBuffer buf = senc_file_name.ToUTF8();
        if(buf.data()) 
            strncpy(msg.senc_name, buf.data(), sizeof(msg.senc_name));
        
        buf = crypto_key.ToUTF8();
        if(buf.data()) 
            strncpy(msg.senc_key, buf.data(), sizeof(msg.senc_key));
        
        msg.cmd = cmd;
        
        write(publicSocket, (char*) &msg, sizeof(msg));
        
        return true;
    }
    else{                        // not encrypted
        m_uncrypt_stream = new wxFileInputStream(senc_file_name);
        return m_uncrypt_stream->IsOk();
    }
}

Osenc_instream &Osenc_instream::Read(void *buffer, size_t size)
{
    #define READ_SIZE 64000;
    #define MAX_TRIES 100;
    if(!m_uncrypt_stream){
        size_t max_read = READ_SIZE;
        //    bool blk = fcntl(privatefifo, F_GETFL) & O_NONBLOCK;
        
        if( -1 != publicSocket){
            
            int remains = size;
            char *bufRun = (char *)buffer;
            int totalBytesRead = 0;
            int nLoop = MAX_TRIES;
            do{
                int bytes_to_read = MIN(remains, max_read);
                if(bytes_to_read > 10000)
                    int yyp = 2;
                
                int bytesRead;
                
                #if 1
                struct pollfd fd;
                int ret;
                
                fd.fd = publicSocket; // your socket handler 
                fd.events = POLLIN;
                ret = poll(&fd, 1, 100); // 1 second for timeout
                switch (ret) {
                    case -1:
                        // Error
                        bytesRead = 0;
                        break;
                    case 0:
                        // Timeout 
                        bytesRead = -1;
                        break;
                    default:
                        bytesRead = read(publicSocket, bufRun, bytes_to_read );
                        break;
                }
                
                #else                
                bytesRead = read(publicSocket, bufRun, bytes_to_read );
                #endif                
                
                // Server may not have opened the Write end of the FIFO yet
                if(bytesRead == 0){
                    //                    printf("miss %d %d %d\n", nLoop, bytes_to_read, size);
                    nLoop --;
                    wxMilliSleep(1);
                }
                else if(bytesRead == -1)
                    nLoop = 0;
                else
                    nLoop = MAX_TRIES;
                
                remains -= bytesRead;
                bufRun += bytesRead;
                totalBytesRead += bytesRead;
            } while( (remains > 0) && (nLoop) );
            
            m_OK = (totalBytesRead == size);
            if(!m_OK)
                int yyp = 4;
            m_lastBytesRead = totalBytesRead;
            m_lastBytesReq = size;
        }
        
        return *this;
    }
    else{
        if(m_uncrypt_stream->IsOk())
            m_uncrypt_stream->Read(buffer, size);
        m_OK = m_uncrypt_stream->IsOk();
        return *this;
    }
}

bool Osenc_instream::IsOk()
{
    if(!m_OK)
        int yyp = 4;
    
    return m_OK;
}

#endif

#if !defined(__WXMSW__) && !defined( __OCPN__ANDROID__)   // i.e. linux and Mac
//--------------------------------------------------------------------------
//      Osenc_instream implementation as Named Pipe 
//--------------------------------------------------------------------------
Osenc_instream::Osenc_instream()
{
    Init();
}

Osenc_instream::~Osenc_instream()
{
    Close();
}

void Osenc_instream::Init()
{
    privatefifo = -1;
    publicfifo = -1;
    m_OK = true;
    m_lastBytesRead = 0;
    m_lastBytesReq = 0;
    m_uncrypt_stream = 0;
}

void Osenc_instream::Close()
{
    if(-1 != privatefifo){
        if(g_debugLevel)printf("   Close private fifo: %s \n", privatefifo_name);
        close(privatefifo);
        if(g_debugLevel)printf("   unlink private fifo: %s \n", privatefifo_name);
        unlink(privatefifo_name);
    }
    
    if(-1 != publicfifo)
        close(publicfifo);
    
    if(m_uncrypt_stream){
        delete m_uncrypt_stream;
    }
    
    Init();             // In case it want to be used again
}


bool Osenc_instream::isAvailable( wxString user_key )
{
    if(g_debugLevel)printf("TestAvail\n");

    if(m_uncrypt_stream){
        return m_uncrypt_stream->IsOk();
    }
    else{
        if( Open(CMD_TEST_AVAIL, _T(""), user_key) ){
            if(g_debugLevel)printf("TestAvail Open OK\n");
            char response[8];
            memset( response, 0, 8);
            int nTry = 5;
            do{
                if( Read(response, 2).IsOk() ){
                    if(g_debugLevel)printf("TestAvail Response OK\n");
                    return( !strncmp(response, "OK", 2) );
                }
                
                if(g_debugLevel)printf("Sleep on TestAvail: %d\n", nTry);
                wxMilliSleep(100);
                nTry--;
            }while(nTry);
            
            return false;
        }
        else{
            if(g_debugLevel)printf("TestAvail Open Error\n");
            return false;
        }
    }

}

void Osenc_instream::Shutdown()
{
//     if(!m_uncrypt_stream){
//         Open(CMD_EXIT,  _T(""), _T("")) ;
//         char response[8];
//         memset( response, 0, 8);
//         Read(response, 3);
//     }

    if(Open(CMD_EXIT,  _T(""), _T("?"))) {
        char response[8];
        memset( response, 0, 8);
        Read(response, 3);
    }
}


bool Osenc_instream::Open( unsigned char cmd, wxString senc_file_name, wxString crypto_key )
{
    if(crypto_key.Length()){
        fifo_msg msg;

        wxCharBuffer buf = senc_file_name.ToUTF8();
        if(buf.data()) 
            strncpy(msg.senc_name, buf.data(), sizeof(msg.senc_name));
        
        //printf("\n\n            Opening senc: %s\n", msg.senc_name);
        
        // Create a unique name for the private (i.e. data) pipe, valid for this session
        
        wxString tmp_file = wxFileName::CreateTempFileName( _T("") );
        wxCharBuffer bufn = tmp_file.ToUTF8();
        if(bufn.data()) 
            strncpy(privatefifo_name, bufn.data(), sizeof(privatefifo_name));
        
            // Create the private FIFO
        if(-1 == mkfifo(privatefifo_name, 0666)){
            if(g_debugLevel)printf("   mkfifo private failed: %s\n", privatefifo_name);
        }
        else{
            if(g_debugLevel)printf("   mkfifo OK: %s\n", privatefifo_name);
        }
        
        
        
        // Open the well known public FIFO for writing
        if( (publicfifo = open(PUBLIC, O_WRONLY | O_NDELAY) ) == -1) {
            //wxLogMessage(_T("oesenc_pi: Could not open PUBLIC pipe"));
            return false;               // This will be retried...
            //if( errno == ENXIO )
        }
        
        //  Send the command over the public pipe
        strncpy(msg.fifo_name, privatefifo_name, sizeof(privatefifo_name));
            
        
        buf = crypto_key.ToUTF8();
        if(buf.data()) 
            strncpy(msg.senc_key, buf.data(), sizeof(msg.senc_key));
        
        msg.cmd = cmd;
        
        write(publicfifo, (char*) &msg, sizeof(msg));
        
        // Open the private FIFO for reading to get output of command
        // from the server.
        if((privatefifo = open(privatefifo_name, O_RDONLY) ) == -1) {
            wxLogMessage(_T("oesenc_pi: Could not open private pipe"));
            return false;
        }
        return true;
    }
    else{                        // not encrypted
        m_uncrypt_stream = new wxFileInputStream(senc_file_name);
        return m_uncrypt_stream->IsOk();
    }
}

Osenc_instream &Osenc_instream::Read(void *buffer, size_t size)
{
#define READ_SIZE 64000;
#define MAX_TRIES 100;
    if(!m_uncrypt_stream){
        size_t max_read = READ_SIZE;
    //    bool blk = fcntl(privatefifo, F_GETFL) & O_NONBLOCK;
        
        if( -1 != privatefifo){
//            printf("           Read private fifo: %s, %d bytes\n", privatefifo_name, size);
            
            size_t remains = size;
            char *bufRun = (char *)buffer;
            size_t totalBytesRead = 0;
            int nLoop = MAX_TRIES;
            do{
                size_t bytes_to_read = MIN(remains, max_read);
                
                size_t bytesRead = read(privatefifo, bufRun, bytes_to_read );

                // Server may not have opened the Write end of the FIFO yet
                if(bytesRead == 0){
//                    printf("miss %d %d %d\n", nLoop, bytes_to_read, size);
                    nLoop --;
                    wxMilliSleep(1);
                }
                else
                    nLoop = MAX_TRIES;
                
                remains -= bytesRead;
                bufRun += bytesRead;
                totalBytesRead += bytesRead;
            } while( (remains > 0) && (nLoop) );
            
            m_OK = (totalBytesRead == size);
            if(!m_OK)
                int yyp = 4;
            m_lastBytesRead = totalBytesRead;
            m_lastBytesReq = size;
        }
        
        return *this;
    }
    else{
        if(m_uncrypt_stream->IsOk())
            m_uncrypt_stream->Read(buffer, size);
        m_OK = m_uncrypt_stream->IsOk();
        return *this;
    }
}

bool Osenc_instream::IsOk()
{
    if(!m_OK)
        int yyp = 4;
    
    return m_OK;
}

#elif defined(__WXMSW__)  //MSW

#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#define BUFSIZE 1024

Osenc_instream::Osenc_instream()
{
    Init();
}

Osenc_instream::~Osenc_instream()
{
    Close();
}

void Osenc_instream::Init()
{
    hPipe = (HANDLE)-1;
    m_OK = true;
    m_lastBytesRead = 0;
    m_lastBytesReq = 0;
}


void Osenc_instream::Close()
{
    if((HANDLE)-1 != hPipe)
        CloseHandle(hPipe);
    
    Init();             // In case it wants to be reused....
}


bool Osenc_instream::isAvailable( wxString user_key )
{
    if( Open(CMD_TEST_AVAIL, _T(""), user_key ) ){
        char response[8];
        memset( response, 0, 8);
        int nTry = 5;
        do{
            if( Read(response, 3).IsOk() ){
                if(strncmp(response, "OK", 2))
                    return false;
                
                if(response[2] == '1')
                    wxLogMessage(_T("Osenc_instream decrypt T"));
                else if(response[2] == '2')
                    wxLogMessage(_T("Osenc_instream decrypt S"));
                else if(response[2] == '3')
                    wxLogMessage(_T("Osenc_instream decrypt L"));
                else
                    wxLogMessage(_T("Osenc_instream decrypt ?"));
                    
                return true;    
            }
            
            wxMilliSleep(100);
            nTry--;
        }while(nTry);
        
        return false;
    }
    else{
        if(g_debugLevel) wxLogMessage(_T("Osenc_instream OPEN() failed"));
        return false;
    }
    
}

void Osenc_instream::Shutdown()
{
    Open(CMD_EXIT,  _T(""), _T("")) ;
    char response[8];
    memset( response, 0, 8);
    Read(response, 3);
    
}

bool Osenc_instream::Open( unsigned char cmd, wxString senc_file_name, wxString crypto_key )
{
    fifo_msg msg;
    
    LPTSTR lpvMessage=TEXT("Default message from client."); 
    BOOL   fSuccess = FALSE; 
    DWORD  cbToWrite, cbWritten; 
   
    wxString pipeName;
    if(g_pipeParm.Length())
        pipeName = _T("\\\\.\\pipe\\") + g_pipeParm;
    else
        pipeName = _T("\\\\.\\pipe\\mynamedpipe");
    
    LPCWSTR lpszPipename = pipeName.wc_str();
    
    // Try to open a named pipe; wait for it, if necessary. 
        
    while (1) 
    { 
            hPipe = CreateFile( 
            lpszPipename,   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE, 
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file 
            
            // Break if the pipe handle is valid. 
            
            if (hPipe != INVALID_HANDLE_VALUE) 
                break; 
            
            // Exit if an error other than ERROR_PIPE_BUSY occurs. 
                
            if (GetLastError() != ERROR_PIPE_BUSY) 
            {
                    _tprintf( TEXT("Could not open pipe. GLE=%d\n"), GetLastError() ); 
                    return false;
            }
                
                // All pipe instances are busy, so wait for 20 seconds. 
                
            if ( ! WaitNamedPipe(lpszPipename, 20000)) 
            { 
                    printf("Could not open pipe: 20 second wait timed out."); 
                    return false;
            } 
    }
    
#if 0    
    // The pipe connected; change to message-read mode. 
    
    dwMode = PIPE_READMODE_MESSAGE; 
    fSuccess = SetNamedPipeHandleState( 
    hPipe,    // pipe handle 
    &dwMode,  // new pipe mode 
    NULL,     // don't set maximum bytes 
    NULL);    // don't set maximum time 
    if ( ! fSuccess) 
    {
        _tprintf( TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError() ); 
        return -1;
    }
#endif

    // Create a command message
    
    wxCharBuffer buf = senc_file_name.ToUTF8();
    if(buf.data()) 
        strncpy(msg.senc_name, buf.data(), sizeof(msg.senc_name));
    
    buf = crypto_key.ToUTF8();
    if(buf.data()) 
        strncpy(msg.senc_key, buf.data(), sizeof(msg.senc_key));
    
    msg.cmd = cmd;
    
    // Send the command message to the pipe server. 
    
    cbToWrite = sizeof(msg);
    _tprintf( TEXT("Sending %d byte message \n"), cbToWrite); 
    
    fSuccess = WriteFile( 
        hPipe,                  // pipe handle 
        &msg,                   // message 
        cbToWrite,              // message length 
        &cbWritten,             // bytes written 
        NULL);                  // not overlapped 
    
    if ( ! fSuccess) 
    {
        _tprintf( TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError() ); 
        return false;
    }
    
    printf("\nMessage sent to server.\n");
    
                
    
#if 0    
    
    
    
    // Create a unique name for the private (i.e. data) pipe, valid for this session
    sprintf(privatefifo_name, "/tmp/fifo%d",44);
    
    // Create the private FIFO
    //     if( mkfifo(privatefifo_name, 0666) < 0 ) 
    //         return false;
    mkfifo(privatefifo_name, 0666);
    
    // Open the well known public FIFO for writing
    if( (publicfifo = open(PUBLIC, O_WRONLY | O_NDELAY) ) == -1) {
        return false;
        //if( errno == ENXIO )
    }
    
    //  Send the command over the public pipe
    strncpy(msg.fifo_name, privatefifo_name, sizeof(privatefifo_name));
    
    wxCharBuffer buf = senc_file_name.ToUTF8();
    if(buf.data()) 
        strncpy(msg.senc_name, buf.data(), sizeof(msg.senc_name));
    
    buf = crypto_key.ToUTF8();
    if(buf.data()) 
        strncpy(msg.senc_key, buf.data(), sizeof(msg.senc_key));
    
    
    write(publicfifo, (char*) &msg, sizeof(msg));
    
    // Open the private FIFO for reading to get output of command
    // from the server.
    if((privatefifo = open(privatefifo_name, O_RDONLY) ) == -1) {
        return false;
    }
    
#endif    
    return true;
}

Osenc_instream &Osenc_instream::Read(void *buffer, size_t size)
{
    #define READ_SIZE 64000;
    size_t max_read = READ_SIZE;
    
    if(size > max_read)
        int yyp = 4;
    
    if( (HANDLE)-1 != hPipe){
        size_t remains = size;
        char *bufRun = (char *)buffer;
        int totalBytesRead = 0;
        DWORD bytesRead;
        BOOL fSuccess = FALSE;
        
        do{
            int bytes_to_read = MIN(remains, max_read);
            
            // Read from the pipe. 
            
            fSuccess = ReadFile( 
                hPipe,                  // pipe handle 
                bufRun,                 // buffer to receive reply 
                bytes_to_read,          // size of buffer 
                &bytesRead,             // number of bytes read 
                NULL);                  // not overlapped 
            
            if ( ! fSuccess && GetLastError() != ERROR_MORE_DATA )
                break; 
            
            //_tprintf( TEXT("\"%s\"\n"), chBuf ); 
            
            if(bytesRead == 0){
                int yyp = 3;
                remains = 0;    // break it
            }
            
            remains -= bytesRead;
            bufRun += bytesRead;
            totalBytesRead += bytesRead;
        } while(remains > 0);

        
        m_OK = (totalBytesRead == size) && fSuccess;
        m_lastBytesRead = totalBytesRead;
        m_lastBytesReq = size;
        
    }
    else
        m_OK = false;
    
    return *this;
}

bool Osenc_instream::IsOk()
{
    if(!m_OK)
        int yyp = 4;
    
    return m_OK;
}

#endif



//--------------------------------------------------------------------------
//      Osenc implementation
//--------------------------------------------------------------------------

Osenc::Osenc()
{
    init();
}

Osenc::~Osenc()
{
    // Free the coverage arrays, if they exist
    SENCFloatPtrArray &AuxPtrArray = getSENCReadAuxPointArray();
    wxArrayInt &AuxCntArray = getSENCReadAuxPointCountArray();
    int nCOVREntries = AuxCntArray.GetCount();
    for( unsigned int j = 0; j < (unsigned int) nCOVREntries; j++ ) {
        free(AuxPtrArray.Item(j));
    }

    SENCFloatPtrArray &AuxNoPtrArray = getSENCReadNOCOVRPointArray();
    wxArrayInt &AuxNoCntArray = getSENCReadNOCOVRPointCountArray();
    int nNoCOVREntries = AuxNoCntArray.GetCount();
    for( unsigned int j = 0; j < (unsigned int) nNoCOVREntries; j++ ) {
        free(AuxNoPtrArray.Item(j));
    }

    free(pBuffer);
}

void Osenc::init( void )
{
    m_LOD_meters = 0;
    m_poRegistrar = NULL;
    m_senc_file_read_version = 0;
    s_ProgDialog = NULL;
    InitializePersistentBuffer();
    
}

int Osenc::verifySENC(Osenc_instream &fpx, const wxString &senc_file_name)
{
    if(g_debugLevel) wxLogMessage(_T("verifySENC"));
    
    if(!fpx.IsOk()){
        if(g_debugLevel) wxLogMessage(_T("verifySENC E1"));
        return ERROR_SENC_CORRUPT;
    }
  
    //  We read the first record, and confirm the compatible SENC version number
    OSENC_Record_Base first_record;
    fpx.Read(&first_record, sizeof(OSENC_Record_Base));
    
    //  Bad read?
    if(!fpx.IsOk()){
        if(g_debugLevel)printf("verifySENC E2\n");
        wxLogMessage(_T("verifySENC E2"));
        
        // Server may be slow, so try the read again
        wxMilliSleep(100);
        fpx.Read(&first_record, sizeof(OSENC_Record_Base));
        
        if(!fpx.IsOk()){
            if(g_debugLevel)printf("verifySENC E2.5\n");
            wxLogMessage(_T("verifySENC E2.5"));
            return ERROR_SENC_CORRUPT;
        }
    }
    
    if(( first_record.record_type == HEADER_SENC_VERSION ) && (first_record.record_length < 16) ){
        unsigned char *buf = getBuffer( first_record.record_length - sizeof(OSENC_Record_Base));
        if(!fpx.Read(buf, first_record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
            return ERROR_SENC_CORRUPT;        
        }
        uint16_t *pint = (uint16_t*)buf;
        m_senc_file_read_version = *pint;
        
        //  Is there a Signature error?
        if(m_senc_file_read_version == 1024){
            if(g_debugLevel) wxLogMessage(_T("verifySENC E3"));
            return ERROR_SIGNATURE_FAILURE;
        }
        
        //  Is the oeSENC version compatible?
        if((m_senc_file_read_version < 200) || (m_senc_file_read_version > 299)){
            if(g_debugLevel) wxLogMessage(_T("verifySENC E4"));
            return ERROR_SENC_VERSION_MISMATCH;
        }
    }
    else{
        
        if(g_debugLevel) wxLogMessage(_T("verifySENC E5"));
        
        fpx.Close();
        //  Try  with empty key, in case the SENC is unencrypted
        if( !fpx.Open(CMD_READ_ESENC, senc_file_name, _T("")) ){    
            if(g_debugLevel) wxLogMessage(_T("ingestHeader Open failed "));
            return ERROR_SENC_CORRUPT;        
        }
        
        fpx.Read(&first_record, sizeof(OSENC_Record_Base));
        
        //  Bad read?
        if(!fpx.IsOk()){
            if(g_debugLevel) wxLogMessage(_T("verifySENC E6"));
            return ERROR_SENC_CORRUPT;        
        }
        
        if( first_record.record_type == HEADER_SENC_VERSION ){
            if(first_record.record_length < 16){
                unsigned char *buf = getBuffer( first_record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, first_record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    if(g_debugLevel) wxLogMessage(_T("verifySENC E7"));
                    return ERROR_SENC_CORRUPT;        
                }
                
                uint16_t *pint = (uint16_t*)buf;
                m_senc_file_read_version = *pint;
                
                //  Is there a Signature error?
                if(m_senc_file_read_version == 1024){
                    if(g_debugLevel) wxLogMessage(_T("verifySENC E8"));
                    return ERROR_SIGNATURE_FAILURE;
                }
                
                //  Is the oeSENC version compatible?
                if((m_senc_file_read_version < 200) || (m_senc_file_read_version > 299)){
                    if(g_debugLevel) wxLogMessage(_T("verifySENC E9"));
                    return ERROR_SENC_VERSION_MISMATCH;
                }
            }
            else{
                if(g_debugLevel) wxLogMessage(_T("verifySENC E10"));
                return ERROR_SENC_CORRUPT;
            }
                
        }
        else{
            if(g_debugLevel) wxLogMessage(_T("verifySENC E11"));
            return ERROR_SENC_CORRUPT;
        }
        
        
    }
    
    if(g_debugLevel) wxLogMessage(_T("verifySENC NE2"));
    
    return SENC_NO_ERROR;
    

}


int Osenc::ingestHeader(const wxString &senc_file_name)
{
    //  Read oSENC header records, stopping at the first Feature_ID record
    //  Then check to see if everything is defined as required.
    
    if(g_debugLevel) wxLogMessage(_T("ingestHeader"));
    
    int ret_val = SENC_NO_ERROR;                    // default is OK
    
    wxFileName fn(senc_file_name);
    
    
    
    
/*    
    wxFFileInputStream fpx_u( ifs );
    if (!fpx_u.IsOk()) {
        return ERROR_SENCFILE_NOT_FOUND;
    }
    wxBufferedInputStream fpx( fpx_u );
 */   
    Osenc_instream fpx;
    
    if( !fpx.Open(CMD_READ_ESENC_HDR, senc_file_name, m_key) ){
        if(g_debugLevel) wxLogMessage(_T("ingestHeader Open failed first"));
        wxMilliSleep(100);
        if( !fpx.Open(CMD_READ_ESENC_HDR, senc_file_name, m_key) ){
            if(g_debugLevel) wxLogMessage(_T("ingestHeader Open failed second"));
            return ERROR_SENCFILE_NOT_FOUND;
        }
    }

    int verify_val = verifySENC(fpx, senc_file_name);
    
    if(verify_val != SENC_NO_ERROR)
        return verify_val;
    
    if(g_debugLevel) wxLogMessage(_T("ingestHeader SENC verified OK"));
    
    S57Obj *obj = 0;
    
    //  Read the rest of the records in the header
    int dun = 0;
    
    while( !dun ) {
        //      Read a record Header
        OSENC_Record_Base record;
        
        fpx.Read(&record, sizeof(OSENC_Record_Base));
        if(!fpx.IsOk()){
            dun = 1;
            break;
        }
        
        // Process Records
        switch( record.record_type){
            case HEADER_SENC_VERSION:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                uint16_t *pint = (uint16_t*)buf;
                m_senc_file_read_version = *pint;
                break;
            }
            case HEADER_CELL_NAME:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_Name = wxString( buf, wxConvUTF8 );
                break;
            }
            case HEADER_CELL_PUBLISHDATE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_sdate000 = wxString( buf, wxConvUTF8 );
                
                break;
            }
            
            case HEADER_CELL_EDITION:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                uint16_t *pint = (uint16_t*)buf;
                m_read_base_edtn.Printf(_T("%d"), *pint);
                break;
            }
            
            case HEADER_CELL_UPDATEDATE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_LastUpdateDate = wxString( buf, wxConvUTF8 );
                
                break;
            }
            
            case HEADER_CELL_UPDATE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                uint16_t *pint = (uint16_t*)buf;
                m_read_last_applied_update = *pint;
                break;
            }   
            
            case HEADER_CELL_NATIVESCALE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                uint32_t *pint = (uint32_t*)buf;
                m_Chart_Scale = *pint;
                
                break;
            }   
            
            case HEADER_CELL_SOUNDINGDATUM:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_SoundingDatum = wxString( buf, wxConvUTF8 );
                
                break;
            }   
            
            case HEADER_CELL_SENCCREATEDATE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_readFileCreateDate = wxString( buf, wxConvUTF8 );
                
                break;
            }
            
            case CELL_EXTENT_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                _OSENC_EXTENT_Record_Payload *pPayload = (_OSENC_EXTENT_Record_Payload *)buf;
                m_extent.NLAT = pPayload->extent_nw_lat;
                m_extent.SLAT = pPayload->extent_se_lat;
                m_extent.WLON = pPayload->extent_nw_lon;
                m_extent.ELON = pPayload->extent_se_lon;
                
                break;
            }
            
            case CELL_COVR_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                _OSENC_COVR_Record_Payload *pPayload = (_OSENC_COVR_Record_Payload*)buf;
                
                int point_count = pPayload->point_count;
                m_AuxCntArray.Add(point_count);
                
                float *pf = (float *)malloc(point_count * 2 * sizeof(float));
                memcpy(pf, &pPayload->point_array, point_count * 2 * sizeof(float));
                m_AuxPtrArray.Add(pf);
                
                break;
            }
            
            case CELL_NOCOVR_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                _OSENC_NOCOVR_Record_Payload *pPayload = (_OSENC_NOCOVR_Record_Payload*)buf;
                
                int point_count = pPayload->point_count;
                m_NoCovrCntArray.Add(point_count);
                
                float *pf = (float *)malloc(point_count * 2 * sizeof(float));
                memcpy(pf, &pPayload->point_array, point_count * 2 * sizeof(float));
                m_NoCovrPtrArray.Add(pf);
                
                
                break;
            }
            
            case FEATURE_ID_RECORD:
            {
                dun = 1;
                break;
            }

            default:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                dun = 1;
                break;
            }
            
        } // switch
        
    }   // while
                
    return ret_val;
}

std::string Osenc::GetFeatureAcronymFromTypecode( int typeCode)
{
    if(m_pRegistrarMan){
        std::string  acronym = m_pRegistrarMan->getFeatureAcronym(typeCode);
        return acronym.c_str();
    }
    else
        return "";
}

std::string Osenc::GetAttributeAcronymFromTypecode( int typeCode)
{
    if(m_pRegistrarMan)
        return m_pRegistrarMan->getAttributeAcronym(typeCode);
    else
        return "";
}


int Osenc::ingest200(const wxString &senc_file_name,
                  S57ObjVector *pObjectVector,
                  VE_ElementVector *pVEArray,
                  VC_ElementVector *pVCArray)
{
    
    int ret_val = SENC_NO_ERROR;                    // default is OK
    int senc_file_version = 0;
    
    wxFileName fn(senc_file_name);
    m_ID = fn.GetName();                          // This will be the NOAA File name, usually
    
    //    Sanity check for existence of file
    if(!fn.Exists())
        return ERROR_SENCFILE_NOT_FOUND;
    
    int nProg = 0;
    
    wxString ifs( senc_file_name );

//      wxFFileInputStream fpx_u( ifs );
//      if (!fpx_u.IsOk()) {
//          return ERROR_SENCFILE_NOT_FOUND;
//      }
//      wxBufferedInputStream fpx( fpx_u );

    Osenc_instream fpx;
    
    
    if( !fpx.Open(CMD_READ_ESENC, senc_file_name, m_key) ){
        if(g_debugLevel) wxLogMessage(_T("ingest200 Open failed first"));
        wxMilliSleep(100);
        if( !fpx.Open(CMD_READ_ESENC, senc_file_name, m_key) ){
            if(g_debugLevel) wxLogMessage(_T("ingest200 Open failed second"));

            //  Try  with empty key, in case the SENC is unencrypted
            if( !fpx.Open(CMD_READ_ESENC, senc_file_name, _T("")) ){    
                if(g_debugLevel) wxLogMessage(_T("ingest200 Open failed third"));
                return ERROR_SENCFILE_NOT_FOUND;
            }
        }
    }
    
    
    int verify_val = verifySENC(fpx, senc_file_name);

    if(verify_val != SENC_NO_ERROR)
        return verify_val;

    if(g_debugLevel) wxLogMessage(_T("ingest200 SENC verified OK"));
                                     
    S57Obj *obj = 0;
    int featureID;
    uint32_t primitiveType;
        
    int dun = 0;
        
    while( !dun ) {
        
        //      Read a record Header
        OSENC_Record_Base record;
        
//        long off = fpx.TellI();
        
        fpx.Read(&record, sizeof(OSENC_Record_Base));
        if(!fpx.IsOk()){
            dun = 1;
            break;
        }
        
        // Process Records
        switch( record.record_type){
            case HEADER_SENC_VERSION:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                uint16_t *pint = (uint16_t*)buf;
                m_senc_file_read_version = *pint;
                break;
            }
            case HEADER_CELL_NAME:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_Name = wxString( buf, wxConvUTF8 );
                break;
            }
            case HEADER_CELL_PUBLISHDATE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_sdate000 = wxString( buf, wxConvUTF8 );
                break;
            }
            
            case HEADER_CELL_EDITION:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                uint16_t *pint = (uint16_t*)buf;
                m_read_base_edtn.Printf(_T("%d"), *pint);
                
                break;
            }
            
            case HEADER_CELL_UPDATEDATE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_LastUpdateDate = wxString( buf, wxConvUTF8 );
                break;
            }
                
            case HEADER_CELL_UPDATE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                uint16_t *pint = (uint16_t*)buf;
                m_read_last_applied_update = *pint;
                
                break;
            }   
            
            case HEADER_CELL_NATIVESCALE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                uint32_t *pint = (uint32_t*)buf;
                m_Chart_Scale = *pint;
                break;
            }   
            
            case HEADER_CELL_SOUNDINGDATUM:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                m_SoundingDatum = wxString( buf, wxConvUTF8 );
                
                break;
            }   
            
            case HEADER_CELL_SENCCREATEDATE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                break;
            }
            
            case CELL_EXTENT_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                _OSENC_EXTENT_Record_Payload *pPayload = (_OSENC_EXTENT_Record_Payload *)buf;
                m_extent.NLAT = pPayload->extent_nw_lat;
                m_extent.SLAT = pPayload->extent_se_lat;
                m_extent.WLON = pPayload->extent_nw_lon;
                m_extent.ELON = pPayload->extent_se_lon;
                
                //  We declare the ref_lat/ref_lon to be the centroid of the extents
                //  This is how the SENC was created....
                m_ref_lat = (m_extent.NLAT + m_extent.SLAT) / 2.;
                m_ref_lon = (m_extent.ELON + m_extent.WLON) / 2.;
                
                break;
            }
            
            case CELL_COVR_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                break;
            }
            
            case CELL_NOCOVR_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                break;
            }
            
            case FEATURE_ID_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                // Starting definition of a new feature
                _OSENC_Feature_Identification_Record_Payload *pPayload = (_OSENC_Feature_Identification_Record_Payload *)buf;
                
                // Get the Feature type code and ID
                int featureTypeCode = pPayload->feature_type_code;
                featureID = pPayload->feature_ID;

                //  Look up the FeatureName from the Registrar
                
//                if(featureTypeCode == 7)
//                    int yyp = 5;
                
                std::string acronym = GetFeatureAcronymFromTypecode( featureTypeCode );
                
                if(acronym.length()){
                    obj = new S57Obj(acronym.c_str());
                    obj->Index = featureID;
                    
                    pObjectVector->push_back(obj);
                }
                
                break;
            }
                
            case FEATURE_ATTRIBUTE_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                // Get the payload
                OSENC_Attribute_Record_Payload *pPayload = (OSENC_Attribute_Record_Payload *)buf;
                
                int attributeTypeCode = pPayload->attribute_type_code;
                
                //  Handle Special cases...
                
                //  The primitive type of the Feature is encoded in the SENC as an attribute of defined type.
                if( ATTRIBUTE_ID_PRIM == attributeTypeCode ){
                    primitiveType = pPayload->attribute_value_int;
                }
                
                    
                //  Get the standard attribute acronym
                std::string acronym = GetAttributeAcronymFromTypecode( attributeTypeCode );
                
                int attributeValueType = pPayload->attribute_value_type;

                
                if( acronym.length() ){
                    switch(attributeValueType){
                        case 0:
                        {
                            uint32_t val = pPayload->attribute_value_int;
                            if(obj){
                                obj->AddIntegerAttribute( acronym.c_str(), (int)val );
                            }
                            break;
                        }
                            
                        case 1:             // Integer list
                        {
                            // Calculate the number of elements from the record size
                            //int nCount = (record.record_length - sizeof(_OSENC_Attribute_Record)) ;
                            
                            break;
                            
                        }
                        case 2:             // Single double precision real
                        {
                            double val = pPayload->attribute_value_double;
                            if(obj)
                                obj->AddDoubleAttribute( acronym.c_str(), val );
                            break;
                        }   
                        
                        case 3:             // List of double precision real
                        {
                            //TODO
                            break;
                        }
                        
                        case 4:             // Ascii String
                        {
                            char *val = (char *)&pPayload->attribute_value_char_ptr;
                            if(obj)
                                obj->AddStringAttribute( acronym.c_str(), val );
                                
                            break;
                        }
                        
                        default:
                            break;
                    }
                }
                
                break;
            }
            
            case FEATURE_GEOMETRY_RECORD_POINT:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                // Get the payload
                _OSENC_PointGeometry_Record_Payload *pPayload = (_OSENC_PointGeometry_Record_Payload *)buf;
                
                if(obj){
                    obj->SetPointGeometry( pPayload->lat, pPayload->lon, m_ref_lat, m_ref_lon);
                }
                
                break;
            }

            case FEATURE_GEOMETRY_RECORD_AREA:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }

                // Get the payload
                _OSENC_AreaGeometry_Record_Payload *pPayload = (_OSENC_AreaGeometry_Record_Payload *)buf;
                
                if(obj){
                    unsigned char *next_byte;
                    PolyTessGeo *pPTG = BuildPolyTessGeo( pPayload, &next_byte);
                    
                    obj->SetAreaGeometry(pPTG, m_ref_lat, m_ref_lon ) ;
                    
                    //  Set the Line geometry for the Feature
                    LineGeometryDescriptor *pDescriptor = (LineGeometryDescriptor *)malloc(sizeof(LineGeometryDescriptor));
                    
                    //  Copy some simple stuff
                    pDescriptor->extent_e_lon = pPayload->extent_e_lon;
                    pDescriptor->extent_w_lon = pPayload->extent_w_lon;
                    pDescriptor->extent_s_lat = pPayload->extent_s_lat;
                    pDescriptor->extent_n_lat = pPayload->extent_n_lat;
                    
                    pDescriptor->indexCount = pPayload->edgeVector_count;
                    
                    // Copy the line index table, which in this case is offset in the payload
                    int table_stride = 3;
                    if(m_senc_file_read_version > 200)
                        table_stride = 4;
                        
                    pDescriptor->indexTable = (int *)malloc(pPayload->edgeVector_count * table_stride * sizeof(int));
                    memcpy( pDescriptor->indexTable, next_byte, pPayload->edgeVector_count * table_stride * sizeof(int) );
                    
                    
                    obj->SetLineGeometry( pDescriptor, GEO_AREA, m_ref_lat, m_ref_lon ) ;
                  
                    free( pDescriptor );
                }
                
                break;
                
            }

            case FEATURE_GEOMETRY_RECORD_AREA_EXT:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                // Get the payload
                _OSENC_AreaGeometryExt_Record_Payload *pPayload = (_OSENC_AreaGeometryExt_Record_Payload *)buf;
                
                if(obj){
                    unsigned char *next_byte;
                    PolyTessGeo *pPTG = BuildPolyTessGeoF16( pPayload, &next_byte);
                    
                    obj->SetAreaGeometry(pPTG, m_ref_lat, m_ref_lon ) ;
                    
                    //  Set the Line geometry for the Feature
                    LineGeometryDescriptor *pDescriptor = (LineGeometryDescriptor *)malloc(sizeof(LineGeometryDescriptor));
                    
                    //  Copy some simple stuff
                    pDescriptor->extent_e_lon = pPayload->extent_e_lon;
                    pDescriptor->extent_w_lon = pPayload->extent_w_lon;
                    pDescriptor->extent_s_lat = pPayload->extent_s_lat;
                    pDescriptor->extent_n_lat = pPayload->extent_n_lat;
                    
                    pDescriptor->indexCount = pPayload->edgeVector_count;
                    
                    // Copy the line index table, which in this case is offset in the payload
                    int table_stride = 3;
                    if(m_senc_file_read_version > 200)
                        table_stride = 4;
                    
                    pDescriptor->indexTable = (int *)malloc(pPayload->edgeVector_count * table_stride * sizeof(int));
                    memcpy( pDescriptor->indexTable, next_byte,
                            pPayload->edgeVector_count * table_stride * sizeof(int) );
                    
                    
                    obj->SetLineGeometry( pDescriptor, GEO_AREA, m_ref_lat, m_ref_lon ) ;
                    
                    free( pDescriptor );
                }
                
                break;
                
            }
            
            case FEATURE_GEOMETRY_RECORD_LINE:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                // Get the payload & parse it
                _OSENC_LineGeometry_Record_Payload *pPayload = (_OSENC_LineGeometry_Record_Payload *)buf;
                
                LineGeometryDescriptor *plD = BuildLineGeometry( pPayload );
                
                if(obj)
                    obj->SetLineGeometry( plD, GEO_LINE, m_ref_lat, m_ref_lon ) ;
                
                free( plD );
                break;
                
            }
 
            case FEATURE_GEOMETRY_RECORD_MULTIPOINT:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }

                // Get the payload & parse it
                OSENC_MultipointGeometry_Record_Payload *pPayload = (OSENC_MultipointGeometry_Record_Payload *)buf;

                //  Set the Multipoint geometry for the Feature
                MultipointGeometryDescriptor *pDescriptor = (MultipointGeometryDescriptor *)malloc(sizeof(MultipointGeometryDescriptor));
                
                //  Copy some simple stuff
                pDescriptor->extent_e_lon = pPayload->extent_e_lon;
                pDescriptor->extent_w_lon = pPayload->extent_w_lon;
                pDescriptor->extent_s_lat = pPayload->extent_s_lat;
                pDescriptor->extent_n_lat = pPayload->extent_n_lat;
                
                pDescriptor->pointCount = pPayload->point_count;
                pDescriptor->pointTable = &pPayload->payLoad;
                
                obj->SetMultipointGeometry( pDescriptor, m_ref_lat, m_ref_lon);
                
                free( pDescriptor );
                
                break;
            }
                
            
            case VECTOR_EDGE_NODE_TABLE_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                //  Parse the buffer
                uint8_t *pRun = (uint8_t *)buf;
                
                // The Feature(Object) count
                int nCount = *(int *)pRun;
                pRun += sizeof(int);
                
                for(int i=0 ; i < nCount ; i++ ) {
                    int featureIndex = *(int*)pRun;
                    pRun += sizeof(int);
                    
                    int pointCount = *(int*)pRun;
                    pRun += sizeof(int);
                    
#if 0                    
                    double *pPoints = NULL;
                    if( pointCount ) {
                        pPoints = (double *) malloc( pointCount * 2 * sizeof(double) );
                        memcpy(pPoints, pRun, pointCount * 2 * sizeof(double));
                    }
                    pRun += pointCount * 2 * sizeof(double);
#else
                    float *pPoints = NULL;
                    if( pointCount ) {
                        pPoints = (float *) malloc( pointCount * 2 * sizeof(float) );
                        memcpy(pPoints, pRun, pointCount * 2 * sizeof(float));
                    }
                    pRun += pointCount * 2 * sizeof(float);
#endif                    
                    VE_Element *pvee = new VE_Element;
                    pvee->index = featureIndex;
                    pvee->nCount = pointCount;
                    pvee->pPoints = pPoints;
                    pvee->max_priority = 0;            // Default
                    
                    pVEArray->push_back(pvee);
                    
                }
                
                
                break;
            }

            case VECTOR_EDGE_NODE_TABLE_EXT_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                OSENC_VET_RecordExt_Payload *pRec = (OSENC_VET_RecordExt_Payload *)buf;
                double scaler = pRec->scaleFactor;
                
                //  Parse the buffer
                uint8_t *pRun = (uint8_t *)buf;
                pRun += sizeof(pRec->scaleFactor);
                
                // The Feature(Object) count
                int nCount = *(int *)pRun;
                
                pRun += sizeof(int);
                
                
                for(int i=0 ; i < nCount ; i++ ) {
                    int featureIndex = *(int*)pRun;
                    pRun += sizeof(int);
                    
                    int pointCount = *(int*)pRun;
                    pRun += sizeof(int);
                    
                    float *pPoints = NULL;
                    if( pointCount ) {
                        pPoints = (float *) malloc( pointCount * 2 * sizeof(float) );
                        int16_t *p16 = (int16_t *)pRun;
                        for(int ip=0 ; ip < pointCount ; ip++){
                            int16_t x = p16[ip * 2];
                            int16_t y = p16[(ip*2) + 1];
                            pPoints[ip*2] = x / scaler;
                            pPoints[(ip*2) + 1] = y / scaler;
                        }
                    }
                    pRun += pointCount * 2 * sizeof(int16_t);
                    
                    VE_Element *pvee = new VE_Element;
                    pvee->index = featureIndex;
                    pvee->nCount = pointCount;
                    pvee->pPoints = pPoints; 
                    pvee->max_priority = 0;            // Default
                    
                    pVEArray->push_back(pvee);
                    
                }
                
                
                break;
            }
            
            
            
            case VECTOR_CONNECTED_NODE_TABLE_RECORD:
            {
                int buf_len = record.record_length - sizeof(OSENC_Record_Base);
                unsigned char *buf = getBuffer( buf_len);
                if(!fpx.Read(buf, buf_len).IsOk()){
                    dun = 1; break;
                }
                
                //  Parse the buffer
                uint8_t *pRun = (uint8_t *)buf;
                
                // The Feature(Object) count
                int nCount = *(int *)pRun;
                pRun += sizeof(int);
                
                for(int i=0 ; i < nCount ; i++ ) {
                    int featureIndex = *(int*)pRun;
                    
                    pRun += sizeof(int);
#if 0                    
                    double *pPoint = (double *) malloc( 2 * sizeof(double) );
                    memcpy(pPoint, pRun, 2 * sizeof(double));
                    pRun += 2 * sizeof(double);
#else
                    float *pt = (float *)pRun;
                    pt++;
                    
                    float *pPoint = (float *) malloc( 2 * sizeof(float) );
                    memcpy(pPoint, pRun, 2 * sizeof(float));
                    pRun += 2 * sizeof(float);
#endif                    
                    VC_Element *pvce = new VC_Element;
                    pvce->index = featureIndex;
                    pvce->pPoint = pPoint;
                    
                    pVCArray->push_back(pvce);
                }
                
                
                break;
            }
            
            case VECTOR_CONNECTED_NODE_TABLE_EXT_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                OSENC_VCT_RecordExt_Payload *pRec = (OSENC_VCT_RecordExt_Payload *)buf;
                double scaler = pRec->scaleFactor;
                
                //  Parse the buffer
                uint8_t *pRun = (uint8_t *)buf;
                pRun += sizeof(pRec->scaleFactor);
                
                // The Feature(Object) count
                int nCount = *(int *)pRun;
                pRun += sizeof(int);
                
                for(int i=0 ; i < nCount ; i++ ) {
                    int featureIndex = *(int*)pRun;
                    pRun += sizeof(int);
                    
                    float *pPoint = (float *) malloc( 2 * sizeof(float) );
                    int16_t *p16 = (int16_t *)pRun;
                    int16_t x = p16[0];
                    int16_t y = p16[1];
                    pPoint[0] = x / scaler;
                    pPoint[1] = y / scaler;
                    
                    pRun += 2 * sizeof(int16_t);
                    
                    VC_Element *pvce = new VC_Element;
                    pvce->index = featureIndex;
                    pvce->pPoint = pPoint;
                    
                    pVCArray->push_back(pvce);
                }
                
                break;
            }
            
            case CELL_TXTDSC_INFO_FILE_RECORD:
            {
                unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
                if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
                    dun = 1; break;
                }
                
                // Get the payload & parse it
                OSENC_TXTDSCInfo_Record_Payload *pPayload = (OSENC_TXTDSCInfo_Record_Payload *)buf;
                uint8_t *pRun = (uint8_t *)buf;
                
                int nameLength = *(uint32_t *)pRun;
                pRun += sizeof(uint32_t);
                int contentLength = *(uint32_t *)pRun;
                pRun += sizeof(uint32_t);
                
                wxString name = wxString((char *)pRun, wxConvUTF8);
                pRun += nameLength;
                wxString content = wxString((char *)pRun, wxConvUTF8);
                
                //Might not be UTF-8 encoding, even though it should be....
                if(!content.Len()){
                    wxCSConv conv( _T("iso8859-1") );
                    content = wxString((char *)pRun, conv);
                }
                
                // Add the TXTDSC cfile contents to the class hashmap
                if(content.Len())
                    m_TXTDSC_fileMap[name] = content;
                
                break;
            }
            
            default:                            // unrecognized record types
            {
//                 unsigned char *buf = getBuffer( record.record_length - sizeof(OSENC_Record_Base));
//                 if(!fpx.Read(buf, record.record_length - sizeof(OSENC_Record_Base)).IsOk()){
//                     dun = 1; break;
//                 }
                dun = 1;
                break;
            }
                
        }       // switch
            
    }
    
    if(g_debugLevel) wxLogMessage(_T("ingest200 SENC Ingested OK"));
                                     
    return ret_val;
    
}
        


int Osenc::ingestCell( OGRS57DataSource *poS57DS, const wxString &FullPath000, const wxString &working_dir )
{
#if 0    
    //      Analyze Updates
    //      The OGR library will apply updates automatically, if enabled.
    //      Alternatively, we can explicitely find and apply updates from any source directory.
    //      We need to keep track of the last sequential update applied, to look out for new updates
    
    int last_applied_update = 0;
    wxString LastUpdateDate = m_date000.Format( _T("%Y%m%d") );
    
    m_last_applied_update = ValidateAndCountUpdates( FullPath000, working_dir, LastUpdateDate, true );
    m_LastUpdateDate = LastUpdateDate;
    
    if( m_last_applied_update > 0 ){
        wxString msg1;
        msg1.Printf( _T("Preparing to apply ENC updates, target final update is %3d."), last_applied_update );
        wxLogMessage( msg1 );
    }
    
    
    //      Insert my local error handler to catch OGR errors,
    //      Especially CE_Fatal type errors
    //      Discovered/debugged on US5MD11M.017.  VI 548 geometry deleted
    CPLPushErrorHandler( OpenCPN_OGRErrorHandler );
    
    bool bcont = true;
    int iObj = 0;
    OGRwkbGeometryType geoType;
    wxString sobj;
    
    //  Here comes the actual ISO8211 file reading
    
    //  Set up the options
    char ** papszReaderOptions = NULL;
    //    papszReaderOptions = CSLSetNameValue(papszReaderOptions, S57O_LNAM_REFS, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_UPDATES, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_LINKAGES, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES, "ON" );
    poS57DS->SetOptionList( papszReaderOptions );
    
    //      Open the OGRS57DataSource
    //      This will ingest the .000 file from the working dir, and apply updates
    
    int open_return = poS57DS->Open( m_tmpup_array.Item( 0 ).mb_str(), TRUE, NULL/*&s_ProgressCallBack*/ ); ///172
    //if( open_return == BAD_UPDATE )         ///172
    //    bbad_update = true;
    
    //      Get a pointer to the reader
    S57Reader *poReader = poS57DS->GetModule( 0 );
    
    //      Update the options, removing the RETURN_PRIMITIVES flags
    //      This flag needed to be set on ingest() to create the proper field defns,
    //      but cleared to fetch normal features
    
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES, "OFF" );
    poReader->SetOptions( papszReaderOptions );
    CSLDestroy( papszReaderOptions );
#endif    
    return 0;
}


int Osenc::ValidateAndCountUpdates( const wxFileName file000, const wxString CopyDir,
                                       wxString &LastUpdateDate, bool b_copyfiles )
{
    
    int retval = 0;
#if 0    
    //       wxString DirName000 = file000.GetPath((int)(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME));
    //       wxDir dir(DirName000);
    wxArrayString *UpFiles = new wxArrayString;
    retval = s57chart::GetUpdateFileArray( file000, UpFiles, m_date000, m_edtn000);
    
    if( UpFiles->GetCount() ) {
        //      The s57reader of ogr requires that update set be sequentially complete
        //      to perform all the updates.  However, some NOAA ENC distributions are
        //      not complete, as apparently some interim updates have been  withdrawn.
        //      Example:  as of 20 Dec, 2005, the update set for US5MD11M.000 includes
        //      US5MD11M.017, ...018, and ...019.  Updates 001 through 016 are missing.
        //
        //      Workaround.
        //      Create temporary dummy update files to fill out the set before invoking
        //      ogr file open/ingest.  Delete after SENC file create finishes.
        //      Set starts with .000, which has the effect of copying the base file to the working dir
        
        bool chain_broken_mssage_shown = false;
        
        if( b_copyfiles ) {
          
            for( int iff = 0; iff < retval + 1; iff++ ) {
                wxFileName ufile( file000 );
                wxString sext;
                sext.Printf( _T("%03d"), iff );
                ufile.SetExt( sext );
                
                //      Create the target update file name
                wxString cp_ufile = CopyDir;
                if( cp_ufile.Last() != ufile.GetPathSeparator() ) cp_ufile.Append(
                    ufile.GetPathSeparator() );
                
                cp_ufile.Append( ufile.GetFullName() );
                
                //      Explicit check for a short update file, possibly left over from a crash...
                int flen = 0;
                if( ufile.FileExists() ) {
                    wxFile uf( ufile.GetFullPath() );
                    if( uf.IsOpened() ) {
                        flen = uf.Length();
                        uf.Close();
                    }
                }
                
                if( ufile.FileExists() && ( flen > 25 ) )        // a valid update file or base file
                        {
                            //      Copy the valid file to the SENC directory
                            bool cpok = wxCopyFile( ufile.GetFullPath(), cp_ufile );
                            if( !cpok ) {
                                wxString msg( _T("   Cannot copy temporary working ENC file ") );
                                msg.Append( ufile.GetFullPath() );
                                msg.Append( _T(" to ") );
                                msg.Append( cp_ufile );
                                wxLogMessage( msg );
                            }
                        }
                        
                        else {
                            // Create a dummy ISO8211 file with no real content
                            // Correct this.  We should break the walk, and notify the user  See FS#1406
                            
//                             if( !chain_broken_mssage_shown ){
//                                 OCPNMessageBox(NULL, 
//                                                _T("S57 Cell Update chain incomplete.\nENC features may be incomplete or inaccurate.\nCheck the logfile for details."),
//                                                _T("OpenCPN Create SENC Warning"), wxOK | wxICON_EXCLAMATION, 30 );
//                                                chain_broken_mssage_shown = true;
//                             }
                            
                            wxString msg( _T("WARNING---ENC Update chain incomplete. Substituting NULL update file: "));
                            msg += ufile.GetFullName();
                            wxLogMessage(msg);
                            wxLogMessage(_T("   Subsequent ENC updates may produce errors.") );
                            wxLogMessage(_T("   This ENC exchange set should be updated and SENCs rebuilt.") );
                            
                            bool bstat;
                            DDFModule *dupdate = new DDFModule;
                            dupdate->Initialize( '3', 'L', 'E', '1', '0', "!!!", 3, 4, 4 );
                            bstat = !( dupdate->Create( cp_ufile.mb_str() ) == 0 );
                            dupdate->Close();
                            
                            if( !bstat ) {
                                wxString msg( _T("   Error creating dummy update file: ") );
                                msg.Append( cp_ufile );
                                wxLogMessage( msg );
                            }
                        }
                        
                        m_tmpup_array.Add( cp_ufile );
            }
        }
        
        //      Extract the date field from the last of the update files
        //      which is by definition a valid, present update file....
        
        wxFileName lastfile( file000 );
        wxString last_sext;
        last_sext.Printf( _T("%03d"), retval );
        lastfile.SetExt( last_sext );
        
        bool bSuccess;
        DDFModule oUpdateModule;
        
        bSuccess = !( oUpdateModule.Open( lastfile.GetFullPath().mb_str(), TRUE ) == 0 );
        
        if( bSuccess ) {
            //      Get publish/update date
            oUpdateModule.Rewind();
            DDFRecord *pr = oUpdateModule.ReadRecord();                     // Record 0
            
            int nSuccess;
            char *u = NULL;
            
            if( pr ) u = (char *) ( pr->GetStringSubfield( "DSID", 0, "ISDT", 0, &nSuccess ) );
            
            if( u ) {
                if( strlen( u ) ) {
                    LastUpdateDate = wxString( u, wxConvUTF8 );
                }
            } else {
                wxDateTime now = wxDateTime::Now();
                LastUpdateDate = now.Format( _T("%Y%m%d") );
            }
        }
    }
    
    delete UpFiles;
#endif
    
    return retval;
}

bool Osenc::GetBaseFileAttr( const wxString &FullPath000 )
{
    
    DDFModule oModule;
    if( !oModule.Open( FullPath000.mb_str() ) ) {
        return false;
    }
    
    oModule.Rewind();
    
    //    Read and parse DDFRecord 0 to get some interesting data
    //    n.b. assumes that the required fields will be in Record 0....  Is this always true?
    
    DDFRecord *pr = oModule.ReadRecord();                               // Record 0
    //    pr->Dump(stdout);
    
    //    Fetch the Geo Feature Count, or something like it....
    m_nGeoRecords = pr->GetIntSubfield( "DSSI", 0, "NOGR", 0 );
    if( !m_nGeoRecords ) {
        errorMessage = _T("GetBaseFileAttr:  DDFRecord 0 does not contain DSSI:NOGR ") ;
        
        m_nGeoRecords = 1;                // backstop
    }
    
    //  Use ISDT(Issue Date) here, which is the same as UADT(Updates Applied) for .000 files
    wxString date000;
    char *u = (char *) ( pr->GetStringSubfield( "DSID", 0, "ISDT", 0 ) );
    if( u ) date000 = wxString( u, wxConvUTF8 );
    else {
        errorMessage =  _T("GetBaseFileAttr:  DDFRecord 0 does not contain DSID:ISDT ");
        
        date000 = _T("20000101");             // backstop, very early, so any new files will update?
    }
    m_date000.ParseFormat( date000, _T("%Y%m%d") );
    if( !m_date000.IsValid() ) m_date000.ParseFormat( _T("20000101"), _T("%Y%m%d") );
    
    m_date000.ResetTime();
    
    //    Fetch the EDTN(Edition) field
    u = (char *) ( pr->GetStringSubfield( "DSID", 0, "EDTN", 0 ) );
    if( u ) m_edtn000 = wxString( u, wxConvUTF8 );
    else {
        errorMessage =  _T("GetBaseFileAttr:  DDFRecord 0 does not contain DSID:EDTN ");
        
        m_edtn000 = _T("1");                // backstop
    }
    
    //m_SE = m_edtn000;
    
    //      Fetch the Native Scale by reading more records until DSPM is found
    m_native_scale = 0;
    for( ; pr != NULL; pr = oModule.ReadRecord() ) {
        if( pr->FindField( "DSPM" ) != NULL ) {
            m_native_scale = pr->GetIntSubfield( "DSPM", 0, "CSCL", 0 );
            break;
        }
    }
    if( !m_native_scale ) {
        errorMessage = _T("GetBaseFileAttr:  ENC not contain DSPM:CSCL ");
        
        m_native_scale = 1000;                // backstop
    }
    
    return true;
}




//---------------------------------------------------------------------------------------------------
/*
 *      OpenCPN OSENC Version 2 Implementation
 */ 
//---------------------------------------------------------------------------------------------------






int Osenc::createSenc200(const wxString& FullPath000, const wxString& SENCFileName, bool b_showProg)
{
    return 0;
#if 0    
    m_FullPath000 = FullPath000;
    
    m_senc_file_create_version = 200;
    
    if(!m_poRegistrar){
        errorMessage = _T("S57 Registrar not set.");
        return ERROR_REGISTRAR_NOT_SET;
    }
    
    wxFileName SENCfile = wxFileName( SENCFileName );
    wxFileName file000 = wxFileName( FullPath000 );
    
    
    //      Make the target directory if needed
    if( true != SENCfile.DirExists( SENCfile.GetPath() ) ) {
        if( !SENCfile.Mkdir( SENCfile.GetPath() ) ) {
            errorMessage = _T("Cannot create SENC file directory for ") + SENCfile.GetFullPath();
            return ERROR_CANNOT_CREATE_SENC_DIR;
        }
    }
    
    
    //          Make a temp file to create the SENC in
    wxFileName tfn;
    wxString tmp_file = tfn.CreateTempFileName( _T("") );
    
    FILE *fps57;
    const char *pp = "wb";
    fps57 = fopen( tmp_file.mb_str(), pp );
    
    if( fps57 == NULL ) {
        errorMessage = _T("Unable to create temp SENC file: ");
        errorMessage.Append( tfn.GetFullPath() );
        return ERROR_CANNOT_CREATE_TEMP_SENC_FILE;
    }
    
    
    //  Take a quick scan of the 000 file to get some basic attributes of the exchange set.
    if(!GetBaseFileAttr( FullPath000 ) ){
        return ERROR_BASEFILE_ATTRIBUTES;
    }
    
    
    OGRS57DataSource *poS57DS = new OGRS57DataSource;
    poS57DS->SetS57Registrar( m_poRegistrar );
    
    
    //  Ingest the .000 cell, with updates applied
    
    if(ingestCell( poS57DS, FullPath000, SENCfile.GetPath())){
        errorMessage = _T("Error ingesting: ") + FullPath000;
        return ERROR_INGESTING000;
    }
    
    S57Reader *poReader = poS57DS->GetModule( 0 );
    
    bool bcont = true;
    
    //          Write the Header information
    
    char temp[201];
    
    //fprintf( fps57, "SENC Version= %d\n", 200 );
    
    // The chart cell "nice name"
    wxString nice_name;
    s57chart::GetChartNameFromTXT( FullPath000, nice_name );
    
    string sname = "UTF8Error";
    wxCharBuffer buffer=nice_name.ToUTF8();
    if(buffer.data()) 
        sname = buffer.data();

    if( !WriteHeaderRecord200( fps57, HEADER_SENC_VERSION, (uint16_t)m_senc_file_create_version) ){
        fclose( fps57 );
        return ERROR_SENCFILE_ABORT;
    }
    
    if( !WriteHeaderRecord200( fps57, HEADER_CELL_NAME, sname) ){
        fclose( fps57 );
        return ERROR_SENCFILE_ABORT;
    }
    
    wxString date000 = m_date000.Format( _T("%Y%m%d") );
    string sdata = date000.ToStdString();
    if( !WriteHeaderRecord200( fps57, HEADER_CELL_PUBLISHDATE, sdata) ){
        fclose( fps57 );
        return ERROR_SENCFILE_ABORT;
    }

    
    long n000 = 0;
    m_edtn000.ToLong( &n000 );
    if( !WriteHeaderRecord200( fps57, HEADER_CELL_EDITION, (uint16_t)n000) ){
        fclose( fps57 );
        return ERROR_SENCFILE_ABORT;
    }
    
    sdata = m_LastUpdateDate.ToStdString();
    if( !WriteHeaderRecord200( fps57, HEADER_CELL_UPDATEDATE, sdata) ){
        fclose( fps57 );
        return ERROR_SENCFILE_ABORT;
    }
    
    
    if( !WriteHeaderRecord200( fps57, HEADER_CELL_UPDATE, (uint16_t)m_last_applied_update) ){
        fclose( fps57 );
        return ERROR_SENCFILE_ABORT;
    }

    if( !WriteHeaderRecord200( fps57, HEADER_CELL_NATIVESCALE, (uint32_t)m_native_scale) ){
        fclose( fps57 );
        return ERROR_SENCFILE_ABORT;
    }
    
    wxDateTime now = wxDateTime::Now();
    wxString dateNow = now.Format( _T("%Y%m%d") );
    sdata = dateNow.ToStdString();
    if( !WriteHeaderRecord200( fps57, HEADER_CELL_SENCCREATEDATE, sdata) ){
        fclose( fps57 );
        return ERROR_SENCFILE_ABORT;
    }
    
 
    //  Create and write the Coverage table Records
    if(CreateCOVRTables( poReader, m_poRegistrar )){
        if(!CreateCovrRecords(fps57)){
            fclose( fps57 );
            return ERROR_SENCFILE_ABORT;
        }
    }
    
 

    poReader->Rewind();
    
    
    //        Prepare Vector Edge Helper table
    //        And fill in the table
    int feid = 0;
    OGRFeature *pEdgeVectorRecordFeature = poReader->ReadVector( feid, RCNM_VE );
    while( NULL != pEdgeVectorRecordFeature ) {
        int record_id = pEdgeVectorRecordFeature->GetFieldAsInteger( "RCID" );
        
        m_vector_helper_hash[record_id] = feid;
        
        feid++;
        delete pEdgeVectorRecordFeature;
        pEdgeVectorRecordFeature = poReader->ReadVector( feid, RCNM_VE );
    }
    
    wxString Message = SENCfile.GetFullPath();
    Message.Append( _T("...Ingesting") );
    
    wxString Title( _T("OpenCPN S57 SENC File Create...") );
    Title.append( SENCfile.GetFullPath() );
    
    wxStopWatch progsw;
    int nProg = poReader->GetFeatureCount();
    
    if(b_showProg){
        s_ProgDialog = new wxProgressDialog( Title, Message, nProg, NULL,
                                             wxPD_AUTO_HIDE | wxPD_SMOOTH | wxSTAY_ON_TOP | wxPD_APP_MODAL);
    }
    
    
    
    //  Loop in the S57 reader, extracting Features one-by-one
    OGRFeature *objectDef;
    
    int iObj = 0;
    
    while( bcont ) {
        objectDef = poReader->ReadNextFeature();
        
        if( objectDef != NULL ) {
            
            iObj++;
            
            
            //  Update the progress dialog
            //We update only every 200 milliseconds to improve performance as updating the dialog is very expensive...
            // WXGTK is measurably slower even with 100ms here
            if( s_ProgDialog && progsw.Time() > 200 )
            {
                progsw.Start();
                
                wxString sobj = wxString( objectDef->GetDefnRef()->GetName(), wxConvUTF8 );
                sobj.Append( wxString::Format( _T("  %d/%d       "), iObj, nProg ) );
                
                bcont = s_ProgDialog->Update( iObj, sobj );
            }
            
            OGRwkbGeometryType geoType = wkbUnknown;
            //      This test should not be necessary for real (i.e not C_AGGR) features
            //      However... some update files contain errors, and have deleted some
            //      geometry without deleting the corresponding feature(s).
            //      So, GeometryType becomes Unknown.
            //      e.g. US5MD11M.017
            //      In this case, all we can do is skip the feature....sigh.
            
            if( objectDef->GetGeometryRef() != NULL )
                geoType = objectDef->GetGeometryRef()->getGeometryType();
            
            //      n.b  This next line causes skip of C_AGGR features w/o geometry
                if( geoType != wkbUnknown ){                             // Write only if has wkbGeometry
                    CreateSENCRecord200( objectDef, fps57, 1, poReader );
                }
                
                delete objectDef;
                
        } else
            break;
        
    }
    
    if( bcont ) {
        //      Create and write the Vector Edge Table
        CreateSENCVectorEdgeTableRecord200( fps57, poReader );
        
        //      Create and write the Connected NodeTable
        CreateSENCVectorConnectedTableRecord200( fps57, poReader );
    }
    

    //          All done, so clean up
    fclose( fps57 );
    
    CPLPopErrorHandler();
    
    //  Delete any temporary (working) real and dummy update files,
    //  as well as .000 file created by ValidateAndCountUpdates()
    for( unsigned int iff = 0; iff < m_tmpup_array.GetCount(); iff++ )
        remove( m_tmpup_array.Item( iff ).mb_str() );
    
    int ret_code = 0;
    
    if( !bcont )                // aborted
    {
        wxRemoveFile( tmp_file );                     // kill the temp file
        ret_code = ERROR_SENCFILE_ABORT;
    }
    
    if( bcont ) {
        bool cpok = wxRenameFile( tmp_file, SENCfile.GetFullPath() );
        if( !cpok ) {
            errorMessage =  _T("   Cannot rename temporary SENC file ");
            errorMessage.Append( tmp_file );
            errorMessage.Append( _T(" to ") );
            errorMessage.Append( SENCfile.GetFullPath() );
            ret_code = ERROR_SENCFILE_ABORT;
        } else
            ret_code = SENC_NO_ERROR;
    }
    
    delete s_ProgDialog;
    
    return ret_code;
 
#endif
    
}

bool Osenc::CreateCovrRecords(FILE *fpOut)
{
    // First, create the Extent record
    _OSENC_EXTENT_Record record;
    record.record_type = CELL_EXTENT_RECORD;
    record.record_length = sizeof(_OSENC_EXTENT_Record);
    record.extent_sw_lat = m_extent.SLAT;
    record.extent_sw_lon = m_extent.WLON;
    record.extent_nw_lat = m_extent.NLAT;
    record.extent_nw_lon = m_extent.WLON;
    record.extent_ne_lat = m_extent.NLAT;
    record.extent_ne_lon = m_extent.ELON;
    record.extent_se_lat = m_extent.SLAT;
    record.extent_se_lon = m_extent.ELON;

    size_t targetCount = sizeof(record);
    size_t wCount = fwrite (&record , sizeof(char), targetCount, fpOut);
    
    if(wCount != targetCount)
        return false;
    
    
    for(int i=0 ; i < m_nCOVREntries ; i++){
        
        int nPoints = m_pCOVRTablePoints[i];
        
        float *fpbuf = m_pCOVRTable[i];
        
        //  Ready to write the record
        _OSENC_COVR_Record_Base record;
        record.record_type = CELL_COVR_RECORD;
        record.record_length = sizeof(_OSENC_COVR_Record_Base) + sizeof(uint32_t) + (nPoints * 2 * sizeof(float) );
        
        //  Write the base record
        size_t targetCount = sizeof(record);
        size_t wCount = fwrite (&record , sizeof(char), targetCount, fpOut);
        
        if(wCount != targetCount)
            return false;
        
        //Write the point count
        targetCount = sizeof(uint32_t);
        wCount = fwrite (&nPoints , sizeof(char), targetCount, fpOut);
            
        if(wCount != targetCount)
            return false;
            
        //  Write the point array
        targetCount = nPoints * 2 * sizeof(float);
        wCount = fwrite (fpbuf , sizeof(char), targetCount, fpOut);
            
        if(wCount != targetCount)
            return false;
            
    }

    for(int i=0 ; i < m_nNoCOVREntries ; i++){
        
        int nPoints = m_pNoCOVRTablePoints[i];
        
        float *fpbuf = m_pNoCOVRTable[i];
        
        //  Ready to write the record
        _OSENC_NOCOVR_Record_Base record;
        record.record_type = CELL_NOCOVR_RECORD;
        record.record_length = sizeof(_OSENC_NOCOVR_Record_Base) + sizeof(uint32_t) + (nPoints * 2 * sizeof(float) );
        
        //  Write the base record
        size_t targetCount = sizeof(record);
        size_t wCount = fwrite (&record , sizeof(char), targetCount, fpOut);
        
        if(wCount != targetCount)
            return false;
        
        //Write the point count
        targetCount = sizeof(uint32_t);
        wCount = fwrite (&nPoints , sizeof(char), targetCount, fpOut);
            
        if(wCount != targetCount)
            return false;
            
        //  Write the point array
        targetCount = nPoints * 2 * sizeof(float);
        wCount = fwrite (fpbuf , sizeof(char), targetCount, fpOut);
                
        if(wCount != targetCount)
            return false;
                
    }
   
    return true;
}



bool Osenc::WriteHeaderRecord200( FILE *fileOut, int recordType, std::string payload){
    
    int payloadLength = payload.length() + 1;
    int recordLength = payloadLength + sizeof(OSENC_Record_Base);
    
    //  Get a reference to the class persistent buffer
    unsigned char *pBuffer = getBuffer( recordLength );
    
    OSENC_Record *pRecord = (OSENC_Record *)pBuffer;
    memset(pRecord, 0, recordLength);
    pRecord->record_type = recordType;
    pRecord->record_length = recordLength;
    memcpy(&pRecord->payload, payload.c_str(), payloadLength);
    
    size_t targetCount = recordLength;
    size_t wCount = fwrite (pBuffer , sizeof(char), targetCount, fileOut);
    
    if(wCount != targetCount)
        return false;
    else
        return true;
}

bool Osenc::WriteHeaderRecord200( FILE *fileOut, int recordType, uint16_t val){
    
    int payloadLength = sizeof(uint16_t);
    int recordLength = payloadLength + sizeof(OSENC_Record_Base);
    
    //  Get a reference to the class persistent buffer
    unsigned char *pBuffer = getBuffer( recordLength );
    
    OSENC_Record *pRecord = (OSENC_Record *)pBuffer;
    memset(pRecord, 0, recordLength);
    pRecord->record_type = recordType;
    pRecord->record_length = recordLength;
    memcpy(&pRecord->payload, &val, payloadLength);
    
    size_t targetCount = recordLength;
    size_t wCount = fwrite (pBuffer , sizeof(char), targetCount, fileOut);
    
    if(wCount != targetCount)
        return false;
    else
        return true;
}

bool Osenc::WriteHeaderRecord200( FILE *fileOut, int recordType, uint32_t val){
    
    int payloadLength = sizeof(uint32_t);
    int recordLength = payloadLength + sizeof(OSENC_Record_Base);
    
    //  Get a reference to the class persistent buffer
    unsigned char *pBuffer = getBuffer( recordLength );
    
    OSENC_Record *pRecord = (OSENC_Record *)pBuffer;
    memset(pRecord, 0, recordLength);
    pRecord->record_type = recordType;
    pRecord->record_length = recordLength;
    memcpy(&pRecord->payload, &val, payloadLength);
    
    size_t targetCount = recordLength;
    size_t wCount = fwrite (pBuffer , sizeof(char), targetCount, fileOut);
    
    if(wCount != targetCount)
        return false;
    else
        return true;
}

bool Osenc::WriteFIDRecord200( FILE *fileOut, int nOBJL, int featureID, int prim){
   
    
    OSENC_Feature_Identification_Record_Base record;
    memset(&record, 0, sizeof(record));
    
    
    record.record_type = FEATURE_ID_RECORD;
    record.record_length = sizeof(record);

    record.feature_ID = featureID;
    record.feature_type_code = nOBJL;
    record.feature_primitive = prim;
    
    size_t targetCount = sizeof(record);
    size_t wCount = fwrite (&record , sizeof(char), targetCount, fileOut);
    
    if(wCount != targetCount)
        return false;
    else
        return true;
}

bool Osenc::CreateMultiPointFeatureGeometryRecord200( OGRFeature *pFeature, FILE *fpOut)
{
    return false;
}



bool Osenc::CreateLineFeatureGeometryRecord200( S57Reader *poReader, OGRFeature *pFeature, FILE *fpOut)
{
    return false;
    
}

    



bool Osenc::CreateAreaFeatureGeometryRecord200(S57Reader *poReader, OGRFeature *pFeature, FILE * fpOut)
{
    return false;
    
}

void Osenc::CreateSENCVectorEdgeTableRecord200( FILE * fpOut, S57Reader *poReader )
{

}


void Osenc::CreateSENCVectorConnectedTableRecord200( FILE * fpOut, S57Reader *poReader )
{
}




bool Osenc::CreateSENCRecord200( OGRFeature *pFeature, FILE * fpOut, int mode, S57Reader *poReader )
{
    return true;
}

LineGeometryDescriptor *Osenc::BuildLineGeometry( _OSENC_LineGeometry_Record_Payload *pPayload )
{
    LineGeometryDescriptor *pDescriptor = (LineGeometryDescriptor *)malloc(sizeof(LineGeometryDescriptor));
    
    //  Copy some simple stuff
    pDescriptor->extent_e_lon = pPayload->extent_e_lon;
    pDescriptor->extent_w_lon = pPayload->extent_w_lon;
    pDescriptor->extent_s_lat = pPayload->extent_s_lat;
    pDescriptor->extent_n_lat = pPayload->extent_n_lat;
    
    pDescriptor->indexCount = pPayload->edgeVector_count;
    
    // Copy the payload tables
    int table_stride = 3;
    if(m_senc_file_read_version > 200)
        table_stride = 4;
    
    pDescriptor->indexTable = (int *)malloc(pPayload->edgeVector_count * table_stride * sizeof(int));
    memcpy( pDescriptor->indexTable, &pPayload->payLoad, pPayload->edgeVector_count * table_stride * sizeof(int) );
    
    return pDescriptor;
}




//      Build PolyGeo Object from OSENC200 file record
//      Return an integer count of bytes consumed from the record in creating the PolyTessGeo
PolyTessGeo *Osenc::BuildPolyTessGeo(_OSENC_AreaGeometry_Record_Payload *record, unsigned char **next_byte )
{
   
    PolyTessGeo *pPTG = new PolyTessGeo();
    
    pPTG->SetExtents(record->extent_w_lon, record->extent_s_lat, record->extent_e_lon, record->extent_n_lat);

    unsigned int n_TriPrim = record->triprim_count;
    int nContours = record->contour_count;
    
    //  Get a pointer to the payload
    void *payLoad = &record->payLoad;
    
    //  skip over the contour vertex count array, for now  TODO
    uint8_t *pTriPrims = (uint8_t *)payLoad + (nContours * sizeof(uint32_t));
    
  
    //  Create the head of the linked list of TriPrims
    PolyTriGroup *ppg = new PolyTriGroup;
    ppg->m_bSMSENC = true;
    ppg->data_type = DATA_TYPE_DOUBLE;

    
    ppg->nContours = nContours;
    
    ppg->pn_vertex = (int *)malloc(nContours * sizeof(int));
    int *pctr = ppg->pn_vertex;
    
    //  The point count array is the first element in the payload, length is known
    int *contour_pointcount_array_run = (int*)payLoad;
    for(int i=0 ; i < nContours ; i++){
        *pctr++ = *contour_pointcount_array_run++;
    }
    

    //  Read Raw Geometry
//    ppg->pgroup_geom = NULL;
    
    
    //  Now the triangle primitives

    TriPrim **p_prev_triprim = &(ppg->tri_prim_head);
    
    //  Read the PTG_Triangle Geometry in a loop
    unsigned int tri_type;
    int nvert;
    int nvert_max = 0;
    bool not_finished = true;
    int total_byte_size = 2 * sizeof(float);
    
    uint8_t *pPayloadRun = (uint8_t *)contour_pointcount_array_run; //Points to the start of the triangle primitives
        
    for(unsigned int i=0 ; i < n_TriPrim ; i++){
        tri_type = *pPayloadRun++;
        nvert = *(uint32_t *)pPayloadRun;
        pPayloadRun += sizeof(uint32_t);
        
  
        TriPrim *tp = new TriPrim;
        *p_prev_triprim = tp;                               // make the link
        p_prev_triprim = &(tp->p_next);
        tp->p_next = NULL;
        
        tp->type = tri_type;
        tp->nVert = nvert;

        nvert_max = wxMax(nvert_max, nvert);       // Keep a running tab of largest vertex count
        
        //  Read the triangle primitive bounding box as lat/lon
        double *pbb = (double *)pPayloadRun;
        
        #ifdef OCPN_ARMHF
        double abox[4];
        memcpy(&abox[0], pbb, 4 * sizeof(double));
        tp->minxt = abox[0];
        tp->maxxt = abox[1];
        tp->minyt = abox[2];
        tp->maxyt = abox[3];
        #else            
        tp->minxt = *pbb++;
        tp->maxxt = *pbb++;
        tp->minyt = *pbb++;
        tp->maxyt = *pbb;
        #endif
        
        tp->tri_box.Set(tp->minyt, tp->minxt, tp->maxyt, tp->maxxt);
        
        pPayloadRun += 4 * sizeof(double);
        
 
        int byte_size = nvert * 2 * sizeof(float);              // the vertices
        total_byte_size += byte_size;
        
        tp->p_vertex = (double *)malloc(byte_size);
        memcpy(tp->p_vertex, pPayloadRun, byte_size);
        
        
        pPayloadRun += byte_size;

    }
    
    if(next_byte)
        *next_byte = pPayloadRun;

    //  Convert the vertex arrays into a single float memory allocation to enable efficient access later
    unsigned char *vbuf = (unsigned char *)malloc(total_byte_size);

    TriPrim *p_tp = ppg->tri_prim_head;
    unsigned char *p_run = vbuf;
    while( p_tp ) {
            memcpy(p_run, p_tp->p_vertex, p_tp->nVert * 2 * sizeof(float));
            free( p_tp->p_vertex );
            p_tp->p_vertex = (double  *)p_run;
            p_run += p_tp->nVert * 2 * sizeof(float);
            p_tp = p_tp->p_next; // pick up the next in chain
    }
    ppg->bsingle_alloc = true;
    ppg->single_buffer = vbuf;
    ppg->single_buffer_size = total_byte_size;
    ppg->data_type = DATA_TYPE_FLOAT;
    
    pPTG->SetPPGHead(ppg);
    pPTG->SetnVertexMax( nvert_max );
    
    
    pPTG->Set_OK( true );
    
        
    return pPTG;
    
}

PolyTessGeo *Osenc::BuildPolyTessGeoF16(_OSENC_AreaGeometryExt_Record_Payload *record, unsigned char **next_byte )
{
    
    PolyTessGeo *pPTG = new PolyTessGeo();
    
    pPTG->SetExtents(record->extent_w_lon, record->extent_s_lat, record->extent_e_lon, record->extent_n_lat);
    pPTG->m_ref_lat = m_ref_lat;
    pPTG->m_ref_lon = m_ref_lon;
    
    unsigned int n_TriPrim = record->triprim_count;
    int nContours = record->contour_count;
    
    //  Get a pointer to the payload
    void *payLoad = &record->payLoad;
    
    //  skip over the contour vertex count array, for now  TODO
    //uint8_t *pTriPrims = (uint8_t *)payLoad + (nContours * sizeof(uint32_t));
    
    
    //  Create the head of the linked list of TriPrims
    PolyTriGroup *ppg = new PolyTriGroup;
    ppg->m_bSMSENC = true;
    ppg->data_type = DATA_TYPE_DOUBLE;
    
    
    ppg->nContours = nContours;
    
    ppg->pn_vertex = (int *)malloc(nContours * sizeof(int));
    int *pctr = ppg->pn_vertex;
    
    //  The point count array is the first element in the payload, length is known
    int *contour_pointcount_array_run = (int*)payLoad;
    for(int i=0 ; i < nContours ; i++){
        *pctr++ = *contour_pointcount_array_run++;
    }
    
    
    //  Read Raw Geometry
    //ppg->pgroup_geom = NULL;
    
    
    //  Now the triangle primitives
    
    TriPrim **p_prev_triprim = &(ppg->tri_prim_head);
    
    //  Read the PTG_Triangle Geometry in a loop
    unsigned int tri_type;
    int nvert;
    int nvert_max = 0;
    int float_total_byte_size = 2 * sizeof(float);
    
    uint8_t *pPayloadRun = (uint8_t *)contour_pointcount_array_run; //Points to the start of the triangle primitives
    
    double scaler = record->scaleFactor;
    
    if(n_TriPrim){                              // pre-tesselated, or deferred?
        for(unsigned int i=0 ; i < n_TriPrim ; i++){
            tri_type = *pPayloadRun++;
            nvert = *(uint32_t *)pPayloadRun;
            pPayloadRun += sizeof(uint32_t);
            
            
            TriPrim *tp = new TriPrim;
            *p_prev_triprim = tp;                               // make the link
            p_prev_triprim = &(tp->p_next);
            tp->p_next = NULL;
            
            tp->type = tri_type;
            tp->nVert = nvert;
            
            nvert_max = wxMax(nvert_max, nvert);       // Keep a running tab of largest vertex count
            
            //  Read the triangle primitive bounding box as F16 SM coords
            int16_t *pbb = (int16_t *)pPayloadRun;
            
            double minxt, minyt, maxxt, maxyt;
            //double east_min, north_min, east_max, north_max;
            
            fromSM_Plugin( pbb[0] / scaler, pbb[2] / scaler, m_ref_lat, m_ref_lon, &minyt, &minxt );
            fromSM_Plugin( pbb[1] / scaler, pbb[3] / scaler, m_ref_lat, m_ref_lon, &maxyt, &maxxt );
            
            #if 0        
            #ifdef __ARM_ARCH
            double abox[4];
            memcpy(&abox[0], pbb, 4 * sizeof(double));
            
            minxt = abox[0];
            maxxt = abox[1];
            minyt = abox[2];
            maxyt = abox[3];
            #else            
            minxt = *pbb++;
            maxxt = *pbb++;
            minyt = *pbb++;
            maxyt = *pbb;
            #endif
            #endif
            tp->tri_box.Set(minyt, minxt, maxyt, maxxt);
            
            pPayloadRun += 4 * sizeof(int16_t);
            
            
            int float_byte_size = nvert * 2 * sizeof(float);              // the vertices
            float_total_byte_size += float_byte_size;
            
            int byte_size = nvert * 2 * sizeof(int16_t);                 // the vertices
            
            tp->p_vertex = (double *)malloc(byte_size);
            memcpy(tp->p_vertex, pPayloadRun, byte_size);                 //transcribe the uint16_t vertices
            
            
            pPayloadRun += byte_size;
            
        }
    }
    
    if(next_byte)
        *next_byte = pPayloadRun;
    
    if(n_TriPrim){
        
    //  Convert the vertex arrays into a single float memory allocation to enable efficient access later
        unsigned char *vbuf = (unsigned char *)malloc(float_total_byte_size);
        
        TriPrim *p_tp = ppg->tri_prim_head;
        unsigned char *p_run = vbuf;
        
        while( p_tp ) {
            
            float *p_stuff = (float *)p_run;
            int16_t *pf16 = (int16_t *)p_tp->p_vertex;
            
            for(int i=0 ; i < p_tp->nVert ; i++){
                float x = (float)(pf16[i * 2] / scaler);
                float y = (float)(pf16[(i * 2) + 1] / scaler);
                
                p_stuff[i * 2] = x;
                p_stuff[(i * 2) + 1] = y;
            }            
            free(p_tp->p_vertex);
            p_tp->p_vertex = (double  *)p_stuff;
            
            p_run += p_tp->nVert * 2 * sizeof(float);
            p_tp = p_tp->p_next; // pick up the next in chain
            
        }
        ppg->bsingle_alloc = true;
        ppg->single_buffer = vbuf;
        ppg->single_buffer_size = float_total_byte_size;
        ppg->data_type = DATA_TYPE_FLOAT;
        
        pPTG->SetPPGHead(ppg);
        pPTG->SetnVertexMax( nvert_max );
    }
    
    if(n_TriPrim)
        pPTG->Set_OK( true );
    else
        pPTG->Set_OK( false );                  // mark for deferred tesselation
        
    return pPTG;
}



bool Osenc::CreateCOVRTables( S57Reader *poReader, S57ClassRegistrar *poRegistrar )
{
    return true;
}



void Osenc::InitializePersistentBuffer( void ){

    pBuffer = NULL;
    bufferSize = 0;
}

unsigned char *Osenc::getBuffer( size_t length){
    
    if(length > bufferSize){
        pBuffer = (unsigned char *)realloc(pBuffer, length * 2);
        bufferSize = length * 2;
    }
    
    return pBuffer;
        
}

