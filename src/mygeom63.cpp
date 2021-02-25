/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Tesselated Polygon Object
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register   *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 *
 *
 */

#include <math.h>
// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
#include "wx/wx.h"
#endif //precompiled headers

#include "wx/tokenzr.h"
#include <wx/mstream.h>


#include "config.h"
#include "mygeom.h"
#include "ocpn_plugin.h"

#ifdef ocpnUSE_GL

//#ifdef USE_GLU_TESS
#ifdef __WXOSX__
#include "GL/gl.h"
#include "GL/glu.h"
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

//#endif

#ifdef __WXMSW__
#include <windows.h>
#endif

#endif

//------------------------------------------------------------------------------
//          Some local definitions for opengl/glu types,
//            just enough to build the glu tesselator option.
//          Included here to avoid having to find and include
//            the Microsoft versions of gl.h and glu.h.
//          You are welcome.....
//------------------------------------------------------------------------------
/*
#ifdef __WXMSW__
class GLUtesselator;

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

#define GLU_TESS_BEGIN                     100100
#define GLU_TESS_VERTEX                    100101
#define GLU_TESS_END                       100102
#define GLU_TESS_ERROR                     GLU_ERROR
#define GLU_TESS_EDGE_FLAG                 100104
#define GLU_TESS_COMBINE                   100105
#define GLU_TESS_BEGIN_DATA                100106
#define GLU_TESS_VERTEX_DATA               100107
#define GLU_TESS_END_DATA                  100108
#define GLU_TESS_ERROR_DATA                100109
#define GLU_TESS_EDGE_FLAG_DATA            100110
#define GLU_TESS_COMBINE_DATA              100111
#define GLU_BEGIN                          GLU_TESS_BEGIN
#define GLU_VERTEX                         GLU_TESS_VERTEX
#define GLU_END                            GLU_TESS_END
#define GLU_EDGE_FLAG                      GLU_TESS_EDGE_FLAG
#define GLU_CW                             100120
#define GLU_CCW                            100121
#define GLU_INTERIOR                       100122
#define GLU_EXTERIOR                       100123
#define GLU_UNKNOWN                        100124
#define GLU_TESS_WINDING_RULE              100140
#define GLU_TESS_BOUNDARY_ONLY             100141
#define GLU_TESS_TOLERANCE                 100142
#define GLU_TESS_ERROR1                    100151
#define GLU_TESS_ERROR2                    100152
#define GLU_TESS_ERROR3                    100153
#define GLU_TESS_ERROR4                    100154
#define GLU_TESS_ERROR5                    100155
#define GLU_TESS_ERROR6                    100156
#define GLU_TESS_ERROR7                    100157
#define GLU_TESS_ERROR8                    100158
#define GLU_TESS_MISSING_BEGIN_POLYGON     100151
#define GLU_TESS_MISSING_BEGIN_CONTOUR     100152
#define GLU_TESS_MISSING_END_POLYGON       100153
#define GLU_TESS_MISSING_END_CONTOUR       100154
#define GLU_TESS_COORD_TOO_LARGE           100155
#define GLU_TESS_NEED_COMBINE_CALLBACK     100156
#define GLU_TESS_WINDING_ODD               100130
#define GLU_TESS_WINDING_NONZERO           100131
#define GLU_TESS_WINDING_POSITIVE          100132
#define GLU_TESS_WINDING_NEGATIVE          100133
#define GLU_TESS_WINDING_ABS_GEQ_TWO       100134

#define GL_TRIANGLES                       0x0004
#define GL_TRIANGLE_STRIP                  0x0005
#define GL_TRIANGLE_FAN                    0x0006

#endif
*/

//      Module Internal Prototypes


//#ifdef USE_GLU_TESS
static int            s_nvcall;
static int            s_nvmax;
static double         *s_pwork_buf;
static int            s_buf_len;
static int            s_buf_idx;
static unsigned int   s_gltri_type;
TriPrim               *s_pTPG_Head;
TriPrim               *s_pTPG_Last;
static GLUtesselator  *GLUtessobj;
static double         s_ref_lat;
static double         s_ref_lon;

static bool           s_bmerc_transform;
static double         s_transform_x_rate;
static double         s_transform_x_origin;
static double         s_transform_y_rate;
static double         s_transform_y_origin;
wxArrayPtrVoid        *s_pCombineVertexArray;
static int            tess_orient;
static bool           s_bSENC_SM;

static const double   CM93_semimajor_axis_meters = 6378388.0;            // CM93 semimajor axis

//#endif


int s_idx;


//  For __WXMSW__ builds using GLU_TESS and glu32.dll
//  establish the dll entry points

#ifdef __WXMSW__
#ifdef USE_GLU_TESS
#ifdef USE_GLU_DLL

//  Formal definitions of required functions
typedef void (CALLBACK* LPFNDLLTESSPROPERTY)      ( GLUtesselator *tess,
                                                    GLenum        which,
                                                    GLdouble      value );
typedef GLUtesselator * (CALLBACK* LPFNDLLNEWTESS)( void);
typedef void (CALLBACK* LPFNDLLTESSBEGINCONTOUR)  ( GLUtesselator *);
typedef void (CALLBACK* LPFNDLLTESSENDCONTOUR)    ( GLUtesselator *);
typedef void (CALLBACK* LPFNDLLTESSBEGINPOLYGON)  ( GLUtesselator *, void*);
typedef void (CALLBACK* LPFNDLLTESSENDPOLYGON)    ( GLUtesselator *);
typedef void (CALLBACK* LPFNDLLDELETETESS)        ( GLUtesselator *);
typedef void (CALLBACK* LPFNDLLTESSVERTEX)        ( GLUtesselator *, GLdouble *, GLdouble *);
typedef void (CALLBACK* LPFNDLLTESSCALLBACK)      ( GLUtesselator *, const int, void (CALLBACK *fn)() );

//  Static pointers to the functions
static LPFNDLLTESSPROPERTY      s_lpfnTessProperty;
static LPFNDLLNEWTESS           s_lpfnNewTess;
static LPFNDLLTESSBEGINCONTOUR  s_lpfnTessBeginContour;
static LPFNDLLTESSENDCONTOUR    s_lpfnTessEndContour;
static LPFNDLLTESSBEGINPOLYGON  s_lpfnTessBeginPolygon;
static LPFNDLLTESSENDPOLYGON    s_lpfnTessEndPolygon;
static LPFNDLLDELETETESS        s_lpfnDeleteTess;
static LPFNDLLTESSVERTEX        s_lpfnTessVertex;
static LPFNDLLTESSCALLBACK      s_lpfnTessCallback;

