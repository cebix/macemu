//////////////////////////////////////////////////////
// FILE  : MibAccess.h
//
//

#ifndef _SNMP_ACCESS_H_
#define _SNMP_ACCESS_H_

#include <snmp.h>
#ifndef SNMP_FUNC_TYPE
#define SNMP_FUNC_TYPE WINAPI
#endif

//////////////////////////////////////////////////////////////
// Definition of pointers to the four functions in the Mib Dll
//
typedef BOOL (WINAPI *pSnmpExtensionInit)(
										  IN  DWORD               dwTimeZeroReference,
										  OUT HANDLE              *hPollForTrapEvent,
										  OUT AsnObjectIdentifier *supportedView);

typedef BOOL (WINAPI *pSnmpExtensionTrap)(
										  OUT AsnObjectIdentifier *enterprise,
										  OUT AsnInteger          *genericTrap,
										  OUT AsnInteger          *specificTrap,
										  OUT AsnTimeticks        *timeStamp,
										  OUT RFC1157VarBindList  *variableBindings);

typedef BOOL (WINAPI *pSnmpExtensionQuery)(
										   IN BYTE                   requestType,
										   IN OUT RFC1157VarBindList *variableBindings,
										   OUT AsnInteger            *errorStatus,
										   OUT AsnInteger            *errorIndex);

typedef BOOL (WINAPI *pSnmpExtensionInitEx)(OUT AsnObjectIdentifier *supportedView);


class MibExtLoad
{
public:
	MibExtLoad( LPCTSTR MibDllName, LPCTSTR SnmpDllName );
	~MibExtLoad();
	BOOL Init(DWORD dwTimeZeroReference,HANDLE *hPollForTrapEvent,AsnObjectIdentifier *supportedView);
	BOOL InitEx(AsnObjectIdentifier *supportedView);
	BOOL Query(BYTE requestType,OUT RFC1157VarBindList *variableBindings,
			AsnInteger *errorStatus,AsnInteger *errorIndex);

	BOOL Trap(AsnObjectIdentifier *enterprise, AsnInteger *genericTrap, 
		AsnInteger *specificTrap, AsnTimeticks *timeStamp, 
		RFC1157VarBindList  *variableBindings);

public:
	HINSTANCE             m_hInst;
	HINSTANCE             m_hInst_snmputil;

private:	
	pSnmpExtensionInit    m_Init;
	pSnmpExtensionInitEx  m_InitEx;
	pSnmpExtensionQuery   m_Query;
	pSnmpExtensionTrap    m_Trap;

public:
	VOID (SNMP_FUNC_TYPE *m_SnmpUtilVarBindFree) (SnmpVarBind *);
	SNMPAPI (SNMP_FUNC_TYPE *m_SnmpUtilOidNCmp) (AsnObjectIdentifier *, AsnObjectIdentifier *, UINT);
	SNMPAPI (SNMP_FUNC_TYPE *m_SnmpUtilOidCpy) (AsnObjectIdentifier *, AsnObjectIdentifier *);
};


class MibII: public MibExtLoad
{
public:
	MibII( bool load_winsock );
	~MibII();
	BOOL Init();

	void GetIPAddress(UINT IpArray[],UINT &IpArraySize);
	void GetIPMask(UINT IpArray[],UINT &IpArraySize);

protected:
	bool m_load_winsock;
};

#endif
