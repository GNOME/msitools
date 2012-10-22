/*** Autogenerated by WIDL 1.5.14 from ../../libmsi/msiserver.idl - Do not edit ***/

#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include <rpc.h>
#include <rpcndr.h>

#ifndef COM_NO_WINDOWS_H
#include <windows.h>
#include <ole2.h>
#endif

#ifndef __msiserver_h__
#define __msiserver_h__

/* Headers for imported files */

#include <unknwn.h>
#include <wtypes.h>
#include <objidl.h>
#include <oaidl.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * MsiServer coclass
 */

DEFINE_GUID(CLSID_MsiServer, 0x000c101c, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);

#ifdef __cplusplus
class DECLSPEC_UUID("000c101c-0000-0000-c000-000000000046") MsiServer;
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(MsiServer, 0x000c101c, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
#endif
#endif

/*****************************************************************************
 * MsiServerMessage coclass
 */

DEFINE_GUID(CLSID_MsiServerMessage, 0x000c101d, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);

#ifdef __cplusplus
class DECLSPEC_UUID("000c101d-0000-0000-c000-000000000046") MsiServerMessage;
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(MsiServerMessage, 0x000c101d, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
#endif
#endif

/*****************************************************************************
 * PSFactoryBuffer coclass
 */

DEFINE_GUID(CLSID_PSFactoryBuffer, 0x000c103e, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);

#ifdef __cplusplus
class DECLSPEC_UUID("000c103e-0000-0000-c000-000000000046") PSFactoryBuffer;
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(PSFactoryBuffer, 0x000c103e, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
#endif
#endif

/*****************************************************************************
 * MsiTransform coclass
 */

DEFINE_GUID(CLSID_MsiTransform, 0x000c1082, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);

#ifdef __cplusplus
class DECLSPEC_UUID("000c1082-0000-0000-c000-000000000046") MsiTransform;
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(MsiTransform, 0x000c1082, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
#endif
#endif

/*****************************************************************************
 * MsiDatabase coclass
 */

DEFINE_GUID(CLSID_MsiDatabase, 0x000c1084, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);

#ifdef __cplusplus
class DECLSPEC_UUID("000c1084-0000-0000-c000-000000000046") MsiDatabase;
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(MsiDatabase, 0x000c1084, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
#endif
#endif

/*****************************************************************************
 * MsiPatch coclass
 */

DEFINE_GUID(CLSID_MsiPatch, 0x000c1086, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);

#ifdef __cplusplus
class DECLSPEC_UUID("000c1086-0000-0000-c000-000000000046") MsiPatch;
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(MsiPatch, 0x000c1086, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
#endif
#endif

DEFINE_GUID(DIID_Session, 0x000c109e, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);

/*****************************************************************************
 * MsiInstaller coclass
 */

DEFINE_GUID(CLSID_MsiInstaller, 0x000c1090, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);

#ifdef __cplusplus
class DECLSPEC_UUID("000c1090-0000-0000-c000-000000000046") MsiInstaller;
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(MsiInstaller, 0x000c1090, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* __msiserver_h__ */