/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: Udp.cpp //////////////////////////////////////////////////////////////
// Implementation of UDP socket wrapper class (taken from wnet lib)
// Author: Matthew D. Campbell, July 2001
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Common/GameEngine.h"
//#include "GameNetwork/NetworkInterface.h"
#include "GameNetwork/udp.h"

#ifndef _WIN32
#include <errno.h>
#include <cstring>
#include <cstdio>
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif


//-------------------------------------------------------------------------

#ifdef DEBUG_LOGGING

#define CASE(x) case (x): return #x;

AsciiString GetWSAErrorString( Int error )
{
	switch (error)
	{
		CASE(WSABASEERR)
		CASE(WSAEINTR)
		CASE(WSAEBADF)
		CASE(WSAEACCES)
		CASE(WSAEFAULT)
		CASE(WSAEINVAL)
		CASE(WSAEMFILE)
		CASE(WSAEWOULDBLOCK)
		CASE(WSAEINPROGRESS)
		CASE(WSAEALREADY)
		CASE(WSAENOTSOCK)
		CASE(WSAEDESTADDRREQ)
		CASE(WSAEMSGSIZE)
		CASE(WSAEPROTOTYPE)
		CASE(WSAENOPROTOOPT)
		CASE(WSAEPROTONOSUPPORT)
		CASE(WSAESOCKTNOSUPPORT)
		CASE(WSAEOPNOTSUPP)
		CASE(WSAEPFNOSUPPORT)
		CASE(WSAEAFNOSUPPORT)
		CASE(WSAEADDRINUSE)
		CASE(WSAEADDRNOTAVAIL)
		CASE(WSAENETDOWN)
		CASE(WSAENETUNREACH)
		CASE(WSAENETRESET)
		CASE(WSAECONNABORTED)
		CASE(WSAECONNRESET)
		CASE(WSAENOBUFS)
		CASE(WSAEISCONN)
		CASE(WSAENOTCONN)
		CASE(WSAESHUTDOWN)
		CASE(WSAETOOMANYREFS)
		CASE(WSAETIMEDOUT)
		CASE(WSAECONNREFUSED)
		CASE(WSAELOOP)
		CASE(WSAENAMETOOLONG)
		CASE(WSAEHOSTDOWN)
		CASE(WSAEHOSTUNREACH)
		CASE(WSAENOTEMPTY)
		CASE(WSAEPROCLIM)
		CASE(WSAEUSERS)
		CASE(WSAEDQUOT)
		CASE(WSAESTALE)
		CASE(WSAEREMOTE)
		CASE(WSAEDISCON)
		CASE(WSASYSNOTREADY)
		CASE(WSAVERNOTSUPPORTED)
		CASE(WSANOTINITIALISED)
		CASE(WSAHOST_NOT_FOUND)
		CASE(WSATRY_AGAIN)
		CASE(WSANO_RECOVERY)
		CASE(WSANO_DATA)
		default:
		{
			AsciiString ret;
			ret.format("Not a Winsock error (%d)", error);
			return ret;
		}
	}
	return AsciiString::TheEmptyString; // will not be hit, ever.
}

#undef CASE

#endif // defined(RTS_DEBUG)

//-------------------------------------------------------------------------

UDP::UDP()
{
  fd=0;
}

UDP::~UDP()
{
	if (fd)
		closesocket(fd);
}

Int UDP::Bind(const char *Host,UnsignedShort port)
{
  struct hostent *hostStruct;
  struct in_addr *hostNode;

  if (isdigit(Host[0]))
    return ( Bind( ntohl(inet_addr(Host)), port) );

  hostStruct = gethostbyname(Host);
  if (hostStruct == nullptr)
    return (0);
  hostNode = (struct in_addr *) hostStruct->h_addr;
  return ( Bind(ntohl(hostNode->s_addr),port) );
}

