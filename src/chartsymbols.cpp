/***************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Chart Symbols
 * Author:   Jesper Weissglas
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
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

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/filename.h>
#include <stdlib.h>

#include "chartsymbols.h"
#ifdef ocpnUSE_GL
#include <wx/glcanvas.h>
#endif

extern bool pi_bopengl;


#ifdef ocpnUSE_GL
extern GLenum       g_oe_texture_rectangle_format;
#endif

//--------------------------------------------------------------------------------------
// The below data is global since there will ever only be one ChartSymbols instance,
// and some methods+data of class S52plib are needed inside ChartSymbol, and s2plib
// needs some methods from ChartSymbol. So s52plib only calls static methods in
// order to resolve circular include file dependencies.


wxString configFileDirectory;



//  Some refined HPGL vector rendering strings allowing direct OpenGL rendering without pre-tesselation

char fix3501[] = {"SPA;ST3;PU1944,1950;PM0;PD2245,1500;PD2553,1950;PD1944,1950;PM2;FP;\
PU2096,1940;PM0;PD2096,2566;PD2401,2566;PD2401,1940;PD2096,1940;PM2;FP;\
PU1944,2546;PM0;PD2245,2999;PD2553,2546;PD1944,2546;PM2;FP;\
SPA;SW1;PU2096,1950;PD1944,1950;PD2245,1500;PD2553,1950;PD2401,1950;PD2401,2546;PD2563,2546;PD2252,2999;PD1947,2546;PD2096,2546;PD2096,1950;\
"};

//--------------------------------------------------------------------------------------

OE_ChartSymbols::OE_ChartSymbols( void )
{
}

OE_ChartSymbols::~OE_ChartSymbols( void )
{
}

void OE_ChartSymbols::InitializeGlobals( void )
{
    oe_pi_colorTables = new wxArrayPtrVoid;
    oe_pi_symbolGraphicLocations = new symbolGraphicsHashMap;
    oe_rasterSymbolsLoadedColorMapNumber = -1;
    oe_ColorTableIndex = 0;
    oe_rasterSymbolsTexture = 0;
}

void OE_ChartSymbols::DeleteGlobals( void )
{

    ( *oe_pi_symbolGraphicLocations ).clear();
    delete oe_pi_symbolGraphicLocations;
    oe_pi_symbolGraphicLocations = NULL;

    for( unsigned int i = 0; i < oe_pi_colorTables->GetCount(); i++ ) {
        colTable *ct = (colTable *) oe_pi_colorTables->Item( i );
        delete ct->tableName;
        ct->colors.clear();
        ct->wxColors.clear();
        delete ct;
    }

    oe_pi_colorTables->Clear();
    delete oe_pi_colorTables;
    oe_pi_colorTables = NULL;
}

#define TGET_INT_PROPERTY_VALUE( node, name, target )  \
      propVal = wxString(node->Attribute(name), wxConvUTF8); \
      propVal.ToLong( &numVal, 0 ); \
      target = numVal;

void OE_ChartSymbols::ProcessColorTables( TiXmlElement* colortableNodes )
{

    for( TiXmlNode *childNode = colortableNodes->FirstChild(); childNode;
            childNode = childNode->NextSibling() ) {
        TiXmlElement *child = childNode->ToElement();
        colTable *colortable = new colTable;

        const char *pName = child->Attribute( "name" );
        colortable->tableName = new wxString( pName, wxConvUTF8 );

        TiXmlElement* colorNode = child->FirstChild()->ToElement();

        while( colorNode ) {
            S52color color;
            wxString propVal;
            long numVal;

            if( wxString( colorNode->Value(), wxConvUTF8 ) == _T("graphics-file") ) {
                colortable->rasterFileName = wxString( colorNode->Attribute( "name" ), wxConvUTF8 );
                goto next;
            } else {
                TGET_INT_PROPERTY_VALUE( colorNode, "r", color.R )
                TGET_INT_PROPERTY_VALUE( colorNode, "g", color.G )
                TGET_INT_PROPERTY_VALUE( colorNode, "b", color.B )

                wxString key( colorNode->Attribute( "name" ), wxConvUTF8 );
                strncpy( color.colName, key.char_str(), 5 );
                color.colName[5] = 0;

                colortable->colors[key] = color;

                wxColour wxcolor( color.R, color.G, color.B );
                colortable->wxColors[key] = wxcolor;
            }

            next: colorNode = colorNode->NextSiblingElement();
        }

        oe_pi_colorTables->Add( (void *) colortable );

    }
}

void OE_ChartSymbols::ProcessLookups( TiXmlElement* lookupNodes )
{
    Lookup lookup;
    wxString propVal;
    long numVal;

    for( TiXmlNode *childNode = lookupNodes->FirstChild(); childNode;
            childNode = childNode->NextSibling() ) {
        TiXmlElement *child = childNode->ToElement();

        TGET_INT_PROPERTY_VALUE( child, "id", lookup.id )
        TGET_INT_PROPERTY_VALUE( child, "RCID", lookup.RCID )
        lookup.name = wxString( child->Attribute( "name" ), wxConvUTF8 );
        lookup.attributeCodeArray = NULL;

        TiXmlElement* subNode = child->FirstChild()->ToElement();

        while( subNode ) {
            wxString nodeType( subNode->Value(), wxConvUTF8 );
            wxString nodeText( subNode->GetText(), wxConvUTF8 );

            if( nodeType == _T("type") ) {

                if( nodeText == _T("Area") ) lookup.type = AREAS_T;
                else
                    if( nodeText == _T("Line") ) lookup.type = LINES_T;
                    else
                        lookup.type = POINT_T;

                goto nextNode;
            }

            if( nodeType == _T("disp-prio") ) {
                lookup.displayPrio = PRIO_NODATA;
                if( nodeText == _T("Group 1") ) lookup.displayPrio = PRIO_GROUP1;
                else
                if( nodeText == _T("Area 1") ) lookup.displayPrio = PRIO_AREA_1;
                else
                if( nodeText == _T("Area 2") ) lookup.displayPrio = PRIO_AREA_2;
                else
                if( nodeText == _T("Point Symbol") ) lookup.displayPrio = PRIO_SYMB_POINT;
                else
                if( nodeText == _T("Line Symbol") ) lookup.displayPrio = PRIO_SYMB_LINE;
                else
                if( nodeText == _T("Area Symbol") ) lookup.displayPrio = PRIO_SYMB_AREA;
                else
                if( nodeText == _T("Routing") ) lookup.displayPrio = PRIO_ROUTEING;
                else
                if( nodeText == _T("Hazards") ) lookup.displayPrio = PRIO_HAZARDS;
                else
                if( nodeText == _T("Mariners") ) lookup.displayPrio = PRIO_MARINERS;
                goto nextNode;
            }
            if( nodeType == _T("radar-prio") ) {
                if( nodeText == _T("On Top") ) lookup.radarPrio = RAD_OVER;
                else
                    lookup.radarPrio = RAD_SUPP;
                goto nextNode;
            }
            if( nodeType == _T("table-name") ) {
                if( nodeText == _T("Simplified") ) lookup.tableName = SIMPLIFIED;
                else
                if( nodeText == _T("Lines") ) lookup.tableName = LINES;
                else
                if( nodeText == _T("Plain") ) lookup.tableName = PLAIN_BOUNDARIES;
                else
                if( nodeText == _T("Symbolized") ) lookup.tableName = SYMBOLIZED_BOUNDARIES;
                else
                lookup.tableName = PAPER_CHART;
                goto nextNode;
            }
            if( nodeType == _T("display-cat") ) {
                if( nodeText == _T("Displaybase") ) lookup.displayCat = DISPLAYBASE;
                else
                if( nodeText == _T("Standard") ) lookup.displayCat = STANDARD;
                else
                if( nodeText == _T("Other") ) lookup.displayCat = OTHER;
                else
                if( nodeText == _T("Mariners") ) lookup.displayCat = MARINERS_STANDARD;
                else
                lookup.displayCat = OTHER;
                goto nextNode;
            }
            if( nodeType == _T("comment") ) {
                wxString comment( subNode->GetText(), wxConvUTF8 );
                long value;
                comment.ToLong( &value, 0 );
                lookup.comment = value;
                goto nextNode;
            }

            if( nodeType == _T("instruction") ) {
                lookup.instruction = nodeText;
                lookup.instruction.Append( '\037' );
                goto nextNode;
            }
            if( nodeType == _T("attrib-code") ) {
                if( !lookup.attributeCodeArray )
                    lookup.attributeCodeArray = new wxArrayString();
                wxString value = wxString( subNode->GetText(), wxConvUTF8 );
                if( value.length() == 6 )
                    value << _T(" ");
                lookup.attributeCodeArray->Add( value );
                goto nextNode;
            }

            nextNode: subNode = subNode->NextSiblingElement();
        }

        BuildLookup( lookup );
    }
}

void OE_ChartSymbols::BuildLookup( Lookup &lookup )
{

    LUPrec *LUP = (LUPrec*) calloc( 1, sizeof(LUPrec) );
    plib->pAlloc->Add( LUP );

    LUP->RCID = lookup.RCID;
    LUP->nSequence = lookup.id;
    LUP->DISC = lookup.displayCat;
    LUP->FTYP = lookup.type;
    LUP->DPRI = lookup.displayPrio;
    LUP->RPRI = lookup.radarPrio;
    LUP->TNAM = lookup.tableName;
    LUP->OBCL[6] = 0;
    strncpy( LUP->OBCL, lookup.name.mb_str(), 7 );

    LUP->ATTCArray = lookup.attributeCodeArray;

    LUP->INST = new wxString( lookup.instruction );
    LUP->LUCM = lookup.comment;

    // Add LUP to array
    // Search the LUPArray to see if there is already a LUP with this RCID
    // If found, replace it with the new LUP
    // This provides a facility for updating the LUP tables after loading a basic set

    unsigned int index = 0;
    wxArrayOfLUPrec *pLUPARRAYtyped = plib->SelectLUPARRAY( LUP->TNAM );

    while( index < pLUPARRAYtyped->GetCount() ) {
        LUPrec *pLUPCandidate = pLUPARRAYtyped->Item( index );
        if( LUP->RCID == pLUPCandidate->RCID ) {
            pLUPARRAYtyped->RemoveAt(index);
            plib->DestroyLUP( pLUPCandidate ); // empties the LUP
            break;
        }
        index++;
    }

    pLUPARRAYtyped->Add( LUP );
}

void OE_ChartSymbols::ProcessVectorTag( TiXmlElement* vectorNode, SymbolSizeInfo_t &vectorSize )
{
    wxString propVal;
    long numVal;
    TGET_INT_PROPERTY_VALUE( vectorNode, "width", vectorSize.size.x )
    TGET_INT_PROPERTY_VALUE( vectorNode, "height", vectorSize.size.y )

    TiXmlElement* vectorNodes = vectorNode->FirstChild()->ToElement();

    while( vectorNodes ) {
        wxString nodeType( vectorNodes->Value(), wxConvUTF8 );

        if( nodeType == _T("distance") ) {
            TGET_INT_PROPERTY_VALUE( vectorNodes, "min", vectorSize.minDistance )
            TGET_INT_PROPERTY_VALUE( vectorNodes, "max", vectorSize.maxDistance )
            goto nextVector;
        }
        if( nodeType == _T("origin") ) {
            TGET_INT_PROPERTY_VALUE( vectorNodes, "x", vectorSize.origin.x )
            TGET_INT_PROPERTY_VALUE( vectorNodes, "y", vectorSize.origin.y )
            goto nextVector;
        }
        if( nodeType == _T("pivot") ) {
            TGET_INT_PROPERTY_VALUE( vectorNodes, "x", vectorSize.pivot.x )
            TGET_INT_PROPERTY_VALUE( vectorNodes, "y", vectorSize.pivot.y )
            goto nextVector;
        }
        nextVector: vectorNodes = vectorNodes->NextSiblingElement();
    }
}

void OE_ChartSymbols::ProcessLinestyles( TiXmlElement* linestyleNodes )
{

    LineStyle lineStyle;
    wxString propVal;
    long numVal;

    for( TiXmlNode *childNode = linestyleNodes->FirstChild(); childNode;
            childNode = childNode->NextSibling() ) {
        TiXmlElement *child = childNode->ToElement();

        TGET_INT_PROPERTY_VALUE( child, "RCID", lineStyle.RCID )

        TiXmlElement* subNode = child->FirstChild()->ToElement();

        while( subNode ) {
            wxString nodeType( subNode->Value(), wxConvUTF8 );
            wxString nodeText( subNode->GetText(), wxConvUTF8 );

            if( nodeType == _T("description") ) {
                lineStyle.description = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("name") ) {
                lineStyle.name = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("color-ref") ) {
                lineStyle.colorRef = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("HPGL") ) {
                lineStyle.HPGL = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("vector") ) {
                ProcessVectorTag( subNode, lineStyle.vectorSize );
            }
            nextNode: subNode = subNode->NextSiblingElement();
        }

        BuildLineStyle( lineStyle );
    }
}

void OE_ChartSymbols::BuildLineStyle( LineStyle &lineStyle )
{
    Rule *lnstmp = NULL;
    Rule *lnst = (Rule*) calloc( 1, sizeof(Rule) );
    plib->pAlloc->Add( lnst );

    lnst->RCID = lineStyle.RCID;
    strncpy( lnst->name.PANM, lineStyle.name.mb_str(), 8 );
    lnst->bitmap.PBTM = NULL;

    lnst->vector.LVCT = (char *) malloc( lineStyle.HPGL.Len() + 1 );
    strcpy( lnst->vector.LVCT, lineStyle.HPGL.mb_str() );

    lnst->colRef.LCRF = (char *) malloc( lineStyle.colorRef.Len() + 1 );
    strcpy( lnst->colRef.LCRF, lineStyle.colorRef.mb_str() );

    lnst->pos.line.minDist.PAMI = lineStyle.vectorSize.minDistance;
    lnst->pos.line.maxDist.PAMA = lineStyle.vectorSize.maxDistance;

    lnst->pos.line.pivot_x.PACL = lineStyle.vectorSize.pivot.x;
    lnst->pos.line.pivot_y.PARW = lineStyle.vectorSize.pivot.y;

    lnst->pos.line.bnbox_w.PAHL = lineStyle.vectorSize.size.x;
    lnst->pos.line.bnbox_h.PAVL = lineStyle.vectorSize.size.y;

    lnst->pos.line.bnbox_x.SBXC = lineStyle.vectorSize.origin.x;
    lnst->pos.line.bnbox_y.SBXR = lineStyle.vectorSize.origin.y;

    lnstmp = ( *plib->_line_sym )[lineStyle.name];

    if( NULL == lnstmp ) ( *plib->_line_sym )[lineStyle.name] = lnst;
    else
        if( lnst->name.LINM != lnstmp->name.LINM ) ( *plib->_line_sym )[lineStyle.name] = lnst;
}

void OE_ChartSymbols::ProcessPatterns( TiXmlElement* patternNodes )
{

    OCPNPattern pattern;
    wxString propVal;
    long numVal;

    for( TiXmlNode *childNode = patternNodes->FirstChild(); childNode;
            childNode = childNode->NextSibling() ) {
        TiXmlElement *child = childNode->ToElement();

        TGET_INT_PROPERTY_VALUE( child, "RCID", pattern.RCID )

        pattern.hasVector = false;
        pattern.hasBitmap = false;
        pattern.preferBitmap = true;

        TiXmlElement* subNodes = child->FirstChild()->ToElement();

        while( subNodes ) {
            wxString nodeType( subNodes->Value(), wxConvUTF8 );
            wxString nodeText( subNodes->GetText(), wxConvUTF8 );

            if( nodeType == _T("description") ) {
                pattern.description = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("name") ) {
                pattern.name = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("filltype") ) {
                pattern.fillType = ( subNodes->GetText() )[0];
                goto nextNode;
            }
            if( nodeType == _T("spacing") ) {
                pattern.spacing = ( subNodes->GetText() )[0];
                goto nextNode;
            }
            if( nodeType == _T("color-ref") ) {
                pattern.colorRef = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("definition") ) {
                if( !strcmp( subNodes->GetText(), "V" ) ) pattern.hasVector = true;
                goto nextNode;
            }
            if( nodeType == _T("prefer-bitmap") ) {
                if( nodeText.Lower() == _T("no") ) pattern.preferBitmap = false;
                if( nodeText.Lower() == _T("false") ) pattern.preferBitmap = false;
                goto nextNode;
            }
            if( nodeType == _T("bitmap") ) {
                TGET_INT_PROPERTY_VALUE( subNodes, "width", pattern.bitmapSize.size.x )
                TGET_INT_PROPERTY_VALUE( subNodes, "height", pattern.bitmapSize.size.y )
                pattern.hasBitmap = true;

                TiXmlElement* bitmapNodes = subNodes->FirstChild()->ToElement();
                while( bitmapNodes ) {
                    wxString bitmapnodeType( bitmapNodes->Value(), wxConvUTF8 );

                    if( bitmapnodeType == _T("distance") ) {
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "min",
                                pattern.bitmapSize.minDistance )
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "max",
                                pattern.bitmapSize.maxDistance )
                        goto nextBitmap;
                    }
                    if( bitmapnodeType == _T("origin") ) {
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "x", pattern.bitmapSize.origin.x )
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "y", pattern.bitmapSize.origin.y )
                        goto nextBitmap;
                    }
                    if( bitmapnodeType == _T("pivot") ) {
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "x", pattern.bitmapSize.pivot.x )
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "y", pattern.bitmapSize.pivot.y )
                        goto nextBitmap;
                    }
                    if( bitmapnodeType == _T("graphics-location") ) {
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "x", pattern.bitmapSize.graphics.x )
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "y", pattern.bitmapSize.graphics.y )
                    }
                    nextBitmap: bitmapNodes = bitmapNodes->NextSiblingElement();
                }
                goto nextNode;
            }
            if( nodeType == _T("HPGL") ) {
                pattern.hasVector = true;
                pattern.HPGL = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("vector") ) {
                ProcessVectorTag( subNodes, pattern.vectorSize );
            }
            nextNode: subNodes = subNodes->NextSiblingElement();
        }

        BuildPattern( pattern );

    }

}

void OE_ChartSymbols::BuildPattern( OCPNPattern &pattern )
{
    Rule *pattmp = NULL;

    Rule *patt = (Rule*) calloc( 1, sizeof(Rule) );
    plib->pAlloc->Add( patt );

    patt->RCID = pattern.RCID;
    patt->exposition.PXPO = new wxString( pattern.description );
    strncpy( patt->name.PANM, pattern.name.mb_str(), 8 );
    patt->bitmap.PBTM = NULL;
    patt->fillType.PATP = pattern.fillType;
    patt->spacing.PASP = pattern.spacing;

    patt->vector.PVCT = (char *) malloc( pattern.HPGL.Len() + 1 );
    strcpy( patt->vector.PVCT, pattern.HPGL.mb_str() );

    patt->colRef.PCRF = (char *) malloc( pattern.colorRef.Len() + 1 );
    strcpy( patt->colRef.PCRF, pattern.colorRef.mb_str() );

    SymbolSizeInfo_t patternSize;

    if( pattern.hasVector && !( pattern.preferBitmap && pattern.hasBitmap ) ) {
        patt->definition.PADF = 'V';
        patternSize = pattern.vectorSize;
    } else {
        patt->definition.PADF = 'R';
        patternSize = pattern.bitmapSize;
    }

    patt->pos.patt.minDist.PAMI = patternSize.minDistance;
    patt->pos.patt.maxDist.PAMA = patternSize.maxDistance;

    patt->pos.patt.pivot_x.PACL = patternSize.pivot.x;
    patt->pos.patt.pivot_y.PARW = patternSize.pivot.y;

    patt->pos.patt.bnbox_w.PAHL = patternSize.size.x;
    patt->pos.patt.bnbox_h.PAVL = patternSize.size.y;

    patt->pos.patt.bnbox_x.SBXC = patternSize.origin.x;
    patt->pos.patt.bnbox_y.SBXR = patternSize.origin.y;

    wxRect graphicsLocation( pattern.bitmapSize.graphics, pattern.bitmapSize.size );
    ( *oe_pi_symbolGraphicLocations )[pattern.name] = graphicsLocation;

    // check if key already there

    pattmp = ( *plib->_patt_sym )[pattern.name];

    if( NULL == pattmp ) {
        ( *plib->_patt_sym )[pattern.name] = patt; // insert in hash table
    } else // already something here with same key...
    { // if the pattern names are not identical
        if( patt->name.PANM != pattmp->name.PANM ) {
            ( *plib->_patt_sym )[pattern.name] = patt; // replace the pattern
            plib->DestroyPatternRuleNode( pattmp ); // remember to free to replaced node
            // the node itself is destroyed as part of pAlloc
        }
    }
}

void OE_ChartSymbols::ProcessSymbols( TiXmlElement* symbolNodes )
{

    ChartSymbol symbol;
    wxString propVal;
    long numVal;

    for( TiXmlNode *childNode = symbolNodes->FirstChild(); childNode;
            childNode = childNode->NextSibling() ) {
        TiXmlElement *child = childNode->ToElement();

        TGET_INT_PROPERTY_VALUE( child, "RCID", symbol.RCID )

        symbol.hasVector = false;
        symbol.hasBitmap = false;
        symbol.preferBitmap = true;

        TiXmlElement* subNodes = child->FirstChild()->ToElement();

        while( subNodes ) {
            wxString nodeType( subNodes->Value(), wxConvUTF8 );
            wxString nodeText( subNodes->GetText(), wxConvUTF8 );

            if( nodeType == _T("description") ) {
                symbol.description = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("name") ) {
                symbol.name = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("color-ref") ) {
                symbol.colorRef = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("definition") ) {
                if( !strcmp( subNodes->GetText(), "V" ) ) symbol.hasVector = true;
                goto nextNode;
            }
            if( nodeType == _T("HPGL") ) {
                symbol.HPGL = nodeText;
                goto nextNode;
            }
            if( nodeType == _T("prefer-bitmap") ) {
                if( nodeText.Lower() == _T("no") ) symbol.preferBitmap = false;
                if( nodeText.Lower() == _T("false") ) symbol.preferBitmap = false;
                goto nextNode;
            }
            if( nodeType == _T("bitmap") ) {
                TGET_INT_PROPERTY_VALUE( subNodes, "width", symbol.bitmapSize.size.x )
                TGET_INT_PROPERTY_VALUE( subNodes, "height", symbol.bitmapSize.size.y )
                symbol.hasBitmap = true;

                TiXmlElement* bitmapNodes = subNodes->FirstChild()->ToElement();
                while( bitmapNodes ) {
                    wxString bitmapnodeType( bitmapNodes->Value(), wxConvUTF8 );
                    if( bitmapnodeType == _T("distance") ) {
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "min", symbol.bitmapSize.minDistance )
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "max", symbol.bitmapSize.maxDistance )
                        goto nextBitmap;
                    }
                    if( bitmapnodeType == _T("origin") ) {
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "x", symbol.bitmapSize.origin.x )
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "y", symbol.bitmapSize.origin.y )
                        goto nextBitmap;
                    }
                    if( bitmapnodeType == _T("pivot") ) {
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "x", symbol.bitmapSize.pivot.x )
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "y", symbol.bitmapSize.pivot.y )
                        goto nextBitmap;
                    }
                    if( bitmapnodeType == _T("graphics-location") ) {
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "x", symbol.bitmapSize.graphics.x )
                        TGET_INT_PROPERTY_VALUE( bitmapNodes, "y", symbol.bitmapSize.graphics.y )
                    }
                    nextBitmap: bitmapNodes = bitmapNodes->NextSiblingElement();
                }
                goto nextNode;
            }
            if( nodeType == _T("vector") ) {
                TGET_INT_PROPERTY_VALUE( subNodes, "width", symbol.vectorSize.size.x )
                TGET_INT_PROPERTY_VALUE( subNodes, "height", symbol.vectorSize.size.y )
                symbol.hasVector = true;

                TiXmlElement* vectorNodes = subNodes->FirstChild()->ToElement();
                while( vectorNodes ) {
                    wxString vectornodeType( vectorNodes->Value(), wxConvUTF8 );
                    if( vectornodeType == _T("distance") ) {
                        TGET_INT_PROPERTY_VALUE( vectorNodes, "min", symbol.vectorSize.minDistance )
                        TGET_INT_PROPERTY_VALUE( vectorNodes, "max", symbol.vectorSize.maxDistance )
                        goto nextVector;
                    }
                    if( vectornodeType == _T("origin") ) {
                        TGET_INT_PROPERTY_VALUE( vectorNodes, "x", symbol.vectorSize.origin.x )
                        TGET_INT_PROPERTY_VALUE( vectorNodes, "y", symbol.vectorSize.origin.y )
                        goto nextVector;
                    }
                    if( vectornodeType == _T("pivot") ) {
                        TGET_INT_PROPERTY_VALUE( vectorNodes, "x", symbol.vectorSize.pivot.x )
                        TGET_INT_PROPERTY_VALUE( vectorNodes, "y", symbol.vectorSize.pivot.y )
                        goto nextVector;
                    }
                    if( vectornodeType == _T("HPGL") ) {
                        
                        // Substitute some vector rendering strings
                        if(symbol.RCID == 3501)
                            symbol.HPGL = wxString( fix3501, wxConvUTF8 );
                        else
                            symbol.HPGL = wxString( vectorNodes->GetText(), wxConvUTF8 );
                    }
                    nextVector: vectorNodes = vectorNodes->NextSiblingElement();
                }
            }
            nextNode: subNodes = subNodes->NextSiblingElement();
        }

        BuildSymbol( symbol );
    }

}

void OE_ChartSymbols::BuildSymbol( ChartSymbol& symbol )
{

    Rule *symb = (Rule*) calloc( 1, sizeof(Rule) );
    plib->pAlloc->Add( symb );

    wxString SVCT;
    wxString SCRF;

    symb->RCID = symbol.RCID;
    strncpy( symb->name.SYNM, symbol.name.char_str(), 8 );

    symb->exposition.SXPO = new wxString( symbol.description );

    symb->vector.SVCT = (char *) malloc( symbol.HPGL.Len() + 1 );
    strcpy( symb->vector.SVCT, symbol.HPGL.mb_str() );

    symb->colRef.SCRF = (char *) malloc( symbol.colorRef.Len() + 1 );
    strcpy( symb->colRef.SCRF, symbol.colorRef.mb_str() );

    symb->bitmap.SBTM = NULL;

    SymbolSizeInfo_t symbolSize;

    if( symbol.hasVector && !( symbol.preferBitmap && symbol.hasBitmap ) ) {
        symb->definition.SYDF = 'V';
        symbolSize = symbol.vectorSize;
    } else {
        symb->definition.SYDF = 'R';
        symbolSize = symbol.bitmapSize;
    }

    symb->pos.symb.minDist.PAMI = symbolSize.minDistance;
    symb->pos.symb.maxDist.PAMA = symbolSize.maxDistance;

    symb->pos.symb.pivot_x.SYCL = symbolSize.pivot.x;
    symb->pos.symb.pivot_y.SYRW = symbolSize.pivot.y;

    symb->pos.symb.bnbox_w.SYHL = symbolSize.size.x;
    symb->pos.symb.bnbox_h.SYVL = symbolSize.size.y;

    symb->pos.symb.bnbox_x.SBXC = symbolSize.origin.x;
    symb->pos.symb.bnbox_y.SBXR = symbolSize.origin.y;

    wxRect graphicsLocation( symbol.bitmapSize.graphics, symbol.bitmapSize.size );
    ( *oe_pi_symbolGraphicLocations )[symbol.name] = graphicsLocation;

    // Already something here with same key? Then free its strings, otherwise they leak.
    Rule* symbtmp = ( *plib->_symb_sym )[symbol.name];
    if( symbtmp ) {
        free( symbtmp->colRef.SCRF );
        free( symbtmp->vector.SVCT );
        delete symbtmp->exposition.SXPO;
    }

    ( *plib->_symb_sym )[symbol.name] = symb;

}

bool OE_ChartSymbols::LoadConfigFile(s52plib* plibArg, const wxString & s52ilePath)
{
    TiXmlDocument doc;

    plib = plibArg;

    // Expect to find library data XML file in same folder as other S52 data.
    // Files in CWD takes precedence.

    wxString name, extension;
    wxString xmlFileName = _T("chartsymbols.xml");

    wxFileName::SplitPath( s52ilePath, &configFileDirectory, &name, &extension );
    wxString fullFilePath = configFileDirectory + wxFileName::GetPathSeparator() + xmlFileName;

    if( wxFileName::FileExists( xmlFileName ) ) {
        fullFilePath = xmlFileName;
        configFileDirectory = _T(".");
    }

    if( !wxFileName::FileExists( fullFilePath ) ) {
        wxString msg( _T("ChartSymbols ConfigFile not found: ") );
        msg += fullFilePath;
        wxLogMessage( msg );
        return false;
    }

    if( !doc.LoadFile( (const char *) fullFilePath.mb_str() ) ) {
        wxString msg( _T("    ChartSymbols ConfigFile Failed to load ") );
        msg += fullFilePath;
        wxLogMessage( msg );
        return false;
    }

    wxString msg( _T("ChartSymbols loaded from ") );
    msg += fullFilePath;
    wxLogMessage( msg );

    TiXmlHandle hRoot( doc.RootElement() );

    wxString root = wxString( doc.RootElement()->Value(), wxConvUTF8 );
    if( root != _T("chartsymbols" ) ) {
        wxLogMessage(
                _T("    ChartSymbols::LoadConfigFile(): Expected XML Root <chartsymbols> not found.") );
        return false;
    }

    TiXmlElement* pElem = hRoot.FirstChild().Element();

    for( ; pElem != 0; pElem = pElem->NextSiblingElement() ) {
        wxString child = wxString( pElem->Value(), wxConvUTF8 );

        if( child == _T("color-tables") ) ProcessColorTables( pElem );
        if( child == _T("lookups") ) ProcessLookups( pElem );
        if( child == _T("line-styles") ) ProcessLinestyles( pElem );
        if( child == _T("patterns") ) ProcessPatterns( pElem );
        if( child == _T("symbols") ) ProcessSymbols( pElem );

    }

    return true;
}

bool OE_ChartSymbols::PatchConfigFile(s52plib* plibArg, const wxString &xmlPatchFileName)
{
    TiXmlDocument doc;
    
    plib = plibArg;
    
    if( !wxFileName::FileExists( xmlPatchFileName ) ) {
        wxString msg( _T("ChartSymbols PatchFile not found: ") );
        msg += xmlPatchFileName;
        wxLogMessage( msg );
        return false;
    }
    
    if( !doc.LoadFile( (const char *) xmlPatchFileName.mb_str() ) ) {
        wxString msg( _T("    ChartSymbols PatchFile Failed to load ") );
        msg += xmlPatchFileName;
        wxLogMessage( msg );
        return false;
    }
    
    wxString msg( _T("ChartSymbols PatchFile loaded from ") );
    msg += xmlPatchFileName;
    wxLogMessage( msg );
    
    TiXmlHandle hRoot( doc.RootElement() );
    
    wxString root = wxString( doc.RootElement()->Value(), wxConvUTF8 );
    if( root != _T("chartsymbols" ) ) {
        wxLogMessage(
            _T("    ChartSymbols::LoadConfigFile(): Expected XML Root <chartsymbols> not found.") );
            return false;
    }
    
    TiXmlElement* pElem = hRoot.FirstChild().Element();
    
    for( ; pElem != 0; pElem = pElem->NextSiblingElement() ) {
        wxString child = wxString( pElem->Value(), wxConvUTF8 );
        
        if( child == _T("color-tables") ) ProcessColorTables( pElem );
        if( child == _T("lookups") ) ProcessLookups( pElem );
        if( child == _T("line-styles") ) ProcessLinestyles( pElem );
        if( child == _T("patterns") ) ProcessPatterns( pElem );
        if( child == _T("symbols") ) ProcessSymbols( pElem );
                     
    }
    
    return true;
}


void OE_ChartSymbols::SetColorTableIndex( int index )
{
    oe_ColorTableIndex = index;
    LoadRasterFileForColorTable(oe_ColorTableIndex);
}


void OE_ChartSymbols::ResetRasterTextureCache()
{
    oe_rasterSymbolsTexture = 0;               // This will leak one texture
                                                // but we cannot just delete it, since it might have been erroeously created
                                                // while there was no valid GLcontext.
                                                
    LoadRasterFileForColorTable(oe_ColorTableIndex, true);
}
    
int OE_ChartSymbols::LoadRasterFileForColorTable( int tableNo, bool flush )
{
    if( tableNo == oe_rasterSymbolsLoadedColorMapNumber && !flush ){
        if( pi_bopengl) {
            if(oe_rasterSymbolsTexture > 0)
                return true;
#ifdef ocpnUSE_GL            
            else if( !g_oe_texture_rectangle_format && oe_rasterSymbols.IsOk()) 
                return true;
#endif            
        }
        if( oe_rasterSymbols.IsOk())
            return true;
    }
        
    colTable* coltab = (colTable *) oe_pi_colorTables->Item( tableNo );

    wxString filename = configFileDirectory + wxFileName::GetPathSeparator()
            + coltab->rasterFileName;

    wxImage rasterFileImg;
    if( rasterFileImg.LoadFile( filename, wxBITMAP_TYPE_PNG ) ) {
#ifdef ocpnUSE_GL
        /* for opengl mode, load the symbols into a texture */
        if( pi_bopengl && g_oe_texture_rectangle_format) {

            int w = rasterFileImg.GetWidth();
            int h = rasterFileImg.GetHeight();

            //    Get the glRGBA format data from the wxImage
            unsigned char *d = rasterFileImg.GetData();
            unsigned char *a = rasterFileImg.GetAlpha();

            /* combine rgb with alpha */
            unsigned char *e = (unsigned char *) malloc( w * h * 4 );
            if(d && a){
                for( int y = 0; y < h; y++ )
                    for( int x = 0; x < w; x++ ) {
                        int off = ( y * w + x );

                        e[off * 4 + 0] = d[off * 3 + 0];
                        e[off * 4 + 1] = d[off * 3 + 1];
                        e[off * 4 + 2] = d[off * 3 + 2];
                        e[off * 4 + 3] = a[off];
                    }
            }
            
            glEnable(GL_TEXTURE_2D);


            if(oe_rasterSymbolsTexture == 0)
            {
                if(oe_rasterSymbolsTexture)
                    glDeleteTextures(1, &oe_rasterSymbolsTexture);
                glGenTextures(1, &oe_rasterSymbolsTexture);
                wxString msg;
                msg.Printf(_T("oeSENC_PI RasterSymbols texture: %d"), oe_rasterSymbolsTexture);
                wxLogMessage(msg);
            }

            glBindTexture(g_oe_texture_rectangle_format, oe_rasterSymbolsTexture);
            glTexParameteri(g_oe_texture_rectangle_format, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(g_oe_texture_rectangle_format, GL_TEXTURE_MAX_LEVEL, 0);
             glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
             glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

            /* unfortunately this texture looks terrible with compression */
            glTexImage2D(g_oe_texture_rectangle_format, 0, GL_RGBA, w, h,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, e);

//              glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
//              glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
//              glTexParameteri( g_texture_rectangle_format, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
//              glTexParameteri( g_texture_rectangle_format, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

            oe_rasterSymbolsTextureSize = wxSize(w, h);

            glDisable(GL_TEXTURE_2D);
            free(e);
        } 
#endif
        {
            oe_rasterSymbols = wxBitmap( rasterFileImg, -1/*32*/);
        }

        oe_rasterSymbolsLoadedColorMapNumber = tableNo;
        return true;
    }

    wxString msg( _T("ChartSymbols...Failed to load raster symbols file ") );
    msg += filename;
    wxLogMessage( msg );
    return false;
}

