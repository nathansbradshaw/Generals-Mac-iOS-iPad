# NGMP file inventory (T0.2)

Source: `references/generalsonline-gameclient` @ b7cfeaf0, 2026-06-20
Paths relative to `GeneralsMD/Code/GameEngine/` in their fork.

## (a) NGMP code — import in T3.2

```
Include/GameNetwork/GeneralsOnline/GeneralsOnline_Settings.h
Include/GameNetwork/GeneralsOnline/HTTP/HTTPManager.h
Include/GameNetwork/GeneralsOnline/HTTP/HTTPRequest.h
Include/GameNetwork/GeneralsOnline/NGMPGame.h
Include/GameNetwork/GeneralsOnline/NGMP_include.h
Include/GameNetwork/GeneralsOnline/NGMP_interfaces.h
Include/GameNetwork/GeneralsOnline/NGMP_types.h
Include/GameNetwork/GeneralsOnline/NetworkBitstream.h
Include/GameNetwork/GeneralsOnline/NetworkMesh.h
Include/GameNetwork/GeneralsOnline/NetworkPacket.h
Include/GameNetwork/GeneralsOnline/NextGenMP_defines.h
Include/GameNetwork/GeneralsOnline/NextGenTransport.h
Include/GameNetwork/GeneralsOnline/OnlineServices_Auth.h
Include/GameNetwork/GeneralsOnline/OnlineServices_Init.h
Include/GameNetwork/GeneralsOnline/OnlineServices_LobbyInterface.h
Include/GameNetwork/GeneralsOnline/OnlineServices_MatchmakingInterface.h
Include/GameNetwork/GeneralsOnline/OnlineServices_RoomsInterface.h
Include/GameNetwork/GeneralsOnline/OnlineServices_SocialInterface.h
Include/GameNetwork/GeneralsOnline/OnlineServices_StatsInterface.h
Include/GameNetwork/GeneralsOnline/json.hpp
Source/GameNetwork/GeneralsOnline/GeneralsOnline_Settings.cpp
Source/GameNetwork/GeneralsOnline/HTTP/HTTPManager.cpp
Source/GameNetwork/GeneralsOnline/HTTP/HTTPRequest.cpp
Source/GameNetwork/GeneralsOnline/NGMPGame.cpp
Source/GameNetwork/GeneralsOnline/NGMP_Helpers.cpp
Source/GameNetwork/GeneralsOnline/NetworkBitstream.cpp
Source/GameNetwork/GeneralsOnline/NetworkMesh.cpp
Source/GameNetwork/GeneralsOnline/NextGenTransport.cpp
Source/GameNetwork/GeneralsOnline/OnlineServices_Auth.cpp
Source/GameNetwork/GeneralsOnline/OnlineServices_Init.cpp
Source/GameNetwork/GeneralsOnline/OnlineServices_LobbyInterface.cpp
Source/GameNetwork/GeneralsOnline/OnlineServices_MatchmakingInterface.cpp
Source/GameNetwork/GeneralsOnline/OnlineServices_RoomsInterface.cpp
Source/GameNetwork/GeneralsOnline/OnlineServices_SocialInterface.cpp
Source/GameNetwork/GeneralsOnline/OnlineServices_StatsInterface.cpp
```

Note: `json.hpp` (nlohmann/json, MIT) rides along in (a) since it's a single header in the NGMP include dir.

## (b) Vendor — replace or import selectively

### (b1) Windows prebuilt binaries — DO NOT import; replaced by vcpkg ports
(GameNetworkingSockets, libcurl, openssl, protobuf, abseil, libsodium, zlib)
```
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/GameNetworkingSockets.dll
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/GameNetworkingSockets.lib
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/abseil_dll.dll
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/abseil_dll.lib
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/libcrypto-3.dll
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/libcrypto.lib
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/libprotobuf.dll
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/libprotobuf.lib
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/libssl-3.dll
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/libssl.lib
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steamwebrtc.lib
Source/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/webrtc-lite.lib
Source/GameNetwork/GeneralsOnline/Vendor/libcurl/libcrypto-3.dll
Source/GameNetwork/GeneralsOnline/Vendor/libcurl/libcurl.dll
Source/GameNetwork/GeneralsOnline/Vendor/libcurl/libcurl.lib
Source/GameNetwork/GeneralsOnline/Vendor/libcurl/libcurl.pdb
Source/GameNetwork/GeneralsOnline/Vendor/libcurl/libssl-3.dll
Source/GameNetwork/GeneralsOnline/Vendor/libcurl/zlib1.dll
Source/GameNetwork/GeneralsOnline/Vendor/libsodium/libsodium.lib
```

### (b2) Vendor headers — import only those the NGMP code #includes
```
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/isteamnetworkingmessages.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/isteamnetworkingsockets.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/isteamnetworkingutils.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/steam_api_common.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/steamclientpublic.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/steamnetworkingcustomsignaling.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/steamnetworkingsockets.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/steamnetworkingsockets_flat.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/steamnetworkingtypes.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/steamtypes.h
Include/GameNetwork/GeneralsOnline/Vendor/ValveNetworkingSockets/steam/steamuniverse.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/curl.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/curlver.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/easy.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/header.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/mprintf.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/multi.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/options.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/stdcheaders.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/system.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/typecheck-gcc.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/urlapi.h
Include/GameNetwork/GeneralsOnline/Vendor/libcurl/websockets.h
Include/GameNetwork/GeneralsOnline/Vendor/stb_image/stb_image_resize.h
Include/GameNetwork/GeneralsOnline/Vendor/stb_image/stb_image_write.h
```

### (b3) Vendored NAT-traversal sources (libplum, miniupnpc, libnatpmp) — LAN MVP likely stubs these; decide during T3.3 burn-down
```
Source/GameNetwork/GeneralsOnline/Vendor/libnatpmp/getgateway.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libnatpmp/natpmp.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libnatpmp/wingettimeofday.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/addr.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/client.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/dummytls.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/http.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/log.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/natpmp.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/net.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/noprotocol.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/pcp.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/plum.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/random.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/tcp.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/timestamp.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/udp.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/upnp.cpp
Source/GameNetwork/GeneralsOnline/Vendor/libplum/util.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/addr_is_reserved.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/connecthostport.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/igd_desc_parse.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/listdevices.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/minisoap.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/minissdpc.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/miniupnpc.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/miniwget.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/minixml.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/portlistingparse.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/receivedata.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/upnpc.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/upnpcommands.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/upnpdev.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/upnperrors.cpp
Source/GameNetwork/GeneralsOnline/Vendor/miniupnpc/upnpreplyparse.cpp
```

## (c) Anti-cheat / telemetry — DO NOT import; stub call sites
```
Include/GameNetwork/GeneralsOnline/Vendor/sentry/sentry.h
Source/GameNetwork/GeneralsOnline/Vendor/sentry/sentry.lib
Include/GameNetwork/GeneralsOnline/PluginInterfaces.h
Source/GameNetwork/GeneralsOnline/PluginInterfaces.cpp
```

`PluginInterfaces.*` is the anti-cheat plugin interface (their AntiCheatPlugin_* repos implement it) — verify at import time; if lobby code requires the type, import the header but stub the implementation.