// You must call bind, implicit binding is for sissies
//   Well... you can get implicit binding if you pass 0 for either arg
Int UDP::Bind(UnsignedInt IP,UnsignedShort Port)
{
  int retval;
  int status;

  IP=htonl(IP);
  Port=htons(Port);

  addr.sin_family=AF_INET;
  addr.sin_port=Port;
#ifndef _WIN32
  // GeneralsX @bugfix POSIX: bind to INADDR_ANY instead of the specific local
  // IP. On Windows a socket bound to a unicast address still receives
  // broadcast datagrams; on BSD/Linux it never does (destination x.x.x.255
  // doesn't match the bound address, so the kernel drops it). LAN discovery
  // depends on receiving those broadcasts. The game's notion of its own IP
  // (LANAPI m_localIP, message payloads) is tracked separately and unaffected.
  addr.sin_addr.s_addr=htonl(INADDR_ANY);
#else
  addr.sin_addr.s_addr=IP;
#endif
  fd=socket(AF_INET,SOCK_DGRAM,DEFAULT_PROTOCOL);
  #ifdef _WIN32
  if (fd==SOCKET_ERROR)
    fd=-1;
  #endif
  if (fd==-1)
    return(UNKNOWN);

  retval=bind(fd,(struct sockaddr *)&addr,sizeof(addr));

  #ifdef _WIN32
  if (retval==SOCKET_ERROR)
	{
    retval=-1;
		m_lastError = WSAGetLastError();
	}
  #endif
  if (retval==-1)
  {
    status=GetStatus();
    //CERR("Bind failure (" << status << ") IP " << IP << " PORT " << Port )
    return(status);
  }

// GeneralsX @bugfix BenderAI 13/02/2026 Use socklen_t for POSIX socket functions (fighter19 pattern)
socklen_t namelen=sizeof(addr);
  retval=SetBlocking(FALSE);
  if (retval==-1)
    fprintf(stderr,"Couldn't set nonblocking mode!\n");

#if defined(__APPLE__) && TARGET_OS_IPHONE
  // GeneralsX @bugfix iOS: sendto() to the limited broadcast address
  // (255.255.255.255) fails with EHOSTUNREACH ("No route to host") on Darwin
  // unless the socket is explicitly bound to an egress interface — the kernel
  // won't guess between Wi-Fi/cellular/hotspot for that ambiguous destination.
  // iOS-only: on a multi-NIC Mac this would also filter *receives* to one
  // interface and drop unicast arriving on the others; macOS routes the
  // subnet-directed broadcast fine without the hint.
  // Prefer 'en0' (the Wi-Fi radio on every iPhone/iPad) over any other
  // active IPv4 interface — other en* interfaces can be USB/debug-tunnel
  // links (e.g. the devicectl/Xcode connection) with no route to the LAN.
  {
    struct ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0)
    {
      const char *chosenName = nullptr;
      unsigned int chosenIndex = 0;

      for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
      {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
          continue;
        if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0)
          continue;

        unsigned int ifIndex = if_nametoindex(ifa->ifa_name);
        if (ifIndex == 0)
          continue;

        if (strcmp(ifa->ifa_name, "en0") == 0)
        {
          chosenName = ifa->ifa_name;
          chosenIndex = ifIndex;
          break; // en0 is always preferred when present
        }
        if (chosenName == nullptr)
        {
          chosenName = ifa->ifa_name;
          chosenIndex = ifIndex;
        }
      }

      if (chosenIndex != 0)
      {
        if (setsockopt(fd, IPPROTO_IP, IP_BOUND_IF, &chosenIndex, sizeof(chosenIndex)) != 0)
        {
          fprintf(stderr, "DEBUG: UDP::Bind() - IP_BOUND_IF to '%s' failed, errno=%d (%s)\n",
            chosenName, errno, strerror(errno));
        }
        else
        {
          fprintf(stderr, "DEBUG: UDP::Bind() - bound egress interface to '%s' (index %u)\n",
            chosenName, chosenIndex);
        }
      }
      freeifaddrs(ifaddr);
    }
  }