//  Mapping of pointers to glu functions by substitute macro
#define gluTessProperty         s_lpfnTessProperty
#define gluNewTess              s_lpfnNewTess
#define gluTessBeginContour     s_lpfnTessBeginContour
#define gluTessEndContour       s_lpfnTessEndContour
#define gluTessBeginPolygon     s_lpfnTessBeginPolygon
#define gluTessEndPolygon       s_lpfnTessEndPolygon
#define gluDeleteTess           s_lpfnDeleteTess
#define gluTessVertex           s_lpfnTessVertex
#define gluTessCallback         s_lpfnTessCallback


#endif
#endif
//  Flag to tell that dll is ready
bool           s_glu_dll_ready;
HINSTANCE      s_hGLU_DLL;                   // Handle to DLL
#endif

/**
 * Returns TRUE if the ring has clockwise winding.
 *
 * @return TRUE if clockwise otherwise FALSE.
 */

bool isRingClockwise(wxPoint2DDouble *pp, int nPointCount)

{
    double dfSum = 0.0;
    
    for( int iVert = 0; iVert < nPointCount-1; iVert++ )
    {
        dfSum += pp[iVert].m_x * pp[iVert+1].m_y
        - pp[iVert].m_y * pp[iVert+1].m_x;
    }
    
    dfSum += pp[nPointCount-1].m_x * pp[0].m_y
    - pp[nPointCount-1].m_y * pp[0].m_x;
    
    return dfSum < 0.0;
}

/*
bool ispolysame(polyout *p1, polyout *p2)
{
    int i2;

    if(p1->nvert != p2->nvert)
        return false;

    int v10 = p1->vertex_index_list[0];

    for(i2 = 0 ; i2 < p2->nvert ; i2++)
    {
        if(p2->vertex_index_list[i2] == v10)
            break;
    }
    if(i2 == p2->nvert)
        return false;

    for(int j = 0 ; j<p1->nvert ; j++)
    {
        if(p1->vertex_index_list[j] != p2->vertex_index_list[i2])
            return false;
        i2++;
        if(i2 == p2->nvert)
            i2 = 0;
    }

    return true;
}

*/

//------------------------------------------------------------------------------
//          Extended_Geometry Implementation
//------------------------------------------------------------------------------
Extended_Geometry::Extended_Geometry()
{
      vertex_array = NULL;
      contour_array = NULL;
}

Extended_Geometry::~Extended_Geometry()
{
      free(vertex_array);
      free(contour_array);
}


//------------------------------------------------------------------------------
//          PolyTessGeo Implementation
//------------------------------------------------------------------------------
PolyTessGeo::PolyTessGeo()
{
    m_pxgeom = NULL;
    m_ppg_head = NULL;
    ErrorCode = 0;
}





void PolyTessGeo::SetExtents(double x_left, double y_bot, double x_right, double y_top)
{
    xmin = x_left;
    ymin = y_bot;
    xmax = x_right;
    ymax = y_top;
}

int PolyTessGeo::my_bufgets( char *buf, int buf_len_max )
{
    char        chNext;
    int         nLineLen = 0;
    char        *lbuf;

    lbuf = buf;


    while( (nLineLen < buf_len_max) &&((m_buf_ptr - m_buf_head) < m_nrecl) )
    {
        chNext = *m_buf_ptr++;

        /* each CR/LF (or LF/CR) as if just "CR" */
        if( chNext == 10 || chNext == 13 )
        {
            chNext = '\n';
        }

        *lbuf = chNext; lbuf++, nLineLen++;

        if( chNext == '\n' )
        {
            *lbuf = '\0';
            return nLineLen;
        }
    }

    *(lbuf) = '\0';
    return nLineLen;
}



PolyTessGeo::~PolyTessGeo()
{

    delete  m_ppg_head;

    delete  m_pxgeom;

#ifdef USE_GLU_TESS
    if(s_pwork_buf)
        free( s_pwork_buf );
    s_pwork_buf = NULL;
#endif

}


int PolyTessGeo::BuildDeferredTess(void)
{
//#ifdef USE_GLU_TESS
    return BuildTessGLFromXG();
//#else
//    return 0;
//#endif
}


void PolyTessGeo::GetRefPos( double *lat, double *lon)
{
    if(lat)
        *lat = m_ref_lat;
    if(lon)
        *lon = m_ref_lon;
}


//#ifdef USE_GLU_TESS


#ifdef __WXMSW__
#define __CALL_CONVENTION __stdcall
#else
#define __CALL_CONVENTION
#endif


void __CALL_CONVENTION beginCallback(GLenum which);
void __CALL_CONVENTION errorCallback(GLenum errorCode);
void __CALL_CONVENTION endCallback(void);
void __CALL_CONVENTION vertexCallback(GLvoid *vertex);
void __CALL_CONVENTION combineCallback(GLdouble coords[3],
                     GLdouble *vertex_data[4],
                     GLfloat weight[4], GLdouble **dataOut );


