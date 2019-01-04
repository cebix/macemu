/*
 *  MibAccess.cpp
 *
 *	The original code by Stas Khirman modified by Lauri Pesonen, December, 2000:
 *	
 *	SnmpUtilVarBindFree(), SnmpUtilOidNCmp() and SnmpUtilOidCpy() now loaded from
 *	"snmpapi.dll" dynamically instead of linking statically.
 *	
 *	MibII ctor now takes a parameter whether to load Winsock or not.
 *	WSAStartup maintains an internal reference counter so it would have been ok
 *	to let it load always.
 *	
 *	Fixed a bug where the return value of LoadLibrary() was compared against
 *	HINSTANCE_ERROR instead of NULL.
 *	
 *	Removed some type conversion warnings by casting.
 *	
 *	Added a check in MibExtLoad ctor that the function entry points were found.
 *	
 *	Added a check in GetIPMask() and GetIPAddress() that the library was loaded
 *	before accessing the functions.
 *	
 *	Changed the return type of GetIPAddress() and GetIPMask() from BOOL  to void
 *	as they always returned TRUE.
 *	
 */

/************************************************************************/
/*      Copyright (C) Stas Khirman 1998.  All rights reserved.          */
/*      Written by Stas Khirman (staskh@rocketmail.com).                */
/*					  and											    */
/*                 Raz Galili (razgalili@hotmail.com)				    */
/*                                                                      */
/*      Free software: no warranty; use anywhere is ok; spread the      */
/*      sources; note any modifications; share variations and           */
/*      derivatives (including sending to staskh@rocketmail.com).       */
/*                                                                      */
/************************************************************************/