#endif

  return(OK);
}

Int UDP::getLocalAddr(UnsignedInt &ip, UnsignedShort &port)
{
  ip=myIP;
  port=myPort;
  return(OK);
}


// private function
Int UDP::SetBlocking(Int block)
{
  #ifdef _WIN32
   unsigned long flag=1;
   if (block)
     flag=0;
   int retval;
   retval=ioctlsocket(fd,FIONBIO,&flag);
   if (retval==SOCKET_ERROR)
     return(UNKNOWN);
   else
     return(OK);
  #else  // UNIX
   int flags = fcntl(fd, F_GETFL, 0);
   if (block==FALSE)          // set nonblocking
     flags |= O_NONBLOCK;
   else                       // set blocking
     flags &= ~(O_NONBLOCK);

   if (fcntl(fd, F_SETFL, flags) < 0)
   {
     return(UNKNOWN);
   }
   return(OK);
  #endif
}


Int UDP::Write(const unsigned char *msg,UnsignedInt len,UnsignedInt IP,UnsignedShort port)
{
  Int retval;
  struct sockaddr_in to;

  // This happens frequently
  if ((IP==0)||(port==0)) return(ADDRNOTAVAIL);

#ifdef _UNIX
  errno=0;
#endif

#if defined(__APPLE__)
  // GeneralsX @bugfix iOS: the limited broadcast address (255.255.255.255) is
  // rejected by the Darwin kernel (EHOSTUNREACH) even with IP_BOUND_IF set.
  // Rewrite it to the bound interface's subnet-directed broadcast address
  // (e.g. 192.168.1.255), which has a concrete route. Cached after first lookup.
  if (IP == 0xFFFFFFFFu)
  {
    static UnsignedInt s_directedBroadcast = 0;
    if (s_directedBroadcast == 0)
    {
      struct ifaddrs *ifaddr = nullptr;
      if (getifaddrs(&ifaddr) == 0)
      {
        UnsignedInt fallback = 0;
        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
          if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
          if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0)
            continue;
          if ((ifa->ifa_flags & IFF_BROADCAST) == 0 || ifa->ifa_broadaddr == nullptr)
            continue;

          UnsignedInt bcast = ntohl(((struct sockaddr_in *)ifa->ifa_broadaddr)->sin_addr.s_addr);
          if (bcast == 0 || bcast == 0xFFFFFFFFu)
            continue;

          if (strcmp(ifa->ifa_name, "en0") == 0)
          {
            s_directedBroadcast = bcast;
            break; // en0 (Wi-Fi) always wins
          }
          if (fallback == 0)
            fallback = bcast;
        }
        freeifaddrs(ifaddr);
        if (s_directedBroadcast == 0)
          s_directedBroadcast = fallback;
      }
    }
    if (s_directedBroadcast != 0)
      IP = s_directedBroadcast;
  }
#endif

  to.sin_port=htons(port);
  to.sin_addr.s_addr=htonl(IP);
  to.sin_family=AF_INET;

  ClearStatus();
  retval=sendto(fd,(const char *)msg,len,0,(struct sockaddr *)&to,sizeof(to));
  #ifdef _WIN32
  if (retval==SOCKET_ERROR)
	{
    retval=-1;
		m_lastError = WSAGetLastError();
#ifdef DEBUG_LOGGING
		static Int errCount = 0;
#endif
		DEBUG_ASSERTLOG(errCount++ > 100, ("UDP::Write() - WSA error is %s", GetWSAErrorString(WSAGetLastError()).str()));
	}
  #else
  if (retval < 0)
	{
		m_lastError = errno;
		// Log the first few failures only — an unreachable peer would otherwise
		// spam one line per packet for the rest of the session.
		static Int errCount = 0;
		if (errCount++ < 10)
		{
			fprintf(stderr, "UDP::Write() - sendto() to %d.%d.%d.%d:%d failed, errno=%d (%s)\n",
				(IP >> 24) & 0xFF, (IP >> 16) & 0xFF, (IP >> 8) & 0xFF, IP & 0xFF, port,
				errno, strerror(errno));
		}
	}
  #endif

  return(retval);
}