#if 0
//      Build PolyTessGeo Object from OGR Polygon
//      Using OpenGL/GLU tesselator
int PolyTessGeo::PolyTessGeoGL(OGRPolygon *poly, bool bSENC_SM, double ref_lat, double ref_lon)
{
#ifdef ocpnUSE_GL

    int iir, ip;
    int *cntr;
    GLdouble *geoPt;

    wxString    sout;
    wxString    sout1;
    wxString    stemp;

    //  Make a quick sanity check of the polygon coherence
    bool b_ok = true;
    OGRLineString *tls = poly->getExteriorRing();
    if(!tls) {
        b_ok = false;
    }
    else {
        int tnpta  = poly->getExteriorRing()->getNumPoints();
        if(tnpta < 3 )
            b_ok = false;
    }


    for( iir=0 ; iir < poly->getNumInteriorRings() ; iir++)
    {
        int tnptr = poly->getInteriorRing(iir)->getNumPoints();
        if( tnptr < 3 )
            b_ok = false;
    }

    if( !b_ok )
        return ERROR_BAD_OGRPOLY;


#ifdef __WXMSW__
//  If using the OpenGL dlls provided with Windows,
//  load the dll and establish addresses of the entry points needed

#ifdef USE_GLU_DLL

    if(!s_glu_dll_ready)
    {


        s_hGLU_DLL = LoadLibrary("glu32.dll");
        if (s_hGLU_DLL != NULL)
        {
            s_lpfnTessProperty = (LPFNDLLTESSPROPERTY)GetProcAddress(s_hGLU_DLL,"gluTessProperty");
            s_lpfnNewTess = (LPFNDLLNEWTESS)GetProcAddress(s_hGLU_DLL, "gluNewTess");
            s_lpfnTessBeginContour = (LPFNDLLTESSBEGINCONTOUR)GetProcAddress(s_hGLU_DLL, "gluTessBeginContour");
            s_lpfnTessEndContour = (LPFNDLLTESSENDCONTOUR)GetProcAddress(s_hGLU_DLL, "gluTessEndContour");
            s_lpfnTessBeginPolygon = (LPFNDLLTESSBEGINPOLYGON)GetProcAddress(s_hGLU_DLL, "gluTessBeginPolygon");
            s_lpfnTessEndPolygon = (LPFNDLLTESSENDPOLYGON)GetProcAddress(s_hGLU_DLL, "gluTessEndPolygon");
            s_lpfnDeleteTess = (LPFNDLLDELETETESS)GetProcAddress(s_hGLU_DLL, "gluDeleteTess");
            s_lpfnTessVertex = (LPFNDLLTESSVERTEX)GetProcAddress(s_hGLU_DLL, "gluTessVertex");
            s_lpfnTessCallback = (LPFNDLLTESSCALLBACK)GetProcAddress(s_hGLU_DLL, "gluTessCallback");

            s_glu_dll_ready = true;
        }
        else
        {
            return ERROR_NO_DLL;
        }
    }

#endif
#endif


    //  Allocate a work buffer, which will be grown as needed
#define NINIT_BUFFER_LEN 10000
    s_pwork_buf = (GLdouble *)malloc(NINIT_BUFFER_LEN * 2 * sizeof(GLdouble));
    s_buf_len = NINIT_BUFFER_LEN * 2;
    s_buf_idx = 0;

      //    Create an array to hold pointers to allocated vertices created by "combine" callback,
      //    so that they may be deleted after tesselation.
    s_pCombineVertexArray = new wxArrayPtrVoid;

    //  Create tesselator
    GLUtessobj = gluNewTess();

    //  Register the callbacks
    gluTessCallback(GLUtessobj, GLU_TESS_BEGIN,   (GLvoid (__CALL_CONVENTION *) ())&beginCallback);
    gluTessCallback(GLUtessobj, GLU_TESS_BEGIN,   (GLvoid (__CALL_CONVENTION *) ())&beginCallback);
    gluTessCallback(GLUtessobj, GLU_TESS_VERTEX,  (GLvoid (__CALL_CONVENTION *) ())&vertexCallback);
    gluTessCallback(GLUtessobj, GLU_TESS_END,     (GLvoid (__CALL_CONVENTION *) ())&endCallback);
    gluTessCallback(GLUtessobj, GLU_TESS_COMBINE, (GLvoid (__CALL_CONVENTION *) ())&combineCallback);

//    gluTessCallback(GLUtessobj, GLU_TESS_ERROR,   (GLvoid (__CALL_CONVENTION *) ())&errorCallback);

//    glShadeModel(GL_SMOOTH);
    gluTessProperty(GLUtessobj, GLU_TESS_WINDING_RULE,
                    GLU_TESS_WINDING_POSITIVE );

    //  gluTess algorithm internally selects vertically oriented triangle strips and fans.
    //  This orientation is not optimal for conventional memory-mapped raster display shape filling.
    //  We can "fool" the algorithm by interchanging the x and y vertex values passed to gluTessVertex
    //  and then reverting x and y on the resulting vertexCallbacks.
    //  In this implementation, we will explicitely set the preferred orientation.

    //Set the preferred orientation
    tess_orient = TESS_HORZ;                    // prefer horizontal tristrips



//    PolyGeo BBox as lat/lon
    OGREnvelope Envelope;
    poly->getEnvelope(&Envelope);
    xmin = Envelope.MinX;
    ymin = Envelope.MinY;
    xmax = Envelope.MaxX;
    ymax = Envelope.MaxY;


//      Get total number of contours
    ncnt = 1;                         // always exterior ring
    int nint = poly->getNumInteriorRings();  // interior rings
    ncnt += nint;


//      Allocate cntr array
    cntr = (int *)malloc(ncnt * sizeof(int));


//      Get total number of points(vertices)
    int npta  = poly->getExteriorRing()->getNumPoints();
    cntr[0] = npta;
    npta += 2;                            // fluff

    for( iir=0 ; iir < nint ; iir++)
    {
        int nptr = poly->getInteriorRing(iir)->getNumPoints();
        cntr[iir+1] = nptr;

        npta += nptr + 2;
    }

//    printf("pPoly npta: %d\n", npta);

    geoPt = (GLdouble *)malloc((npta) * 3 * sizeof(GLdouble));     // vertex array



   //  Grow the work buffer if necessary

    if((npta * 4) > s_buf_len)
    {
        s_pwork_buf = (GLdouble *)realloc(s_pwork_buf, npta * 4 * 2 * sizeof(GLdouble *));
        s_buf_len = npta * 4 * 2;
    }


//  Define the polygon
    gluTessBeginPolygon(GLUtessobj, NULL);


//      Create input structures

//    Exterior Ring
    int npte  = poly->getExteriorRing()->getNumPoints();
    cntr[0] = npte;

    GLdouble *ppt = geoPt;


//  Check and account for winding direction of ring
    bool cw = !(poly->getExteriorRing()->isClockwise() == 0);

    double x0, y0, x, y;
    OGRPoint p;

    if(cw)
    {
        poly->getExteriorRing()->getPoint(0, &p);
        x0 = p.getX();
        y0 = p.getY();
    }
    else
    {
        poly->getExteriorRing()->getPoint(npte-1, &p);
        x0 = p.getX();
        y0 = p.getY();
    }


    gluTessBeginContour(GLUtessobj);

//  Transcribe points to vertex array, in proper order with no duplicates
//   also, accounting for tess_orient
    for(ip = 0 ; ip < npte ; ip++)
    {
        int pidx;
        if(cw)
            pidx = npte - ip - 1;

        else
            pidx = ip;

        poly->getExteriorRing()->getPoint(pidx, &p);
        x = p.getX();
        y = p.getY();

        if((fabs(x-x0) > EQUAL_EPS) || (fabs(y-y0) > EQUAL_EPS))
        {
            GLdouble *ppt_temp = ppt;
            if(tess_orient == TESS_VERT)
            {
                *ppt++ = x;
                *ppt++ = y;
            }
            else
            {
                *ppt++ = y;
                *ppt++ = x;
            }

            *ppt++ = 0.0;

            gluTessVertex( GLUtessobj, ppt_temp, ppt_temp ) ;
  //printf("tess from pPoly, external vertex %g %g\n", x, y);


        }
        else
            cntr[0]--;

        x0 = x;
        y0 = y;
    }

    gluTessEndContour(GLUtessobj);




//  Now the interior contours
    for(iir=0 ; iir < nint ; iir++)
    {
        gluTessBeginContour(GLUtessobj);

        int npti = poly->getInteriorRing(iir)->getNumPoints();

      //  Check and account for winding direction of ring
        bool cw = !(poly->getInteriorRing(iir)->isClockwise() == 0);

        if(!cw)
        {
            poly->getInteriorRing(iir)->getPoint(0, &p);
            x0 = p.getX();
            y0 = p.getY();
        }
        else
        {
            poly->getInteriorRing(iir)->getPoint(npti-1, &p);
            x0 = p.getX();
            y0 = p.getY();
        }

//  Transcribe points to vertex array, in proper order with no duplicates
//   also, accounting for tess_orient
        for(int ip = 0 ; ip < npti ; ip++)
        {
            OGRPoint p;
            int pidx;
            if(!cw)                               // interior contours must be cw
                pidx = npti - ip - 1;
            else
                pidx = ip;

            poly->getInteriorRing(iir)->getPoint(pidx, &p);
            x = p.getX();
            y = p.getY();

            if((fabs(x-x0) > EQUAL_EPS) || (fabs(y-y0) > EQUAL_EPS))
            {
                GLdouble *ppt_temp = ppt;
                if(tess_orient == TESS_VERT)
                {
                    *ppt++ = x;
                    *ppt++ = y;
                }
                else
                {
                    *ppt++ = y;
                    *ppt++ = x;
                }
                *ppt++ = 0.0;

                gluTessVertex( GLUtessobj, ppt_temp, ppt_temp ) ;

//       printf("tess from Poly, internal vertex %d %g %g\n", ip, x, y);

            }
            else
                cntr[iir+1]--;

            x0 = x;
            y0 = y;

        }

        gluTessEndContour(GLUtessobj);
    }

    //  Store some SM conversion data in static store,
    //  for callback access
    s_ref_lat = ref_lat;
    s_ref_lon = ref_lon;
    s_bSENC_SM = bSENC_SM;

    s_bmerc_transform = false;

    //      Ready to kick off the tesselator

    s_pTPG_Last = NULL;
    s_pTPG_Head = NULL;

    s_nvmax = 0;

    gluTessEndPolygon(GLUtessobj);          // here it goes

    m_nvertex_max = s_nvmax;               // record largest vertex count, updates in callback


    //  Tesselation all done, so...

    //  Create the data structures

    m_ppg_head = new PolyTriGroup;
    m_ppg_head->m_bSMSENC = s_bSENC_SM;

    m_ppg_head->nContours = ncnt;

    m_ppg_head->pn_vertex = cntr;             // pointer to array of poly vertex counts
    m_ppg_head->data_type = DATA_TYPE_DOUBLE;


//  Transcribe the raw geometry buffer
//  Converting to float as we go, and
//  allowing for tess_orient
//  Also, convert to SM if requested

    nwkb = (npta +1) * 2 * sizeof(float);
    m_ppg_head->pgroup_geom = (float *)malloc(nwkb);
    float *vro = m_ppg_head->pgroup_geom;
    ppt = geoPt;
    float tx,ty;

    for(ip = 0 ; ip < npta ; ip++)
    {
        if(TESS_HORZ == tess_orient)
        {
            ty = *ppt++;
            tx = *ppt++;
        }
        else
        {
            tx = *ppt++;
            ty = *ppt++;
        }

        if(bSENC_SM)
        {
            //  Calculate SM from chart common reference point
            double easting, northing;
            toSM(ty, tx, ref_lat, ref_lon, &easting, &northing);
            *vro++ = easting;              // x
            *vro++ = northing;             // y
        }
        else
        {
            *vro++ = tx;                  // lon
            *vro++ = ty;                  // lat
        }

        ppt++;                      // skip z
    }

    m_ppg_head->tri_prim_head = s_pTPG_Head;         // head of linked list of TriPrims

    //  Convert the Triangle vertex arrays into a single memory allocation of floats
    //  to reduce SENC size and enable efficient access later

    //  First calculate the total byte size
    int total_byte_size = 0;
    TriPrim *p_tp = m_ppg_head->tri_prim_head;
    while( p_tp ) {
        total_byte_size += p_tp->nVert * 2 * sizeof(float);
        p_tp = p_tp->p_next; // pick up the next in chain
    }

    float *vbuf = (float *)malloc(total_byte_size);
    p_tp = m_ppg_head->tri_prim_head;
    float *p_run = vbuf;
    while( p_tp ) {
        float *pfbuf = p_run;
        for( int i=0 ; i < p_tp->nVert * 2 ; ++i){
            float x = (float)(p_tp->p_vertex[i]);
            *p_run++ = x;
        }

        free(p_tp->p_vertex);
        p_tp->p_vertex = (double *)pfbuf;
        p_tp = p_tp->p_next; // pick up the next in chain
    }
    m_ppg_head->bsingle_alloc = true;
    m_ppg_head->single_buffer = (unsigned char *)vbuf;
    m_ppg_head->single_buffer_size = total_byte_size;
    m_ppg_head->data_type = DATA_TYPE_FLOAT;


    gluDeleteTess(GLUtessobj);

    free( s_pwork_buf );
    s_pwork_buf = NULL;

    free (geoPt);

    //      Free up any "Combine" vertices created
    for(unsigned int i = 0; i < s_pCombineVertexArray->GetCount() ; i++)
          free (s_pCombineVertexArray->Item(i));
    delete s_pCombineVertexArray;

    m_bOK = true;

#endif          //    #ifdef ocpnUSE_GL

    return 0;
}
#endif

