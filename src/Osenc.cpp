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

#include "mygeom63.h"
//#include <../opencpn/plugins/chartdldr_pi/src/unrar/rartypes.hpp>
//#include "georef.h"

extern int g_debugLevel;
extern wxString g_pipeParm;

using namespace std;

#include <wx/arrimpl.cpp>
WX_DEFINE_ARRAY( float*, MyFloatPtrArray );
WX_DEFINE_OBJARRAY( SENCFloatPtrArray );

//      External definitions
void OpenCPN_OGRErrorHandler( CPLErr eErrClass, int nError,
                              const char * pszErrorMsg );               // installed GDAL OGR library error handler


#ifdef __OCPN__ANDROID__
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
        if(g_debugLevel)printf("   Cannot makeAddr...: %s \n", publicsocket_name);
        wxLogMessage(_T("oesenc_pi: Could not makeAddr for PUBLIC socket"));
    }
    
    publicSocket = socket(AF_LOCAL, SOCK_STREAM, PF_UNIX);
    if (publicSocket < 0) {
        if(g_debugLevel)printf("   Cannot make socket...: %s \n", publicsocket_name);
        wxLogMessage(_T("oesenc_pi: Could not make PUBLIC socket"));
    }
    
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
    
    if(-1 != publicSocket){
        if(g_debugLevel)printf("   Close publicSocket: %s \n", publicsocket_name);
        close( publicSocket );
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

#if defined(__LINUX__) && (!defined __OCPN__ANDROID__)
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
        if(-1 == mkfifo(privatefifo_name, 0666))
            if(g_debugLevel)printf("   mkfifo private failed: %s\n", privatefifo_name);
        else
            if(g_debugLevel)printf("   mkfifo OK: %s\n", privatefifo_name);
        
        
        
        // Open the well known public FIFO for writing
        if( (publicfifo = open(PUBLIC, O_WRONLY | O_NDELAY) ) == -1) {
            wxLogMessage(_T("oesenc_pi: Could not open PUBLIC pipe"));
            return false;
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
            
            int remains = size;
            char *bufRun = (char *)buffer;
            int totalBytesRead = 0;
            int nLoop = MAX_TRIES;
            do{
                int bytes_to_read = MIN(remains, max_read);
                if(bytes_to_read > 10000)
                    int yyp = 2;
                
                int bytesRead = read(privatefifo, bufRun, bytes_to_read );

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
        
        if(!fpx.IsOk())
            return ERROR_SENC_CORRUPT;        
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
        if(m_senc_file_read_version != 200){
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
                if(m_senc_file_read_version != 200){
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
                
//                if(!strncmp("BCNLAT", acronym.c_str(), 6))
//                    int yyp = 0; 
                
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
                    pDescriptor->indexTable = (int *)malloc(pPayload->edgeVector_count * 3 * sizeof(int));
                    memcpy( pDescriptor->indexTable, next_byte,
                            pPayload->edgeVector_count * 3 * sizeof(int) );
                    
                    
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
#if 0    
    OGRGeometry *pGeo = pFeature->GetGeometryRef();
    
    int wkb_len = pGeo->WkbSize();
    unsigned char *pwkb_buffer = (unsigned char *) malloc( wkb_len );
    
    //  Get the GDAL data representation
    pGeo->exportToWkb( wkbNDR, pwkb_buffer );
    
    
    //  Capture a buffer of the raw geometry

    unsigned char *ps = pwkb_buffer;
    ps += 5;
    int nPoints = *( (int *) ps );                     // point count
    
    int sb_len = (  nPoints * 3 * sizeof(float) ) ;    //points as floats
    
    unsigned char *psb_buffer = (unsigned char *) malloc( sb_len );
    unsigned char *pd = psb_buffer;
    
    ps = pwkb_buffer;
    ps += 9;            // skip byte order, type, and count
    
    float *pdf = (float *) pd;
    
    //  Set absurd bbox starting limits
    float lonmax = -1000;
    float lonmin = 1000;
    float latmax = -1000;
    float latmin = 1000;
    
    
    for( int ip = 0; ip < nPoints; ip++ ) {
        
        // Workaround a bug?? in OGRGeometryCollection
        // While exporting point geometries serially, OGRPoint->exportToWkb assumes that
        // if Z is identically 0, then the point must be a 2D point only.
        // So, the collection Wkb is corrupted with some 3D, and some 2D points.
        // Workaround:  Get reference to the points serially, and explicitly read X,Y,Z
        // Ignore the previously read Wkb buffer
        
        OGRGeometryCollection *temp_geometry_collection = (OGRGeometryCollection *) pGeo;
        OGRGeometry *temp_geometry = temp_geometry_collection->getGeometryRef( ip );
        OGRPoint *pt_geom = (OGRPoint *) temp_geometry;
        
        double lon = pt_geom->getX();
        double lat = pt_geom->getY();
        double depth = pt_geom->getZ();
        
        //  Calculate SM from chart common reference point
        double easting, northing;
        toSM( lat, lon, m_ref_lat, m_ref_lon, &easting, &northing );
        
        #ifdef ARMHF
        float east = easting;
        float north = northing;
        float deep = depth;
        memcpy(pdf++, &east, sizeof(float));
        memcpy(pdf++, &north, sizeof(float));
        memcpy(pdf++, &deep, sizeof(float));
        
        #else                    
        *pdf++ = easting;
        *pdf++ = northing;
        *pdf++ = (float) depth;
        #endif
        
        //  Keep a running calculation of min/max
        lonmax = fmax(lon, lonmax);
        lonmin = fmin(lon, lonmin);
        latmax = fmax(lat, latmax);
        latmin = fmin(lat, latmin);
    }
    
    
    
    //  Ready to write the record
    OSENC_MultipointGeometry_Record_Base record;
    record.record_type = FEATURE_GEOMETRY_RECORD_MULTIPOINT;
    record.record_length = sizeof(OSENC_MultipointGeometry_Record_Base) +
                            (nPoints * 3 * sizeof(float) );
    record.extent_e_lon = lonmax;
    record.extent_w_lon = lonmin;
    record.extent_n_lat = latmax;
    record.extent_s_lat = latmin;
    record.point_count = nPoints;
    
    //  Write the base record
    size_t targetCount = sizeof(record);
    size_t wCount = fwrite (&record , sizeof(char), targetCount, fpOut);
    
    if(wCount != targetCount)
        return false;
    
    //  Write the 3D point array
    targetCount = nPoints * 3 * sizeof(float);
    wCount = fwrite (psb_buffer , sizeof(char), targetCount, fpOut);
        
    if(wCount != targetCount)
        return false;
        

    //  Free the buffers
    free( psb_buffer );
    free( pwkb_buffer );
    
    return true;
#endif    
}



bool Osenc::CreateLineFeatureGeometryRecord200( S57Reader *poReader, OGRFeature *pFeature, FILE *fpOut)
{
    return false;
#if 0    
    OGRGeometry *pGeo = pFeature->GetGeometryRef();
    
    int wkb_len = pGeo->WkbSize();
    unsigned char *pwkb_buffer = (unsigned char *) malloc( wkb_len );
    
    //  Get the GDAL data representation
    pGeo->exportToWkb( wkbNDR, pwkb_buffer );
    
    
    //  Capture a buffer of the raw geometry
                    
    int sb_len = ( ( wkb_len - 9 ) / 2 ) + 9 + 16;  // data will be 4 byte float, not double
    
    unsigned char *psb_buffer = (unsigned char *) malloc( sb_len );
    unsigned char *pd = psb_buffer;
    unsigned char *ps = pwkb_buffer;
    
    memcpy( pd, ps, 9 );                                  // byte order, type, and count
    
    int ip = *( (int *) ( ps + 5 ) );                              // point count
    
    pd += 9;
    ps += 9;
    double *psd = (double *) ps;
    float *pdf = (float *) pd;
    
    //  Set absurd bbox starting limits
    float lonmax = -1000;
    float lonmin = 1000;
    float latmax = -1000;
    float latmin = 1000;
    
    for( int i = 0; i < ip; i++ ){                           // convert doubles to floats
                                                         // computing bbox as we go
        float lon, lat;
        double easting, northing;
    #ifdef ARMHF
        double east_d, north_d;
        memcpy(&east_d, psd++, sizeof(double));
        memcpy(&north_d, psd++, sizeof(double));
        lon = east_d;
        lat = north_d;
    
    //  Calculate SM from chart common reference point
        toSM( lat, lon, m_ref_lat, m_ref_lon, &easting, &northing );
        memcpy(pdf++, &easting, sizeof(float));
        memcpy(pdf++, &northing, sizeof(float));
    
    #else                    
        lon = (float) *psd++;
        lat = (float) *psd++;
    
    //  Calculate SM from chart common reference point
        toSM( lat, lon, m_ref_lat, m_ref_lon, &easting, &northing );
    
        *pdf++ = easting;
        *pdf++ = northing;
    #endif
        
        lonmax = fmax(lon, lonmax);
        lonmin = fmin(lon, lonmin);
        latmax = fmax(lat, latmax);
        latmin = fmin(lat, latmin);
    
    }

  


    //    Capture the Vector Table geometry indices into a memory buffer
    int *pNAME_RCID;
    int *pORNT;
    int nEdgeVectorRecords;
    OGRFeature *pEdgeVectorRecordFeature;

    pNAME_RCID = (int *) pFeature->GetFieldAsIntegerList( "NAME_RCID", &nEdgeVectorRecords );
    pORNT = (int *) pFeature->GetFieldAsIntegerList( "ORNT", NULL );

//fprintf( fpOut, "LSINDEXLIST %d\n", nEdgeVectorRecords );
//                    fwrite(pNAME_RCID, 1, nEdgeVectorRecords * sizeof(int), fpOut);


    unsigned char *pvec_buffer = (unsigned char *)malloc(nEdgeVectorRecords * 3 * sizeof(int));
    unsigned char *pvRun = pvec_buffer;
    
    //  Set up the options, adding RETURN_PRIMITIVES
    char ** papszReaderOptions = NULL;
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_UPDATES, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_LINKAGES,"ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES,"ON" );
    poReader->SetOptions( papszReaderOptions );

//    Capture the beginning and end point connected nodes for each edge vector record
    for( int i = 0; i < nEdgeVectorRecords; i++ ) {
        
        int *pI = (int *)pvRun;
        
        int start_rcid, end_rcid;
        int target_record_feid = m_vector_helper_hash[pNAME_RCID[i]];
        pEdgeVectorRecordFeature = poReader->ReadVector( target_record_feid, RCNM_VE );
    
        if( NULL != pEdgeVectorRecordFeature ) {
            start_rcid = pEdgeVectorRecordFeature->GetFieldAsInteger( "NAME_RCID_0" );
            end_rcid = pEdgeVectorRecordFeature->GetFieldAsInteger( "NAME_RCID_1" );
        
        //    Make sure the start and end points exist....
        //    Note this poReader method was converted to Public access to
        //     facilitate this test.  There might be another clean way....
        //    Problem first found on Holand ENC 1R5YM009.000
            if( !poReader->FetchPoint( RCNM_VC, start_rcid, NULL, NULL, NULL, NULL ) )
                start_rcid = -1;
            if( !poReader->FetchPoint( RCNM_VC, end_rcid, NULL, NULL, NULL, NULL ) )
                end_rcid = -2;
        
            int edge_ornt = 1;
        //  Allocate some storage for converted points
        
             if( edge_ornt == 1 ){                                    // forward
                *pI++ = start_rcid;
                *pI++ = pNAME_RCID[i];
                *pI++ = end_rcid;
             //   fwrite( &start_rcid, 1, sizeof(int), fpOut );
             //   fwrite( &pNAME_RCID[i], 1, sizeof(int), fpOut );
             //   fwrite( &end_rcid, 1, sizeof(int), fpOut );
            } else {                                                  // reverse
                *pI++ = end_rcid;
                *pI++ = pNAME_RCID[i];
                *pI++ = start_rcid;

                //fwrite( &end_rcid, 1, sizeof(int), fpOut );
                //fwrite( &pNAME_RCID[i], 1, sizeof(int), fpOut );
                //fwrite( &start_rcid, 1, sizeof(int), fpOut );
            }

            delete pEdgeVectorRecordFeature;
        } else {
            start_rcid = -1;                                    // error indication
            end_rcid = -2;
    
            *pI++ = start_rcid;
            *pI++ = pNAME_RCID[i];
            *pI++ = end_rcid;
            //fwrite( &start_rcid, 1, sizeof(int), fpOut );
            //fwrite( &pNAME_RCID[i], 1, sizeof(int), fpOut );
            //fwrite( &end_rcid, 1, sizeof(int), fpOut );
        }
        
        pvRun += 3 * sizeof(int);
    }


        //  Reset the options
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES,"OFF" );
    poReader->SetOptions( papszReaderOptions );
    CSLDestroy( papszReaderOptions );

    
    //  Ready to write the record
    OSENC_LineGeometry_Record_Base record;
    record.record_type = FEATURE_GEOMETRY_RECORD_LINE;
    record.record_length = sizeof(OSENC_LineGeometry_Record_Base) +
                            (nEdgeVectorRecords * 3 * sizeof(int) );
    record.extent_e_lon = lonmax;
    record.extent_w_lon = lonmin;
    record.extent_n_lat = latmax;
    record.extent_s_lat = latmin;
    record.edgeVector_count = nEdgeVectorRecords;
    
    //  Write the base record
    size_t targetCount = sizeof(record);
    size_t wCount = fwrite (&record , sizeof(char), targetCount, fpOut);
    
    if(wCount != targetCount)
        return false;
    
    //  Write the table index array
    targetCount = nEdgeVectorRecords * 3 * sizeof(int);
    wCount = fwrite (pvec_buffer , sizeof(char), targetCount, fpOut);
        
    if(wCount != targetCount)
        return false;
        

    //  Free the buffers
    free(pvec_buffer);
    free( psb_buffer );
    free( pwkb_buffer );
    
    return true;
#endif
    
}

    



bool Osenc::CreateAreaFeatureGeometryRecord200(S57Reader *poReader, OGRFeature *pFeature, FILE * fpOut)
{
    return false;
#if 0    
    int error_code;
    
    PolyTessGeo *ppg = NULL;
    
    OGRGeometry *pGeo = pFeature->GetGeometryRef();
    OGRPolygon *poly = (OGRPolygon *) ( pGeo );
    
    ppg = new PolyTessGeo( poly, true, m_ref_lat, m_ref_lon, false, m_LOD_meters );   //try to use glu library
    
    error_code = ppg->ErrorCode;
    if( error_code == ERROR_NO_DLL ) {
        //                        if( !bGLUWarningSent ) {
            //                            wxLogMessage( _T("   Warning...Could not find glu32.dll, trying internal tess.") );
            //                            bGLUWarningSent = true;
            //                        }
            
            delete ppg;
            //  Try with internal tesselator
            ppg = new PolyTessGeo( poly, true, m_ref_lat, m_ref_lon, true, m_LOD_meters );
            error_code = ppg->ErrorCode;
    }
    
    if( error_code ){
        wxLogMessage( _T("   Warning: S57 SENC Geometry Error %d, Some Features ignored."), ppg->ErrorCode );
        delete ppg;
        
        return false;
    }
    
    
    
    //  Ready to create the record
    
    //  The base record, with writing deferred until length is known
    OSENC_AreaGeometry_Record_Base baseRecord;
    memset(&baseRecord, 0, sizeof(baseRecord));
    
    baseRecord.record_type = FEATURE_GEOMETRY_RECORD_AREA;
    
    //  Length calculation is deferred...
    
    baseRecord.extent_s_lat = ppg->Get_ymin();
    baseRecord.extent_n_lat = ppg->Get_ymax();
    baseRecord.extent_e_lon = ppg->Get_xmax();
    baseRecord.extent_w_lon = ppg->Get_xmin();
    
    baseRecord.contour_count = ppg->GetnContours();
    
    //  Create the array of contour point counts
    
    int contourPointCountArraySize = ppg->GetnContours() * sizeof(uint32_t);
    uint32_t *contourPointCountArray = (uint32_t *)malloc(contourPointCountArraySize );
    
    uint32_t *pr = contourPointCountArray;
    
    for(int i=0 ; i<ppg->GetnContours() ; i++){
        *pr++ = ppg->Get_PolyTriGroup_head()->pn_vertex[i];
    }
    
    //  All that is left is the tesselation result Triangle lists...
    //  This could be a large array, and we don't want to use a buffer.
    //  Rather, we want to write directly to the output file.
    
    //  So, we 
    //  a  walk the TriPrim chain once to get it's length,
    //  b update the total record length,
    //  c write everything before the TriPrim chain,
    //  d and then directly write the TriPrim chain.
    
    //  Walk the TriPrim chain
    int geoLength = 0;
    
    TriPrim *pTP = ppg->Get_PolyTriGroup_head()->tri_prim_head;         // head of linked list of TriPrims
  
    int n_TriPrims = 0;
    while(pTP)
    {
        geoLength += sizeof(uint8_t) + sizeof(uint32_t);              // type, nvert
        geoLength += pTP->nVert * 2 * sizeof(float);                  // vertices  
        geoLength += 4 * sizeof(double);                              // Primitive bounding box
        pTP = pTP->p_next;
        
        n_TriPrims++;
    }
    
    baseRecord.triprim_count = n_TriPrims;                            //  Set the number of TriPrims
    
 
     //  Create the Vector Edge Index table into a memory buffer
     //  This buffer will follow the triangle buffer in the output stream
    int *pNAME_RCID;
    int *pORNT;
    int nEdgeVectorRecords;
    OGRFeature *pEdgeVectorRecordFeature;
    
    pNAME_RCID = (int *) pFeature->GetFieldAsIntegerList( "NAME_RCID", &nEdgeVectorRecords );
    pORNT = (int *) pFeature->GetFieldAsIntegerList( "ORNT", NULL );
    
    baseRecord.edgeVector_count = nEdgeVectorRecords;
    
    unsigned char *pvec_buffer = (unsigned char *)malloc(nEdgeVectorRecords * 3 * sizeof(int));
    unsigned char *pvRun = pvec_buffer;
    
    //  Set up the options, adding RETURN_PRIMITIVES
    char ** papszReaderOptions = NULL;
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_UPDATES, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_LINKAGES,"ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES,"ON" );
    poReader->SetOptions( papszReaderOptions );
    
    //    Capture the beginning and end point connected nodes for each edge vector record
    for( int i = 0; i < nEdgeVectorRecords; i++ ) {
        
        int *pI = (int *)pvRun;
        
        int start_rcid, end_rcid;
        int target_record_feid = m_vector_helper_hash[pNAME_RCID[i]];
        pEdgeVectorRecordFeature = poReader->ReadVector( target_record_feid, RCNM_VE );
        
        if( NULL != pEdgeVectorRecordFeature ) {
            start_rcid = pEdgeVectorRecordFeature->GetFieldAsInteger( "NAME_RCID_0" );
            end_rcid = pEdgeVectorRecordFeature->GetFieldAsInteger( "NAME_RCID_1" );
            
            //    Make sure the start and end points exist....
            //    Note this poReader method was converted to Public access to
            //     facilitate this test.  There might be another clean way....
            //    Problem first found on Holand ENC 1R5YM009.000
            if( !poReader->FetchPoint( RCNM_VC, start_rcid, NULL, NULL, NULL, NULL ) )
                start_rcid = -1;
            if( !poReader->FetchPoint( RCNM_VC, end_rcid, NULL, NULL, NULL, NULL ) )
                end_rcid = -2;
            
            int edge_ornt = 1;
            //  Allocate some storage for converted points
            
            if( edge_ornt == 1 ){                                    // forward
                *pI++ = start_rcid;
                *pI++ = pNAME_RCID[i];
                *pI++ = end_rcid;
            } else {                                                  // reverse
                *pI++ = end_rcid;
                *pI++ = pNAME_RCID[i];
                *pI++ = start_rcid;
            }
            
            delete pEdgeVectorRecordFeature;
        } else {
            start_rcid = -1;                                    // error indication
            end_rcid = -2;
            
            *pI++ = start_rcid;
            *pI++ = pNAME_RCID[i];
            *pI++ = end_rcid;
        }
        
        pvRun += 3 * sizeof(int);
    }
    
    
    //  Reset the options
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES,"OFF" );
    poReader->SetOptions( papszReaderOptions );
    CSLDestroy( papszReaderOptions );
    

 
    //  Calculate the total record length
    int recordLength = sizeof(OSENC_AreaGeometry_Record_Base);
    recordLength += contourPointCountArraySize;
    recordLength += geoLength;
    recordLength += nEdgeVectorRecords * 3 * sizeof(int);
    baseRecord.record_length = recordLength;
    
    //  Write the base record
    size_t targetCount = sizeof(baseRecord);
    size_t wCount = fwrite (&baseRecord , sizeof(char), targetCount, fpOut);
    
    if(wCount != targetCount)
        return false;
    
    //  Write the contour point count array
    targetCount = contourPointCountArraySize;
    wCount = fwrite (contourPointCountArray , sizeof(char), targetCount, fpOut);
        
    if(wCount != targetCount)
        return false;
    
    //  Walk and transcribe the TriPrim chain
    pTP = ppg->Get_PolyTriGroup_head()->tri_prim_head;         // head of linked list of TriPrims
    while(pTP)
    {
        fwrite (&pTP->type , sizeof(uint8_t), 1, fpOut);
        fwrite (&pTP->nVert , sizeof(uint32_t), 1, fpOut);
        
        fwrite (&pTP->minx , sizeof(double), 1, fpOut);
        fwrite (&pTP->maxx , sizeof(double), 1, fpOut);
        fwrite (&pTP->miny , sizeof(double), 1, fpOut);
        fwrite (&pTP->maxy , sizeof(double), 1, fpOut);
        
        // Testing TODO
        float *pf = (float *)pTP->p_vertex;
        float a = *pf++;
        float b = *pf;
        
        
        fwrite (pTP->p_vertex , sizeof(uint8_t), pTP->nVert * 2 * sizeof(float), fpOut);
            
        pTP = pTP->p_next;
    }
    
    //  Write the Edge Vector index table
    targetCount = nEdgeVectorRecords * 3 * sizeof(int);
    wCount = fwrite (pvec_buffer , sizeof(char), targetCount, fpOut);
    
    if(wCount != targetCount)
        return false;
    
    
    delete ppg;
    free(contourPointCountArray);
    free( pvec_buffer );
    
    return true;
#endif
    
}

void Osenc::CreateSENCVectorEdgeTableRecord200( FILE * fpOut, S57Reader *poReader )
{
#if 0    
    //  We create the payload first, so we can calculate the total record length
    uint8_t *pPayload = NULL;
    int payloadSize = 0;
    uint8_t *pRun = pPayload;
    
    //  Set up the S57Reader options, adding RETURN_PRIMITIVES
    char ** papszReaderOptions = NULL;
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_UPDATES, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_LINKAGES, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES, "ON" );
    poReader->SetOptions( papszReaderOptions );
    
    int feid = 0;
    OGRLineString *pLS = NULL;
    OGRGeometry *pGeo;
    OGRFeature *pEdgeVectorRecordFeature = poReader->ReadVector( feid, RCNM_VE );
    
    //  Read all the EdgeVector Features
    while( NULL != pEdgeVectorRecordFeature ) {

        int new_size = payloadSize + (2 * sizeof(int));
        pPayload = (uint8_t *)realloc(pPayload, new_size );
        pRun = pPayload + payloadSize;                   //  recalculate the running pointer,
                                                        //  since realloc may have moved memory
        payloadSize = new_size;
        
        //  Fetch and store the Record ID
        int record_id = pEdgeVectorRecordFeature->GetFieldAsInteger( "RCID" );
        *(int*)pRun = record_id;
        pRun += sizeof(int);
        
        int nPoints = 0;
        if( pEdgeVectorRecordFeature->GetGeometryRef() != NULL ) {
            pGeo = pEdgeVectorRecordFeature->GetGeometryRef();
            if( pGeo->getGeometryType() == wkbLineString ) {
                pLS = (OGRLineString *) pGeo;
                nPoints = pLS->getNumPoints();
            } else
                nPoints = 0;
        }
        
        //  Transcribe points to a buffer
        
        if(nPoints){
            double *ppd = (double *)malloc(nPoints * sizeof(MyPoint));
            double *ppr = ppd;
            
            for( int i = 0; i < nPoints; i++ ) {
                OGRPoint p;
                pLS->getPoint( i, &p );
                
                //  Calculate SM from chart common reference point
                double easting, northing;
                toSM( p.getY(), p.getX(), m_ref_lat, m_ref_lon, &easting, &northing );
                
                *ppr++ = easting;
                *ppr++ = northing;
            }
            
            //      Reduce the LOD of this linestring
            wxArrayInt index_keep;
            if(nPoints > 5 && (m_LOD_meters > .01)){
                index_keep.Clear();
                index_keep.Add(0);
                index_keep.Add(nPoints-1);
                
                DouglasPeucker(ppd, 0, nPoints-1, m_LOD_meters, &index_keep);
                //               printf("DP Reduction: %d/%d\n", index_keep.GetCount(), nPoints);
                
            }
            else {
                index_keep.Clear();
                for(int i = 0 ; i < nPoints ; i++)
                    index_keep.Add(i);
            }
            
            
            //  Store the point count in the payload
            int nPointReduced = index_keep.GetCount();
            *(int *)pRun = nPointReduced;
            pRun += sizeof(int);
            
            
            //  transcribe the (possibly) reduced linestring to the payload
            
            //  Grow the payload buffer
            int new_size = payloadSize + (nPointReduced* 2 * sizeof(double)) ;
            pPayload = (uint8_t *)realloc(pPayload, new_size );
            pRun = pPayload + payloadSize;              //  recalculate the running pointer,
                                                        //  since realloc may have moved memory
            payloadSize = new_size;
            
            double *npp = (double *)pRun;
            double *npp_run = npp;
            ppr = ppd;  
            for(int ip = 0 ; ip < nPoints ; ip++)
            {
                double x = *ppr++;
                double y = *ppr++;
                
                for(unsigned int j=0 ; j < index_keep.GetCount() ; j++){
                    if(index_keep.Item(j) == ip){
                        *npp_run++ = x;
                        *npp_run++ = y;
                        pRun += 2 * sizeof(double);
                        break;
                    }
                }
            }
            
            free( ppd );
        }
        else{
            //  Store the (zero)point count in the payload
            *(int *)pRun = nPoints;
            pRun += sizeof(int);
        }
        
        //    Next vector record
        feid++;
        delete pEdgeVectorRecordFeature;
        pEdgeVectorRecordFeature = poReader->ReadVector( feid, RCNM_VE );
        
    }   // while
    
    // Now we know the payload length and the Feature count
    
    //  Now write the record out
    OSENC_VET_Record record;
    
    record.record_type = VECTOR_EDGE_NODE_TABLE_RECORD;
    record.record_length = sizeof(OSENC_VET_Record_Base) + payloadSize + sizeof(uint32_t);

    // Write out the record
    fwrite (&record , sizeof(OSENC_VET_Record_Base), 1, fpOut);

    //  Write out the Feature(Object) count
    fwrite (&feid, sizeof(uint32_t), 1, fpOut);
    
    //  Write out the payload
    fwrite (pPayload, sizeof(uint8_t), payloadSize, fpOut);
    
    //  All done with buffer
    free(pPayload);
    
    //  Reset the S57Reader options
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES, "OFF" );
    poReader->SetOptions( papszReaderOptions );
    CSLDestroy( papszReaderOptions );
    
#endif

}