Int UDP::Read(unsigned char *msg,UnsignedInt len,sockaddr_in *from)
{
  Int retval;
  // GeneralsX @bugfix BenderAI 13/02/2026 Use socklen_t for POSIX socket functions (fighter19 pattern)
  socklen_t alen=sizeof(sockaddr_in);

  if (from!=nullptr)
  {
    retval=recvfrom(fd,(char *)msg,len,0,(struct sockaddr *)from,&alen);
    #ifdef _WIN32
    if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				// failing because of a blocking error isn't really such a bad thing.
				m_lastError = WSAGetLastError();
#ifdef DEBUG_LOGGING
				static Int errCount = 0;
#endif
				DEBUG_ASSERTLOG(errCount++ > 100, ("UDP::Read() - WSA error is %s", GetWSAErrorString(WSAGetLastError()).str()));
				retval = -1;
			} else {
				retval = 0;
			}
		}
    #endif
  }
  else
  {
    retval=recvfrom(fd,(char *)msg,len,0,nullptr,nullptr);
    #ifdef _WIN32
    if (retval==SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				// failing because of a blocking error isn't really such a bad thing.
				m_lastError = WSAGetLastError();
#ifdef DEBUG_LOGGING
				static Int errCount = 0;
#endif
				DEBUG_ASSERTLOG(errCount++ > 100, ("UDP::Read() - WSA error is %s", GetWSAErrorString(WSAGetLastError()).str()));
				retval = -1;
			} else {
				retval = 0;
			}
		}
    #endif
  }
  return(retval);
}


void UDP::ClearStatus()
{
  #ifndef _WIN32
  errno=0;
  #endif

	m_lastError = 0;
}

UDP::sockStat UDP::GetStatus()
{
	Int status = m_lastError;
 #ifdef _WIN32
  //int status=WSAGetLastError();
  switch (status) {
    case NO_ERROR:
      return OK;
    case WSAEINTR:
      return INTR;
    case WSAEINPROGRESS:
      return INPROGRESS;
    case WSAECONNREFUSED:
      return CONNREFUSED;
    case WSAEINVAL:
      return INVAL;
    case WSAEISCONN:
      return ISCONN;
    case WSAENOTSOCK:
      return NOTSOCK;
    case WSAETIMEDOUT:
      return TIMEDOUT;
    case WSAEALREADY:
      return ALREADY;
    case WSAEWOULDBLOCK:
      return WOULDBLOCK;
    case WSAEBADF:
      return BADF;
    default:
      return (UDP::sockStat)status;
  }
 #else
  //int status=errno;
  switch (status) {
    case 0:
      return OK;
    case EINTR:
      return INTR;
    case EINPROGRESS:
      return INPROGRESS;
    case ECONNREFUSED:
      return CONNREFUSED;
    case EINVAL:
      return INVAL;
    case EISCONN:
      return ISCONN;
    case ENOTSOCK:
      return NOTSOCK;
    case ETIMEDOUT:
      return TIMEDOUT;
    case EALREADY:
      return ALREADY;
    case EAGAIN:
      return AGAIN;
    // GeneralsX @bugfix BenderAI 13/02/2026 EWOULDBLOCK == EAGAIN on Linux (duplicate case)
    #if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
      return WOULDBLOCK;
    #endif
    case EBADF:
      return BADF;
    default:
      return UNKNOWN;
  }
 #endif
}