int PolyTessGeo::BuildTessGLFromXG(void)
{
    
#ifdef ocpnUSE_GL

      int iir, ip;
      int *cntr;
      GLdouble *geoPt;

      wxString    sout;
      wxString    sout1;
      wxString    stemp;


#ifdef __WXMSW__
//  If using the OpenGL dlls provided with Windows,
//  load the dll and establish addresses of the entry points needed

#ifdef USE_GLU_DLL

      if(!s_glu_dll_ready)
      {


            s_hGLU_DLL = LoadLibrary("glu32.dll");
            if (s_hGLU_DLL != NULL)
            {
                  s_lpfnTessProperty = (LPFNDLLTESSPROPERTY)GetProcAddress(s_hGLU_DLL,"gluTessProperty");
                  s_lpfnNewTess = (LPFNDLLNEWTESS)GetProcAddress(s_hGLU_DLL, "gluNewTess");
                  s_lpfnTessBeginContour = (LPFNDLLTESSBEGINCONTOUR)GetProcAddress(s_hGLU_DLL, "gluTessBeginContour");
                  s_lpfnTessEndContour = (LPFNDLLTESSENDCONTOUR)GetProcAddress(s_hGLU_DLL, "gluTessEndContour");
                  s_lpfnTessBeginPolygon = (LPFNDLLTESSBEGINPOLYGON)GetProcAddress(s_hGLU_DLL, "gluTessBeginPolygon");
                  s_lpfnTessEndPolygon = (LPFNDLLTESSENDPOLYGON)GetProcAddress(s_hGLU_DLL, "gluTessEndPolygon");
                  s_lpfnDeleteTess = (LPFNDLLDELETETESS)GetProcAddress(s_hGLU_DLL, "gluDeleteTess");
                  s_lpfnTessVertex = (LPFNDLLTESSVERTEX)GetProcAddress(s_hGLU_DLL, "gluTessVertex");
                  s_lpfnTessCallback = (LPFNDLLTESSCALLBACK)GetProcAddress(s_hGLU_DLL, "gluTessCallback");

                  s_glu_dll_ready = true;
            }
            else
            {
                  return ERROR_NO_DLL;
            }
      }

#endif
#endif


    //  Allocate a work buffer, which will be grown as needed
#define NINIT_BUFFER_LEN 10000
      s_pwork_buf = (GLdouble *)malloc(NINIT_BUFFER_LEN * 2 * sizeof(GLdouble));
      s_buf_len = NINIT_BUFFER_LEN * 2;
      s_buf_idx = 0;

      //    Create an array to hold pointers to allocated vertices created by "combine" callback,
      //    so that they may be deleted after tesselation.
      s_pCombineVertexArray = new wxArrayPtrVoid;

    //  Create tesselator
      GLUtessobj = gluNewTess();

    //  Register the callbacks
      gluTessCallback(GLUtessobj, GLU_TESS_BEGIN,   (GLvoid (__CALL_CONVENTION *) ())&beginCallback);
      gluTessCallback(GLUtessobj, GLU_TESS_BEGIN,   (GLvoid (__CALL_CONVENTION *) ())&beginCallback);
      gluTessCallback(GLUtessobj, GLU_TESS_VERTEX,  (GLvoid (__CALL_CONVENTION *) ())&vertexCallback);
      gluTessCallback(GLUtessobj, GLU_TESS_END,     (GLvoid (__CALL_CONVENTION *) ())&endCallback);
      gluTessCallback(GLUtessobj, GLU_TESS_COMBINE, (GLvoid (__CALL_CONVENTION *) ())&combineCallback);

//    gluTessCallback(GLUtessobj, GLU_TESS_ERROR,   (GLvoid (__CALL_CONVENTION *) ())&errorCallback);

//    glShadeModel(GL_SMOOTH);
      gluTessProperty(GLUtessobj, GLU_TESS_WINDING_RULE,
                      GLU_TESS_WINDING_POSITIVE );

    //  gluTess algorithm internally selects vertically oriented triangle strips and fans.
    //  This orientation is not optimal for conventional memory-mapped raster display shape filling.
    //  We can "fool" the algorithm by interchanging the x and y vertex values passed to gluTessVertex
    //  and then reverting x and y on the resulting vertexCallbacks.
    //  In this implementation, we will explicitely set the preferred orientation.

    //Set the preferred orientation
      tess_orient = TESS_HORZ;                    // prefer horizontal tristrips



//      Get total number of contours
      ncnt  = m_pxgeom->n_contours;

//      Allocate cntr array
      cntr = (int *)malloc(ncnt * sizeof(int));

//      Get total number of points(vertices)
      int npta  = m_pxgeom->contour_array[0];
      cntr[0] = npta;
      npta += 2;                            // fluff

      for( iir=0 ; iir < ncnt-1 ; iir++)
      {
            int nptr = m_pxgeom->contour_array[iir+1];
            cntr[iir+1] = nptr;

            npta += nptr + 2;             // fluff
      }



//      printf("xgeom npta: %d\n", npta);
      geoPt = (GLdouble *)malloc((npta) * 3 * sizeof(GLdouble));     // vertex array



   //  Grow the work buffer if necessary

      if((npta * 4) > s_buf_len)
      {
            s_pwork_buf = (GLdouble *)realloc(s_pwork_buf, npta * 4 * 2 * sizeof(GLdouble *));
            s_buf_len = npta * 4 * 2;
      }


//  Define the polygon
      gluTessBeginPolygon(GLUtessobj, NULL);


//      Create input structures

//    Exterior Ring
      int npte = m_pxgeom->contour_array[0];
      cntr[0] = npte;

      GLdouble *ppt = geoPt;


//  Check and account for winding direction of ring
      wxPoint2DDouble *pp = m_pxgeom->vertex_array;
      
      bool cw = isRingClockwise(pp, npte);
      

      double x0, y0, x, y;

      if(cw)
      {
            x0 = pp->m_x;
            y0 = pp->m_y;
      }
      else
      {
            x0 = pp[npte-1].m_x;
            y0 = pp[npte-1].m_y;
      }

      gluTessBeginContour(GLUtessobj);

//  Transcribe points to vertex array, in proper order with no duplicates
//   also, accounting for tess_orient
      for(ip = 0 ; ip < npte ; ip++)
      {
            int pidx;
            if(cw)
                  pidx = npte - ip - 1;

            else
                  pidx = ip;

            x = pp[pidx].m_x;
            y = pp[pidx].m_y;


            if((fabs(x-x0) > EQUAL_EPS) || (fabs(y-y0) > EQUAL_EPS))
            {
                  GLdouble *ppt_temp = ppt;
                  if(tess_orient == TESS_VERT)
                  {
                        *ppt++ = x;
                        *ppt++ = y;
                  }
                  else
                  {
                        *ppt++ = y;
                        *ppt++ = x;
                  }

                  *ppt++ = 0.0;

                  gluTessVertex( GLUtessobj, ppt_temp, ppt_temp ) ;

            }
            else
                  cntr[0]--;

            x0 = x;
            y0 = y;
      }

      gluTessEndContour(GLUtessobj);


      int index_offset = npte;
//  Now the interior contours
      for(iir=0; iir < ncnt-1; iir++)
      {
            gluTessBeginContour(GLUtessobj);

            int npti  = m_pxgeom->contour_array[iir+1];

      //  Check and account for winding direction of ring
            bool cw = isRingClockwise(&pp[index_offset], npti);
            
            if(!cw)
            {
                  x0 = pp[index_offset].m_x;
                  y0 = pp[index_offset].m_y;
            }
            else
            {
                  x0 = pp[index_offset + npti-1].m_x;
                  y0 = pp[index_offset + npti-1].m_y;
            }

//  Transcribe points to vertex array, in proper order with no duplicates
//   also, accounting for tess_orient
            for(int ip = 0 ; ip < npti ; ip++)
            {
                  int pidx;
                  if(!cw)                               // interior contours must be cw
                        pidx = npti - ip - 1;
                  else
                        pidx = ip;


                  x = pp[pidx + index_offset].m_x;
                  y = pp[pidx + index_offset].m_y;

                  if((fabs(x-x0) > EQUAL_EPS) || (fabs(y-y0) > EQUAL_EPS))
                  {
                        GLdouble *ppt_temp = ppt;
                        if(tess_orient == TESS_VERT)
                        {
                              *ppt++ = x;
                              *ppt++ = y;
                        }
                        else
                        {
                              *ppt++ = y;
                              *ppt++ = x;
                        }
                        *ppt++ = 0.0;

                        gluTessVertex( GLUtessobj, ppt_temp, ppt_temp ) ;
//      printf("tess from xgeom, internal vertex %d %g %g\n", ip, x, y);

                  }
                  else
                        cntr[iir+1]--;

                  x0 = x;
                  y0 = y;

            }

            gluTessEndContour(GLUtessobj);

            index_offset += npti;
      }

    //  Store some SM conversion data in static store,
    //  for callback access
      s_bSENC_SM =  false;

      s_ref_lat = m_ref_lat;
      s_ref_lon = m_ref_lon;
      

    //      Ready to kick off the tesselator

      s_pTPG_Last = NULL;
      s_pTPG_Head = NULL;

      s_nvmax = 0;

      gluTessEndPolygon(GLUtessobj);          // here it goes

      m_nvertex_max = s_nvmax;               // record largest vertex count, updates in callback


    //  Tesselation all done, so...

    //  Create the data structures

      m_ppg_head = new PolyTriGroup;
      m_ppg_head->m_bSMSENC = s_bSENC_SM;
      m_ppg_head->data_type = DATA_TYPE_DOUBLE;

      m_ppg_head->nContours = ncnt;
      m_ppg_head->pn_vertex = cntr;             // pointer to array of poly vertex counts


//  Transcribe the raw geometry buffer
//  Converting to float as we go, and
//  allowing for tess_orient
//  Also, convert to SM if requested

#if 0
    int nptfinal = npta;

      //  No longer need the full geometry in the SENC,
      nptfinal = 1;

      int m_nwkb = (nptfinal +1) * 2 * sizeof(float);
      m_ppg_head->pgroup_geom = (float *)malloc(m_nwkb);
      float *vro = m_ppg_head->pgroup_geom;
      ppt = geoPt;
      float tx,ty;

      for(ip = 0 ; ip < npta ; ip++)
      {
            if(TESS_HORZ == tess_orient)
            {
                  ty = *ppt++;
                  tx = *ppt++;
            }
            else
            {
                  tx = *ppt++;
                  ty = *ppt++;
            }

            if(0/*bSENC_SM*/)
            {
            //  Calculate SM from chart common reference point
                  double easting, northing;
//                  toSM(ty, tx, ref_lat, ref_lon, &easting, &northing);
                  *vro++ = easting;              // x
                  *vro++ = northing;             // y
            }
            else
            {
                  *vro++ = tx;                  // lon
                  *vro++ = ty;                  // lat
            }

            ppt++;                      // skip z
      }
#endif
      m_ppg_head->tri_prim_head = s_pTPG_Head;         // head of linked list of TriPrims

      //  Convert the Triangle vertex arrays into a single memory allocation of floats
      //  to reduce SENC size and enable efficient access later

      //  First calculate the total byte size
      int total_byte_size = 0;
      TriPrim *p_tp = m_ppg_head->tri_prim_head;
      while( p_tp ) {
          total_byte_size += p_tp->nVert * 2 * sizeof(float);
          p_tp = p_tp->p_next; // pick up the next in chain
      }

      float *vbuf = (float *)malloc(total_byte_size);
      p_tp = m_ppg_head->tri_prim_head;
      float *p_run = vbuf;
      while( p_tp ) {
          float *pfbuf = p_run;
          for( int i=0 ; i < p_tp->nVert * 2 ; ++i){
              float x = (float)(p_tp->p_vertex[i]);
              *p_run++ = x;
          }

          free(p_tp->p_vertex);
          p_tp->p_vertex = (double *)pfbuf;
          p_tp = p_tp->p_next; // pick up the next in chain
      }
      m_ppg_head->bsingle_alloc = true;
      m_ppg_head->single_buffer = (unsigned char *)vbuf;
      m_ppg_head->single_buffer_size = total_byte_size;
      m_ppg_head->data_type = DATA_TYPE_FLOAT;

      gluDeleteTess(GLUtessobj);

      free( s_pwork_buf );
      s_pwork_buf = NULL;

      free (geoPt);

      //    All allocated buffers are owned now by the m_ppg_head
      //    And will be freed on dtor of this object
      delete m_pxgeom;

      //      Free up any "Combine" vertices created
      for(unsigned int i = 0; i < s_pCombineVertexArray->GetCount() ; i++)
            free (s_pCombineVertexArray->Item(i));
      delete s_pCombineVertexArray;


      m_pxgeom = NULL;

      m_bOK = true;

#endif          //#ifdef ocpnUSE_GL

      return 0;
}