void Osenc::CreateSENCVectorConnectedTableRecord200( FILE * fpOut, S57Reader *poReader )
{
#if 0    
    //  We create the payload first, so we can calculate the total record length
    uint8_t *pPayload = NULL;
    int payloadSize = 0;
    uint8_t *pRun = pPayload;
    
    //  Set up the S57Reader options, adding RETURN_PRIMITIVES
    char ** papszReaderOptions = NULL;
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_UPDATES, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_LINKAGES, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES, "ON" );
    poReader->SetOptions( papszReaderOptions );
 
    
    int feid = 0;
    OGRPoint *pP;
    OGRGeometry *pGeo;
    OGRFeature *pConnNodeRecordFeature = poReader->ReadVector( feid, RCNM_VC );
    int featureCount = 0;
    
    //  Read all the ConnectedVector Features
    while( NULL != pConnNodeRecordFeature ) {
        
         
        if( pConnNodeRecordFeature->GetGeometryRef() != NULL ) {
            pGeo = pConnNodeRecordFeature->GetGeometryRef();
            if( pGeo->getGeometryType() == wkbPoint ) {
                
                
                int new_size = payloadSize + sizeof(int) + (2 * sizeof(double));
                pPayload = (uint8_t *)realloc(pPayload, new_size );
                pRun = pPayload + payloadSize;                   //  recalculate the running pointer,
                                                                 //  since realloc may have moved memory
                payloadSize = new_size;
                
                //  Fetch and store the Record ID
                int record_id = pConnNodeRecordFeature->GetFieldAsInteger( "RCID" );
                *(int*)pRun = record_id;
                pRun += sizeof(int);
                
                
                
                pP = (OGRPoint *) pGeo;
                
                //  Calculate SM from chart common reference point
                double easting, northing;
                toSM( pP->getY(), pP->getX(), m_ref_lat, m_ref_lon, &easting, &northing );
                
                MyPoint pd;
                pd.x = easting;
                pd.y = northing;
                memcpy(pRun, &pd, sizeof(MyPoint));
                
                featureCount++;
            }
        }
        
        //    Next vector record
        feid++;
        delete pConnNodeRecordFeature;
        pConnNodeRecordFeature = poReader->ReadVector( feid, RCNM_VC );
    }   // while
    
    // Now we know the payload length and the Feature count
    
    //  Now write the record out
    OSENC_VCT_Record record;
    
    record.record_type = VECTOR_CONNECTED_NODE_TABLE_RECORD;
    record.record_length = sizeof(OSENC_VCT_Record_Base) + payloadSize;
    
    // Write out the record
    fwrite (&record , sizeof(OSENC_VCT_Record_Base), 1, fpOut);
    
    //  Write out the Feature(Object) count
    fwrite (&featureCount, sizeof(uint32_t), 1, fpOut);
    
    //  Write out the payload
    fwrite (pPayload, sizeof(uint8_t), payloadSize, fpOut);
    
    //  All done with buffer
    free(pPayload);
    
    //  Reset the S57Reader options
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES, "OFF" );
    poReader->SetOptions( papszReaderOptions );
    CSLDestroy( papszReaderOptions );