// Convenience method for old s52plib code.
wxArrayPtrVoid* OE_ChartSymbols::GetColorTables()
{
    return oe_pi_colorTables;
}

S52color* OE_ChartSymbols::GetColor( const char *colorName, int fromTable )
{
    colTable *colortable;
    wxString key( colorName, wxConvUTF8, 5 );
    colortable = (colTable *)oe_pi_colorTables->Item( fromTable );
    return &( colortable->colors[key] );
}

wxColor OE_ChartSymbols::GetwxColor( const wxString &colorName, int fromTable )
{
    colTable *colortable;
    colortable = (colTable *) oe_pi_colorTables->Item( fromTable );
    wxColor c = colortable->wxColors[colorName];
    return c;
}

wxColor OE_ChartSymbols::GetwxColor( const char *colorName, int fromTable )
{
    wxString key( colorName, wxConvUTF8, 5 );
    return GetwxColor( key, fromTable );
}

int OE_ChartSymbols::FindColorTable(const wxString & tableName)
{
    for( unsigned int i = 0; i < oe_pi_colorTables->GetCount(); i++ ) {
        colTable *ct = (colTable *) oe_pi_colorTables->Item( i );
        if( tableName.IsSameAs( *ct->tableName ) ) {
            return i;
        }
    }
    return 0;
}

wxString OE_ChartSymbols::HashKey( const char* symbolName )
{
    char key[9];
    key[8] = 0;
    strncpy( key, symbolName, 8 );
    return wxString( key, wxConvUTF8 );
}

wxImage OE_ChartSymbols::GetImage( const char* symbolName )
{
    wxRect bmArea = ( *oe_pi_symbolGraphicLocations )[HashKey( symbolName )];
    if(oe_rasterSymbols.IsOk()){
        wxBitmap bitmap = oe_rasterSymbols.GetSubBitmap( bmArea );
        return bitmap.ConvertToImage();
    }
    else
        return wxImage(1,1);
}

unsigned int OE_ChartSymbols::GetGLTextureRect( wxRect &rect, const char* symbolName )
{
    rect = ( *oe_pi_symbolGraphicLocations )[HashKey( symbolName )];
    return oe_rasterSymbolsTexture;
}

wxSize OE_ChartSymbols::GLTextureSize()
{
    return oe_rasterSymbolsTextureSize;
}