// GLU tesselation support functions
void __CALL_CONVENTION beginCallback(GLenum which)
{
    s_buf_idx = 0;
    s_nvcall = 0;
    s_gltri_type = which;
}

/*
void __CALL_CONVENTION errorCallback(GLenum errorCode)
{
    const GLubyte *estring;

    estring = gluErrorString(errorCode);
    printf("Tessellation Error: %s\n", estring);
    exit(0);
}
*/

void __CALL_CONVENTION endCallback(void)
{
    //      Create a TriPrim

    char buf[40];

    if(s_nvcall > s_nvmax)                            // keep track of largest number of triangle vertices
          s_nvmax = s_nvcall;

    switch(s_gltri_type)
    {
        case GL_TRIANGLE_FAN:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLES:
        {
            TriPrim *pTPG = new TriPrim;
            if(NULL == s_pTPG_Last)
            {
                s_pTPG_Head = pTPG;
                s_pTPG_Last = pTPG;
            }
            else
            {
                s_pTPG_Last->p_next = pTPG;
                s_pTPG_Last = pTPG;
            }

            pTPG->p_next = NULL;
            pTPG->type = s_gltri_type;
            pTPG->nVert = s_nvcall;

        //  Calculate bounding box in lat/lon

            float sxmax = -1000;                   // this poly BBox
            float sxmin = 1000;
            float symax = -90;
            float symin = 90;

            GLdouble *pvr = s_pwork_buf;
            for(int iv=0 ; iv < s_nvcall ; iv++)
            {
                GLdouble xd, yd;
                xd = *pvr++;
                yd = *pvr++;

                double lat, lon;
                fromSM_Plugin(xd, yd, s_ref_lat, s_ref_lon, &lat, &lon);
                
                sxmax = fmax(lon, sxmax);
                sxmin = fmin(lon, sxmin);
                symax = fmax(lat, symax);
                symin = fmin(lat, symin);
            }

            pTPG->tri_box.Set(symin, sxmin, symax, sxmax);
            

            //  Transcribe this geometry to TriPrim

            pTPG->p_vertex = (double *)malloc(s_nvcall * 2 * sizeof(double));
            memcpy(pTPG->p_vertex, s_pwork_buf, s_nvcall * 2 * sizeof(double));


            break;
        }
        default:
        {
            sprintf(buf, "....begin Callback  unknown\n");
            break;
        }
    }
}