#endif    
}




bool Osenc::CreateSENCRecord200( OGRFeature *pFeature, FILE * fpOut, int mode, S57Reader *poReader )
{
    return true;
#if 0    
    
    //  Create the Feature Identification Record
    
    // Fetch the S57 Standard Object Class identifier    
    OGRFeatureDefn *pFD = pFeature->GetDefnRef();
    int nOBJL = pFD->GetOBJL();
    
    OGRGeometry *pGeo = pFeature->GetGeometryRef();
    OGRwkbGeometryType gType = pGeo->getGeometryType();
    
    int primitive = 0;
    switch(gType){
        
        case wkbLineString:
            primitive = GEO_LINE;
            break;
        case wkbPoint:
            primitive = GEO_POINT;
            break;
        case wkbPolygon:
            primitive = GEO_AREA;
            break;
        default:
            primitive = 0;
    }
    

    if(!WriteFIDRecord200( fpOut, nOBJL, pFeature->GetFID(), primitive) )
        return false;
        
    
    #define MAX_HDR_LINE    400
    
    char line[MAX_HDR_LINE + 1];
    wxString sheader;
    
//    fprintf( fpOut, "OGRFeature(%s):%ld\n", pFeature->GetDefnRef()->GetName(), pFeature->GetFID() );
    
    //  In a loop, fetch attributes, and create OSENC Feature Attribute Records
    //  In the interests of output file size, DO NOT report fields that are not set.

    //TODO Debugging
    if(!strncmp(pFeature->GetDefnRef()->GetName(), "LNDMRK", 6))
        int yyp = 4;
    
    if(pFeature->GetFID() == 6919)
        int yyp = 4;
    
    int payloadLength = 0;
    void *payloadBuffer = NULL;
    unsigned int payloadBufferLength = 0;
    
    for( int iField = 0; iField < pFeature->GetFieldCount(); iField++ ) {
        if( pFeature->IsFieldSet( iField ) ) {
            if( ( iField == 1 ) || ( iField > 7 ) ) {
                OGRFieldDefn *poFDefn = pFeature->GetDefnRef()->GetFieldDefn( iField );
                //const char *pType = OGRFieldDefn::GetFieldTypeName( poFDefn->GetType() );
                const char *pAttrName = poFDefn->GetNameRef();
                const char *pAttrVal = pFeature->GetFieldAsString( iField );
                
                //TODO Debugging
                if(!strncmp(pAttrName, "CATLIT", 6))
                    int yyp = 2;
                
                //  Use the OCPN Registrar Manager to map attribute acronym to an identifier.
                //  The mapping is defined by the file {csv_dir}/s57attributes.csv
                int attributeID = m_pRegistrarMan->getAttributeID(pAttrName);
                
                //  Determine the {attribute_value_type} needed in the record
                int OGRvalueType = (int)poFDefn->GetType();
                int valueType = 0;

                //Check for special cases
                if( -1 == attributeID){
                    if(!strncmp(pAttrName, "PRIM", 4) ){
                        attributeID = ATTRIBUTE_ID_PRIM;
                    }
                }
                
#if 0                
                /** Simple 32bit integer */                   OFTInteger = 0,
                /** List of 32bit integers */                 OFTIntegerList = 1,
                /** Double Precision floating point */        OFTReal = 2,
                /** List of doubles */                        OFTRealList = 3,
                /** String of ASCII chars */                  OFTString = 4,
                /** Array of strings */                       OFTStringList = 5,
                /** Double byte string (unsupported) */       OFTWideString = 6,
                /** List of wide strings (unsupported) */     OFTWideStringList = 7,
                /** Raw Binary data (unsupported) */          OFTBinary = 8
#endif
                switch(OGRvalueType){
                    case 0:             // Single integer
                    {
                        valueType = OGRvalueType;
                        
                        if(payloadBufferLength < 4){
                            payloadBuffer = realloc(payloadBuffer, 4);
                            payloadBufferLength = 4;
                        }
                        
                        int aValue = pFeature->GetFieldAsInteger( iField );
                        memcpy(payloadBuffer, &aValue, sizeof(int) );
                        payloadLength = sizeof(int);
                        
                        break;
                    }   
                    case 1:             // Integer list
                    {
                        valueType = OGRvalueType;
                        
                        int nCount = 0;
                        const int *aValueList = pFeature->GetFieldAsIntegerList( iField, &nCount );

                        if(payloadBufferLength < nCount * sizeof(int)){
                            payloadBuffer = realloc(payloadBuffer, nCount * sizeof(int));
                            payloadBufferLength = nCount * sizeof(int);
                        }
                        
                        int *pBuffRun = (int *)payloadBuffer;
                        for(int i=0 ; i < nCount ; i++){
                            *pBuffRun++ = aValueList[i];
                        }                            
                        payloadLength = nCount * sizeof(int);

                        break;
                    }
                    case 2:             // Single double precision real
                    {
                        valueType = OGRvalueType;
                        
                        if(payloadBufferLength < sizeof(double)){
                            payloadBuffer = realloc(payloadBuffer, sizeof(double));
                            payloadBufferLength = sizeof(double);
                        }
                        
                        double aValue = pFeature->GetFieldAsDouble( iField );
                        memcpy(payloadBuffer, &aValue, sizeof(double) );
                        payloadLength = sizeof(double);
                        
                        break;
                    }   

                    case 3:             // List of double precision real
                    {
                        valueType = OGRvalueType;
                        
                        int nCount = 0;
                        const double *aValueList = pFeature->GetFieldAsDoubleList( iField, &nCount );
                        
                        if(payloadBufferLength < nCount * sizeof(double)){
                            payloadBuffer = realloc(payloadBuffer, nCount * sizeof(double));
                            payloadBufferLength = nCount * sizeof(double);
                        }
                        
                        double *pBuffRun = (double *)payloadBuffer;
                        for(int i=0 ; i < nCount ; i++){
                            *pBuffRun++ = aValueList[i];
                        }                            
                        payloadLength = nCount * sizeof(double);
                        
                        break;
                    }
                    
                    case 4:             // Ascii String
                    {
                        valueType = OGRvalueType;
                        
                        const char *pAttrVal = pFeature->GetFieldAsString( iField );
                        unsigned int stringPayloadLength = strlen(pAttrVal);

                        if(stringPayloadLength){
                            if(payloadBufferLength < stringPayloadLength + 1){
                                payloadBuffer = realloc(payloadBuffer, stringPayloadLength + 1);
                                payloadBufferLength = stringPayloadLength + 1;
                            }
                            
                            strcpy((char *)payloadBuffer, pAttrVal );
                            payloadLength = stringPayloadLength + 1;
                        }
                        else
                            attributeID = -1;                   // cancel this attribute record
                            
                        
                        break;
                    }
                    
                    default:
                        valueType = -1;
                        break;
                }

                if( -1 != attributeID){
                    // Build the record
                    int recordLength = sizeof( OSENC_Attribute_Record_Base ) + payloadLength;
                
                    //  Get a reference to the class persistent buffer
                    unsigned char *pBuffer = getBuffer( recordLength );
                
                    OSENC_Attribute_Record *pRecord = (OSENC_Attribute_Record *)pBuffer;
                    memset(pRecord, 0, sizeof(OSENC_Attribute_Record));
                    pRecord->record_type = FEATURE_ATTRIBUTE_RECORD;
                    pRecord->record_length = recordLength;
                    pRecord->attribute_type = attributeID;
                    pRecord->attribute_value_type = valueType;
                    memcpy( &pRecord->payload, payloadBuffer, payloadLength );
                    
                    // Write the record out....
                    size_t targetCount = recordLength;
                    size_t wCount = fwrite (pBuffer , sizeof(char), targetCount, fpOut);
                    
                    if(wCount != targetCount)
                        return false;
                    
                }

            }
        }
    }
    
    
#if 0    
    //    Special geometry cases
    ///172
    if( wkbPoint == pGeo->getGeometryType() ) {
        OGRPoint *pp = (OGRPoint *) pGeo;
        int nqual = pp->getnQual();
        if( 10 != nqual )                    // only add attribute if nQual is not "precisely known"
                {
                    snprintf( line, MAX_HDR_LINE - 2, "  %s (%c) = %d", "QUALTY", 'I', nqual );
                    sheader += wxString( line, wxConvUTF8 );
                    sheader += '\n';
                }
                
    }
    
    if( mode == 1 ) {
        sprintf( line, "  %s %f %f\n", pGeo->getGeometryName(), m_ref_lat, m_ref_lon );
        sheader += wxString( line, wxConvUTF8 );
    }
    
    wxCharBuffer buffer=sheader.ToUTF8();
    fprintf( fpOut, "HDRLEN=%lu\n", (unsigned long) strlen(buffer) );
    fwrite( buffer.data(), 1, strlen(buffer), fpOut );

#endif

    if( ( pGeo != NULL ) ) {
        
        wxString msg;
        
        OGRwkbGeometryType gType = pGeo->getGeometryType();
        switch( gType ){
            
            case wkbLineString: {
                
                if( !CreateLineFeatureGeometryRecord200(poReader, pFeature, fpOut) )
                    return false;
                
                break;
                
            case wkbPoint: {
                
                OSENC_PointGeometry_Record record;
                record.record_type = FEATURE_GEOMETRY_RECORD_POINT;
                record.record_length = sizeof( record );

                int wkb_len = pGeo->WkbSize();
                unsigned char *pwkb_buffer = (unsigned char *) malloc( wkb_len );
                
                //  Get the GDAL data representation
                pGeo->exportToWkb( wkbNDR, pwkb_buffer );
                
                int nq_len = 4;                                     // nQual length
                unsigned char *ps = pwkb_buffer;
                
                ps += 5 + nq_len;
                double *psd = (double *) ps;
                
                double lat, lon;
                #ifdef ARMHF
                double lata, lona;
                memcpy(&lona, psd, sizeof(double));
                memcpy(&lata, &psd[1], sizeof(double));
                lon = lona;
                lat = lata;
                #else                
                lon = *psd++;                                      // fetch the point
                lat = *psd;
                #endif                

                free( pwkb_buffer );
                
                record.lat = lat;
                record.lon = lon;
                
                // Write the record out....
                size_t targetCount = record.record_length;
                size_t wCount = fwrite (&record, sizeof(char), targetCount, fpOut);
                
                if(wCount != targetCount)
                    return false;
                
                break;
            }
            
            case wkbMultiPoint:
            case wkbMultiPoint25D:{
                
                if(!CreateMultiPointFeatureGeometryRecord200( pFeature, fpOut))
                    return false;
                
                break;
            }
                
#if 1                
                //      Special case, polygons are handled separately
                case wkbPolygon: {
                    
                    if( !CreateAreaFeatureGeometryRecord200(poReader, pFeature, fpOut) )
                        return false;
                }
                   
                    break;
                }
#endif                
                //      All others
                default:
                    msg = _T("   Warning: Unimplemented ogr geotype record ");
                    wxLogMessage( msg );
                    
                    break;
        }       // switch
        
    }


    free( payloadBuffer );
    return true;
#endif    
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
    pDescriptor->indexTable = (int *)malloc(pPayload->edgeVector_count * 3 * sizeof(int));
    memcpy( pDescriptor->indexTable, &pPayload->payLoad, pPayload->edgeVector_count * 3 * sizeof(int) );
    
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
        
        #ifdef ARMHF
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
        
        tp->box.Set(tp->minyt, tp->minxt, tp->maxyt, tp->maxxt);
        
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



bool Osenc::CreateCOVRTables( S57Reader *poReader, S57ClassRegistrar *poRegistrar )
{
#if 0    
    OGRFeature *pFeat;
    int catcov;
    float LatMax, LatMin, LonMax, LonMin;
    LatMax = -90.;
    LatMin = 90.;
    LonMax = -179.;
    LonMin = 179.;
    
    m_pCOVRTablePoints = NULL;
    m_pCOVRTable = NULL;
    
    //  Create arrays to hold geometry objects temporarily
    MyFloatPtrArray *pAuxPtrArray = new MyFloatPtrArray;
    wxArrayInt *pAuxCntArray = new wxArrayInt;
    
    MyFloatPtrArray *pNoCovrPtrArray = new MyFloatPtrArray;
    wxArrayInt *pNoCovrCntArray = new wxArrayInt;
    
    //Get the first M_COVR object
    pFeat = GetChartFirstM_COVR( catcov, poReader, poRegistrar );
    
    while( pFeat ) {
        //    Get the next M_COVR feature, and create possible additional entries for COVR
        OGRPolygon *poly = (OGRPolygon *) ( pFeat->GetGeometryRef() );
        OGRLinearRing *xring = poly->getExteriorRing();
        
        int npt = xring->getNumPoints();
        
        float *pf = NULL;
        
        if( npt >= 3 ) {
            pf = (float *) malloc( 2 * npt * sizeof(float) );
            float *pfr = pf;
            
            for( int i = 0; i < npt; i++ ) {
                OGRPoint p;
                xring->getPoint( i, &p );
                
                if( catcov == 1 ) {
                    LatMax = fmax(LatMax, p.getY());
                    LatMin = fmin(LatMin, p.getY());
                    LonMax = fmax(LonMax, p.getX());
                    LonMin = fmin(LonMin, p.getX());
                }
                
                pfr[0] = p.getY();             // lat
                pfr[1] = p.getX();             // lon
                
                pfr += 2;
            }
            
            if( catcov == 1 ) {
                pAuxPtrArray->Add( pf );
                pAuxCntArray->Add( npt );
            }
            else if( catcov == 2 ){
                pNoCovrPtrArray->Add( pf );
                pNoCovrCntArray->Add( npt );
            }
        }
        
        
        delete pFeat;
        pFeat = GetChartNextM_COVR( catcov, poReader );
    }         // while
    
    //    Allocate the storage
    
    m_nCOVREntries = pAuxCntArray->GetCount();
    
    //    If only one M_COVR,CATCOV=1 object was found,
    //    assign the geometry to the one and only COVR
    
    if( m_nCOVREntries == 1 ) {
        m_pCOVRTablePoints = (int *) malloc( sizeof(int) );
        *m_pCOVRTablePoints = pAuxCntArray->Item( 0 );
        m_pCOVRTable = (float **) malloc( sizeof(float *) );
        *m_pCOVRTable = (float *) malloc( pAuxCntArray->Item( 0 ) * 2 * sizeof(float) );
        memcpy( *m_pCOVRTable, pAuxPtrArray->Item( 0 ),
                pAuxCntArray->Item( 0 ) * 2 * sizeof(float) );
    }
    
    else if( m_nCOVREntries > 1 ) {
        //    Create new COVR entries
        m_pCOVRTablePoints = (int *) malloc( m_nCOVREntries * sizeof(int) );
        m_pCOVRTable = (float **) malloc( m_nCOVREntries * sizeof(float *) );
        
        for( unsigned int j = 0; j < (unsigned int) m_nCOVREntries; j++ ) {
            m_pCOVRTablePoints[j] = pAuxCntArray->Item( j );
            m_pCOVRTable[j] = (float *) malloc( pAuxCntArray->Item( j ) * 2 * sizeof(float) );
            memcpy( m_pCOVRTable[j], pAuxPtrArray->Item( j ),
                    pAuxCntArray->Item( j ) * 2 * sizeof(float) );
        }
    }
    
    else                                     // strange case, found no CATCOV=1 M_COVR objects
        {
            wxString msg( _T("   ENC contains no useable M_COVR, CATCOV=1 features:  ") );
            msg.Append( m_FullPath000 );
            wxLogMessage( msg );
        }
        
        
        //      And for the NoCovr regions
        m_nNoCOVREntries = pNoCovrCntArray->GetCount();
    
    if( m_nNoCOVREntries ) {
        //    Create new NoCOVR entries
        m_pNoCOVRTablePoints = (int *) malloc( m_nNoCOVREntries * sizeof(int) );
        m_pNoCOVRTable = (float **) malloc( m_nNoCOVREntries * sizeof(float *) );
        
        for( unsigned int j = 0; j < (unsigned int) m_nNoCOVREntries; j++ ) {
            int npoints = pNoCovrCntArray->Item( j );
            m_pNoCOVRTablePoints[j] = npoints;
            m_pNoCOVRTable[j] = (float *) malloc( npoints * 2 * sizeof(float) );
            memcpy( m_pNoCOVRTable[j], pNoCovrPtrArray->Item( j ),
                    npoints * 2 * sizeof(float) );
        }
    }
    else {
        m_pNoCOVRTablePoints = NULL;
        m_pNoCOVRTable = NULL;
    }
    
    delete pAuxPtrArray;
    delete pAuxCntArray;
    delete pNoCovrPtrArray;
    delete pNoCovrCntArray;
    
    
    if( 0 == m_nCOVREntries ) {                        // fallback
        wxString msg( _T("   ENC contains no M_COVR features:  ") );
        msg.Append( m_FullPath000 );
        wxLogMessage( msg );
        
        msg =  _T("   Calculating Chart Extents as fallback.");
        wxLogMessage( msg );
        
        OGREnvelope Env;
        
        if( poReader->GetExtent( &Env, true ) == OGRERR_NONE ) {
            
            LatMax = Env.MaxY;
            LonMax = Env.MaxX;
            LatMin = Env.MinY;
            LonMin = Env.MinX;
            
            m_nCOVREntries = 1;
            m_pCOVRTablePoints = (int *) malloc( sizeof(int) );
            *m_pCOVRTablePoints = 4;
            m_pCOVRTable = (float **) malloc( sizeof(float *) );
            float *pf = (float *) malloc( 2 * 4 * sizeof(float) );
            *m_pCOVRTable = pf;
            float *pfe = pf;
            
            *pfe++ = LatMax;
            *pfe++ = LonMin;
            
            *pfe++ = LatMax;
            *pfe++ = LonMax;
            
            *pfe++ = LatMin;
            *pfe++ = LonMax;
            
            *pfe++ = LatMin;
            *pfe++ = LonMin;
            
        } else {
            wxString msg( _T("   Cannot calculate Extents for ENC:  ") );
            msg.Append( m_FullPath000 );
            wxLogMessage( msg );
            
            return false;                     // chart is completely unusable
        }
    }
    
    //    Populate the oSENC clone of the chart's extent structure
     m_extent.NLAT = LatMax;
     m_extent.SLAT = LatMin;
     m_extent.ELON = LonMax;
     m_extent.WLON = LonMin;
    
#endif    
    return true;
}

#if 0
OGRFeature *Osenc::GetChartFirstM_COVR( int &catcov, S57Reader *pENCReader, S57ClassRegistrar *poRegistrar )
{
  
    if( ( NULL != pENCReader ) && ( NULL != poRegistrar ) ) {
        
        //      Select the proper class
        poRegistrar->SelectClass( "M_COVR" );

        //      find this feature
        bool bFound = false;
        OGRFeature *pobjectDef = pENCReader->ReadNextFeature( );
        while(!bFound){
            if( pobjectDef ) {
            
                OGRFeatureDefn *poDefn = pobjectDef->GetDefnRef();
                if(poDefn && (poDefn->GetOBJL() == 302/*poRegistrar->GetOBJL()*/)){
                //  Fetch the CATCOV attribute
                    catcov = pobjectDef->GetFieldAsInteger( "CATCOV" );
                    return pobjectDef;
                }
            }
            else
                return NULL;
        
            pobjectDef = pENCReader->ReadNextFeature( );
        }
    }
    
    return NULL;
}
#endif

#if 0
OGRFeature *Osenc::GetChartNextM_COVR( int &catcov, S57Reader *pENCReader )
{
    catcov = -1;
    
    
    
    if( pENCReader ) {
        bool bFound = false;
        OGRFeature *pobjectDef = pENCReader->ReadNextFeature( );
        
        while(!bFound){
            if( pobjectDef ) {
                OGRFeatureDefn *poDefn = pobjectDef->GetDefnRef();
                if(poDefn && (poDefn->GetOBJL() == 302)){
                //  Fetch the CATCOV attribute
                    catcov = pobjectDef->GetFieldAsInteger( "CATCOV" );
                    return pobjectDef;
                }
            }
            else
                return NULL;
            
            pobjectDef = pENCReader->ReadNextFeature( );
            
        }
    }
    
    return NULL;
}
#endif












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