/*
//
// Wait for net activity on this socket
//
int UDP::Wait(Int sec,Int usec,fd_set &returnSet)
{
  fd_set inputSet;

  FD_ZERO(&inputSet);
  FD_SET(fd,&inputSet);

  return(Wait(sec,usec,inputSet,returnSet));
}
*/

/*
//
// Wait for net activity on a list of sockets
//
int UDP::Wait(Int sec,Int usec,fd_set &givenSet,fd_set &returnSet)
{
  Wtime        timeout,timenow,timethen;
  fd_set       backupSet;
  int          retval=0,done,givenMax;
  Bool         noTimeout=FALSE;
  timeval      tv;

  returnSet=givenSet;
  backupSet=returnSet;

  if ((sec==-1)&&(usec==-1))
    noTimeout=TRUE;

  timeout.SetSec(sec);
  timeout.SetUsec(usec);
  timethen+=timeout;

  givenMax=fd;
  for (UnsignedInt i=0; i<(sizeof(fd_set)*8); i++)   // i=maxFD+1
  {
    if (FD_ISSET(i,&givenSet))
      givenMax=i;
  }
  ///DBGMSG("WAIT  fd="<<fd<<"  givenMax="<<givenMax);

  done=0;
  while( ! done)
  {
    if (noTimeout)
      retval=select(givenMax+1,&returnSet,0,0,nullptr);
    else
    {
      timeout.GetTimevalMT(tv);
      retval=select(givenMax+1,&returnSet,0,0,&tv);
    }

    if (retval>=0)
      done=1;

    else if ((retval==-1)&&(errno==EINTR))  // in case of signal
    {
      if (noTimeout==FALSE)
      {
        timenow.Update();
        timeout=timethen-timenow;
      }
      if ((noTimeout==FALSE)&&(timenow.GetSec()==0)&&(timenow.GetUsec()==0))
        done=1;
      else
        returnSet=backupSet;
    }
    else  // maybe out of memory?
    {
      done=1;
    }
  }
  ///DBGMSG("Wait retval: "<<retval);
  return(retval);
}
*/




// Set the kernel buffer sizes for incoming, and outgoing packets
//
// Linux seems to have a buffer max of 32767 bytes for this,
//  (which is the default). If you try and set the size to
//  greater than the default it just sets it to 32767.

Int UDP::SetInputBuffer(UnsignedInt bytes)
{
   int retval,arg=bytes;

   retval=setsockopt(fd,SOL_SOCKET,SO_RCVBUF,
     (char *)&arg,sizeof(int));
   if (retval==0)
     return(TRUE);
   else
     return(FALSE);
}

// Same note goes for the output buffer

Int UDP::SetOutputBuffer(UnsignedInt bytes)
{
   int retval,arg=bytes;

   retval=setsockopt(fd,SOL_SOCKET,SO_SNDBUF,
     (char *)&arg,sizeof(int));
   if (retval==0)
     return(TRUE);
   else
     return(FALSE);
}

// Get the system buffer sizes

int UDP::GetInputBuffer()
{
   int retval,arg=0;
   // GeneralsX @bugfix BenderAI 13/02/2026 Use socklen_t for POSIX socket functions (fighter19 pattern)
   socklen_t len=sizeof(int);

   retval=getsockopt(fd,SOL_SOCKET,SO_RCVBUF,
     (char *)&arg,&len);
   return(arg);
}


int UDP::GetOutputBuffer()
{
   int retval,arg=0;
   // GeneralsX @bugfix BenderAI 13/02/2026 Use socklen_t for POSIX socket functions (fighter19 pattern)
   socklen_t len=sizeof(int);

   retval=getsockopt(fd,SOL_SOCKET,SO_SNDBUF,
     (char *)&arg,&len);
   return(arg);
}

Int UDP::AllowBroadcasts(Bool status)
{
	int retval;
	BOOL val = status;
	retval = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char *)&val, sizeof(BOOL));
	if (retval == 0)
		return TRUE;
	else
		return FALSE;
}