void __CALL_CONVENTION vertexCallback(GLvoid *vertex)
{
    GLdouble *pointer;

    pointer = (GLdouble *) vertex;

    if(s_buf_idx > s_buf_len - 4)
    {
        int new_buf_len = s_buf_len + 100;
        GLdouble * tmp = s_pwork_buf;

        s_pwork_buf = (GLdouble *)realloc(s_pwork_buf, new_buf_len * sizeof(GLdouble));
        if (NULL == s_pwork_buf)
        {
            free(tmp);
            tmp = NULL;
        }
        else
            s_buf_len = new_buf_len;
    }

    if(tess_orient == TESS_VERT)
    {
        s_pwork_buf[s_buf_idx++] = pointer[0];
        s_pwork_buf[s_buf_idx++] = pointer[1];
    }
    else
    {
        s_pwork_buf[s_buf_idx++] = pointer[1];
        s_pwork_buf[s_buf_idx++] = pointer[0];
    }


    s_nvcall++;

}

/*  combineCallback is used to create a new vertex when edges
 *  intersect.
 */
void __CALL_CONVENTION combineCallback(GLdouble coords[3],
                     GLdouble *vertex_data[4],
                     GLfloat weight[4], GLdouble **dataOut )
{
    GLdouble *vertex = (GLdouble *)malloc(6 * sizeof(GLdouble));

    vertex[0] = coords[0];
    vertex[1] = coords[1];
    vertex[2] = coords[2];
    vertex[3] = vertex[4] = vertex[5] = 0. ; //01/13/05 bugfix

    *dataOut = vertex;

    s_pCombineVertexArray->Add(vertex);
}