/*
 *  MibAccess.cpp - ip router
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  Windows platform specific code copyright (C) Lauri Pesonen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "sysdeps.h"
#include "main.h"
#include "mibaccess.h"
#include "../dynsockets.h"
#include "../dump.h"

#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"

MibExtLoad::MibExtLoad( LPCTSTR MibDllName, LPCTSTR SnmpDllName )
{
	m_Init = NULL;

	m_InitEx = NULL;
	m_Query = NULL;	
	m_Trap = NULL;

	m_hInst_snmputil = NULL;

	m_SnmpUtilVarBindFree = NULL;
	m_SnmpUtilOidNCmp = NULL;
	m_SnmpUtilOidCpy = NULL;
	
	m_hInst = LoadLibrary( MibDllName );
	if(!m_hInst) {
		D(bug(TEXT("MIB: library %s could not be loaded.\r\n"), MibDllName));
		return;
	}
	D(bug(TEXT("MIB: library %s loaded ok.\r\n"), MibDllName));

	m_Init	=	(pSnmpExtensionInit)GetProcAddress(m_hInst ,"SnmpExtensionInit");
	m_InitEx=	(pSnmpExtensionInitEx)GetProcAddress(m_hInst ,"SnmpExtensionInitEx");
	m_Query	=	(pSnmpExtensionQuery)GetProcAddress(m_hInst ,"SnmpExtensionQuery");
	m_Trap	=	(pSnmpExtensionTrap)GetProcAddress(m_hInst ,"SnmpExtensionTrap");

	if( !m_Init || !m_InitEx || !m_Query || !m_Trap )
	{
		D(bug(TEXT("MIB: required entry points not found in library %s.\r\n"), MibDllName));
		FreeLibrary( m_hInst );
		m_hInst = NULL;
	}

	m_hInst_snmputil = LoadLibrary( SnmpDllName );
	if(!m_hInst_snmputil){
		D(bug(TEXT("MIB: library %s could not be loaded.\r\n"), SnmpDllName));
		FreeLibrary( m_hInst );
		m_hInst = NULL;
		return;
	}
	D(bug(TEXT("MIB: library %s loaded ok.\r\n"), SnmpDllName));

	m_SnmpUtilVarBindFree = (VOID (SNMP_FUNC_TYPE *)(SnmpVarBind *))GetProcAddress( m_hInst_snmputil, "SnmpUtilVarBindFree" );
	m_SnmpUtilOidNCmp = (SNMPAPI (SNMP_FUNC_TYPE *)(AsnObjectIdentifier *, AsnObjectIdentifier *, UINT))GetProcAddress( m_hInst_snmputil, "SnmpUtilOidNCmp" );
	m_SnmpUtilOidCpy = (SNMPAPI (SNMP_FUNC_TYPE *)(AsnObjectIdentifier *, AsnObjectIdentifier *))GetProcAddress( m_hInst_snmputil, "SnmpUtilOidCpy" );

	if( !m_SnmpUtilVarBindFree || !m_SnmpUtilOidNCmp || !m_SnmpUtilOidCpy )
	{
		D(bug(TEXT("MIB: required entry points not found in library %s.\r\n"), SnmpDllName));
		FreeLibrary( m_hInst );
		FreeLibrary( m_hInst_snmputil );
		m_hInst = NULL;
		m_hInst_snmputil = NULL;
	}

	#undef SNMP_FreeVarBind
	#undef SNMP_oidncmp
	#undef SNMP_oidcpy

	#define SNMP_FreeVarBind   m_SnmpUtilVarBindFree
	#define SNMP_oidncmp       m_SnmpUtilOidNCmp
	#define SNMP_oidcpy        m_SnmpUtilOidCpy
}

MibExtLoad::~MibExtLoad()
{
	if( m_hInst ) {
		FreeLibrary( m_hInst );
		m_hInst = NULL;
	}
	if( m_hInst_snmputil ) {
		FreeLibrary( m_hInst_snmputil );
		m_hInst_snmputil = NULL;
	}
}

BOOL MibExtLoad::Init(DWORD dwTimeZeroReference,HANDLE *hPollForTrapEvent,AsnObjectIdentifier *supportedView)
{
	if(m_hInst && m_Init)
		return m_Init(dwTimeZeroReference,hPollForTrapEvent,supportedView);
	return FALSE;
}
BOOL MibExtLoad::InitEx(AsnObjectIdentifier *supportedView)
{
	if(m_hInst && m_InitEx)
		return m_InitEx(supportedView);
	
	return FALSE;
}

BOOL MibExtLoad::Query(BYTE requestType,OUT RFC1157VarBindList *variableBindings,
					   AsnInteger *errorStatus,AsnInteger *errorIndex)
{
	if(m_hInst && m_Query)
		return m_Query(requestType,variableBindings,errorStatus,errorIndex);
	
	return FALSE;
}

BOOL MibExtLoad::Trap(AsnObjectIdentifier *enterprise, AsnInteger *genericTrap,
					  AsnInteger *specificTrap, AsnTimeticks *timeStamp, 
					  RFC1157VarBindList  *variableBindings)
{
	if(m_hInst && m_Trap)
		return m_Trap(enterprise, genericTrap,specificTrap, timeStamp, variableBindings);
	
	return FALSE;
}

MibII::MibII(bool load_winsock) : MibExtLoad(TEXT("inetmib1.dll"), TEXT("snmpapi.dll"))
{
  WSADATA wsa;
	m_load_winsock = load_winsock;
	if(load_winsock) {
		int err = _WSAStartup( 0x0101, &wsa );  
	}
}

MibII::~MibII()
{
  if(m_load_winsock) _WSACleanup();
}

BOOL MibII::Init()
{
	HANDLE PollForTrapEvent;
	AsnObjectIdentifier SupportedView;

	return MibExtLoad::Init(GetTickCount(),&PollForTrapEvent,&SupportedView);

}


void MibII::GetIPAddress( UINT IpArray[], UINT &IpArraySize )
{
	if(!m_hInst) {
		IpArraySize = 0;
		return;
	}
	
	UINT OID_ipAdEntAddr[] = { 1, 3, 6, 1, 2, 1, 4 , 20, 1 ,1 };
	AsnObjectIdentifier MIB_ipAdEntAddr = { sizeof(OID_ipAdEntAddr)/sizeof(UINT), OID_ipAdEntAddr };
	RFC1157VarBindList  varBindList;
	RFC1157VarBind      varBind[1];
	AsnInteger          errorStatus;
	AsnInteger          errorIndex;
	AsnObjectIdentifier MIB_NULL = {0,0};
	BOOL                Exit;
	int                 ret;
	int                 IpCount=0;
	DWORD               dtmp;
	
	varBindList.list = varBind;
	varBindList.len  = 1;
	varBind[0].name  = MIB_NULL;
	SNMP_oidcpy(&varBind[0].name,&MIB_ipAdEntAddr);
	Exit = FALSE;
	
	IpCount=0;
	while(!Exit){
		ret = Query(ASN_RFC1157_GETNEXTREQUEST,&varBindList,&errorStatus,&errorIndex);
		
		if(!ret)
			Exit=TRUE;
		else{
			ret = SNMP_oidncmp(&varBind[0].name,&MIB_ipAdEntAddr,MIB_ipAdEntAddr.idLength);
			if(ret!=0){
				Exit=TRUE;
			}
			else{
				dtmp = *((DWORD *)varBind[0].value.asnValue.address.stream);
				IpArray[IpCount] = dtmp;
				IpCount++;
				if(IpCount>=(int)IpArraySize)
					Exit = TRUE;
			}
		}
	}
	
	IpArraySize = IpCount;
	
	SNMP_FreeVarBind(&varBind[0]);
}

void MibII::GetIPMask( UINT IpArray[], UINT &IpArraySize )
{
	if(!m_hInst) {
		IpArraySize = 0;
		return;
	}
	
	UINT OID_ipAdEntMask[] = { 1, 3, 6, 1, 2, 1, 4 , 20, 1 ,3 };
	AsnObjectIdentifier MIB_ipAdEntMask = { sizeof(OID_ipAdEntMask)/sizeof(UINT), OID_ipAdEntMask };
	RFC1157VarBindList  varBindList;
	RFC1157VarBind      varBind[1];
	AsnInteger          errorStatus;
	AsnInteger          errorIndex;
	AsnObjectIdentifier MIB_NULL = {0,0};
	BOOL                Exit;
	int                 ret;
	int                 IpCount=0;
	DWORD               dtmp;
	
	varBindList.list = varBind;
	varBindList.len  = 1;
	varBind[0].name  = MIB_NULL;
	SNMP_oidcpy(&varBind[0].name,&MIB_ipAdEntMask);
	Exit = FALSE;
	
	IpCount=0;
	while(!Exit){
		ret = Query(ASN_RFC1157_GETNEXTREQUEST,&varBindList,&errorStatus,&errorIndex);
		
		if(!ret)
			Exit=TRUE;
		else{
			ret = SNMP_oidncmp(&varBind[0].name,&MIB_ipAdEntMask,MIB_ipAdEntMask.idLength);
			if(ret!=0){
				Exit=TRUE;
			}
			else{
				dtmp = *((DWORD *)varBind[0].value.asnValue.address.stream);
				IpArray[IpCount] = dtmp;
				IpCount++;
				if(IpCount>=(int)IpArraySize)
					Exit = TRUE;
			}
		}
	}
	
	IpArraySize = IpCount;
	
	SNMP_FreeVarBind(&varBind[0]);
}