//#endif
wxStopWatch *s_stwt;

#if 0
//      Build Trapezoidal PolyTessGeoTrap Object from Extended_Geometry
PolyTessGeoTrap::PolyTessGeoTrap(Extended_Geometry *pxGeom)
{
      m_bOK = false;

      m_ptg_head = new PolyTrapGroup(pxGeom);
      m_nvertex_max = pxGeom->n_max_vertex;           // record the maximum number of segment vertices

      //    All allocated buffers are owned now by the m_ptg_head
      //    And will be freed on dtor of this object
      delete pxGeom;
}


PolyTessGeoTrap::~PolyTessGeoTrap()
{
      delete m_ptg_head;
}

void PolyTessGeoTrap::BuildTess()
{
           //    Flip the passed vertex array, contour-by-contour
      int offset = 1;
      for(int ict=0 ; ict < m_ptg_head->nContours ; ict++)
      {
            int nvertex = m_ptg_head->pn_vertex[ict];
/*
            for(int iv=0 ; iv < nvertex/2 ; iv++)
            {
                  wxPoint2DDouble a = m_ptg_head->ptrapgroup_geom[iv + offset];
                  wxPoint2DDouble b = m_ptg_head->ptrapgroup_geom[(nvertex - 1) - iv + offset];
                  m_ptg_head->ptrapgroup_geom[iv + offset] = b;
                  m_ptg_head->ptrapgroup_geom[(nvertex - 1) - iv + offset] = a;
            }
*/
            wxPoint2DDouble *pa = &m_ptg_head->ptrapgroup_geom[offset];
            wxPoint2DDouble *pb = &m_ptg_head->ptrapgroup_geom[(nvertex - 1) + offset];

            for(int iv=0 ; iv < nvertex/2 ; iv++)
            {

                  wxPoint2DDouble a = *pa;
                  *pa = *pb;
                  *pb = a;

                  pa++;
                  pb--;
            }

            offset += nvertex;
      }


      itrap_t *itr;
      isegment_t *iseg;
      int n_traps;

      int trap_err = int_trapezate_polygon(m_ptg_head->nContours, m_ptg_head->pn_vertex, (double (*)[2])m_ptg_head->ptrapgroup_geom, &itr, &iseg, &n_traps);

     m_ptg_head->m_trap_error = trap_err;

      if(0 != n_traps)
      {
       //  Now the Trapezoid Primitives

      //    Iterate thru the trapezoid structure counting valid, non-empty traps

            int nvtrap = 0;
            for(int it=1 ; it< n_traps ; it++)
            {
//            if((itr[i].state == ST_VALID) && (itr[i].hi.y != itr[i].lo.y) && (itr[i].lseg != -1) && (itr[i].rseg != -1) && (itr[i].inside == 1))
                  if(itr[it].inside == 1)
                        nvtrap++;
            }

            m_ptg_head->ntrap_count = nvtrap;

      //    Allocate enough memory
            m_ptg_head->trap_array = (trapz_t *)malloc(nvtrap * sizeof(trapz_t));

      //    Iterate again and capture the valid trapezoids
            trapz_t *prtrap = m_ptg_head->trap_array;
            for(int i=1 ; i< n_traps ; i++)
            {
//            if((itr[i].state == ST_VALID) && (itr[i].hi.y != itr[i].lo.y) && (itr[i].lseg != -1) && (itr[i].rseg != -1) && (itr[i].inside == 1))
                  if(itr[i].inside == 1)
                  {


                  //    Fix up the trapezoid segment indices to account for ring closure points in the input vertex array
                        int i_adjust = 0;
                        int ic = 0;
                        int pcount = m_ptg_head->pn_vertex[0]-1;
                        while(itr[i].lseg > pcount)
                        {
                              i_adjust++;
                              ic++;
                              if(ic >= m_ptg_head->nContours)
                                    break;
                              pcount += m_ptg_head->pn_vertex[ic]-1;
                        }
                        prtrap->ilseg = itr[i].lseg + i_adjust;


                        i_adjust = 0;
                        ic = 0;
                        pcount = m_ptg_head->pn_vertex[0]-1;
                        while(itr[i].rseg > pcount)
                        {
                              i_adjust++;
                              ic++;
                              if(ic >=  m_ptg_head->nContours)
                                    break;
                              pcount += m_ptg_head->pn_vertex[ic]-1;
                        }
                        prtrap->irseg = itr[i].rseg + i_adjust;

                  //    Set the trap y values

                        prtrap->hiy = itr[i].hi.y;
                        prtrap->loy = itr[i].lo.y;

                        prtrap++;
                  }
            }


      }     // n_traps_ok
      else
            m_nvertex_max = 0;



//  Free the trapezoid structure array
      free(itr);
      free(iseg);

      //    Always OK, even if trapezator code faulted....
      //    Contours should be OK, anyway, and    m_ptg_head->ntrap_count will be 0;

      m_bOK = true;

}


#endif

//------------------------------------------------------------------------------
//          PolyTriGroup Implementation
//------------------------------------------------------------------------------
PolyTriGroup::PolyTriGroup()
{
    pn_vertex = NULL;             // pointer to array of poly vertex counts
//    pgroup_geom = NULL;           // pointer to Raw geometry, used for contour line drawing
    tri_prim_head = NULL;         // head of linked list of TriPrims
    m_bSMSENC = false;
    bsingle_alloc = false;
    single_buffer = NULL;
    single_buffer_size = 0;
    data_type = DATA_TYPE_DOUBLE;
    sfactor = 1.0;
    soffset = 0.0;
    

}

PolyTriGroup::~PolyTriGroup()
{
    free(pn_vertex);
//    free(pgroup_geom);
    //Walk the list of TriPrims, deleting as we go
    TriPrim *tp_next;
    TriPrim *tp = tri_prim_head;

    if(bsingle_alloc){
        free(single_buffer);
        while(tp) {
            tp_next = tp->p_next;
            delete tp;
            tp = tp_next;
        }
    }
    else {
        while(tp) {
            tp_next = tp->p_next;
            tp->FreeMem();
            delete tp;
            tp = tp_next;
        }
    }
}

//------------------------------------------------------------------------------
//          TriPrim Implementation
//------------------------------------------------------------------------------
TriPrim::TriPrim()
{
}

TriPrim::~TriPrim()
{
}

void TriPrim::FreeMem()
{
    free(p_vertex);
}


//------------------------------------------------------------------------------
//          PolyTrapGroup Implementation
//------------------------------------------------------------------------------
PolyTrapGroup::PolyTrapGroup()
{
      pn_vertex = NULL;             // pointer to array of poly vertex counts
      ptrapgroup_geom = NULL;           // pointer to Raw geometry, used for contour line drawing
      trap_array = NULL;            // pointer to trapz_t array

      ntrap_count = 0;
}

PolyTrapGroup::PolyTrapGroup(Extended_Geometry *pxGeom)
{
      m_trap_error = 0;

      nContours = pxGeom->n_contours;

      pn_vertex = pxGeom->contour_array;             // pointer to array of poly vertex counts
      pxGeom->contour_array = NULL;

      ptrapgroup_geom = pxGeom->vertex_array;
      pxGeom->vertex_array = NULL;

      ntrap_count = 0;                                // provisional
      trap_array = NULL;                              // pointer to generated trapz_t array
}

PolyTrapGroup::~PolyTrapGroup()
{
      free(pn_vertex);
      free(ptrapgroup_geom);
      free (trap_array);
}









